/**
 * @file unit.cc
 * @brief DRUTEUS drumlogue synth unit — Proteus/1 SF2 playback via TinySoundFont
 *
 * Architecture based on loguetsf.cc by Oleg Burdaev (dukesrg), MIT License.
 * TinySoundFont by Bernhard Schelling, MIT License.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <new>         // placement new for stereo_widener.h
#include <sys/stat.h>
#include <limits.h>  // PATH_MAX

#include "unit.h"
#include "logue_fs.h"
#include "param_format.h"
#include "voice_allocator.h"
#include "../common/stereo_widener.h"
#include "rings/dsp/fx/reverb.h"

// TinySoundFont — single-header SF2 synthesizer.
// TSF_IMPLEMENTATION must be defined in exactly one translation unit.
// TSF_NO_STDIO prevents tsf.h from pulling in stdio (we use it ourselves).
// TSF_STATIC marks all tsf_ functions static to avoid symbol conflicts.
#define TSF_IMPLEMENTATION
#define TSF_NO_STDIO
#define TSF_STATIC
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#include "tsf.h"
#pragma GCC diagnostic pop

// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------

#define SOUNDFONT_PATH   "/var/lib/drumlogued/userfs/Programs"
#define CHUNK_SIZE       131072                    // bytes per render frame during load
#define VELOCITY_SCALE   (1.f / 127.f)
#define OUTPUT_MODE      TSF_STEREO_INTERLEAVED    // drumlogue: stereo interleaved float

// ---------------------------------------------------------------------------
// Parameter indices — must match header.c descriptor order exactly
// ---------------------------------------------------------------------------

enum {
  param_soundfont    = 0,  // Page 1 — sound source
  param_preset       = 1,
  param_max_voices   = 2,
  param_transpose    = 3,
  param_fine_tune    = 4,  // Page 2 — pitch & mix
  param_volume       = 5,
  param_pan          = 6,
  param_unused_7     = 7,
  param_env_attack   = 8,  // Page 3 — envelope
  param_env_decay    = 9,
  param_env_sustain  = 10,
  param_env_release  = 11,
  param_chorus       = 12, // Page 4 — effects & feel
  param_reverb       = 13,
  param_velocity_curve = 14,
  param_unused_15   = 15,
  param_unused_16    = 16,
  param_unused_17    = 17,
  param_unused_18    = 18,
  param_unused_19    = 19,
  param_lfo_rate     = 20,
  param_lfo_amount   = 21,
  param_lfo_dest     = 22,
  param_lfo_wave     = 23,
  param_num,
};

static const int32_t kParamDefaults[param_num] = {
  0,    // SFONT
  0,    // PRESET
  16,   // VOICES
  0,    // TUNE: no transpose
  0,    // FINETN: concert pitch
  100,  // VOLUME
  64,   // PAN: center
  0,    // unused
  0,    // ENV ATK: instant
  0,    // ENV DEC: none
  99,   // ENV SUS: full
  99,   // ENV REL: transparent
  0,    // CHORUS: off
  0,    // REVERB: off
  0,    // V.CURVE: linear
  0,    // unused (was DELAY)
  0,    // unused (was SOLO)
  0,    // unused
  0,    // unused
  0,    // unused
  0,    // LFO RTE: off
  0,    // LFO AMT: off
  0,    // LFO DST: pitch
  1,    // LFO WAV: sine
};

// ---------------------------------------------------------------------------
// Loading state machine
// ---------------------------------------------------------------------------

enum {
  load_idle = 0,
  load_start,     // fopen the SF2 file
  load_alloc,     // fstat + malloc buffer
  load_read,      // fread CHUNK_SIZE bytes per frame until EOF
  load_close,     // fclose + release old TSF instance
  load_tsf_load,  // tsf_load_memory — parse SF2 into TSF
  load_tsf_set,   // configure output, voices, preset, sustain
  load_finished,  // one-frame landing; next frame → load_idle
};

// ---------------------------------------------------------------------------
// Static state
// ---------------------------------------------------------------------------

// SF2 file list — scanned at static construction time.
// count < 0 → directory doesn't exist; count == 0 → no .sf2 files found.
static const char *sf2_prefix = "";
static const char *sf2_suffix = ".sf2";
static fs_dir soundfont_list = fs_dir(SOUNDFONT_PATH, sf2_prefix, sf2_suffix);

// TSF synthesizer instance (one stereo instance for drumlogue).
static tsf *soundfont = nullptr;

// Buffer used during chunked file loading.
static char * __attribute__((aligned(32))) soundfont_buf = nullptr;

// Current parameter values — initialised to kParamDefaults in unit_init.
static int32_t Params[param_num];

// Loading state.
static uint32_t state = load_idle;

// True during unit_suspend(); render returns silence without advancing state.
static bool suspended = false;

// DSP effects — ChorusStereoWidener from drumlogue/common, Reverb from eurorack/rings.
static common::ChorusStereoWidener chorus_dsp;
static rings::Reverb            reverb_dsp;
static uint16_t                 reverb_buffer[32768];
static float                    fx_buf_l[256];  // de-interleave temp
static float                    fx_buf_r[256];
static uint64_t                 sample_count  = 0;
static uint16_t                 last_pitch_bend = 8192;

// Voice allocator for polyphony management.
static common::VoiceAllocatorCore voice_allocator;

// ADSR envelope state.
static bool     env_active           = false;
static uint64_t env_note_on_sample   = 0;
static uint64_t env_note_off_sample  = 0;
static float    env_release_start_level = 1.0f;
static int      env_post_release_frames = 0;  // silence hold after release
static int      active_notes           = 0;  // polyphonic note count

// LFO state.
static float    lfo_phase            = 0.0f;

// LFO wave shape: uses phase [0, 1) → output [-1, 1].
static float lfo_wave_shape(float phase, uint8_t shape) {
  switch (shape) {
    case 0: { // Triangle
      float v = phase > 0.5f ? 1.0f - phase : phase;
      return v * 4.0f - 1.0f;
    }
    case 1: return sinf(phase * 2.0f * M_PI);  // Sine
    case 2: return phase < 0.5f ? 1.0f : -1.0f; // Square
    case 3: return phase * 2.0f - 1.0f;  // Sawtooth
    case 4: { // Random (sample & hold per-frame)
      static float s_h = 0.0f;
      static float s_h_prev_phase = 0.0f;
      if (phase < s_h_prev_phase) // wrapped → new random value
        s_h = (float)rand() / (float)RAND_MAX * 2.0f - 1.0f;
      s_h_prev_phase = phase;
      return s_h;
    }
    default: return sinf(phase * 2.0f * M_PI);
  }
}

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

// Map Proteus-style envelope time (0-99) to samples at 48kHz.
// Exponential curve: 0→1ms, 50→32ms, 99→956ms.
static float env_time_to_samples(uint32_t value) {
  return 48.0f * powf(2.0f, value / 10.0f);
}

// Compute the envelope level at a given absolute sample time.
static float env_level_at_sample(uint64_t sample,
    uint32_t atk, uint32_t dec, uint32_t sus,
    float atk_samples, float dec_samples, float sus_level,
    uint64_t note_on) {
  uint64_t elapsed = sample - note_on;
  if (elapsed < atk_samples)
    return (float)elapsed / atk_samples;
  if (elapsed < atk_samples + dec_samples) {
    float dec_t = (float)(elapsed - atk_samples) / dec_samples;
    return 1.0f - dec_t * (1.0f - sus_level);
  }
  return sus_level;
}


// ---------------------------------------------------------------------------
// Internal helper: apply current Params to a loaded TSF instance
// ---------------------------------------------------------------------------

static void s_apply_params() {
  if (soundfont == nullptr)
    return;
  tsf_set_max_voices(soundfont, Params[param_max_voices]);
  tsf_channel_set_presetindex(soundfont, 0, Params[param_preset]);
  tsf_channel_midi_control(soundfont, 0, 7, Params[param_volume]);
  tsf_channel_set_pan(soundfont, 0, Params[param_pan] / 127.0f);
  tsf_channel_set_tuning(soundfont, 0, Params[param_fine_tune] / 64.0f);
  tsf_channel_set_pitchwheel(soundfont, 0, (int)last_pitch_bend);
}

// ---------------------------------------------------------------------------
// Internal helper: apply velocity curve to a 0–127 velocity value
// ---------------------------------------------------------------------------

static float s_apply_velocity_curve(uint8_t velocity) {
  float v = velocity * VELOCITY_SCALE;
  switch (Params[param_velocity_curve]) {
    case 0:  return v;                                   // linear
    case 1:  return v * v;                               // exponential (softer lows)
    case 2:  return sqrtf(v);                            // logarithmic (compressed)
    case 3:  return 0.5f + 0.5f * v;                     // compressed floor
    case 4:  return v * v * v;                           // steep exponential
    default: return v;
  }
}

// ---------------------------------------------------------------------------
// Internal helper: trigger a note with all transforms applied
// ---------------------------------------------------------------------------

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

  // Count active notes from allocator.
  active_notes = 0;
  for (uint8_t i = 0; i < voice_allocator.GetMaxVoices(); ++i) {
    if (voice_allocator.GetVoice(i).active)
      active_notes++;
  }

  // Only retrigger envelope for the first note.
  if (active_notes == 1) {
    env_active             = true;
    env_note_on_sample     = sample_count;
    env_note_off_sample    = 0;
    env_post_release_frames = 0;
  }

  tsf_channel_note_on(soundfont, 0, (uint8_t)adjusted, vel);
}

// ---------------------------------------------------------------------------
// unit_init
// ---------------------------------------------------------------------------

__unit_callback int8_t unit_init(const unit_runtime_desc_t *desc) {
  if (!desc)
    return k_unit_err_undef;
  if (desc->target != unit_header.target)
    return k_unit_err_target;
  if (!UNIT_API_IS_COMPAT(desc->api))
    return k_unit_err_api_version;
  if (desc->samplerate != 48000)
    return k_unit_err_samplerate;
  if (desc->output_channels != 2)
    return k_unit_err_geometry;

  for (int i = 0; i < param_num; i++)
    Params[i] = kParamDefaults[i];

  // Rescan the Programs folder so SF2 files added after the library was
  // first loaded are visible without a power cycle.
  soundfont_list.refresh();

  // Init DSP effects.
  chorus_dsp.Init(48000.0f);
  chorus_dsp.SetLfoRate(0.5f);
  chorus_dsp.SetModDepth(2.0f);
  chorus_dsp.SetMix(0.0f);
  reverb_dsp.Init(reverb_buffer);
  reverb_dsp.set_amount(0.0f);
  reverb_dsp.set_input_gain(0.15f);
  reverb_dsp.set_time(0.5f);
  reverb_dsp.set_diffusion(0.625f);
  reverb_dsp.set_lp(0.7f);

  // Init voice allocator with default polyphony (16 voices).
  voice_allocator.Init(16);
  voice_allocator.SetMode(common::VoiceMode::Polyphonic);
  voice_allocator.SetAllocationStrategy(common::VoiceAllocationStrategy::OldestNote);

  sample_count    = 0;
  last_pitch_bend = 8192;
  env_active      = false;
  lfo_phase       = 0.0f;
  env_post_release_frames = 0;
  active_notes    = 0;

  if (soundfont_list.count > 0)
    state = load_start;

  return k_unit_err_none;
}

// ---------------------------------------------------------------------------
// unit_teardown
// ---------------------------------------------------------------------------

__unit_callback void unit_teardown() {
  soundfont_list.cleanup();
  tsf_close(soundfont);
  soundfont = nullptr;
  free(soundfont_buf);
  soundfont_buf = nullptr;
}

// ---------------------------------------------------------------------------
// unit_reset
// ---------------------------------------------------------------------------

__unit_callback void unit_reset() {
  if (soundfont != nullptr)
    tsf_reset(soundfont);
  chorus_dsp.Reset();
  reverb_dsp.Clear();
  sample_count  = 0;
  env_active    = false;
  lfo_phase     = 0.0f;
  env_post_release_frames = 0;
  active_notes  = 0;
}

// ---------------------------------------------------------------------------
// unit_suspend / unit_resume
// ---------------------------------------------------------------------------

__unit_callback void unit_suspend() {
  suspended = true;
}

__unit_callback void unit_resume() {
  suspended = false;
}

// ---------------------------------------------------------------------------
// unit_render
//
// State machine advances one step per frame; renders silence while loading.
// Loading timeline for a 4.2 MB SF2 (128-frame buffer @ 48 kHz ≈ 2.67 ms/frame):
//   load_start:      1 frame  (~3 ms)   fopen
//   load_alloc:      1 frame  (~3 ms)   fstat + malloc
//   load_read:      ~33 frames (~88 ms)  chunked fread (4.2 MB / 131072 B)
//   load_close:      1 frame  (~3 ms)   fclose + tsf_close
//   load_tsf_load:   1 frame  (~5-20ms) tsf_load_memory (SF2 parse)
//   load_tsf_set:    1 frame  (~3 ms)   configure TSF
//   load_finished:   1 frame  (~3 ms)   landing
//   load_idle:       audio starts
// ---------------------------------------------------------------------------

__unit_callback void unit_render(const float *in, float *out, uint32_t frames) {
  (void)in;

  static FILE   *fp       = nullptr;
  static size_t  buf_size = 0;
  static size_t  buf_pos  = 0;

  if (suspended) {
    memset(out, 0, frames * 2 * sizeof(float));
    return;
  }

  switch (state) {

    // -----------------------------------------------------------------------
    case load_start: {
      char *path = (char *)malloc(PATH_MAX);
      if (path == nullptr) {
        state = load_idle;
        break;
      }
      snprintf(path, PATH_MAX, "%s/%s",
               SOUNDFONT_PATH,
               soundfont_list.get(Params[param_soundfont]));
      if (fp != nullptr) {
        fclose(fp);
        fp = nullptr;
      }
      fp = fopen(path, "rb");
      free(path);
      if (fp == nullptr) {
        state = load_idle;
        break;
      }
      break;  // → load_alloc next frame
    }

    // -----------------------------------------------------------------------
    case load_alloc: {
      struct stat st;
      if (fstat(fileno(fp), &st) != 0) {
        fclose(fp);
        fp = nullptr;
        state = load_idle;
        break;
      }
      buf_size = (size_t)st.st_size;
      free(soundfont_buf);
      soundfont_buf = (char *)malloc(buf_size);
      if (soundfont_buf == nullptr) {
        fclose(fp);
        fp = nullptr;
        buf_size = 0;
        state = load_idle;
        break;
      }
      buf_pos = 0;
      break;  // → load_read next frame
    }

    // -----------------------------------------------------------------------
    case load_read: {
      size_t n = fread(soundfont_buf + buf_pos, 1, CHUNK_SIZE, fp);
      buf_pos += n;
      if (n < CHUNK_SIZE) {
        // Short read = EOF (or error) — advance to load_close.
        break;
      }
      // Full chunk read — stay in load_read by cancelling the state++ below.
      state--;
      break;
    }

    // -----------------------------------------------------------------------
    case load_close: {
      if (fp != nullptr) {
        fclose(fp);
        fp = nullptr;
      }
      tsf_close(soundfont);
      soundfont = nullptr;
      break;  // → load_tsf_load next frame
    }

    // -----------------------------------------------------------------------
    case load_tsf_load: {
      soundfont = tsf_load_memory(soundfont_buf, (int)buf_size);
      if (soundfont == nullptr) {
        state = load_idle;
        break;
      }
      // Clamp preset now that we know the preset count.
      int max_preset = tsf_get_presetcount(soundfont) - 1;
      if (Params[param_preset] > max_preset)
        Params[param_preset] = max_preset;
      break;  // → load_tsf_set next frame
    }

    // -----------------------------------------------------------------------
    case load_tsf_set: {
      tsf_set_output(soundfont, OUTPUT_MODE, 48000, 0.f);
      s_apply_params();
      break;  // → load_finished next frame
    }

    // -----------------------------------------------------------------------
    case load_finished: {
      // No-op landing frame; state++ below → load_idle + 1, caught by default.
      break;
    }

    // -----------------------------------------------------------------------
    default:
      // Catches load_idle and any overflow past load_finished.
      state = load_idle;
  }

  if (state != load_idle) {
    state++;
    memset(out, 0, frames * 2 * sizeof(float));
    return;
  }

  // Normal rendering
  if (soundfont == nullptr) {
    memset(out, 0, frames * 2 * sizeof(float));
    return;
  }

  sample_count += frames;

  // Apply LFO — advance phase when active.
  if (Params[param_lfo_amount] > 0) {
    float rate   = 0.05f + Params[param_lfo_rate] * (24.95f / 127.0f);
    float amt    = Params[param_lfo_amount] / 127.0f;
    uint8_t wave = Params[param_lfo_wave];
    uint8_t dest = Params[param_lfo_dest];
    lfo_phase += rate * (float)frames / 48000.0f;
    if (lfo_phase >= 1.0f) lfo_phase -= 1.0f;
    float lfo_val = lfo_wave_shape(lfo_phase, wave);

    // Pitch modulation (pre-TSF).
    if (dest == 0 || dest == 2) {
      float base_tune = Params[param_fine_tune] / 64.0f;
      tsf_channel_set_tuning(soundfont, 0, base_tune + lfo_val * amt * 0.5f);
    } else {
      float base_tune = Params[param_fine_tune] / 64.0f;
      tsf_channel_set_tuning(soundfont, 0, base_tune);
    }
  } else {
    float base_tune = Params[param_fine_tune] / 64.0f;
    tsf_channel_set_tuning(soundfont, 0, base_tune);
  }

  tsf_render_float(soundfont, out, (int)frames, TSF_FALSE);

  // Apply ADSR envelope (per-sample volume scaling).
  if (env_active) {
    uint32_t atk = Params[param_env_attack];
    uint32_t dec = Params[param_env_decay];
    uint32_t sus = Params[param_env_sustain];
    uint32_t rel = Params[param_env_release];
    float atk_samples = env_time_to_samples(atk);
    float dec_samples = env_time_to_samples(dec);
    float rel_samples = env_time_to_samples(rel);
    float sus_level   = sus / 99.0f;

    for (uint32_t i = 0; i < frames; i++) {
      uint64_t t = sample_count + i;
      float level;

      if (env_note_off_sample == 0 || t < env_note_off_sample) {
        // Attack / Decay / Sustain.
        level = env_level_at_sample(t, atk, dec, sus,
            atk_samples, dec_samples, sus_level, env_note_on_sample);
      } else {
        // Release.
        uint64_t elapsed = t - env_note_off_sample;
        if (elapsed < rel_samples) {
          float rel_t = (float)elapsed / rel_samples;
          level = env_release_start_level * (1.0f - rel_t);
        } else {
          level = 0.0f;
          env_active = false;
          env_post_release_frames = 10;
          tsf_channel_sounds_off_all(soundfont, 0);
        }
      }

      out[i * 2]     *= level;
      out[i * 2 + 1] *= level;
    }
  }

  // Post-release silence hold — suppress TSF fast-release tail.
  if (env_post_release_frames > 0) {
    memset(out, 0, frames * 2 * sizeof(float));
    env_post_release_frames--;
  }

  // Apply LFO volume modulation (per-frame, phase updated by pitch LFO above).
  if (Params[param_lfo_amount] > 0 && Params[param_lfo_dest] != 0) {
    float amt  = Params[param_lfo_amount] / 127.0f;
    float lfo_val = lfo_wave_shape(lfo_phase, Params[param_lfo_wave]);
    float vol_mod = 1.0f + lfo_val * amt;
    if (vol_mod < 0.0f) vol_mod = 0.0f;
    for (uint32_t i = 0; i < frames * 2; i++)
      out[i] *= vol_mod;
  }

  // Apply DSP effects post-TSF.
  {
    float chorus_mix = Params[param_chorus] / 15.0f;
    float reverb_amount = Params[param_reverb] / 127.0f;

    if (chorus_mix > 0.0f || reverb_amount > 0.0f) {
      // De-interleave once.
      for (uint32_t i = 0; i < frames; i++) {
        fx_buf_l[i] = out[i * 2];
        fx_buf_r[i] = out[i * 2 + 1];
      }

      // Chorus on de-interleaved.
      if (chorus_mix > 0.0f) {
        chorus_dsp.SetMix(chorus_mix);
        chorus_dsp.ProcessStereoBatch(fx_buf_l, fx_buf_r, frames);
      }

      // Reverb on result (includes chorus if active).
      if (reverb_amount > 0.0f) {
        reverb_dsp.set_amount(reverb_amount * 0.4f);
        reverb_dsp.Process(fx_buf_l, fx_buf_r, frames);
      }

      // Re-interleave.
      for (uint32_t i = 0; i < frames; i++) {
        out[i * 2]     = fx_buf_l[i];
        out[i * 2 + 1] = fx_buf_r[i];
      }
    }
  }
}

// ---------------------------------------------------------------------------
// unit_set_param_value
// ---------------------------------------------------------------------------

__unit_callback void unit_set_param_value(uint8_t index, int32_t value) {
  if (index >= param_num)
    return;

  switch (index) {
    case param_soundfont:
      if (soundfont_list.count <= 0)
        break;
      if (value >= soundfont_list.count)
        value = soundfont_list.count - 1;
      if (value < 0)
        value = 0;
      if (value == Params[index])
        break;
      if (soundfont != nullptr)
        tsf_channel_sounds_off_all(soundfont, 0);
      state = load_start;
      break;

    case param_preset:
      if (soundfont == nullptr)
        break;
      {
        int max_preset = tsf_get_presetcount(soundfont) - 1;
        if (value > max_preset)
          value = max_preset;
        if (value < 0)
          value = 0;
      }
      if (value != Params[index]) {
        tsf_channel_note_off_all(soundfont, 0);
        tsf_channel_set_presetindex(soundfont, 0, value);
      }
      break;

    case param_max_voices:
      if (value < 1)  value = 1;
      if (value > 16) value = 16;  // VoiceAllocatorCore supports max 16
      voice_allocator.Init((uint8_t)value);
      voice_allocator.SetMode(common::VoiceMode::Polyphonic);
      voice_allocator.SetAllocationStrategy(common::VoiceAllocationStrategy::OldestNote);
      if (soundfont != nullptr)
        tsf_set_max_voices(soundfont, value);
      break;

    case param_volume:
      if (value < 0)   value = 0;
      if (value > 127) value = 127;
      if (soundfont != nullptr)
        tsf_channel_midi_control(soundfont, 0, 7, value);
      break;

    case param_pan:
      if (value < 0)   value = 0;
      if (value > 127) value = 127;
      if (soundfont != nullptr)
        tsf_channel_set_pan(soundfont, 0, value / 127.0f);
      break;

    case param_velocity_curve:
      if (value < 0) value = 0;
      if (value > 4) value = 4;
      break;

    case param_fine_tune:
      if (value < -63) value = -63;
      if (value > 63)  value = 63;
      if (soundfont != nullptr)
        tsf_channel_set_tuning(soundfont, 0, value / 64.0f);
      break;

    case param_env_attack:
      if (value < 0)  value = 0;
      if (value > 99) value = 99;
      break;

    case param_env_decay:
      if (value < 0)  value = 0;
      if (value > 99) value = 99;
      break;

    case param_env_sustain:
      if (value < 0)  value = 0;
      if (value > 99) value = 99;
      break;

    case param_env_release:
      if (value < 0)  value = 0;
      if (value > 99) value = 99;
      break;

    case param_lfo_rate:
      if (value < 0)   value = 0;
      if (value > 127) value = 127;
      break;

    case param_lfo_amount:
      if (value < 0)   value = 0;
      if (value > 127) value = 127;
      break;

    case param_lfo_dest:
      if (value < 0) value = 0;
      if (value > 2) value = 2;
      break;

    default:
      break;
  }

  Params[index] = value;
}

// ---------------------------------------------------------------------------
// unit_get_param_value
// ---------------------------------------------------------------------------

__unit_callback int32_t unit_get_param_value(uint8_t index) {
  if (index < param_num)
    return Params[index];
  return 0;
}

// ---------------------------------------------------------------------------
// unit_get_param_str_value
// ---------------------------------------------------------------------------

__unit_callback const char *unit_get_param_str_value(uint8_t index, int32_t value) {
  value = (int16_t)value;

  switch (index) {
    case param_soundfont:
      if (soundfont_list.count <= 0)
        return "NO SF2";
      if (value < 0)
        value = 0;
      if (value >= soundfont_list.count)
        value = soundfont_list.count - 1;
      return soundfont_list.get(value);

    case param_preset:
      if (soundfont == nullptr)
        return "LOADING";
      {
        int max_preset = tsf_get_presetcount(soundfont) - 1;
        if (value < 0)
          value = 0;
        if (value > max_preset)
          value = max_preset;
      }
      return tsf_get_presetname(soundfont, value);

    case param_velocity_curve: {
      static const char *curve_names[] = {
        "LINEAR", "EXP", "LOG", "COMP", "STEEP"
      };
      if (value < 0) value = 0;
      if (value > 4) value = 4;
      return curve_names[value];
    }

    case param_lfo_dest: {
      static const char *dest_names[] = { "PITCH", "VOL", "BOTH" };
      if (value < 0) value = 0;
      if (value > 2) value = 2;
      return dest_names[value];
    }

    case param_lfo_wave: {
      static const char *wave_names[] = { "TRI", "SINE", "SQR", "SAW", "RND" };
      if (value < 0) value = 0;
      if (value > 4) value = 4;
      return wave_names[value];
    }

    default:
      return nullptr;
  }
}

// ---------------------------------------------------------------------------
// unit_get_param_bmp_value — not used in Phase 1
// ---------------------------------------------------------------------------

__unit_callback const uint8_t *unit_get_param_bmp_value(uint8_t index, int32_t value) {
  (void)index;
  (void)value;
  return nullptr;
}

// ---------------------------------------------------------------------------
// MIDI note handlers
// ---------------------------------------------------------------------------

__unit_callback void unit_note_on(uint8_t note, uint8_t velocity) {
  s_trigger_note(note, velocity);
}

__unit_callback void unit_note_off(uint8_t note) {
  common::NoteOffResult result = voice_allocator.NoteOff(note);
  
  // Count active notes from allocator.
  active_notes = 0;
  for (uint8_t i = 0; i < voice_allocator.GetMaxVoices(); ++i) {
    if (voice_allocator.GetVoice(i).active)
      active_notes++;
  }
  
  if (active_notes == 0 && env_active && env_note_off_sample == 0) {
    env_release_start_level = env_level_at_sample(sample_count,
        Params[param_env_attack], Params[param_env_decay], Params[param_env_sustain],
        env_time_to_samples(Params[param_env_attack]),
        env_time_to_samples(Params[param_env_decay]),
        Params[param_env_sustain] / 99.0f,
        env_note_on_sample);
    env_note_off_sample = sample_count;
  }
  
  if (soundfont != nullptr) {
    int8_t transpose = (int8_t)Params[param_transpose];
    int adjusted = (int)note + transpose;
    if (adjusted < 0) adjusted = 0; if (adjusted > 127) adjusted = 127;
    tsf_channel_note_off(soundfont, 0, (uint8_t)adjusted);
  }
}

__unit_callback void unit_gate_off() {
  int8_t transpose = (int8_t)Params[param_transpose];
  int note = 60 + transpose;
  if (note < 0) note = 0; if (note > 127) note = 127;
  
  common::NoteOffResult result = voice_allocator.NoteOff((uint8_t)note);
  
  // Count active notes from allocator.
  active_notes = 0;
  for (uint8_t i = 0; i < voice_allocator.GetMaxVoices(); ++i) {
    if (voice_allocator.GetVoice(i).active)
      active_notes++;
  }
  
  if (active_notes == 0 && env_active && env_note_off_sample == 0) {
    env_release_start_level = env_level_at_sample(sample_count,
        Params[param_env_attack], Params[param_env_decay], Params[param_env_sustain],
        env_time_to_samples(Params[param_env_attack]),
        env_time_to_samples(Params[param_env_decay]),
        Params[param_env_sustain] / 99.0f,
        env_note_on_sample);
    env_note_off_sample = sample_count;
  }
  
  if (soundfont != nullptr) {
    tsf_channel_note_off(soundfont, 0, (uint8_t)note);
  }
}

__unit_callback void unit_all_note_off() {
  env_active    = false;
  env_post_release_frames = 0;
  active_notes  = 0;
  if (soundfont != nullptr)
    tsf_channel_sounds_off_all(soundfont, 0);
}

__unit_callback void unit_pitch_bend(uint16_t pitch_bend) {
  last_pitch_bend = pitch_bend;
  if (soundfont != nullptr)
    tsf_channel_set_pitchwheel(soundfont, 0, (int)pitch_bend);
}

__unit_callback void unit_channel_pressure(uint8_t pressure) {
  // Map channel pressure to expression (CC11).
  if (soundfont != nullptr)
    tsf_channel_midi_control(soundfont, 0, 11, pressure);
}

__unit_callback void unit_aftertouch(uint8_t note, uint8_t aftertouch) {
  // Polyphonic aftertouch — not supported by TSF channel API.
  (void)note;
  (void)aftertouch;
}

// ---------------------------------------------------------------------------
// Drumlogue gate handlers
// ---------------------------------------------------------------------------

__unit_callback void unit_gate_on(uint8_t velocity) {
  s_trigger_note(60, velocity);
}

// ---------------------------------------------------------------------------
// Tempo / transport
// ---------------------------------------------------------------------------

__unit_callback void unit_set_tempo(uint32_t tempo) {
  (void)tempo;
}

__unit_callback void unit_tempo_4ppqn_tick(uint32_t counter) {
  (void)counter;
}

// ---------------------------------------------------------------------------
// Preset save/load — not used in Phase 1
// ---------------------------------------------------------------------------

__unit_callback void unit_load_preset(uint8_t idx) {
  (void)idx;
}

__unit_callback uint8_t unit_get_preset_index() {
  return 0;
}

__unit_callback const char *unit_get_preset_name(uint8_t idx) {
  (void)idx;
  return nullptr;
}
