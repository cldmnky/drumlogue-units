// TSF_IMPLEMENTATION + TSF_STATIC: this TU needs the struct definitions
// (for tsf_kill_note and voice_process_envelopes) but doesn't export any
// TSF symbols. The public extern implementations are linked from unit.o.
// IMPORTANT: these macros must be defined BEFORE any include of tsf.h,
// including indirect includes via druteus_state.h, because tsf.h has a
// header guard that prevents re-processing.
#include <arm_neon.h>
#define TSF_IMPLEMENTATION
#define TSF_NO_STDIO
#define TSF_STATIC
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#include "tsf.h"
#pragma GCC diagnostic pop

#include "voice_engine.h"
#include "druteus_state.h"
#include "params.h"
#include "patch_engine.h"
#include "dsp_primitives.h"
#include "tools/proteus_instrument_map.h"

common::VoiceAllocatorCore voice_allocator;
uint64_t sample_count = 0;
uint16_t last_pitch_bend = 8192;
VoiceEnv voice_env[16] = {};
int active_notes = 0;

void tsf_kill_note(tsf* f, int channel, int preset, int key) {
  for (int i = 0; i < (int)f->voiceNum; i++) {
    tsf_voice* v = &f->voices[i];
    if (v->playingPreset == preset && v->playingKey == key &&
        v->playingChannel == channel) {
      v->ampGain = 0.0f;
      tsf_voice_kill(v);
    }
  }
}

void voice_process_envelopes() {
  if (cached_env_enabled) {
    float atk_samples  = env_time_to_samples_attack(cached_env_atk);
    float hold_samples = env_time_to_samples_hold(cached_env_hold);
    float dec_samples  = env_time_to_samples_decay(cached_env_dec);
    float rel_samples  = env_time_to_samples_release(cached_env_rel);
    float sus_level    = cached_env_sus / 99.0f;
    float note_gain_pri[128] = {};
    float note_gain_sec[128] = {};

    for (int vi = 0; vi < 16; vi++) {
      if (!voice_env[vi].active) continue;
      float level;
      uint64_t note_on  = voice_env[vi].note_on_sample;
      uint64_t note_off = voice_env[vi].note_off_sample;

      if (note_off == 0 || sample_count < note_off) {
        level = env_level_at_sample(sample_count,
            atk_samples, hold_samples, dec_samples, sus_level, note_on);
      } else {
        uint64_t rel_elapsed = sample_count - note_off;
        if (rel_elapsed < (uint64_t)rel_samples) {
          float rel_t = (float)rel_elapsed / rel_samples;
          level = voice_env[vi].release_start_level * (1.0f - rel_t);
        } else {
          level = 0.0f;
          voice_env[vi].active = false;
          voice_allocator.SetVoiceActive((uint8_t)vi, false);
          tsf_kill_note(soundfont, 0, voice_preset_primary, voice_env[vi].note);
          if (patch_has_secondary)
            tsf_kill_note(soundfont, 1, voice_preset_secondary, voice_env[vi].note);
        }
      }

      note_gain_pri[voice_env[vi].note] = level;
      if (patch_has_secondary)
        note_gain_sec[voice_env[vi].note] = level;
    }

    for (int i = 0; i < (int)soundfont->voiceNum; i++) {
      tsf_voice* v = &soundfont->voices[i];
      if (v->playingPreset == -1) continue;
      if (v->playingChannel == 0 && v->playingPreset == voice_preset_primary)
        v->ampGain = note_gain_pri[v->playingKey];
      else if (v->playingChannel == 1 && v->playingPreset == voice_preset_secondary)
        v->ampGain = note_gain_sec[v->playingKey];
    }
  }

  if (patch_has_secondary && cached_env2_enabled) {
    float atk2_samples  = env_time_to_samples_attack(cached_env2_atk);
    float hold2_samples = env_time_to_samples_hold(cached_env2_hold);
    float dec2_samples  = env_time_to_samples_decay(cached_env2_dec);
    float rel2_samples  = env_time_to_samples_release(cached_env2_rel);
    float sus2_level    = cached_env2_sus / 99.0f;
    float note_gain_sec2[128] = {};

    for (int vi = 0; vi < 16; vi++) {
      if (!voice_env[vi].active) continue;
      float level2;
      uint64_t note2_on  = voice_env[vi].note2_on_sample;
      uint64_t note2_off = voice_env[vi].note2_off_sample;

      if (note2_off == 0 || sample_count < note2_off) {
        level2 = env_level_at_sample(sample_count,
            atk2_samples, hold2_samples, dec2_samples, sus2_level, note2_on);
      } else {
        uint64_t rel2_elapsed = sample_count - note2_off;
        if (rel2_elapsed < (uint64_t)rel2_samples) {
          float rel2_t = (float)rel2_elapsed / rel2_samples;
          level2 = voice_env[vi].release2_start_level * (1.0f - rel2_t);
        } else {
          level2 = 0.0f;
        }
      }

      note_gain_sec2[voice_env[vi].note] = level2;
    }

    for (int i = 0; i < (int)soundfont->voiceNum; i++) {
      tsf_voice* v = &soundfont->voices[i];
      if (v->playingPreset == -1) continue;
      if (v->playingChannel == 1 && v->playingPreset == voice_preset_secondary)
        v->ampGain = note_gain_sec2[v->playingKey];
    }
  }
}

