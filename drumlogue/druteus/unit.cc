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

#define NEON_DSP_NS druteus
#include "../common/neon_dsp.h"
#include "../common/simd_utils.h"
namespace ndsp = druteus::neon;

#include "tools/proteus_patches.h"
#include "tools/proteus_instrument_map.h"

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

  soundfont_list.refresh();
  {
    const int required_sf2_idx = sf_find_index_by_name("Proteus1_Instruments.sf2");
    if (required_sf2_idx >= 0)
      Params[param_soundfont] = required_sf2_idx;
  }

  dsp_init();
  voice_init();
  lfo_init();

  if (soundfont_list.count > 0)
    state = SF_LOAD_START;

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
  suspended = true;
}

__unit_callback void unit_resume() {
  suspended = false;
}

// ---------------------------------------------------------------------------
// unit_render
// ---------------------------------------------------------------------------

__unit_callback void unit_render(const float *in, float *out, uint32_t frames) {
  // Synth unit — input buffer is not used.
  (void)in;

  if (suspended) {
    memset(out, 0, frames * 2 * sizeof(float));
    return;
  }

  // ── 1. Soundfont loader state machine ─────────────────────────
  // Advances the async SF2 loader one step per callback.  The loader
  // outputs silence (via the early-return above) until the font is
  // fully loaded and configured.
  sf_load_step(frames);

  if (state != SF_LOAD_IDLE) {
    memset(out, 0, frames * 2 * sizeof(float));
    return;
  }

  if (soundfont == nullptr) {
    memset(out, 0, frames * 2 * sizeof(float));
    return;
  }

  // ── 2. Apply deferred parameter changes (control → audio thread) ──
  // param_preset is written by the control thread.  We apply the
  // associated s_load_patch here so that all current_patch reads
  // happen on a single thread.
  if (patch_dirty.load(std::memory_order_acquire)) {
    s_load_patch((uint16_t)Params[param_preset]);
    patch_dirty.store(false, std::memory_order_relaxed);
  }

  sample_count += frames;

  // ── 3. LFO pitch offset ──────────────────────────────────────
  float lfo_pitch_offset = 0.0f;

  if (Params[param_lfo_amount] > 0) {
    uint8_t dest = Params[param_lfo_dest];
    lfo_pitch_offset = lfo_process_user_mod(frames, dest);
  } else {
    lfo_pitch_offset = lfo_process_lfo_pitch_offset(frames);
  }

  // ── 4. Apply combined tuning (patch coarse/fine + user fine-tune + LFO) ──
  {
    float base_tune = Params[param_fine_tune] / 64.0f;
    float tuned0 = patch_tune_primary + base_tune + lfo_pitch_offset;
    tsf_channel_set_tuning(soundfont, 0, tuned0);
    if (patch_has_secondary)
      tsf_channel_set_tuning(soundfont, 1,
                             patch_tune_secondary + base_tune + lfo_pitch_offset);
  }

  // ── 5. Process pending delayed note-ons & envelopes ──────────
  // Envelopes must be applied *before* tsf_render_float so that
  // finished voices are killed before their samples go to the DAC.
  voice_process_pending_notes();
  voice_process_envelopes();

  // ── 5.5. Realtime pitch modulation (AuxEnv/LFO→pitch from matrix) ──
  // Applied after envelopes so aux env levels are current.  Re-tunes
  // channels before rendering so the pitch offset affects this buffer.
  {
    float base_tune = Params[param_fine_tune] / 64.0f;
    float pri_pitch = lfo_get_realtime_pitch_offset(0);
    float sec_pitch = patch_has_secondary ? lfo_get_realtime_pitch_offset(1) : 0.0f;
    float tuned0 = patch_tune_primary + base_tune + lfo_pitch_offset + pri_pitch;
    tsf_channel_set_tuning(soundfont, 0, tuned0);
    if (patch_has_secondary)
      tsf_channel_set_tuning(soundfont, 1,
                             patch_tune_secondary + base_tune + lfo_pitch_offset + sec_pitch);
  }

  // ── 6. Render TinySoundFont audio ────────────────────────────
  static_assert(sizeof(int) >= sizeof(uint32_t),
                "int must be at least 32 bits for the frames cast");
  tsf_render_float(soundfont, out, (int)frames, TSF_FALSE);

  if (Params[param_lfo_amount] > 0) {
    // User LFO is active — its phase was advanced by lfo_process_user_mod
    // above, so lfo_phase is at user rate, NOT patch LFO1 rate.
    if (Params[param_lfo_dest] != 0) {
      // User LFO targets volume (dest != pitch-only) — apply volume mod.
      float amt  = Params[param_lfo_amount] / 127.0f;
      float lfo_val = lfo_wave_shape(lfo_phase, Params[param_lfo_wave]);
      float vol_mod = 1.0f + lfo_val * amt;
      if (vol_mod < 0.0f) vol_mod = 0.0f;
      ndsp::ApplyGain(out, vol_mod, frames * 2);
    }
    // When user LFO targets pitch only, volume is unchanged and patch
    // modulation is NOT applied because lfo_phase is no longer valid
    // for the patch LFO1 source.
  } else {
    // No user LFO — apply patch realtime modulation.
    // lfo_phase/lfo2_phase were advanced at patch LFO rate by
    // lfo_process_lfo_pitch_offset and are correct for the patch LFO
    // source waveforms.
    lfo_apply_patch_mod(out, frames);
  }

  dsp_process_filter(out, frames);
  dsp_process_effects(out, frames);
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
