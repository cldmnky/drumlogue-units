// Copyright 2024 CLDMNKY
// Micro-granular processor for drumlogue - simplified grain cloud texture
// Inspired by Mutable Instruments Clouds
//
// Memory-efficient implementation using ~16KB buffer for short grain textures

#ifndef CLOUDS_REVFX_MICRO_GRANULAR_H_
#define CLOUDS_REVFX_MICRO_GRANULAR_H_

#include "stmlib/stmlib.h"
#include "stmlib/dsp/dsp.h"
#include "stmlib/utils/random.h"
#include "clouds/dsp/frame.h"
#include "dsp/neon_dsp.h"

#include <algorithm>

#ifdef USE_NEON
#include "../common/simd_utils.h"
#endif

namespace clouds_revfx {

// Buffer size: 2048 stereo samples = ~42ms at 48kHz = 16KB
static constexpr size_t kMicroGranularBufferSize = 2048;
// Maximum number of simultaneous grains (reduced from Clouds' 64)
static constexpr size_t kMaxMicroGrains = 8;
// Minimum grain size in samples (~5ms)
static constexpr size_t kMinGrainSize = 240;
// Maximum grain size in samples (~40ms)  
static constexpr size_t kMaxGrainSize = 1920;

// Simple grain structure
struct MicroGrain {
  bool active;
  int32_t start_position;    // Where in buffer grain starts
  int32_t size;              // Grain duration in samples
  int32_t phase;             // Current playback position (16.16 fixed point)
  int32_t phase_increment;   // Playback rate (16.16 fixed point, 65536 = 1.0)
  float envelope_phase;      // 0.0 - 2.0 (0-1 = attack, 1-2 = decay)
  float envelope_increment;
  float gain_l;
  float gain_r;
  
  void Init() {
    active = false;
    phase = 0;
    envelope_phase = 2.0f;
  }
  
  void Start(int32_t buffer_size, int32_t start, int32_t grain_size, 
             int32_t pitch_ratio, float pan, float stereo_spread) {
    start_position = (start + buffer_size) % buffer_size;
    size = grain_size;
    phase = 0;
    phase_increment = pitch_ratio;
    envelope_phase = 0.0f;
    envelope_increment = 2.0f / static_cast<float>(grain_size);
    
    // Stereo panning with spread
    float pan_offset = stereo_spread * (stmlib::Random::GetFloat() - 0.5f);
    float final_pan = 0.5f + pan_offset;
    if (final_pan < 0.0f) final_pan = 0.0f;
    if (final_pan > 1.0f) final_pan = 1.0f;
    
    // Equal power panning approximation
    gain_l = 1.0f - final_pan;
    gain_r = final_pan;
    
    active = true;
  }
  
  // Simple triangle/hann-ish envelope without lookup table
  inline float GetEnvelope() {
    float env = envelope_phase;
    // Triangle: 0->1->0 over phase 0->1->2
    env = env >= 1.0f ? 2.0f - env : env;
    // Smooth with simple curve (approximates hann)
    env = env * env * (3.0f - 2.0f * env);
    return env;
  }
};

class MicroGranular {
 public:
  MicroGranular() { }
  ~MicroGranular() { }
  
  void Init(float* buffer_l, float* buffer_r) {
    buffer_l_ = buffer_l;
    buffer_r_ = buffer_r;
    buffer_size_ = kMicroGranularBufferSize;
    write_head_ = 0;
    freeze_ = false;
    
    // Parameters defaults
    position_ = 0.5f;
    size_ = 0.5f;
    density_ = 0.5f;
    pitch_ = 0.0f;
    stereo_spread_ = 0.3f;
    amount_ = 0.0f;
    
    grain_rate_counter_ = 0.0f;
    
    for (size_t i = 0; i < kMaxMicroGrains; ++i) {
      grains_[i].Init();
    }
    
    // Clear buffer
    Clear();
  }
  