static int8_t find_youngest_active_voice_for_note(uint8_t midi_note) {
  int8_t found = -1;
  uint32_t youngest_time = 0;
  for (uint8_t i = 0; i < voice_allocator.GetMaxVoices(); ++i) {
    const common::VoiceSlot& slot = voice_allocator.GetVoice(i);
    if (!slot.active || slot.midi_note != midi_note)
      continue;
    if (found < 0 || slot.note_on_time > youngest_time) {
      found = static_cast<int8_t>(i);
      youngest_time = slot.note_on_time;
    }
  }
  return found;
}

static float s_apply_velocity_curve(uint8_t velocity) {
  float v = velocity * (1.f / 127.f);
  switch (Params[param_velocity_curve]) {
    case 0:  return v;
    case 1:  return v * v;
    case 2:  return sqrtf(v);
    case 3:  return 0.5f + 0.5f * v;
    case 4:  return v * v * v;
    default: return v;
  }
}

static void s_trigger_note(uint8_t note, uint8_t velocity) {
  if (soundfont == nullptr)
    return;

  int8_t transpose = (int8_t)Params[param_transpose];
  int adjusted = (int)note + transpose;
  if (adjusted < 0)   adjusted = 0;
  if (adjusted > 127) adjusted = 127;

  float vel = s_apply_velocity_curve(velocity);

  common::NoteOnResult result = voice_allocator.NoteOn(note, velocity);
  if (result.voice_index < 0)
    return;

  active_notes = 0;
  for (uint8_t i = 0; i < voice_allocator.GetMaxVoices(); ++i) {
    if (voice_allocator.GetVoice(i).active)
      active_notes++;
  }

  if (result.voice_index >= 0 && result.voice_index < 16) {
    if (voice_env[result.voice_index].active) {
      uint8_t old_note = voice_env[result.voice_index].note;
      tsf_kill_note(soundfont, 0, voice_preset_primary, old_note);
      if (patch_has_secondary)
        tsf_kill_note(soundfont, 1, voice_preset_secondary, old_note);
    }
    voice_env[result.voice_index].active             = true;
    voice_env[result.voice_index].note               = (uint8_t)adjusted;
    voice_env[result.voice_index].note_on_sample     = sample_count;
    voice_env[result.voice_index].note_off_sample    = 0;
    voice_env[result.voice_index].release_start_level = 1.0f;
    voice_env[result.voice_index].note2_on_sample    = sample_count;
    voice_env[result.voice_index].note2_off_sample   = 0;
    voice_env[result.voice_index].release2_start_level = 1.0f;
    lfo_delay_completed = 0.0f;
  }

  float vel0 = vel, vel1 = vel;

  uint8_t xfade  = Params[param_xfade];
  uint8_t layers = Params[param_layers];

  uint8_t crossfade_mode = (xfade > 0) ? xfade : current_patch.crossfademode;
  float primary_weight = 1.0f;
  float secondary_weight = 1.0f;
  compute_crossfade_weights(crossfade_mode, velocity, (uint8_t)adjusted,
                             current_patch.switchpoint,
                             current_patch.crossfadebalance,
                             current_patch.crossfadeamount,
                             current_patch.crossfadedirection,
                             &primary_weight,
                             &secondary_weight);
  vel0 *= primary_weight;
  vel1 *= secondary_weight;

  bool play_primary = true, play_secondary = true;
  if (layers == 1)       play_secondary = false;
  else if (layers == 2)  play_primary = false;
  if (current_patch.i1lowkey > 0 || current_patch.i1highkey < 127) {
    play_primary = play_primary &&
                   ((uint8_t)adjusted >= current_patch.i1lowkey &&
                    (uint8_t)adjusted <= current_patch.i1highkey);
  }
  if (patch_has_secondary && (current_patch.i2lowkey > 0 || current_patch.i2highkey < 127)) {
    play_secondary = play_secondary &&
                     ((uint8_t)adjusted >= current_patch.i2lowkey &&
                      (uint8_t)adjusted <= current_patch.i2highkey);
  }

  if (play_primary && vel0 > 0.0f)
    tsf_channel_note_on(soundfont, 0, (uint8_t)adjusted, vel0);
  if (patch_has_secondary && play_secondary && vel1 > 0.0f)
    tsf_channel_note_on(soundfont, 1, (uint8_t)adjusted, vel1);
}

