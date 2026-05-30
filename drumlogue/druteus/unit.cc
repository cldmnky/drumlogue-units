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

// NEON SIMD utilities (requires -DUSE_NEON, -mfpu=neon, -mfloat-abi=hard).
#define NEON_DSP_NS druteus
#include "../common/neon_dsp.h"
#include "../common/simd_utils.h"
namespace ndsp = druteus::neon;

#include "tools/proteus_patches.h"

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
  param_xfade        = 8,  // Page 3 — layer control
  param_layers       = 9,
  param_unused_10    = 10,
  param_unused_11    = 11,
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
  0,    // XFADE: off
  0,    // LAYERS: both
  0,    // unused
  0,    // unused
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

// Current Proteus patch and dual-layer state.
static proteus_patch_t current_patch;
static bool patch_has_secondary = false;
static int  voice_preset_primary   = 1;  // TSF preset index for CH0
static int  voice_preset_secondary = 1;  // TSF preset index for CH1

// Cached patch envelope values (read in render loop, set by s_load_patch).
static uint32_t cached_env_atk = 0;
static uint32_t cached_env_hold = 0;
static uint32_t cached_env_dec = 0;
static uint32_t cached_env_sus = 99;
static uint32_t cached_env_rel = 99;
static bool     cached_env_enabled = false;

// Per-voice AHDSR envelope state (indexed by voice allocator voice_index).
struct VoiceEnv {
  bool     active;
  uint8_t  note;
  uint64_t note_on_sample;
  uint64_t note_off_sample;
  float    release_start_level;
};
static VoiceEnv voice_env[16];
static int      active_notes = 0;  // polyphonic note count

// LFO state.
static float    lfo_phase            = 0.0f;
static float    lfo2_phase           = 0.0f;
static float    lfo_delay_completed  = 0.0f;  // frames since note-on for delay ramp

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

// Proteus envelope time curves — piecewise-linear from the owner's manual.
// Each function maps a 0-99 param value to time in samples at 48 kHz.

typedef struct { uint32_t knob; float seconds; } env_point_t;

static float env_lookup_seconds(uint32_t value, const env_point_t *table, int n) {
  if (value <= table[0].knob)    return table[0].seconds;
  if (value >= table[n - 1].knob) return table[n - 1].seconds;
  for (int i = 1; i < n; i++) {
    if (value <= table[i].knob) {
      float frac = (float)(value - table[i - 1].knob) /
                   (float)(table[i].knob - table[i - 1].knob);
      return table[i - 1].seconds +
             frac * (table[i].seconds - table[i - 1].seconds);
    }
  }
  return table[n - 1].seconds;
}

static const env_point_t kReleasePts[] = {
  {0, 0.0f}, {5, 0.125f}, {10, 0.25f}, {15, 0.4f}, {20, 0.6f},
  {30, 1.2f}, {40, 2.2f}, {50, 4.0f}, {60, 9.0f}, {75, 15.0f},
  {80, 20.0f}, {99, 60.0f},
};

static const env_point_t kDecayPts[] = {
  {0, 0.0f}, {5, 0.125f}, {10, 0.25f}, {20, 0.4f}, {30, 0.75f},
  {40, 1.5f}, {50, 3.0f}, {60, 5.0f}, {70, 9.0f}, {75, 12.0f},
  {80, 18.0f}, {99, 40.0f},
};

static const env_point_t kHoldPts[] = {
  {0, 0.0f}, {5, 0.125f}, {10, 0.25f}, {20, 0.4f}, {30, 0.8f},
  {40, 1.3f}, {50, 1.75f}, {60, 2.3f}, {70, 3.2f}, {75, 3.5f},
  {80, 4.2f}, {99, 6.5f},
};

static float env_time_to_samples_attack(uint32_t v)  { return env_lookup_seconds(v, kReleasePts, 12) * 48000.0f; }
static float env_time_to_samples_hold(uint32_t v)     { return env_lookup_seconds(v, kHoldPts,    12) * 48000.0f; }
static float env_time_to_samples_decay(uint32_t v)     { return env_lookup_seconds(v, kDecayPts,   12) * 48000.0f; }
static float env_time_to_samples_release(uint32_t v)   { return env_lookup_seconds(v, kReleasePts, 12) * 48000.0f; }

