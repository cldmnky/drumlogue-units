// Adapted from Mutable Instruments Clouds diffuser.h for 48kHz sample rate
// Original Copyright 2014 Emilie Gillet (MIT License)
//
// AP diffusion network - provides texture/smearing effect
// Delay line sizes scaled by 1.5x for 48kHz (original: 32kHz)

#ifndef CLOUDS_REVFX_DIFFUSER_H_
#define CLOUDS_REVFX_DIFFUSER_H_

#include "stmlib/stmlib.h"
#include "clouds/dsp/fx/fx_engine.h"

namespace clouds_revfx {

class Diffuser48k {
 public:
  Diffuser48k() { }
  ~Diffuser48k() { }
  
  void Init(uint16_t* buffer) {
    engine_.Init(buffer);
    amount_ = 0.5f;
  }
  
  void Clear() {
    engine_.Clear();
  }
  
  void Process(clouds::FloatFrame* in_out, size_t size) {
    // Original delay line sizes at 32kHz:
    // Left: 126, 180, 269, 444
    // Right: 151, 205, 245, 405
    //
    // Scaled by 1.5x for 48kHz:
    // Left: 189, 270, 404, 666
    // Right: 227, 308, 368, 608
    
    typedef E::Reserve<189,
      E::Reserve<270,
      E::Reserve<404,
      E::Reserve<666,
      E::Reserve<227,
      E::Reserve<308,
      E::Reserve<368,
      E::Reserve<608> > > > > > > > Memory;
    E::DelayLine<Memory, 0> apl1;
    E::DelayLine<Memory, 1> apl2;
    E::DelayLine<Memory, 2> apl3;
    E::DelayLine<Memory, 3> apl4;
    E::DelayLine<Memory, 4> apr1;
    E::DelayLine<Memory, 5> apr2;
    E::DelayLine<Memory, 6> apr3;
    E::DelayLine<Memory, 7> apr4;
    E::Context c;
    
    // All-pass coefficient (same as original)
    const float kap = 0.625f;
    
    while (size--) {
      engine_.Start(&c);
      
      float wet = 0.0f;
      
      // Left channel - cascade of 4 all-pass filters
      c.Read(in_out->l);
      c.Read(apl1 TAIL, kap);
      c.WriteAllPass(apl1, -kap);
      c.Read(apl2 TAIL, kap);
      c.WriteAllPass(apl2, -kap);
      c.Read(apl3 TAIL, kap);
      c.WriteAllPass(apl3, -kap);
      c.Read(apl4 TAIL, kap);
      c.WriteAllPass(apl4, -kap);
      c.Write(wet, 0.0f);
      in_out->l += amount_ * (wet - in_out->l);
      
      // Right channel - cascade of 4 all-pass filters (different delays)
      c.Read(in_out->r);
      c.Read(apr1 TAIL, kap);
      c.WriteAllPass(apr1, -kap);
      c.Read(apr2 TAIL, kap);
      c.WriteAllPass(apr2, -kap);
      c.Read(apr3 TAIL, kap);
      c.WriteAllPass(apr3, -kap);
      c.Read(apr4 TAIL, kap);
      c.WriteAllPass(apr4, -kap);
      c.Write(wet, 0.0f);
      in_out->r += amount_ * (wet - in_out->r);

      ++in_out;
    }
  }
  
  void set_amount(float amount) {
    amount_ = amount;
  }
  
 private:
  // Optimized: Using 16-bit format instead of 32-bit float
  // Memory reduced from 16KB to 8KB with minimal quality loss for diffusion
  // Memory requirement: 189+270+404+666+227+308+368+608 = 3040 samples
  // With padding we use 4096 samples = 8KB (uint16_t buffer)
  typedef clouds::FxEngine<4096, clouds::FORMAT_16_BIT> E;
  E engine_;
  
  float amount_;
  
  DISALLOW_COPY_AND_ASSIGN(Diffuser48k);
};

}  // namespace clouds_revfx

#endif  // CLOUDS_REVFX_DIFFUSER_H_
