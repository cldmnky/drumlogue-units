#include "dsp_chain.h"
#include "druteus_state.h"
#include "params.h"
#include "../common/neon_dsp.h"
#include "../common/simd_utils.h"
#include <string.h>

common::ChorusStereoWidener chorus_dsp;
rings::Reverb reverb_dsp;
uint16_t reverb_buffer[32768];

SVFilter filter_l;
SVFilter filter_r;
float fx_buf_l[256];
float fx_buf_r[256];

void dsp_init() {
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
  filter_l.Init(48000.0f);
  filter_r.Init(48000.0f);
}

void dsp_reset() {
  chorus_dsp.Reset();
  reverb_dsp.Clear();
}

void dsp_process_filter(float *out, uint32_t frames) {
  int cutoff_param = Params[param_cutoff];
  int res_param    = Params[param_resonance];
  if (cutoff_param < 127 || res_param > 0) {
    float cutoff = cutoff_param / 127.0f;
    float res    = res_param / 127.0f;
    filter_l.SetCutoff(cutoff);
    filter_l.SetResonance(res);
    filter_r.SetCutoff(cutoff);
    filter_r.SetResonance(res);
    for (uint32_t i = 0; i < frames; i++) {
      float l = out[i * 2];
      float r = out[i * 2 + 1];
      out[i * 2]     = filter_l.Process(l);
      out[i * 2 + 1] = filter_r.Process(r);
    }
  }
}

void dsp_process_effects(float *out, uint32_t frames) {
  float global_chorus = Params[param_chorus] / 15.0f;
  float patch_chorus  = (current_patch.i1chorus + current_patch.i2chorus) / 30.0f;
  float chorus_mix    = global_chorus * 0.5f + patch_chorus * 0.5f;
  float reverb_amount = Params[param_reverb] / 127.0f;

  if (chorus_mix > 0.0f || reverb_amount > 0.0f) {
    simd_deinterleave_stereo(out, fx_buf_l, fx_buf_r, frames);

    if (chorus_mix > 0.0f) {
      chorus_dsp.SetMix(chorus_mix);
      chorus_dsp.ProcessStereoBatch(fx_buf_l, fx_buf_r, frames);
    }

    if (reverb_amount > 0.0f) {
      reverb_dsp.set_amount(reverb_amount * 0.4f);
      reverb_dsp.Process(fx_buf_l, fx_buf_r, frames);
    }

    simd_interleave_stereo(fx_buf_l, fx_buf_r, out, frames);
  }
}
