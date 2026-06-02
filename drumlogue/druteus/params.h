#pragma once

#include <stdint.h>

enum {
  param_soundfont      = 0,
  param_preset         = 1,
  param_max_voices     = 2,
  param_transpose      = 3,
  param_fine_tune      = 4,
  param_volume         = 5,
  param_pan            = 6,
  param_unused_7       = 7,
  param_xfade          = 8,
  param_layers         = 9,
  param_unused_10      = 10,
  param_unused_11      = 11,
  param_chorus         = 12,
  param_reverb         = 13,
  param_velocity_curve = 14,
  param_unused_15      = 15,
  param_cutoff         = 16,
  param_resonance      = 17,
  param_unused_18      = 18,
  param_unused_19      = 19,
  param_lfo_rate       = 20,
  param_lfo_amount     = 21,
  param_lfo_dest       = 22,
  param_lfo_wave       = 23,
  param_num,
};

extern const int32_t kParamDefaults[param_num];
extern int32_t Params[param_num];

void params_set(uint8_t index, int32_t value);
int32_t params_get(uint8_t index);
const char *params_get_str(uint8_t index, int32_t value);
