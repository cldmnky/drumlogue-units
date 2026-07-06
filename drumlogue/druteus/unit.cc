/**
 * @file unit.cc
 * @brief DRUTEUS drumlogue synth unit — thin coordinator
 *
 * Delegates to module files:
 *   params, sf_loader, patch_engine, voice_engine, lfo_engine, dsp_chain
 *
 * Architecture based on loguetsf.cc by Oleg Burdaev (dukesrg), MIT License.
 * TinySoundFont by Bernhard Schelling, MIT License.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <new>
#include <sys/stat.h>
#include <limits.h>

#include "unit.h"
#include "logue_fs.h"
#include "param_format.h"
#include "voice_allocator.h"
#include "../common/stereo_widener.h"
#include "rings/dsp/fx/reverb.h"
#include "filter.h"
#include "../common/smoothed_value.h"
#include "../common/perf_mon.h"

#define NEON_DSP_NS druteus
#include "../common/neon_dsp.h"
#include "../common/simd_utils.h"
namespace ndsp = druteus::neon;

#include "tools/proteus_patches.h"

// TSF_IMPLEMENTATION: the public extern implementations live here.
// voice_engine.cc has its own static copy (under TSF_IMPLEMENTATION+TSF_STATIC)
// for accessing the struct internals; it does not export any TSF symbols.
#define TSF_IMPLEMENTATION
#define TSF_NO_STDIO
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#include "tsf.h"
#pragma GCC diagnostic pop

#include "druteus_state.h"
#include "params.h"
#include "sf_loader.h"
#include "patch_engine.h"
#include "voice_engine.h"
#include "lfo_engine.h"
#include "dsp_chain.h"
#include "dsp_primitives.h"
#include "trance_gate.h"

// One-pole smoothed volume for the user LFO (review #14).
// Coef 0.05 ≈ ~1 ms time constant at 48 kHz.
static dsp::SmoothedValue s_vol_smooth;

#ifdef PERF_MON
static uint8_t perf_render_total;
static uint8_t perf_tsf_render;
static uint8_t perf_envelopes;
static uint8_t perf_post_dsp;
#endif

static void s_apply_pending_tsf_state() {
  int vol  = pending_volume.load(std::memory_order_relaxed);
  int pan  = pending_pan.load(std::memory_order_relaxed);
  int fine = pending_fine_tune.load(std::memory_order_relaxed);

  if (soundfont == nullptr)
    return;

  tsf_channel_midi_control(soundfont, 0, 7, vol);
  if (patch_has_secondary)
    tsf_channel_midi_control(soundfont, 1, 7, vol);

  tsf_channel_set_pan(soundfont, 0, pan / 127.0f);
  if (patch_has_secondary)
    tsf_channel_set_pan(soundfont, 1, pan / 127.0f);

  tsf_channel_set_tuning(soundfont, 0,
      patch_tune_primary + fine / 64.0f);
  if (patch_has_secondary)
    tsf_channel_set_tuning(soundfont, 1,
        patch_tune_secondary + fine / 64.0f);
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
  // Review #11: the fixed-size per-render FX buffers in dsp_chain are
  // 256 frames; refuse to load if the host promises more so we never
  // overrun them.
  if (desc->frames_per_buffer == 0 || desc->frames_per_buffer > 256)
    return k_unit_err_geometry;

  for (int i = 0; i < param_num; i++)
    Params[i] = kParamDefaults[i];

  // Seed pending_* to defaults so the first render applies them
  // consistently (review #10).
  pending_max_voices.store(Params[param_max_voices], std::memory_order_relaxed);
  pending_volume.store(Params[param_volume], std::memory_order_relaxed);
  pending_pan.store(Params[param_pan], std::memory_order_relaxed);
  pending_fine_tune.store(Params[param_fine_tune], std::memory_order_relaxed);
  voices_dirty.store(true, std::memory_order_release);

  soundfont_list.refresh();
  {
    const int required_sf2_idx = sf_find_index_by_name("Proteus1_Instruments.sf2");
    if (required_sf2_idx >= 0)
      Params[param_soundfont] = required_sf2_idx;
  }

  dsp_init();
  voice_init();
  lfo_init();
  trance_gate_init();
  dsp_init_smoothers(Params[param_cutoff] / 127.0f,
                     Params[param_resonance] / 127.0f);

#ifdef PERF_MON
  PERF_MON_INIT();
  perf_render_total = PERF_MON_REGISTER("RenderTotal");
  perf_tsf_render   = PERF_MON_REGISTER("TSF_Render");
  perf_envelopes    = PERF_MON_REGISTER("Envelopes");
  perf_post_dsp     = PERF_MON_REGISTER("PostDSP");
#endif

  s_vol_smooth.Init(1.0f, 0.05f);

  if (soundfont_list.count > 0)
    reload_requested.store(true, std::memory_order_release);

  return k_unit_err_none;
}

// ---------------------------------------------------------------------------
// unit_teardown
// ---------------------------------------------------------------------------

__unit_callback void unit_teardown() {
  sf_teardown();
}

// ---------------------------------------------------------------------------
// unit_reset
// ---------------------------------------------------------------------------

__unit_callback void unit_reset() {
  sf_reset();
  dsp_reset();
  voice_reset();
  lfo_init();
}

// ---------------------------------------------------------------------------
// unit_suspend / unit_resume
// ---------------------------------------------------------------------------

__unit_callback void unit_suspend() {
  suspended.store(true, std::memory_order_release);
}

__unit_callback void unit_resume() {
  suspended.store(false, std::memory_order_release);
}

// ---------------------------------------------------------------------------
// unit_render
// ---------------------------------------------------------------------------

__unit_callback void unit_render(const float *in, float *out, uint32_t frames) {
  // Synth unit — input buffer is not used.
  (void)in;

  if (suspended.load(std::memory_order_acquire)) {
    memset(out, 0, frames * 2 * sizeof(float));
    return;
  }

#ifdef PERF_MON
  PERF_MON_START(perf_render_total);
#endif

  // ── 1. Soundfont loader state machine ─────────────────────────
  // Advances the async SF2 loader one step per callback.  The loader
  // outputs silence (via the early-return above) until the font is
  // fully loaded and configured.
  sf_load_step(frames);

  // Fold in deferred TSF state changes (voices/volume/pan/fine-tune)
  // raised by the control thread — review #1 / #10.
  sf_apply_pending();

  if (state.load(std::memory_order_acquire) != SF_LOAD_IDLE) {
    memset(out, 0, frames * 2 * sizeof(float));
    return;
  }

  if (soundfont == nullptr) {
    memset(out, 0, frames * 2 * sizeof(float));
    return;
  }

  // ── 2. Apply deferred patch change (control → audio thread) ──
  // param_preset is written by the control thread.  We apply the
  // associated s_load_patch here so that all current_patch reads
  // happen on a single thread.  Kill all TSF voices on patch change
  // so the stale note_gain_* tables don't bleed into the new patch
  // (review #17).
  if (patch_dirty.load(std::memory_order_acquire)) {
    s_load_patch((uint16_t)Params[param_preset]);
    for (int ch = 0; ch < 2; ch++) {
      tsf_channel_sounds_off_all(soundfont, ch);
    }
    patch_dirty.store(false, std::memory_order_relaxed);
  }

  // Apply pending TSF channel state (vol/pan/tuning) right after the
  // patch load so the new patch sees the current user values.
  s_apply_pending_tsf_state();

  sample_count += frames;

  // ── 2.5. Cache aux envelope once per block (review #21) ──
  lfo_update_aux_env_cache();

  // ── 3. LFO phase advancement & pitch offset ──────────────────
  // Pitch from the realtime modulation matrix is applied separately
  // below (step 5.5) only when user LFO is inactive.  The global
  // LFO1/LFO2 pitch offset is NOT used — it would double-apply.
  bool user_lfo_active = (Params[param_lfo_amount] > 0);
  float lfo_pitch_offset = 0.0f;

  if (user_lfo_active) {
    uint8_t dest = Params[param_lfo_dest];
    lfo_pitch_offset = lfo_process_user_mod(frames, dest);
  } else {
    lfo_process_lfo_pitch_offset(frames);
  }

  // ── 4. Apply combined tuning (patch coarse/fine + user fine-tune + LFO) ──
  // (Moved into step 5.5 below so the matrix path runs once and uses
  // the cached aux env — review #21.)

  // ── 5. Process pending delayed note-ons & envelopes ──────────
  // Envelopes must be applied *before* tsf_render_float so that
  // finished voices are killed before their samples go to the DAC.
#ifdef PERF_MON
  PERF_MON_START(perf_envelopes);
#endif
  voice_process_pending_notes();
  voice_process_envelopes();
#ifdef PERF_MON
  PERF_MON_END(perf_envelopes);
#endif

  // ── 5.5. Patch realtime pitch modulation (matrix) ────────────
  // Single tsf_channel_set_tuning call per channel per block.  The
  // previous code wrote twice (once in step 4, once in step 5.5);
  // consolidating here removes the redundant voice-walk
  // (review #21).
  {
    float base_tune = Params[param_fine_tune] / 64.0f;
    float pri_pitch = user_lfo_active
        ? lfo_pitch_offset
        : lfo_get_realtime_pitch_offset(0);
    float sec_pitch = (patch_has_secondary && !user_lfo_active)
        ? lfo_get_realtime_pitch_offset(1) : 0.0f;
    float tuned0 = patch_tune_primary + base_tune + pri_pitch;
    tsf_channel_set_tuning(soundfont, 0, tuned0);
    if (patch_has_secondary) {
      float tuned1 = patch_tune_secondary + base_tune + sec_pitch;
      tsf_channel_set_tuning(soundfont, 1, tuned1);
    }
  }

  // ── 6. Render TinySoundFont audio ────────────────────────────
  static_assert(sizeof(int) >= sizeof(uint32_t),
                "int must be at least 32 bits for the frames cast");
#ifdef PERF_MON
  PERF_MON_START(perf_tsf_render);
#endif
  tsf_render_float(soundfont, out, (int)frames, TSF_FALSE);
#ifdef PERF_MON
  PERF_MON_END(perf_tsf_render);
#endif

  // ── 7. Volume modulation (smoothed — review #14) ──────────
  float target_vol = 1.0f;
  if (user_lfo_active && Params[param_lfo_dest] != 0) {
    float amt  = Params[param_lfo_amount] / 127.0f;
    float lfo_val = lfo_wave_shape_slot(lfo_phase, Params[param_lfo_wave], 0);
    float vol_mod = 1.0f + lfo_val * amt;
    if (vol_mod < 0.0f) vol_mod = 0.0f;
    target_vol = vol_mod;
  }
  s_vol_smooth.SetTarget(target_vol);
  // Step the smoother once per block.  The smoother's one-pole
  // filter never reaches exactly 1.0f (IEEE 754 convergence), so
  // compare via epsilon (review BUG 1).
  float smooth_vol = s_vol_smooth.Process();
  if (!s_vol_smooth.HasReachedTarget(1e-5f)) {
    ndsp::ApplyGain(out, smooth_vol, frames * 2);
  } else {
    // Snap explicitly so the next block's comparison is reliable.
    s_vol_smooth.SetImmediate(1.0f);
    // Patch realtime modulation (LFO1/LFO2/AuxEnv → volume)
    lfo_apply_patch_mod(out, frames);
  }

#ifdef PERF_MON
  PERF_MON_START(perf_post_dsp);
#endif
  dsp_process_filter(out, frames);
  dsp_process_effects(out, frames);
#ifdef PERF_MON
  PERF_MON_END(perf_post_dsp);
  PERF_MON_END(perf_render_total);
#endif
}

// ---------------------------------------------------------------------------
// unit_set_param_value
// ---------------------------------------------------------------------------

__unit_callback void unit_set_param_value(uint8_t index, int32_t value) {
  params_set(index, value);
}

// ---------------------------------------------------------------------------
// unit_get_param_value
// ---------------------------------------------------------------------------

__unit_callback int32_t unit_get_param_value(uint8_t index) {
  return params_get(index);
}

// ---------------------------------------------------------------------------
// unit_get_param_str_value
// ---------------------------------------------------------------------------

__unit_callback const char *unit_get_param_str_value(uint8_t index, int32_t value) {
  return params_get_str(index, value);
}

// ---------------------------------------------------------------------------
// unit_get_param_bmp_value
// ---------------------------------------------------------------------------

__unit_callback const uint8_t *unit_get_param_bmp_value(uint8_t index, int32_t value) {
  (void)index;
  (void)value;
  return nullptr;
}

// ---------------------------------------------------------------------------
// MIDI and gate handlers
// ---------------------------------------------------------------------------

__unit_callback void unit_note_on(uint8_t note, uint8_t velocity) {
  voice_note_on(note, velocity);
}

__unit_callback void unit_note_off(uint8_t note) {
  voice_note_off(note);
}

__unit_callback void unit_gate_on(uint8_t velocity) {
  voice_note_on(60, velocity);
}

__unit_callback void unit_gate_off() {
  voice_gate_off();
}

__unit_callback void unit_all_note_off() {
  voice_all_note_off();
}

__unit_callback void unit_pitch_bend(uint16_t pitch_bend) {
  voice_pitch_bend(pitch_bend);
}

__unit_callback void unit_channel_pressure(uint8_t pressure) {
  voice_channel_pressure(pressure);
}

__unit_callback void unit_aftertouch(uint8_t note, uint8_t aftertouch) {
  (void)note;
  (void)aftertouch;
}

// ---------------------------------------------------------------------------
// Tempo / transport
// ---------------------------------------------------------------------------

__unit_callback void unit_set_tempo(uint32_t tempo) {
  trance_gate_set_tempo(tempo);
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
