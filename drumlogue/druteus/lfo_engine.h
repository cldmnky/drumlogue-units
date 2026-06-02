#pragma once

#include <stdint.h>

void lfo_init();

// Advance patch LFO1+LFO2 and return combined pitch offset.
float lfo_process_lfo_pitch_offset(uint32_t frames);

// Advance user LFO (params) and return pitch offset based on dest:
//   dest=0 (pitch) or 2 (both) → returns wave value
//   dest=1 (volume)             → returns 0
float lfo_process_user_mod(uint32_t frames, uint8_t dest);

// Evaluate wave shape at given phase; shape 0=tri, 1=sin, 2=square, 3=saw, 4=s&h.
float lfo_wave_shape(float phase, uint8_t shape);