// LFO delay time curve from owner's manual (0-127 → 0 to 13 seconds).
static const env_point_t kDelayPts[] = {
  {0, 0.0f}, {5, 0.125f}, {10, 0.25f}, {20, 0.6f}, {32, 1.0f},
  {40, 1.5f}, {64, 2.5f}, {75, 3.5f}, {80, 4.2f}, {96, 6.2f},
  {100, 7.0f}, {127, 13.0f},
};

static float lfo_delay_to_samples(uint32_t v) { return env_lookup_seconds(v, kDelayPts, 12) * 48000.0f; }

// Compute the envelope level at a given absolute sample time (AHDSR).
static float env_level_at_sample(uint64_t sample,
    uint32_t atk, uint32_t dec, uint32_t sus,
    float atk_samples, float hold_samples, float dec_samples, float sus_level,
    uint64_t note_on) {
  uint64_t elapsed = sample - note_on;
  if (elapsed < atk_samples)
    return (float)elapsed / atk_samples;
  if (elapsed < atk_samples + hold_samples)
    return 1.0f;
  if (elapsed < atk_samples + hold_samples + dec_samples) {
    float dec_t = (float)(elapsed - atk_samples - hold_samples) / dec_samples;
    return 1.0f - dec_t * (1.0f - sus_level);
  }
  return sus_level;
}


// ---------------------------------------------------------------------------
// Internal helper: load a Proteus patch onto TSF channels 0 (primary) and 1 (secondary)
// ---------------------------------------------------------------------------

static void s_load_patch(uint16_t patch_idx) {
  if (patch_idx >= PROTEUS_PATCH_COUNT)
    return;

  current_patch = kProteusPatchTable[patch_idx];
  patch_has_secondary = (current_patch.i2volume > 0);

  cached_env_atk     = current_patch.i1attack;
  cached_env_hold    = current_patch.i1hold;
  cached_env_dec     = current_patch.i1decay;
  cached_env_sus     = current_patch.i1sustain;
  cached_env_rel     = current_patch.i1release;
  cached_env_enabled = (current_patch.i1envelopeon != 0);

  if (soundfont == nullptr)
    return;

  int max_preset = tsf_get_presetcount(soundfont);
  if (max_preset <= 0)
    return;

  int idx0 = (int)current_patch.i1instrument;
  if (idx0 <= 0) idx0 = 1;
  if (idx0 >= max_preset) idx0 = max_preset - 1;
  voice_preset_primary = idx0;

  tsf_channel_set_presetindex(soundfont, 0, idx0);
  tsf_channel_midi_control(soundfont, 0, 7, current_patch.i1volume);
  tsf_channel_set_pan(soundfont, 0,
      ((int)current_patch.i1pan + 7) * 9 / 127.0f);
  float tune1 = (float)(current_patch.i1tuningcoarse * 100 +
                        current_patch.i1tuningfine) / 100.0f;
  tsf_channel_set_tuning(soundfont, 0, tune1);

  if (patch_has_secondary) {
    int idx1 = (int)current_patch.i2instrument;
    if (idx1 <= 0) idx1 = 1;
    if (idx1 >= max_preset) idx1 = max_preset - 1;
    voice_preset_secondary = idx1;

    tsf_channel_set_presetindex(soundfont, 1, idx1);
    tsf_channel_midi_control(soundfont, 1, 7, current_patch.i2volume);
    tsf_channel_set_pan(soundfont, 1,
        ((int)current_patch.i2pan + 7) * 9 / 127.0f);
    float tune2 = (float)(current_patch.i2tuningcoarse * 100 +
                          current_patch.i2tuningfine) / 100.0f;
    tsf_channel_set_tuning(soundfont, 1, tune2);
  }
}

// ---------------------------------------------------------------------------
// Internal helper: apply current Params to a loaded TSF instance
// ---------------------------------------------------------------------------