void voice_init() {
  voice_allocator.Init(16);
  voice_allocator.SetMode(common::VoiceMode::Polyphonic);
  voice_allocator.SetAllocationStrategy(common::VoiceAllocationStrategy::OldestNote);
  sample_count    = 0;
  last_pitch_bend = 8192;
  active_notes    = 0;
  memset(voice_env, 0, sizeof(voice_env));
}

void voice_reset() {
  sample_count  = 0;
  memset(voice_env, 0, sizeof(voice_env));
  active_notes  = 0;
}

void voice_note_on(uint8_t note, uint8_t velocity) {
  s_trigger_note(note, velocity);
}

void voice_note_off(uint8_t note) {
  voice_allocator.NoteOff(note);

  int8_t released_voice_idx = find_youngest_active_voice_for_note(note);
  if (released_voice_idx >= 0)
    voice_allocator.SetVoiceActive((uint8_t)released_voice_idx, false);

  active_notes = 0;
  for (uint8_t i = 0; i < voice_allocator.GetMaxVoices(); ++i) {
    if (voice_allocator.GetVoice(i).active)
      active_notes++;
  }

  int8_t trans = (int8_t)Params[param_transpose];
  int adj = (int)note + trans;
  if (adj < 0) adj = 0;
  if (adj > 127) adj = 127;

  if (cached_env_enabled) {
    float atk_s = env_time_to_samples_attack(cached_env_atk);
    float hld_s = env_time_to_samples_hold(cached_env_hold);
    float dec_s = env_time_to_samples_decay(cached_env_dec);
    float sus_l = cached_env_sus / 99.0f;
    if (released_voice_idx >= 0 && released_voice_idx < 16 &&
        voice_env[released_voice_idx].active &&
        voice_env[released_voice_idx].note == (uint8_t)adj &&
        voice_env[released_voice_idx].note_off_sample == 0) {
      voice_env[released_voice_idx].release_start_level = env_level_at_sample(
            sample_count, atk_s, hld_s, dec_s, sus_l,
          voice_env[released_voice_idx].note_on_sample);
      voice_env[released_voice_idx].note_off_sample = sample_count;
    } else {
      for (int i = 0; i < 16; i++) {
        if (voice_env[i].active && voice_env[i].note == (uint8_t)adj &&
            voice_env[i].note_off_sample == 0) {
          voice_env[i].release_start_level = env_level_at_sample(
            sample_count, atk_s, hld_s, dec_s, sus_l,
              voice_env[i].note_on_sample);
          voice_env[i].note_off_sample = sample_count;
          break;
        }
      }
    }
  }

  if (cached_env2_enabled) {
    float atk_s = env_time_to_samples_attack(cached_env2_atk);
    float hld_s = env_time_to_samples_hold(cached_env2_hold);
    float dec_s = env_time_to_samples_decay(cached_env2_dec);
    float sus_l = cached_env2_sus / 99.0f;
    if (released_voice_idx >= 0 && released_voice_idx < 16 &&
        voice_env[released_voice_idx].active &&
        voice_env[released_voice_idx].note == (uint8_t)adj &&
        voice_env[released_voice_idx].note2_off_sample == 0) {
      voice_env[released_voice_idx].release2_start_level = env_level_at_sample(
            sample_count, atk_s, hld_s, dec_s, sus_l,
          voice_env[released_voice_idx].note2_on_sample);
      voice_env[released_voice_idx].note2_off_sample = sample_count;
    } else {
      for (int i = 0; i < 16; i++) {
        if (voice_env[i].active && voice_env[i].note == (uint8_t)adj &&
            voice_env[i].note2_off_sample == 0) {
          voice_env[i].release2_start_level = env_level_at_sample(
            sample_count, atk_s, hld_s, dec_s, sus_l,
              voice_env[i].note2_on_sample);
          voice_env[i].note2_off_sample = sample_count;
          break;
        }
      }
    }
  }

  if (soundfont != nullptr) {
    tsf_channel_note_off(soundfont, 0, (uint8_t)adj);
    if (patch_has_secondary)
      tsf_channel_note_off(soundfont, 1, (uint8_t)adj);
  }
}

