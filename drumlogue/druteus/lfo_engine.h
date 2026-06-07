#pragma once

#include <stdint.h>

void lfo_init();

// Advance patch LFO1+LFO2 and return combined pitch offset.
float lfo_process_lfo_pitch_offset(uint32_t frames);

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

// Evaluate wave shape at given phase; shape 0=tri, 1=sin, 2=square, 3=saw, 4=s&h.
float lfo_wave_shape(float phase, uint8_t shape);
