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
#include "lfo_engine.h"

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

// Minimum release floor (~5ms at 48kHz) to prevent instantaneous
// cutoff click on patches with release=0 (e.g. Syn Clav).
static constexpr float kMinReleaseSamples = 240.0f;

static void s_apply_realtime_xfade(int vi, float shift, float* pri_gain, float* sec_gain);

void voice_process_envelopes() {
  const float realtime_xfade_shift = lfo_get_realtime_crossfade_shift();
  static float note_gain_pri[128];
  static float note_gain_sec[128];
  static float note_gain_sec2[128];

  if (cached_env_enabled) {
    float atk_samples  = env_time_to_samples_attack(cached_env_atk);
    float hold_samples = env_time_to_samples_hold(cached_env_hold);
    float dec_samples  = env_time_to_samples_decay(cached_env_dec);
    float rel_samples  = env_time_to_samples_release(cached_env_rel);
    if (rel_samples < kMinReleaseSamples)
      rel_samples = kMinReleaseSamples;
    float sus_level    = cached_env_sus / 99.0f;

    for (int vi = 0; vi < 16; vi++) {
      if (!voice_env[vi].active) continue;
      float level;
      uint64_t note_on  = voice_env[vi].note_on_sample;
      uint64_t note_off = voice_env[vi].note_off_sample;

      // Use the explicit `released` flag instead of the
      // `note_off_sample == 0` sentinel — at stream start (or
      // immediately after `unit_reset`) sample_count and the sentinel
      // collide and a held-but-just-released note sustains forever
      // (review #16).
      if (!voice_env[vi].released || sample_count < note_off) {
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

      float pri_gain = level;
      float sec_gain = level;
      s_apply_realtime_xfade(vi, realtime_xfade_shift, &pri_gain, &sec_gain);
      note_gain_pri[voice_env[vi].note] = pri_gain;
      if (patch_has_secondary)
        note_gain_sec[voice_env[vi].note] = sec_gain;
    }

    for (int i = 0; i < (int)soundfont->voiceNum; i++) {
      tsf_voice* v = &soundfont->voices[i];
      if (v->playingPreset == -1) continue;
      if (v->playingChannel == 0 && v->playingPreset == voice_preset_primary)
        v->ampGain = note_gain_pri[v->playingKey];
      else if (v->playingChannel == 1 && v->playingPreset == voice_preset_secondary)
        v->ampGain = note_gain_sec[v->playingKey];
    }
  } else {
    // Envelope disabled: set ampGain to unity so the aux envelope below
    // starts from a clean baseline rather than multiplying TSF's internal
    // gain (which can be loud enough to clip, e.g. Emperor with both layers
    // on the same instrument at high volume).
    for (int vi = 0; vi < 16; vi++) {
      if (!voice_env[vi].active) continue;
      float pri_gain = 1.0f;
      float sec_gain = 1.0f;
      s_apply_realtime_xfade(vi, realtime_xfade_shift, &pri_gain, &sec_gain);
      note_gain_pri[voice_env[vi].note] = pri_gain;
      if (patch_has_secondary)
        note_gain_sec[voice_env[vi].note] = sec_gain;
    }

    for (int i = 0; i < (int)soundfont->voiceNum; i++) {
      tsf_voice* v = &soundfont->voices[i];
      if (v->playingPreset == -1) continue;
      if (v->playingChannel == 0 && v->playingPreset == voice_preset_primary)
        v->ampGain = note_gain_pri[v->playingKey];
    }
  }

  if (patch_has_secondary && cached_env2_enabled) {
    float atk2_samples  = env_time_to_samples_attack(cached_env2_atk);
    float hold2_samples = env_time_to_samples_hold(cached_env2_hold);
    float dec2_samples  = env_time_to_samples_decay(cached_env2_dec);
    float rel2_samples  = env_time_to_samples_release(cached_env2_rel);
    if (rel2_samples < kMinReleaseSamples)
      rel2_samples = kMinReleaseSamples;
    float sus2_level    = cached_env2_sus / 99.0f;

    for (int vi = 0; vi < 16; vi++) {
      if (!voice_env[vi].active) continue;
      float level2;
      uint64_t note2_on  = voice_env[vi].note2_on_sample;
      uint64_t note2_off = voice_env[vi].note2_off_sample;

      if (!voice_env[vi].released2 || sample_count < note2_off) {
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

      float pri_gain = 1.0f;
      float sec_gain = level2;
      s_apply_realtime_xfade(vi, realtime_xfade_shift, &pri_gain, &sec_gain);
      note_gain_sec2[voice_env[vi].note] = sec_gain;
    }

    for (int i = 0; i < (int)soundfont->voiceNum; i++) {
      tsf_voice* v = &soundfont->voices[i];
      if (v->playingPreset == -1) continue;
      if (v->playingChannel == 1 && v->playingPreset == voice_preset_secondary)
        v->ampGain = note_gain_sec2[v->playingKey];
    }
  } else if (patch_has_secondary) {
    for (int vi = 0; vi < 16; vi++) {
      if (!voice_env[vi].active) continue;
      float pri_gain = 1.0f;
      float sec_gain = 1.0f;
      s_apply_realtime_xfade(vi, realtime_xfade_shift, &pri_gain, &sec_gain);
      note_gain_sec[voice_env[vi].note] = sec_gain;
    }

    for (int i = 0; i < (int)soundfont->voiceNum; i++) {
      tsf_voice* v = &soundfont->voices[i];
      if (v->playingPreset == -1) continue;
      if (v->playingChannel == 1 && v->playingPreset == voice_preset_secondary)
        v->ampGain = note_gain_sec[v->playingKey];
    }
  }

  // Aux envelope is now processed through the realtime modulation matrix
  // in lfo_apply_patch_mod, not hardcoded to volume. This fixes patches
  // like EasternSands where AuxEnv is routed to pitch, not volume.
}

void voice_process_pending_notes() {
  for (int i = 0; i < 16; i++) {
    if (!voice_env[i].active) continue;

    if (voice_env[i].note1_pending) {
      uint64_t elapsed = sample_count - voice_env[i].note1_pending_sample;
      uint64_t delay_samples = (uint64_t)lfo_delay_to_samples(current_patch.i1delay);
      if (elapsed >= delay_samples) {
        voice_env[i].note1_pending = false;
        voice_env[i].note_on_sample = sample_count;
        voice_env[i].aux_env_on_sample = sample_count;
        tsf_channel_note_on(soundfont, 0,
            voice_env[i].note1_pending_note, voice_env[i].note1_pending_vel);
        if (voice_env[i].keyvel_sample_start_pri > 0 || current_patch.i1reversesound != 0)
          tsf_voice_set_playback_mode(soundfont, voice_preset_primary,
              voice_env[i].note1_pending_note,
              voice_env[i].keyvel_sample_start_pri,
              current_patch.i1reversesound ? -1 : 1);
      }
    }

    if (voice_env[i].note2_pending) {
      uint64_t elapsed = sample_count - voice_env[i].note2_pending_sample;
      uint64_t delay_samples = (uint64_t)lfo_delay_to_samples(current_patch.i2delay);
      if (elapsed >= delay_samples) {
        voice_env[i].note2_pending = false;
        voice_env[i].note2_on_sample = sample_count;
        voice_env[i].aux_env_on_sample = sample_count;
        tsf_channel_note_on(soundfont, 1,
            voice_env[i].note2_pending_note, voice_env[i].note2_pending_vel);
        if (voice_env[i].keyvel_sample_start_sec > 0 || current_patch.i2reversesound != 0)
          tsf_voice_set_playback_mode(soundfont, voice_preset_secondary,
              voice_env[i].note2_pending_note,
              voice_env[i].keyvel_sample_start_sec,
              current_patch.i2reversesound ? -1 : 1);
      }
    }
  }
}

static void tsf_release_layer(tsf* f, int channel, int preset) {
  for (int i = 0; i < (int)f->voiceNum; i++) {
    tsf_voice* v = &f->voices[i];
    if (v->playingPreset == preset && v->playingChannel == channel && v->ampenv.segment < TSF_SEGMENT_RELEASE)
      tsf_channel_note_off(f, channel, v->playingKey);
  }
}

static void s_apply_realtime_xfade(int vi, float shift, float* pri_gain, float* sec_gain) {
  if (!patch_has_secondary || shift == 0.0f)
    return;
  *pri_gain *= clamp01(voice_env[vi].xfade_pri_weight + shift);
  *sec_gain *= clamp01(voice_env[vi].xfade_sec_weight - shift);
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

static void s_apply_keyvel_slot(
    uint8_t source, uint8_t dest, int8_t amount,
    uint8_t midi_velocity, uint8_t midi_note,
    float& vel0, float& vel1,
    float& pri_weight, float& sec_weight,
    uint8_t& sample_start_pri, uint8_t& sample_start_sec,
    int& aux_amount, int& aux_attack, int& aux_decay, int& aux_release)
{
  if (source == 0 || dest == 0 || amount == 0)
    return;
  float src_norm;
  if (source == 1) {
    /* Key Number: bipolar around keyboard center (manual p.34, 58).
     * Keys above center → positive, keys below → negative. */
    int center = (int)current_patch.keyboardcenter;
    int diff = (int)midi_note - center;
    if (diff >= 0) {
      int range = 127 - center;
      src_norm = (range > 0) ? (float)diff / (float)range : 0.0f;
    } else {
      int range = center;
      src_norm = (range > 0) ? (float)diff / (float)range : 0.0f;
    }
  } else {
    src_norm = midi_velocity / 127.0f;
  }
  float mod_scale = amount * (1.0f / 127.0f);
  float mod_factor = 1.0f + mod_scale * src_norm;
  switch (dest) {
    case 4:
      vel0 *= mod_factor;
      vel1 *= mod_factor;
      break;
    case 5:
      vel0 *= mod_factor;
      break;
    case 6:
      vel1 *= mod_factor;
      break;
    case 16: {
      float shift = mod_scale * src_norm * 0.25f;
      // Apply crossfade modulation directly to vel0/vel1 since pri_weight
      // and sec_weight are already baked into them (lines 382-383).
      vel0 *= (1.0f - shift);
      vel1 *= (1.0f + shift);
      break;
    }
    case 21:
      aux_amount += (int)(mod_scale * src_norm * 127.0f);
      break;
    case 22:
      aux_attack += (int)(mod_scale * src_norm * 99.0f);
      break;
    case 23:
      aux_decay += (int)(mod_scale * src_norm * 99.0f);
      break;
    case 24:
      aux_release += (int)(mod_scale * src_norm * 99.0f);
      break;
    case 26: {
      int modded = (int)sample_start_pri + (int)(mod_scale * src_norm * 128.0f);
      if (modded < 0) modded = 0;
      if (modded > 255) modded = 255;
      sample_start_pri = (uint8_t)modded;
      break;
    }
    case 27: {
      int modded = (int)sample_start_sec + (int)(mod_scale * src_norm * 128.0f);
      if (modded < 0) modded = 0;
      if (modded > 255) modded = 255;
      sample_start_sec = (uint8_t)modded;
      break;
    }
  }
}

static void s_trigger_note(uint8_t note, uint8_t velocity) {
  if (soundfont == nullptr) {
    return;
  }

  int8_t transpose = (int8_t)Params[param_transpose];
  int adjusted = (int)note + transpose;
  if (adjusted < 0)   adjusted = 0;
  if (adjusted > 127) adjusted = 127;

  float vel = s_apply_velocity_curve(velocity);

  common::NoteOnResult result = voice_allocator.NoteOn(note, velocity);
  if (result.voice_index < 0) {
    return;
  }

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
    voice_env[result.voice_index].released           = false;
    voice_env[result.voice_index].released2          = false;
    voice_env[result.voice_index].raw_note           = note;       /* review #9 */
    voice_env[result.voice_index].note               = (uint8_t)adjusted;
    voice_env[result.voice_index].note_on_sample     = sample_count;
    voice_env[result.voice_index].note_off_sample    = 0;
    voice_env[result.voice_index].release_start_level = 1.0f;
    voice_env[result.voice_index].note2_on_sample    = sample_count;
    voice_env[result.voice_index].note2_off_sample   = 0;
    voice_env[result.voice_index].release2_start_level = 1.0f;
    voice_env[result.voice_index].note1_pending      = false;
    voice_env[result.voice_index].note1_pending_note = 0;
    voice_env[result.voice_index].note1_pending_vel  = 0;
    voice_env[result.voice_index].note1_pending_sample = 0;
    voice_env[result.voice_index].note2_pending      = false;
    voice_env[result.voice_index].note2_pending_note = 0;
    voice_env[result.voice_index].note2_pending_vel  = 0;
    voice_env[result.voice_index].note2_pending_sample = 0;
    voice_env[result.voice_index].aux_env_on_sample  = 0;
    voice_env[result.voice_index].aux_env_off_sample = 0;
    voice_env[result.voice_index].aux_env_released   = false;
    voice_env[result.voice_index].aux_env_release_start = 1.0f;
    voice_env[result.voice_index].eff_i3amount  = current_patch.i3amount;
    voice_env[result.voice_index].eff_i3attack  = current_patch.i3attack;
    voice_env[result.voice_index].eff_i3decay   = current_patch.i3decay;
    voice_env[result.voice_index].eff_i3release = current_patch.i3release;
    lfo1_delay_completed = 0.0f;
    lfo2_delay_completed = 0.0f;
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
  voice_env[result.voice_index].xfade_pri_weight = primary_weight;
  voice_env[result.voice_index].xfade_sec_weight = secondary_weight;

  uint8_t kvel_sample_start_pri = current_patch.i1samplestartoffset;
  uint8_t kvel_sample_start_sec = current_patch.i2samplestartoffset;
  int eff_aux_amount  = current_patch.i3amount;
  int eff_aux_attack  = current_patch.i3attack;
  int eff_aux_decay   = current_patch.i3decay;
  int eff_aux_release = current_patch.i3release;

  s_apply_keyvel_slot(current_patch.keyvelsource1, current_patch.keyveldest1,
      current_patch.keyvelamount1, velocity, (uint8_t)adjusted,
      vel0, vel1, primary_weight, secondary_weight,
      kvel_sample_start_pri, kvel_sample_start_sec,
      eff_aux_amount, eff_aux_attack, eff_aux_decay, eff_aux_release);
  s_apply_keyvel_slot(current_patch.keyvelsource2, current_patch.keyveldest2,
      current_patch.keyvelamount2, velocity, (uint8_t)adjusted,
      vel0, vel1, primary_weight, secondary_weight,
      kvel_sample_start_pri, kvel_sample_start_sec,
      eff_aux_amount, eff_aux_attack, eff_aux_decay, eff_aux_release);
  s_apply_keyvel_slot(current_patch.keyvelsource3, current_patch.keyveldest3,
      current_patch.keyvelamount3, velocity, (uint8_t)adjusted,
      vel0, vel1, primary_weight, secondary_weight,
      kvel_sample_start_pri, kvel_sample_start_sec,
      eff_aux_amount, eff_aux_attack, eff_aux_decay, eff_aux_release);
  s_apply_keyvel_slot(current_patch.keyvelsource4, current_patch.keyveldest4,
      current_patch.keyvelamount4, velocity, (uint8_t)adjusted,
      vel0, vel1, primary_weight, secondary_weight,
      kvel_sample_start_pri, kvel_sample_start_sec,
      eff_aux_amount, eff_aux_attack, eff_aux_decay, eff_aux_release);
  s_apply_keyvel_slot(current_patch.keyvelsource5, current_patch.keyveldest5,
      current_patch.keyvelamount5, velocity, (uint8_t)adjusted,
      vel0, vel1, primary_weight, secondary_weight,
      kvel_sample_start_pri, kvel_sample_start_sec,
      eff_aux_amount, eff_aux_attack, eff_aux_decay, eff_aux_release);
  s_apply_keyvel_slot(current_patch.keyvelsource6, current_patch.keyveldest6,
      current_patch.keyvelamount6, velocity, (uint8_t)adjusted,
      vel0, vel1, primary_weight, secondary_weight,
      kvel_sample_start_pri, kvel_sample_start_sec,
      eff_aux_amount, eff_aux_attack, eff_aux_decay, eff_aux_release);

  if (vel0 > 1.0f) vel0 = 1.0f;
  if (vel0 < 0.0f) vel0 = 0.0f;
  if (vel1 > 1.0f) vel1 = 1.0f;
  if (vel1 < 0.0f) vel1 = 0.0f;

  voice_env[result.voice_index].keyvel_volume_mod = 1.0f;
  voice_env[result.voice_index].keyvel_pan_mod    = 0.0f;
  voice_env[result.voice_index].keyvel_tone_mod   = 0.0f;
  voice_env[result.voice_index].keyvel_sample_start_pri = kvel_sample_start_pri;
  voice_env[result.voice_index].keyvel_sample_start_sec = kvel_sample_start_sec;
  voice_env[result.voice_index].eff_i3amount  = (int8_t) Clamp((float)eff_aux_amount, -128.0f, 127.0f);
  voice_env[result.voice_index].eff_i3attack  = (uint8_t) Clamp((float)eff_aux_attack, 0.0f, 99.0f);
  voice_env[result.voice_index].eff_i3decay   = (uint8_t) Clamp((float)eff_aux_decay, 0.0f, 99.0f);
  voice_env[result.voice_index].eff_i3release = (uint8_t) Clamp((float)eff_aux_release, 0.0f, 99.0f);

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

  if (play_primary && current_patch.i1solomode != 0)
    tsf_release_layer(soundfont, 0, voice_preset_primary);
  if (patch_has_secondary && play_secondary && current_patch.i2solomode != 0)
    tsf_release_layer(soundfont, 1, voice_preset_secondary);

  if (play_primary && vel0 > 0.0f) {
    if (current_patch.i1delay > 0) {
      voice_env[result.voice_index].note1_pending = true;
      voice_env[result.voice_index].note1_pending_note = (uint8_t)adjusted;
      voice_env[result.voice_index].note1_pending_vel  = vel0;
      voice_env[result.voice_index].note1_pending_sample = sample_count;
    } else {
      voice_env[result.voice_index].aux_env_on_sample = sample_count;
      tsf_channel_note_on(soundfont, 0, (uint8_t)adjusted, vel0);
      if (soundfont && (kvel_sample_start_pri > 0 || current_patch.i1reversesound != 0))
        tsf_voice_set_playback_mode(soundfont, voice_preset_primary, (uint8_t)adjusted,
            kvel_sample_start_pri,
            current_patch.i1reversesound ? -1 : 1);
    }
  }
  if (patch_has_secondary && play_secondary && vel1 > 0.0f) {
    if (current_patch.i2delay > 0) {
      voice_env[result.voice_index].note2_pending = true;
      voice_env[result.voice_index].note2_pending_note = (uint8_t)adjusted;
      voice_env[result.voice_index].note2_pending_vel  = vel1;
      voice_env[result.voice_index].note2_pending_sample = sample_count;
    } else {
      voice_env[result.voice_index].aux_env_on_sample = sample_count;
      tsf_channel_note_on(soundfont, 1, (uint8_t)adjusted, vel1);
      if (soundfont && (kvel_sample_start_sec > 0 || current_patch.i2reversesound != 0))
        tsf_voice_set_playback_mode(soundfont, voice_preset_secondary, (uint8_t)adjusted,
            kvel_sample_start_sec,
            current_patch.i2reversesound ? -1 : 1);
    }
  }
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

  // Find the voice(s) we just released and use their stored adjusted
  // key (review #9).  The previous code recomputed `adj` from the
  // current transpose, so changing TUNE while a note was held caused
  // a stuck note (wrong key targeted at note-off).
  // Match by raw_note (the note the caller actually sent) so multiple
  // voice slots triggered by the same raw note (e.g. legato) all release.
  if (released_voice_idx < 0) {
    // No allocator record — fall through with a recomputed adj as a
    // best-effort, but mark every voice_env with this raw_note as
    // released via the loop below.
  }

  if (cached_env_enabled) {
    float atk_s = env_time_to_samples_attack(cached_env_atk);
    float hld_s = env_time_to_samples_hold(cached_env_hold);
    float dec_s = env_time_to_samples_decay(cached_env_dec);
    float sus_l = cached_env_sus / 99.0f;
    if (released_voice_idx >= 0 && released_voice_idx < 16 &&
        voice_env[released_voice_idx].active &&
        voice_env[released_voice_idx].raw_note == note &&
        !voice_env[released_voice_idx].released) {
      voice_env[released_voice_idx].release_start_level = env_level_at_sample(
            sample_count, atk_s, hld_s, dec_s, sus_l,
          voice_env[released_voice_idx].note_on_sample);
      voice_env[released_voice_idx].note_off_sample = sample_count;
      voice_env[released_voice_idx].released = true;
    } else {
      for (int i = 0; i < 16; i++) {
        if (voice_env[i].active && voice_env[i].raw_note == note &&
            !voice_env[i].released) {
          voice_env[i].release_start_level = env_level_at_sample(
            sample_count, atk_s, hld_s, dec_s, sus_l,
              voice_env[i].note_on_sample);
          voice_env[i].note_off_sample = sample_count;
          voice_env[i].released = true;
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
        voice_env[released_voice_idx].raw_note == note &&
        !voice_env[released_voice_idx].released2) {
      voice_env[released_voice_idx].release2_start_level = env_level_at_sample(
            sample_count, atk_s, hld_s, dec_s, sus_l,
          voice_env[released_voice_idx].note2_on_sample);
      voice_env[released_voice_idx].note2_off_sample = sample_count;
      voice_env[released_voice_idx].released2 = true;
    } else {
      for (int i = 0; i < 16; i++) {
        if (voice_env[i].active && voice_env[i].raw_note == note &&
            !voice_env[i].released2) {
          voice_env[i].release2_start_level = env_level_at_sample(
            sample_count, atk_s, hld_s, dec_s, sus_l,
              voice_env[i].note2_on_sample);
          voice_env[i].note2_off_sample = sample_count;
          voice_env[i].released2 = true;
          break;
        }
      }
    }
  }

  {
    float aux_delay_s = lfo_delay_to_samples(current_patch.i3delay);
    float aux_atk_s   = env_time_to_samples_attack(current_patch.i3attack);
    float aux_hold_s  = env_time_to_samples_hold(current_patch.i3hold);
    float aux_dec_s   = env_time_to_samples_decay(current_patch.i3decay);
    float aux_sus_l   = (float)current_patch.i3sustain / 99.0f;
    float aux_rel_s   = env_time_to_samples_release(current_patch.i3release);
    for (int i = 0; i < 16; i++) {
      if (voice_env[i].active && voice_env[i].raw_note == note &&
          !voice_env[i].aux_env_released) {
        voice_env[i].aux_env_release_start = aux_env_level_at_sample(
            sample_count,
            aux_delay_s, aux_atk_s, aux_hold_s, aux_dec_s, aux_sus_l, aux_rel_s,
            voice_env[i].aux_env_on_sample, 0, 1.0f);
        voice_env[i].aux_env_off_sample = sample_count;
        voice_env[i].aux_env_released   = true;
        break;
      }
    }
  }

  if (soundfont != nullptr) {
    // Use the stored adjusted key from the released voice — review #9.
    // Fall back to a recomputed adj only if no matching voice_env exists
    // (defensive — shouldn't happen in normal flow).
    int fallback_key = (int)note + (int8_t)Params[param_transpose];
    if (fallback_key < 0) fallback_key = 0;
    if (fallback_key > 127) fallback_key = 127;
    uint8_t key = (uint8_t)fallback_key;
    if (released_voice_idx >= 0 && released_voice_idx < 16 &&
        voice_env[released_voice_idx].active) {
      key = voice_env[released_voice_idx].note;
    } else {
      for (int i = 0; i < 16; i++) {
        if (voice_env[i].active && voice_env[i].raw_note == note) {
          key = voice_env[i].note;
          break;
        }
      }
    }
    tsf_channel_note_off(soundfont, 0, key);
    if (patch_has_secondary)
      tsf_channel_note_off(soundfont, 1, key);
  }
}

void voice_gate_off() {
  const uint8_t gate_midi_note = 60;
  // Use the stored adjusted key from the released voice_env (review #9).
  int8_t released_voice_idx = find_youngest_active_voice_for_note(gate_midi_note);
  uint8_t adj_key = 60;
  if (released_voice_idx >= 0 && released_voice_idx < 16 &&
      voice_env[released_voice_idx].active) {
    adj_key = voice_env[released_voice_idx].note;
  } else {
    int tmp = (int)gate_midi_note + (int8_t)Params[param_transpose];
    if (tmp < 0) tmp = 0;
    if (tmp > 127) tmp = 127;
    adj_key = (uint8_t)tmp;
  }

  voice_allocator.NoteOff(gate_midi_note);

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
        voice_env[released_voice_idx].raw_note == gate_midi_note &&
        !voice_env[released_voice_idx].released) {
      voice_env[released_voice_idx].release_start_level = env_level_at_sample(
            sample_count, atk_s, hld_s, dec_s, sus_l,
          voice_env[released_voice_idx].note_on_sample);
      voice_env[released_voice_idx].note_off_sample = sample_count;
      voice_env[released_voice_idx].released = true;
    } else {
      for (int i = 0; i < 16; i++) {
        if (voice_env[i].active && voice_env[i].raw_note == gate_midi_note &&
            !voice_env[i].released) {
          voice_env[i].release_start_level = env_level_at_sample(
            sample_count, atk_s, hld_s, dec_s, sus_l,
              voice_env[i].note_on_sample);
          voice_env[i].note_off_sample = sample_count;
          voice_env[i].released = true;
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
        voice_env[released_voice_idx].raw_note == gate_midi_note &&
        !voice_env[released_voice_idx].released2) {
      voice_env[released_voice_idx].release2_start_level = env_level_at_sample(
            sample_count, atk_s, hld_s, dec_s, sus_l,
          voice_env[released_voice_idx].note2_on_sample);
      voice_env[released_voice_idx].note2_off_sample = sample_count;
      voice_env[released_voice_idx].released2 = true;
    } else {
      for (int i = 0; i < 16; i++) {
        if (voice_env[i].active && voice_env[i].raw_note == gate_midi_note &&
            !voice_env[i].released2) {
          voice_env[i].release2_start_level = env_level_at_sample(
            sample_count, atk_s, hld_s, dec_s, sus_l,
              voice_env[i].note2_on_sample);
          voice_env[i].note2_off_sample = sample_count;
          voice_env[i].released2 = true;
          break;
        }
      }
    }
  }

  {
    float aux_delay_s = lfo_delay_to_samples(current_patch.i3delay);
    float aux_atk_s   = env_time_to_samples_attack(current_patch.i3attack);
    float aux_hold_s  = env_time_to_samples_hold(current_patch.i3hold);
    float aux_dec_s   = env_time_to_samples_decay(current_patch.i3decay);
    float aux_sus_l   = (float)current_patch.i3sustain / 99.0f;
    float aux_rel_s   = env_time_to_samples_release(current_patch.i3release);
    for (int i = 0; i < 16; i++) {
      if (voice_env[i].active && voice_env[i].raw_note == gate_midi_note &&
          !voice_env[i].aux_env_released) {
        voice_env[i].aux_env_release_start = aux_env_level_at_sample(
            sample_count,
            aux_delay_s, aux_atk_s, aux_hold_s, aux_dec_s, aux_sus_l, aux_rel_s,
            voice_env[i].aux_env_on_sample, 0, 1.0f);
        voice_env[i].aux_env_off_sample = sample_count;
        voice_env[i].aux_env_released   = true;
        break;
      }
    }
  }

  if (soundfont != nullptr) {
    tsf_channel_note_off(soundfont, 0, adj_key);
    if (patch_has_secondary)
      tsf_channel_note_off(soundfont, 1, adj_key);
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


