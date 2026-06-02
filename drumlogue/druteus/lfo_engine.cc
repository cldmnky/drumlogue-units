#include "lfo_engine.h"
#include "druteus_state.h"
#include "params.h"
#include "dsp_primitives.h"
#include <math.h>
#include <stdlib.h>

float lfo_phase            = 0.0f;
float lfo2_phase           = 0.0f;
float lfo_delay_completed  = 0.0f;

float lfo_wave_shape(float phase, uint8_t shape) {
  switch (shape) {
    case 0: {
      float v = phase > 0.5f ? 1.0f - phase : phase;
      return v * 4.0f - 1.0f;
    }
    case 1: return sinf(phase * 2.0f * (float)M_PI);
    case 2: return phase < 0.5f ? 1.0f : -1.0f;
    case 3: return phase * 2.0f - 1.0f;
    case 4: {
      static float s_h = 0.0f;
      static float s_h_prev_phase = 0.0f;
      if (phase < s_h_prev_phase)
        s_h = (float)rand() / (float)RAND_MAX * 2.0f - 1.0f;
      s_h_prev_phase = phase;
      return s_h;
    }
    default: return sinf(phase * 2.0f * (float)M_PI);
  }
}

void lfo_init() {
  lfo_phase       = 0.0f;
  lfo2_phase      = 0.0f;
  lfo_delay_completed = 0.0f;
}

float lfo_process_lfo_pitch_offset(uint32_t frames) {
  float lfo_pitch_offset = 0.0f;

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

  return lfo_pitch_offset;
}

float lfo_process_user_mod(uint32_t frames, uint8_t dest) {
  float rate   = 0.05f + Params[param_lfo_rate] * (24.95f / 127.0f);
  float amt    = Params[param_lfo_amount] / 127.0f;
  uint8_t wave = Params[param_lfo_wave];
  lfo_phase += rate * (float)frames / 48000.0f;
  if (lfo_phase >= 1.0f) lfo_phase -= 1.0f;
  float lfo_val = lfo_wave_shape(lfo_phase, wave);
  if (dest == 0 || dest == 2)
    return lfo_val * amt * 0.5f;
  return 0.0f;
}