void voice_gate_off() {
  const uint8_t gate_midi_note = 60;
  int8_t transpose = (int8_t)Params[param_transpose];
  int note = (int)gate_midi_note + transpose;
  if (note < 0) note = 0;
  if (note > 127) note = 127;

  voice_allocator.NoteOff(gate_midi_note);

  int8_t released_voice_idx = find_youngest_active_voice_for_note(gate_midi_note);
  if (released_voice_idx >= 0)
    voice_allocator.SetVoiceActive((uint8_t)released_voice_idx, false);

  active_notes = 0;
  for (uint8_t i = 0; i < voice_allocator.GetMaxVoices(); ++i) {
    if (voice_allocator.GetVoice(i).active)
      active_notes++;
  }

  if (cached_env_enabled) {
    float atk_s = env_time_to_samples_attack(cached_env_atk);
    float hld_s = env_time_to_samples_hold(cached_env_hold);
    float dec_s = env_time_to_samples_decay(cached_env_dec);
    float sus_l = cached_env_sus / 99.0f;
    if (released_voice_idx >= 0 && released_voice_idx < 16 &&
        voice_env[released_voice_idx].active &&
        voice_env[released_voice_idx].note == (uint8_t)note &&
        voice_env[released_voice_idx].note_off_sample == 0) {
      voice_env[released_voice_idx].release_start_level = env_level_at_sample(
            sample_count, atk_s, hld_s, dec_s, sus_l,
          voice_env[released_voice_idx].note_on_sample);
      voice_env[released_voice_idx].note_off_sample = sample_count;
    } else {
      for (int i = 0; i < 16; i++) {
        if (voice_env[i].active && voice_env[i].note == (uint8_t)note &&
            voice_env[i].note_off_sample == 0) {
          voice_env[i].release_start_level = env_level_at_sample(
            sample_count, atk_s, hld_s, dec_s, sus_l,
              voice_env[i].note_on_sample);
          voice_env[i].note_off_sample = sample_count;
          break;
        }
      }
    }
  }

  if (cached_env2_enabled) {
    float atk_s = env_time_to_samples_attack(cached_env2_atk);
    float hld_s = env_time_to_samples_hold(cached_env2_hold);
    float dec_s = env_time_to_samples_decay(cached_env2_dec);
    float sus_l = cached_env2_sus / 99.0f;
    if (released_voice_idx >= 0 && released_voice_idx < 16 &&
        voice_env[released_voice_idx].active &&
        voice_env[released_voice_idx].note == (uint8_t)note &&
        voice_env[released_voice_idx].note2_off_sample == 0) {
      voice_env[released_voice_idx].release2_start_level = env_level_at_sample(
            sample_count, atk_s, hld_s, dec_s, sus_l,
          voice_env[released_voice_idx].note2_on_sample);
      voice_env[released_voice_idx].note2_off_sample = sample_count;
    } else {
      for (int i = 0; i < 16; i++) {
        if (voice_env[i].active && voice_env[i].note == (uint8_t)note &&
            voice_env[i].note2_off_sample == 0) {
          voice_env[i].release2_start_level = env_level_at_sample(
            sample_count, atk_s, hld_s, dec_s, sus_l,
              voice_env[i].note2_on_sample);
          voice_env[i].note2_off_sample = sample_count;
          break;
        }
      }
    }
  }

  if (soundfont != nullptr) {
    tsf_channel_note_off(soundfont, 0, (uint8_t)note);
    if (patch_has_secondary)
      tsf_channel_note_off(soundfont, 1, (uint8_t)note);
  }
}

void voice_all_note_off() {
  voice_allocator.AllNotesOff();
  for (uint8_t i = 0; i < voice_allocator.GetMaxVoices(); ++i)
    voice_allocator.SetVoiceActive(i, false);
  memset(voice_env, 0, sizeof(voice_env));
  active_notes  = 0;
  if (soundfont != nullptr) {
    tsf_channel_sounds_off_all(soundfont, 0);
    tsf_channel_sounds_off_all(soundfont, 1);
  }
}

void voice_pitch_bend(uint16_t pitch_bend) {
  last_pitch_bend = pitch_bend;
  if (soundfont != nullptr) {
    tsf_channel_set_pitchwheel(soundfont, 0, (int)pitch_bend);
    if (patch_has_secondary)
      tsf_channel_set_pitchwheel(soundfont, 1, (int)pitch_bend);
  }
}

void voice_channel_pressure(uint8_t pressure) {
  if (soundfont != nullptr)
    tsf_channel_midi_control(soundfont, 0, 11, pressure);
}


