// Copyright 2014 Emilie Gillet.
// Copyright 2024 CLDMNKY - Adapted for drumlogue 48kHz
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//
// Pitch shifter adapted for drumlogue at 48kHz sample rate
// Original delay sizes scaled by 1.5x (32kHz -> 48kHz)
// Original: 2047 samples @ 32kHz ≈ 64ms
// Adapted:  3072 samples @ 48kHz ≈ 64ms (maintains same time window)

#ifndef CLOUDS_REVFX_PITCH_SHIFTER_H_
#define CLOUDS_REVFX_PITCH_SHIFTER_H_

#include "stmlib/stmlib.h"
#include "clouds/dsp/frame.h"
#include "clouds/dsp/fx/fx_engine.h"

namespace clouds_revfx {

class PitchShifter {
 public:
  PitchShifter() { }
  ~PitchShifter() { }
  
  void Init(uint16_t* buffer) {
    engine_.Init(buffer);
    phase_ = 0;
    // Scaled for 48kHz: 3071 samples ≈ 64ms
    size_ = 3071.0f;
    ratio_ = 1.0f;
    amount_ = 0.0f;
  }
  
  void Clear() {
    engine_.Clear();
  }

  inline void Process(clouds::FloatFrame* input_output, size_t size) {
    while (size--) {
      Process(input_output);
      ++input_output;
    }
  }
  
  void Process(clouds::FloatFrame* input_output) {
    // Delay line sizes scaled for 48kHz: 2047 * 1.5 ≈ 3071
    typedef E::Reserve<3071, E::Reserve<3071> > Memory;
    E::DelayLine<Memory, 0> left;
    E::DelayLine<Memory, 1> right;
    E::Context c;
    engine_.Start(&c);
    
    // Advance phase based on pitch ratio
    phase_ += (1.0f - ratio_) / size_;
    if (phase_ >= 1.0f) {
      phase_ -= 1.0f;
    }
    if (phase_ <= 0.0f) {
      phase_ += 1.0f;
    }
    
    // Triangle crossfade between two grains
    float tri = 2.0f * (phase_ >= 0.5f ? 1.0f - phase_ : phase_);
    float phase = phase_ * size_;
    float half = phase + size_ * 0.5f;
    if (half >= size_) {
      half -= size_;
    }
    
    // Store dry signal for mixing
    float dry_l = input_output->l;
    float dry_r = input_output->r;
    
    // Process left channel
    c.Read(input_output->l, 1.0f);
    c.Write(left, 0.0f);
    c.Interpolate(left, phase, tri);
    c.Interpolate(left, half, 1.0f - tri);
    c.Write(input_output->l, 0.0f);

    // Process right channel
    c.Read(input_output->r, 1.0f);
    c.Write(right, 0.0f);
    c.Interpolate(right, phase, tri);
    c.Interpolate(right, half, 1.0f - tri);
    c.Write(input_output->r, 0.0f);
    
    // Mix dry/wet based on amount
    input_output->l = dry_l * (1.0f - amount_) + input_output->l * amount_;
    input_output->r = dry_r * (1.0f - amount_) + input_output->r * amount_;
  }
  
  // Set pitch ratio (0.5 = octave down, 1.0 = unity, 2.0 = octave up)
  inline void set_ratio(float ratio) {
    ratio_ = ratio;
  }
  
  // Set pitch in semitones (-24 to +24)
  inline void set_pitch(float semitones) {
    // Convert semitones to ratio: 2^(semitones/12)
    float octaves = semitones / 12.0f;
    float x = octaves * 0.6931472f;  // ln(2)
    // Taylor series approximation of 2^x
    ratio_ = 1.0f + x * (1.0f + x * (0.5f + x * (0.166667f + x * 0.041667f)));
    // Clamp to reasonable range
    if (ratio_ < 0.25f) ratio_ = 0.25f;
    if (ratio_ > 4.0f) ratio_ = 4.0f;
  }
  
  // Set grain window size (0.0 to 1.0)
  inline void set_size(float size) {
    // Scaled for 48kHz: original 128-2047 -> 192-3071
    float target_size = 192.0f + (3071.0f - 192.0f) * size * size * size;
    ONE_POLE(size_, target_size, 0.05f)
  }
  
  // Set dry/wet mix amount (0.0 = dry, 1.0 = wet)
  inline void set_amount(float amount) {
    amount_ = amount;
  }
  
  inline float amount() const { return amount_; }
  inline float ratio() const { return ratio_; }
  
 private:
  // Buffer size for 48kHz: 8192 samples = 16KB (uint16_t)
  // This provides enough headroom for the 3071 sample delay lines
  typedef clouds::FxEngine<8192, clouds::FORMAT_16_BIT> E;
  E engine_;
  float phase_;
  float ratio_;
  float size_;
  float amount_;
  
  DISALLOW_COPY_AND_ASSIGN(PitchShifter);
};

}  // namespace clouds_revfx

#endif  // CLOUDS_REVFX_PITCH_SHIFTER_H_