  void Clear() {
    for (size_t i = 0; i < buffer_size_; ++i) {
      buffer_l_[i] = 0.0f;
      buffer_r_[i] = 0.0f;
    }
    write_head_ = 0;
  }
  
  void Process(clouds::FloatFrame* in_out, size_t size) {
    if (amount_ <= 0.001f) {
      // Bypass: just record to buffer for when we enable
      WriteToBuffer(in_out, size);
      return;
    }
    
    // Write input to circular buffer (unless frozen)
    if (!freeze_) {
      WriteToBuffer(in_out, size);
    }
    
    // Try to spawn new grains
    SpawnGrains(size);
    
    // Process all active grains and mix
    float out_l[64];
    float out_r[64];
    // Use NEON-optimized buffer clearing
    clouds_revfx::neon::ClearStereoBuffers(out_l, out_r, static_cast<uint32_t>(size));
    
    for (size_t g = 0; g < kMaxMicroGrains; ++g) {
      if (grains_[g].active) {
        ProcessGrain(&grains_[g], out_l, out_r, size);
      }
    }
    
    // Soft clip the granular output buffers using NEON
#ifdef USE_NEON
    {
      uint32_t i = 0;
      for (; i + 4 <= size; i += 4) {
        float32x4_t l = simd_load4(&out_l[i]);
        float32x4_t r = simd_load4(&out_r[i]);
        simd_store4(&out_l[i], simd_softclip4(l));
        simd_store4(&out_r[i], simd_softclip4(r));
      }
      // Scalar tail
      for (; i < size; ++i) {
        out_l[i] = SoftClip(out_l[i]);
        out_r[i] = SoftClip(out_r[i]);
      }
    }
#else
    for (size_t i = 0; i < size; ++i) {
      out_l[i] = SoftClip(out_l[i]);
      out_r[i] = SoftClip(out_r[i]);
    }
#endif
    
    // Mix granular output with dry signal
    for (size_t i = 0; i < size; ++i) {
      in_out[i].l = in_out[i].l * (1.0f - amount_) + out_l[i] * amount_;
      in_out[i].r = in_out[i].r * (1.0f - amount_) + out_r[i] * amount_;
    }
  }
  
  // Parameter setters
  void set_amount(float amount) { amount_ = amount; }
  void set_position(float position) { position_ = position; }
  void set_size(float size) { size_ = size; }
  void set_density(float density) { density_ = density; }
  void set_pitch(float pitch) { pitch_ = pitch; }  // In semitones
  void set_stereo_spread(float spread) { stereo_spread_ = spread; }
  void set_freeze(bool freeze) { freeze_ = freeze; }
  
  bool freeze() const { return freeze_; }
  
 private:
  void WriteToBuffer(const clouds::FloatFrame* in, size_t size) {
    for (size_t i = 0; i < size; ++i) {
      buffer_l_[write_head_] = in[i].l;
      buffer_r_[write_head_] = in[i].r;
      write_head_ = (write_head_ + 1) % buffer_size_;
    }
  }
  
  void SpawnGrains(size_t block_size) {
    // Density controls how often we spawn grains
    // At density=0.5, spawn roughly every grain_size samples
    // Higher density = more grains, lower = fewer
    float grain_size_samples = kMinGrainSize + size_ * (kMaxGrainSize - kMinGrainSize);
    float spawn_rate = density_ * density_ * 4.0f;  // 0 to 4 grains per grain_size
    if (spawn_rate < 0.1f) spawn_rate = 0.1f;
    
    float spawn_probability = spawn_rate * static_cast<float>(block_size) / grain_size_samples;
    
    // Try to spawn a grain
    if (stmlib::Random::GetFloat() < spawn_probability) {
      // Find an inactive grain
      for (size_t g = 0; g < kMaxMicroGrains; ++g) {
        if (!grains_[g].active) {
          // Calculate grain parameters
          int32_t grain_size = static_cast<int32_t>(grain_size_samples);
          
          // Position in buffer (position_ = 0 means recent, 1 = oldest)
          int32_t max_offset = buffer_size_ - grain_size - 64;
          if (max_offset < 64) max_offset = 64;
          int32_t base_offset = static_cast<int32_t>(position_ * max_offset);
          // Add some randomization
          int32_t random_offset = static_cast<int32_t>(
              stmlib::Random::GetFloat() * max_offset * 0.1f);
          int32_t offset = base_offset + random_offset;
          if (offset < 0) offset = 0;
          if (offset > max_offset) offset = max_offset;
          
          int32_t start = (write_head_ - grain_size - offset + buffer_size_) % buffer_size_;
          
          // Pitch ratio (65536 = 1.0, no pitch shift)
          float pitch_ratio = SemitonesToRatio(pitch_);
          int32_t pitch_fixed = static_cast<int32_t>(pitch_ratio * 65536.0f);
          
          grains_[g].Start(buffer_size_, start, grain_size, pitch_fixed, 
                          0.5f, stereo_spread_);
          break;
        }
      }
    }
  }
  