static void s_apply_params() {
  if (soundfont == nullptr)
    return;
  tsf_set_max_voices(soundfont, Params[param_max_voices]);
  s_load_patch(Params[param_preset]);
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
// Internal helper: immediately kill all TSF voices matching channel+preset+key
// ---------------------------------------------------------------------------

static void tsf_kill_note(tsf* f, int channel, int preset, int key) {
  for (int i = 0; i < (int)f->voiceNum; i++) {
    tsf_voice* v = &f->voices[i];
    if (v->playingPreset == preset && v->playingKey == key &&
        v->playingChannel == channel) {
      v->ampGain = 0.0f;
      tsf_voice_kill(v);
    }
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
    lfo_delay_completed = 0.0f;
  }

  float vel0 = vel, vel1 = vel;

  if (current_patch.crossfademode == 1) {
    float sp     = current_patch.switchpoint / 127.0f;
    sp += (current_patch.crossfadebalance - 64.0f) * 0.0039f;  // ±25%
    if (sp < 0.0f) sp = 0.0f;
    if (sp > 1.0f) sp = 1.0f;
    float xfade  = current_patch.crossfadeamount / 255.0f;
    float v      = velocity / 127.0f;
    if (v < sp) {
      vel0 *= 1.0f;
      vel1 *= 1.0f - (1.0f - v / sp) * xfade;
    } else {
      vel0 *= 1.0f - (v - sp) / (1.0f - sp) * xfade;
      vel1 *= 1.0f;
    }
  } else if (current_patch.crossfademode == 2) {
    if (adjusted < 64) vel1 = 0.0f;
    else               vel0 = 0.0f;
  }

  bool play_primary = true, play_secondary = true;
  if (current_patch.i1lowkey > 0 || current_patch.i1highkey < 127) {
    play_primary = ((uint8_t)adjusted >= current_patch.i1lowkey &&
                    (uint8_t)adjusted <= current_patch.i1highkey);
  }
  if (patch_has_secondary && (current_patch.i2lowkey > 0 || current_patch.i2highkey < 127)) {
    play_secondary = ((uint8_t)adjusted >= current_patch.i2lowkey &&
                      (uint8_t)adjusted <= current_patch.i2highkey);
  }

  if (play_primary && vel0 > 0.0f)
    tsf_channel_note_on(soundfont, 0, (uint8_t)adjusted, vel0);
  if (patch_has_secondary && play_secondary && vel1 > 0.0f)
    tsf_channel_note_on(soundfont, 1, (uint8_t)adjusted, vel1);
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
  lfo_phase       = 0.0f;
  lfo2_phase      = 0.0f;
  lfo_delay_completed = 0.0f;
  active_notes    = 0;
  memset(voice_env, 0, sizeof(voice_env));

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
  lfo_phase     = 0.0f;
  memset(voice_env, 0, sizeof(voice_env));
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

  // Apply LFO — user overrides if AMT > 0, else patch LFO1+LFO2.
  float lfo_pitch_offset = 0.0f;

  if (Params[param_lfo_amount] > 0) {
    float rate   = 0.05f + Params[param_lfo_rate] * (24.95f / 127.0f);
    float amt    = Params[param_lfo_amount] / 127.0f;
    uint8_t wave = Params[param_lfo_wave];
    uint8_t dest = Params[param_lfo_dest];
    lfo_phase += rate * (float)frames / 48000.0f;
    if (lfo_phase >= 1.0f) lfo_phase -= 1.0f;
    float lfo_val = lfo_wave_shape(lfo_phase, wave);
    if (dest == 0 || dest == 2)
      lfo_pitch_offset = lfo_val * amt * 0.5f;
  } else {
    float lfo_delay_scale = 1.0f;
    uint32_t delay_param  = (current_patch.lfo1amount != 0 || current_patch.lfo2amount != 0)
                           ? current_patch.lfo1delay : 0;
    if (delay_param > 0) {
      float delay_samples = lfo_delay_to_samples(delay_param);
      lfo_delay_completed += (float)frames;
      if (lfo_delay_completed < delay_samples)
        lfo_delay_scale = lfo_delay_completed / delay_samples;
    }

    if (current_patch.lfo1amount != 0) {
      float rate = 0.05f + current_patch.lfo1frequency * (24.95f / 127.0f);
      float amt  = (float)current_patch.lfo1amount / 127.0f * lfo_delay_scale;
      uint8_t w  = current_patch.lfo1shape;
      if (current_patch.lfo1variation > 0) {
        float var = (float)current_patch.lfo1variation / 127.0f;
        float rnd = ((float)((sample_count / 64) * 2654435761u & 0x7FFFFFFF)
                     / 2147483648.0f) - 1.0f;
        rate *= 1.0f + rnd * var * 0.5f;
        if (rate < 0.01f) rate = 0.01f;
      }
      lfo_phase += rate * (float)frames / 48000.0f;
      if (lfo_phase >= 1.0f) lfo_phase -= 1.0f;
      lfo_pitch_offset += lfo_wave_shape(lfo_phase, w) * amt * 0.5f;
    }

    if (current_patch.lfo2amount != 0) {
      float rate = 0.05f + current_patch.lfo2frequency * (24.95f / 127.0f);
      float amt  = (float)current_patch.lfo2amount / 127.0f * lfo_delay_scale;
      uint8_t w  = current_patch.lfo2shape;
      if (current_patch.lfo2variation > 0) {
        float var = (float)current_patch.lfo2variation / 127.0f;
        float rnd = ((float)(((sample_count / 64) + 1) * 2654435761u & 0x7FFFFFFF)
                     / 2147483648.0f) - 1.0f;
        rate *= 1.0f + rnd * var * 0.5f;
        if (rate < 0.01f) rate = 0.01f;
      }
      lfo2_phase += rate * (float)frames / 48000.0f;
      if (lfo2_phase >= 1.0f) lfo2_phase -= 1.0f;
      lfo_pitch_offset += lfo_wave_shape(lfo2_phase, w) * amt * 0.5f;
    }
  }

  {
    float base_tune = Params[param_fine_tune] / 64.0f;
    float tuned = base_tune + lfo_pitch_offset;
    tsf_channel_set_tuning(soundfont, 0, tuned);
    if (patch_has_secondary)
      tsf_channel_set_tuning(soundfont, 1, tuned);
  }

  // Per-voice AHDSR — compute per-note gain then apply in single TSF voice pass.
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
        level = env_level_at_sample(sample_count, 0, 0, 0,
            atk_samples, hold_samples, dec_samples, sus_level, note_on);
      } else {
        uint64_t rel_elapsed = sample_count - note_off;
        if (rel_elapsed < (uint64_t)rel_samples) {
          float rel_t = (float)rel_elapsed / rel_samples;
          level = voice_env[vi].release_start_level * (1.0f - rel_t);
        } else {
          level = 0.0f;
          voice_env[vi].active = false;
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

  tsf_render_float(soundfont, out, (int)frames, TSF_FALSE);

  if (Params[param_lfo_amount] > 0 && Params[param_lfo_dest] != 0) {
    float amt  = Params[param_lfo_amount] / 127.0f;
    float lfo_val = lfo_wave_shape(lfo_phase, Params[param_lfo_wave]);
    float vol_mod = 1.0f + lfo_val * amt;
    if (vol_mod < 0.0f) vol_mod = 0.0f;
    ndsp::ApplyGain(out, vol_mod, frames * 2);
  }

  // Apply DSP effects post-TSF.
  {
    float global_chorus = Params[param_chorus] / 15.0f;
    float patch_chorus  = (current_patch.i1chorus + current_patch.i2chorus) / 30.0f;
    float chorus_mix    = global_chorus * 0.5f + patch_chorus * 0.5f;
    float reverb_amount = Params[param_reverb] / 127.0f;

    if (chorus_mix > 0.0f || reverb_amount > 0.0f) {
      simd_deinterleave_stereo(out, fx_buf_l, fx_buf_r, frames);

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

      simd_interleave_stereo(fx_buf_l, fx_buf_r, out, frames);
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
      if (value < 0)  value = 0;
      if (value >= PROTEUS_PATCH_COUNT)
        value = PROTEUS_PATCH_COUNT - 1;
      if (value != Params[index]) {
        Params[index] = value;
        s_load_patch((uint16_t)value);
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
      if (soundfont != nullptr) {
        tsf_channel_midi_control(soundfont, 0, 7, value);
        if (patch_has_secondary)
          tsf_channel_midi_control(soundfont, 1, 7, value);
      }
      break;

    case param_pan:
      if (value < 0)   value = 0;
      if (value > 127) value = 127;
      if (soundfont != nullptr) {
        tsf_channel_set_pan(soundfont, 0, value / 127.0f);
        if (patch_has_secondary)
          tsf_channel_set_pan(soundfont, 1, value / 127.0f);
      }
      break;

    case param_velocity_curve:
      if (value < 0) value = 0;
      if (value > 4) value = 4;
      break;

    case param_fine_tune:
      if (value < -63) value = -63;
      if (value > 63)  value = 63;
      if (soundfont != nullptr) {
        tsf_channel_set_tuning(soundfont, 0, value / 64.0f);
        if (patch_has_secondary)
          tsf_channel_set_tuning(soundfont, 1, value / 64.0f);
      }
      break;

    case param_xfade:
      if (value < 0) value = 0;
      if (value > 2) value = 2;
      break;

    case param_layers:
      if (value < 0) value = 0;
      if (value > 2) value = 2;
      break;

    case param_unused_10:
    case param_unused_11:
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
      if (value < 0) value = 0;
      if (value >= PROTEUS_PATCH_COUNT)
        value = PROTEUS_PATCH_COUNT - 1;
      return kProteusPatchTable[value].name;

    case param_xfade: {
      static const char *xfade_names[] = { "OFF", "VEL", "KEY" };
      if (value < 0) value = 0;
      if (value > 2) value = 2;
      return xfade_names[value];
    }

    case param_layers: {
      static const char *layer_names[] = { "BOTH", "PRI", "SEC" };
      if (value < 0) value = 0;
      if (value > 2) value = 2;
      return layer_names[value];
    }

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
  voice_allocator.NoteOff(note);
  
  // Deactivate matching voice slots (NoteOff doesn't do this in polyphonic mode).
  for (uint8_t i = 0; i < voice_allocator.GetMaxVoices(); ++i) {
    if (voice_allocator.GetVoice(i).active && voice_allocator.GetVoice(i).midi_note == note)
      voice_allocator.SetVoiceActive(i, false);
  }
  
  // Count active notes from allocator.
  active_notes = 0;
  for (uint8_t i = 0; i < voice_allocator.GetMaxVoices(); ++i) {
    if (voice_allocator.GetVoice(i).active)
      active_notes++;
  }
  
  int8_t trans = (int8_t)Params[param_transpose];
  int adj = (int)note + trans;
  if (adj < 0) adj = 0; if (adj > 127) adj = 127;

  if (cached_env_enabled) {
    float atk_s = env_time_to_samples_attack(cached_env_atk);
    float hld_s = env_time_to_samples_hold(cached_env_hold);
    float dec_s = env_time_to_samples_decay(cached_env_dec);
    float sus_l = cached_env_sus / 99.0f;
    for (int i = 0; i < 16; i++) {
      if (voice_env[i].active && voice_env[i].note == (uint8_t)adj &&
          voice_env[i].note_off_sample == 0) {
        voice_env[i].release_start_level = env_level_at_sample(
            sample_count, 0, 0, 0, atk_s, hld_s, dec_s, sus_l,
            voice_env[i].note_on_sample);
        voice_env[i].note_off_sample = sample_count;
      }
    }
  }

  if (soundfont != nullptr) {
    tsf_channel_note_off(soundfont, 0, (uint8_t)adj);
    if (patch_has_secondary)
      tsf_channel_note_off(soundfont, 1, (uint8_t)adj);
  }
}

__unit_callback void unit_gate_off() {
  int8_t transpose = (int8_t)Params[param_transpose];
  int note = 60 + transpose;
  if (note < 0) note = 0; if (note > 127) note = 127;
  
  voice_allocator.NoteOff((uint8_t)note);
  
  // Deactivate matching voice slots (NoteOff doesn't do this in polyphonic mode).
  for (uint8_t i = 0; i < voice_allocator.GetMaxVoices(); ++i) {
    if (voice_allocator.GetVoice(i).active && voice_allocator.GetVoice(i).midi_note == note)
      voice_allocator.SetVoiceActive(i, false);
  }
  
  // Count active notes from allocator.
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
    for (int i = 0; i < 16; i++) {
      if (voice_env[i].active && voice_env[i].note == (uint8_t)note &&
          voice_env[i].note_off_sample == 0) {
        voice_env[i].release_start_level = env_level_at_sample(
            sample_count, 0, 0, 0, atk_s, hld_s, dec_s, sus_l,
            voice_env[i].note_on_sample);
        voice_env[i].note_off_sample = sample_count;
      }
    }
  }
  
  if (soundfont != nullptr) {
    tsf_channel_note_off(soundfont, 0, (uint8_t)note);
    if (patch_has_secondary)
      tsf_channel_note_off(soundfont, 1, (uint8_t)note);
  }
}

__unit_callback void unit_all_note_off() {
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

__unit_callback void unit_pitch_bend(uint16_t pitch_bend) {
  last_pitch_bend = pitch_bend;
  if (soundfont != nullptr) {
    tsf_channel_set_pitchwheel(soundfont, 0, (int)pitch_bend);
    if (patch_has_secondary)
      tsf_channel_set_pitchwheel(soundfont, 1, (int)pitch_bend);
  }
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
