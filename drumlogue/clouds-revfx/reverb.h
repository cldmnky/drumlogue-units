// Adapted from Mutable Instruments Clouds reverb for drumlogue (48kHz)
// Original: Copyright 2014 Emilie Gillet (MIT License)
//
// Changes for drumlogue:
// - LFO frequencies scaled from 32kHz to 48kHz
// - Sample rate constant updated

#ifndef CLOUDS_REVFX_REVERB_H_
#define CLOUDS_REVFX_REVERB_H_

#include "stmlib/stmlib.h"
#include "clouds/dsp/fx/fx_engine.h"

namespace clouds_revfx {

// Sample rate for drumlogue
constexpr float kSampleRate = 48000.0f;
constexpr float kSampleRateRatio = 32000.0f / 48000.0f;  // Original / Target

class Reverb {
 public:
  Reverb() { }
  ~Reverb() { }
  
  void Init(uint16_t* buffer) {
    engine_.Init(buffer);
    // LFO frequencies scaled for 48kHz (original was 32kHz)
    // Original: 0.5f / 32000.0f and 0.3f / 32000.0f
    engine_.SetLFOFrequency(clouds::LFO_1, 0.5f / kSampleRate);
    engine_.SetLFOFrequency(clouds::LFO_2, 0.3f / kSampleRate);
    lp_ = 0.7f;
    diffusion_ = 0.625f;
    lp_decay_1_ = 0.0f;
    lp_decay_2_ = 0.0f;
    amount_ = 0.5f;
    input_gain_ = 0.2f;
    reverb_time_ = 0.5f;
  }
  
  void Clear() {
    engine_.Clear();
    lp_decay_1_ = 0.0f;
    lp_decay_2_ = 0.0f;
  }
  
  void Process(clouds::FloatFrame* in_out, size_t size) {
    // This is the Griesinger topology described in the Dattorro paper
    // (4 AP diffusers on the input, then a loop of 2x 2AP+1Delay).
    // Modulation is applied in the loop of the first diffuser AP for additional
    // smearing; and to the two long delays for a slow shimmer/chorus effect.
    //
    // Delay line sizes scaled by 48/32 = 1.5 for 48kHz sample rate
    // Original sizes: 113, 162, 241, 399, 1653, 2038, 3411, 1913, 1663, 4782
    // Scaled sizes:   170, 243, 362, 599, 2480, 3057, 5117, 2870, 2495, 7173
    typedef E::Reserve<170,
      E::Reserve<243,
      E::Reserve<362,
      E::Reserve<599,
      E::Reserve<2480,
      E::Reserve<3057,
      E::Reserve<5117,
      E::Reserve<2870,
      E::Reserve<2495,
      E::Reserve<7173> > > > > > > > > > Memory;
    E::DelayLine<Memory, 0> ap1;
    E::DelayLine<Memory, 1> ap2;
    E::DelayLine<Memory, 2> ap3;
    E::DelayLine<Memory, 3> ap4;
    E::DelayLine<Memory, 4> dap1a;
    E::DelayLine<Memory, 5> dap1b;
    E::DelayLine<Memory, 6> del1;
    E::DelayLine<Memory, 7> dap2a;
    E::DelayLine<Memory, 8> dap2b;
    E::DelayLine<Memory, 9> del2;
    E::Context c;

    const float kap = diffusion_;
    const float klp = lp_;
    const float krt = reverb_time_;
    const float amount = amount_;
    const float gain = input_gain_;

    float lp_1 = lp_decay_1_;
    float lp_2 = lp_decay_2_;

    while (size--) {
      float wet;
      float apout = 0.0f;
      engine_.Start(&c);
      
      // Smear AP1 inside the loop - scaled offsets for 48kHz
      // Original: 10.0f, 60.0f, 100
      c.Interpolate(ap1, 15.0f, clouds::LFO_1, 90.0f, 1.0f);
      c.Write(ap1, 150, 0.0f);
      
      c.Read(in_out->l + in_out->r, gain);

      // Diffuse through 4 allpasses.
      c.Read(ap1 TAIL, kap);
      c.WriteAllPass(ap1, -kap);
      c.Read(ap2 TAIL, kap);
      c.WriteAllPass(ap2, -kap);
      c.Read(ap3 TAIL, kap);
      c.WriteAllPass(ap3, -kap);
      c.Read(ap4 TAIL, kap);
      c.WriteAllPass(ap4, -kap);
      c.Write(apout);
      
      // Main reverb loop - scaled offsets for 48kHz
      // Original: 4680.0f, 100.0f
      c.Load(apout);
      c.Interpolate(del2, 7020.0f, clouds::LFO_2, 150.0f, krt);
      c.Lp(lp_1, klp);
      c.Read(dap1a TAIL, -kap);
      c.WriteAllPass(dap1a, kap);
      c.Read(dap1b TAIL, kap);
      c.WriteAllPass(dap1b, -kap);
      c.Write(del1, 2.0f);
      c.Write(wet, 0.0f);

      in_out->l += (wet - in_out->l) * amount;

      c.Load(apout);
      c.Read(del1 TAIL, krt);
      c.Lp(lp_2, klp);
      c.Read(dap2a TAIL, kap);
      c.WriteAllPass(dap2a, -kap);
      c.Read(dap2b TAIL, -kap);
      c.WriteAllPass(dap2b, kap);
      c.Write(del2, 2.0f);
      c.Write(wet, 0.0f);

      in_out->r += (wet - in_out->r) * amount;
      
      ++in_out;
    }
    
    lp_decay_1_ = lp_1;
    lp_decay_2_ = lp_2;
  }
  
  inline void set_amount(float amount) {
    amount_ = amount;
  }
  
  inline void set_input_gain(float input_gain) {
    input_gain_ = input_gain;
  }

  inline void set_time(float reverb_time) {
    reverb_time_ = reverb_time;
  }
  
  inline void set_diffusion(float diffusion) {
    diffusion_ = diffusion;
  }
  
  inline void set_lp(float lp) {
    lp_ = lp;
  }
  
 private:
  // Buffer size scaled for 48kHz: original 16384 -> 24576
  // Total delay needed: 170+243+362+599+2480+3057+5117+2870+2495+7173 = 24566
  typedef clouds::FxEngine<32768, clouds::FORMAT_12_BIT> E;
  E engine_;
  
  float amount_;
  float input_gain_;
  float reverb_time_;
  float diffusion_;
  float lp_;
  
  float lp_decay_1_;
  float lp_decay_2_;
  
  DISALLOW_COPY_AND_ASSIGN(Reverb);
};

}  // namespace clouds_revfx

#endif  // CLOUDS_REVFX_REVERB_H_