  void ProcessGrain(MicroGrain* grain, float* out_l, float* out_r, size_t size) {
#ifdef USE_NEON
    // NEON-optimized grain processing: process 4 samples at a time when possible
    // Pre-calculate how many samples remain until grain ends
    int32_t samples_until_end = grain->size - (grain->phase >> 16);
    float env_samples_until_end = (2.0f - grain->envelope_phase) / grain->envelope_increment;
    int32_t max_safe_samples = std::min(samples_until_end, 
                                        static_cast<int32_t>(env_samples_until_end));
    
    size_t i = 0;
    
    // Process 4 samples at a time while we can guarantee grain stays active
    while (i + 4 <= size && max_safe_samples >= 4) {
      // Pre-compute envelope for 4 consecutive samples
      float env0 = grain->GetEnvelope();
      grain->envelope_phase += grain->envelope_increment;
      float env1 = grain->GetEnvelope();
      grain->envelope_phase += grain->envelope_increment;
      float env2 = grain->GetEnvelope();
      grain->envelope_phase += grain->envelope_increment;
      float env3 = grain->GetEnvelope();
      grain->envelope_phase += grain->envelope_increment;
      
      float32x4_t env_vec = {env0, env1, env2, env3};
      float32x4_t gain_l_vec = vdupq_n_f32(grain->gain_l);
      float32x4_t gain_r_vec = vdupq_n_f32(grain->gain_r);
      
      // Load 4 samples with linear interpolation (unrolled)
      float samples_l[4], samples_r[4];
      for (int j = 0; j < 4; ++j) {
        int32_t sample_index = grain->start_position + (grain->phase >> 16);
        sample_index = sample_index % buffer_size_;
        int32_t next_index = (sample_index + 1) % buffer_size_;
        float frac = static_cast<float>(grain->phase & 0xFFFF) * (1.0f / 65536.0f);
        float inv_frac = 1.0f - frac;
        
        samples_l[j] = buffer_l_[sample_index] * inv_frac + buffer_l_[next_index] * frac;
        samples_r[j] = buffer_r_[sample_index] * inv_frac + buffer_r_[next_index] * frac;
        
        grain->phase += grain->phase_increment;
      }
      
      // NEON multiply-accumulate: out += sample * env * gain
      float32x4_t samp_l = vld1q_f32(samples_l);
      float32x4_t samp_r = vld1q_f32(samples_r);
      float32x4_t out_l_vec = vld1q_f32(&out_l[i]);
      float32x4_t out_r_vec = vld1q_f32(&out_r[i]);
      
      // Apply envelope and gain: sample * env * gain
      samp_l = vmulq_f32(samp_l, env_vec);
      samp_l = vmulq_f32(samp_l, gain_l_vec);
      samp_r = vmulq_f32(samp_r, env_vec);
      samp_r = vmulq_f32(samp_r, gain_r_vec);
      
      // Accumulate
      out_l_vec = vaddq_f32(out_l_vec, samp_l);
      out_r_vec = vaddq_f32(out_r_vec, samp_r);
      
      vst1q_f32(&out_l[i], out_l_vec);
      vst1q_f32(&out_r[i], out_r_vec);
      
      i += 4;
      max_safe_samples -= 4;
    }
    
    // Scalar tail: handle remaining samples or when near grain end
    for (; i < size; ++i) {
      if (!grain->active) break;
      
      float env = grain->GetEnvelope();
      grain->envelope_phase += grain->envelope_increment;
      
      if (grain->envelope_phase >= 2.0f) {
        grain->active = false;
        break;
      }
      
      int32_t sample_index = grain->start_position + (grain->phase >> 16);
      sample_index = sample_index % buffer_size_;
      int32_t next_index = (sample_index + 1) % buffer_size_;
      float frac = static_cast<float>(grain->phase & 0xFFFF) * (1.0f / 65536.0f);
      
      float sample_l = buffer_l_[sample_index] * (1.0f - frac) + 
                       buffer_l_[next_index] * frac;
      float sample_r = buffer_r_[sample_index] * (1.0f - frac) + 
                       buffer_r_[next_index] * frac;
      
      out_l[i] += sample_l * env * grain->gain_l;
      out_r[i] += sample_r * env * grain->gain_r;
      
      grain->phase += grain->phase_increment;
      
      if ((grain->phase >> 16) >= grain->size) {
        grain->active = false;
      }
    }
#else
    // Scalar fallback
    for (size_t i = 0; i < size; ++i) {
      if (!grain->active) break;
      
      float env = grain->GetEnvelope();
      grain->envelope_phase += grain->envelope_increment;
      
      if (grain->envelope_phase >= 2.0f) {
        grain->active = false;
        break;
      }
      
      int32_t sample_index = grain->start_position + (grain->phase >> 16);
      sample_index = sample_index % buffer_size_;
      int32_t next_index = (sample_index + 1) % buffer_size_;
      float frac = static_cast<float>(grain->phase & 0xFFFF) / 65536.0f;
      
      float sample_l = buffer_l_[sample_index] * (1.0f - frac) + 
                       buffer_l_[next_index] * frac;
      float sample_r = buffer_r_[sample_index] * (1.0f - frac) + 
                       buffer_r_[next_index] * frac;
      
      out_l[i] += sample_l * env * grain->gain_l;
      out_r[i] += sample_r * env * grain->gain_r;
      
      grain->phase += grain->phase_increment;
      
      if ((grain->phase >> 16) >= grain->size) {
        grain->active = false;
      }
    }
#endif
  }
  
