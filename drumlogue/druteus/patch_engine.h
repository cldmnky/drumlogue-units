#pragma once
#include <stdint.h>

// Extern declarations for crossfade cached values — pre-calculated in
// s_load_patch and consumed by compute_crossfade_weights on every note-on.
extern float cached_xfade_center;
extern float cached_xfade_width;
extern float cached_xfade_lo;
extern float cached_xfade_hi;
extern float cached_xfade_span;
extern uint8_t cached_xfade_split_key;

void s_load_patch(uint16_t patch_idx);
void s_apply_params();
void compute_crossfade_weights(uint8_t mode, uint8_t velocity, uint8_t note,
                                uint8_t switchpoint, uint8_t balance,
                                uint8_t amount, uint8_t direction,
                                float* primary_weight,
                                float* secondary_weight);
