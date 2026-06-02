#pragma once

#include <stdint.h>

void s_load_patch(uint16_t patch_idx);
void s_apply_params();
void compute_crossfade_weights(uint8_t mode, uint8_t velocity, uint8_t note,
                                uint8_t switchpoint, uint8_t balance,
                                uint8_t amount, uint8_t direction,
                                float* primary_weight,
                                float* secondary_weight);