  // Convert semitones to ratio
  static float SemitonesToRatio(float semitones) {
    // 2^(semitones/12) approximation using exp
    // For small values, use polynomial approximation
    float octaves = semitones / 12.0f;
    // Simple 2^x approximation valid for -2 to +2 octaves
    float x = octaves * 0.6931472f;  // ln(2)
    float ratio = 1.0f + x * (1.0f + x * (0.5f + x * (0.166667f + x * 0.041667f)));
    return ratio;
  }
  
  // Soft clipping
  static float SoftClip(float x) {
    if (x > 1.0f) return 1.0f - 1.0f / (x + 1.0f);
    if (x < -1.0f) return -1.0f + 1.0f / (1.0f - x);
    return x;
  }
  
  float* buffer_l_;
  float* buffer_r_;
  size_t buffer_size_;
  size_t write_head_;
  
  bool freeze_;
  
  float position_;       // 0-1: where in buffer to read (0=recent, 1=old)
  float size_;           // 0-1: grain size
  float density_;        // 0-1: grain spawn rate
  float pitch_;          // semitones
  float stereo_spread_;  // 0-1
  float amount_;         // 0-1: dry/wet mix for granular
  
  float grain_rate_counter_;
  
  MicroGrain grains_[kMaxMicroGrains];
  
  DISALLOW_COPY_AND_ASSIGN(MicroGranular);
};

}  // namespace clouds_revfx

#endif  // CLOUDS_REVFX_MICRO_GRANULAR_H_
