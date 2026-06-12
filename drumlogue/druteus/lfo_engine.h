#pragma once

#include <stdint.h>

void lfo_init();

// Per-render cached aux envelope level (review #21).  Set once at the
// top of unit_render and consumed by lfo_get_realtime_pitch_offset /
// lfo_apply_patch_mod to avoid evaluating the 16-voice envelope
// envelope 8+ times per block.
extern float aux_env_cached_;
void lfo_update_aux_env_cache();

// Advance patch LFO1+LFO2 phases and handle per-LFO delay ramps.
// Pitch modulation is applied through the realtime modulation matrix
// (lfo_get_realtime_pitch_offset) — this function only advances state.
void lfo_process_lfo_pitch_offset(uint32_t frames);

// Advance user LFO (params) and return pitch offset based on dest:
//   dest=0 (pitch) or 2 (both) → returns wave value
//   dest=1 (volume)             → returns 0
float lfo_process_user_mod(uint32_t frames, uint8_t dest);

// Apply patch realtime modulation matrix (LFO1/LFO2 sources) to output buffer.
// Reads current lfo_phase/lfo2_phase (already advanced by
// lfo_process_lfo_pitch_offset) and applies volume modulation per the
// realtime modulation matrix in current_patch.
void lfo_apply_patch_mod(float* out, uint32_t frames);

// Compute realtime pitch offset from the modulation matrix for a given
// channel (0=primary, 1=secondary).  Returns offset in semitones to be
// added to the channel tuning in unit_render.
float lfo_get_realtime_pitch_offset(uint8_t channel);

float lfo_get_realtime_crossfade_shift();

// Evaluate wave shape at given phase; shape 0=tri, 1=sin, 2=square, 3=saw, 4=s&h.
// The single-arg overload returns 0 for shape 4 to force callers to use
// the slot-based variant when S&H is needed.
float lfo_wave_shape(float phase, uint8_t shape);

// Slot-aware variant for S&H (review #8).  slot 0=user, 1=patch LFO1, 2=patch LFO2.
float lfo_wave_shape_slot(float phase, uint8_t shape, uint8_t slot);

// Reset per-LFO S&H state (call from unit_reset / lfo_init).
void lfo_reset_sh_state();

// Compute the current global aux envelope level (max across all voices).
// Call once per render and pass to lfo_get_realtime_pitch_offset /
// lfo_apply_patch_mod to avoid redundant computation.
float lfo_compute_global_aux_env_level();
