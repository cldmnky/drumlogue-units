#ifndef LFO_H
#define LFO_H

#include <cstdint>
#include <cmath>

namespace clouds_revfx {

// LFO waveform types
enum LfoWaveform {
  LFO_WAVE_SINE = 0,
  LFO_WAVE_SAW = 1,
  LFO_WAVE_RANDOM = 2,
  LFO_WAVE_COUNT
};

// LFO modulation target (0 = OFF)
enum LfoTarget {
  LFO_TARGET_OFF = 0,
  LFO_TARGET_DRY_WET = 1,
  LFO_TARGET_TIME = 2,
  LFO_TARGET_DIFFUSION = 3,
  LFO_TARGET_LP = 4,
  LFO_TARGET_INPUT_GAIN = 5,
  LFO_TARGET_TEXTURE = 6,
  LFO_TARGET_GRAIN_AMT = 7,
  LFO_TARGET_GRAIN_SIZE = 8,
  LFO_TARGET_GRAIN_DENS = 9,
  LFO_TARGET_GRAIN_PITCH = 10,
  LFO_TARGET_GRAIN_POS = 11,
  LFO_TARGET_SHIFT_AMT = 12,
  LFO_TARGET_SHIFT_PITCH = 13,
  LFO_TARGET_SHIFT_SIZE = 14,
  LFO_TARGET_LFO2_SPEED = 15,  // Cross-modulation: LFO1 can modulate LFO2 speed
  LFO_TARGET_COUNT
};

// Simple LFO with multiple waveforms
// Designed for control-rate operation (once per audio block)
class Lfo {
 public:
  Lfo() : phase_(0.0f), frequency_(1.0f), waveform_(LFO_WAVE_SINE),
          target_(LFO_TARGET_OFF), depth_(0.5f), current_random_(0.0f),
          next_random_(0.0f), random_phase_(0.0f) {}

  void Init(float sample_rate_hz = 48000.0f, uint32_t block_size = 64) {
    sample_rate_ = sample_rate_hz;
    block_size_ = block_size;
    phase_ = 0.0f;
    current_random_ = 0.0f;
    next_random_ = GenerateRandom();
    random_phase_ = 0.0f;
  }

  void Reset() {
    phase_ = 0.0f;
    random_phase_ = 0.0f;
    current_random_ = 0.0f;
    next_random_ = GenerateRandom();
  }

  // Set frequency in Hz (0.05 - 10 Hz typical range)
  void set_frequency(float freq_hz) {
    frequency_ = freq_hz;
  }

  // Set frequency from 0-127 parameter value
  // Maps to approximately 0.05Hz - 10Hz exponential curve
  void set_speed_from_param(int32_t value) {
    // Exponential mapping for more musical feel
    // 0 -> ~0.05Hz, 64 -> ~1Hz, 127 -> ~10Hz
    float normalized = static_cast<float>(value) / 127.0f;
    frequency_ = 0.05f * std::pow(200.0f, normalized);
  }

  void set_waveform(LfoWaveform wf) {
    waveform_ = wf;
  }

  void set_waveform_from_param(int32_t value) {
    waveform_ = static_cast<LfoWaveform>(value % LFO_WAVE_COUNT);
  }

  void set_target(LfoTarget tgt) {
    target_ = tgt;
  }

  void set_target_from_param(int32_t value) {
    target_ = static_cast<LfoTarget>(value % LFO_TARGET_COUNT);
  }

  // Set modulation depth (0.0 - 1.0)
  void set_depth(float depth) {
    depth_ = depth;
  }

  void set_depth_from_param(int32_t value) {
    depth_ = static_cast<float>(value) / 127.0f;
  }

  LfoTarget target() const { return target_; }
  float depth() const { return depth_; }
  LfoWaveform waveform() const { return waveform_; }

  // Process one block and return current LFO value (-1.0 to +1.0)
  // Call once per audio block for efficiency
  float Process() {
    float output = 0.0f;

    switch (waveform_) {
      case LFO_WAVE_SINE:
        output = SineWave(phase_);
        break;
      case LFO_WAVE_SAW:
        output = SawWave(phase_);
        break;
      case LFO_WAVE_RANDOM:
        output = RandomWave();
        break;
      default:
        output = SineWave(phase_);
        break;
    }

    // Advance phase for next block
    // Phase increment = frequency * block_size / sample_rate
    float phase_inc = frequency_ * static_cast<float>(block_size_) / sample_rate_;
    phase_ += phase_inc;
    while (phase_ >= 1.0f) {
      phase_ -= 1.0f;
      // Generate new random target on cycle wrap
      if (waveform_ == LFO_WAVE_RANDOM) {
        current_random_ = next_random_;
        next_random_ = GenerateRandom();
      }
    }

    return output * depth_;
  }

  // Get current value without advancing phase (for reading between Process calls)
  float value() const {
    float output = 0.0f;
    switch (waveform_) {
      case LFO_WAVE_SINE:
        output = SineWave(phase_);
        break;
      case LFO_WAVE_SAW:
        output = SawWave(phase_);
        break;
      case LFO_WAVE_RANDOM:
        // Interpolate between current and next random values
        output = current_random_ + (next_random_ - current_random_) * phase_;
        break;
      default:
        output = SineWave(phase_);
        break;
    }
    return output * depth_;
  }

 private:
  // Sine wave: smooth oscillation
  static float SineWave(float phase) {
    // Use fast sine approximation: sin(2*pi*phase)
    // Parabolic approximation for efficiency
    float x = phase * 2.0f - 1.0f;  // -1 to +1
    float x2 = x * x;
    // 4th order polynomial approximation of sin(pi*x) for x in [-1,1]
    return x * (3.14159265f - x2 * (5.1677f - x2 * 2.5487f)) * 0.32f;
  }

  // Saw wave: ramp down from +1 to -1
  static float SawWave(float phase) {
    return 1.0f - 2.0f * phase;
  }

  // Random wave: smoothly interpolated random values (sample & hold with interpolation)
  float RandomWave() const {
    // Smooth interpolation between random values
    return current_random_ + (next_random_ - current_random_) * phase_;
  }

  // Simple pseudo-random number generator (-1 to +1)
  float GenerateRandom() {
    // Linear congruential generator
    random_seed_ = random_seed_ * 1103515245 + 12345;
    return static_cast<float>(static_cast<int32_t>(random_seed_ >> 16) & 0x7FFF) / 16384.0f - 1.0f;
  }

  float phase_;           // 0.0 - 1.0
  float frequency_;       // Hz
  float sample_rate_;     // Hz
  uint32_t block_size_;   // samples per block
  LfoWaveform waveform_;
  LfoTarget target_;
  float depth_;           // 0.0 - 1.0

  // Random waveform state
  float current_random_;
  float next_random_;
  float random_phase_;
  uint32_t random_seed_ = 0x12345678;
};

// String names for LFO targets (for UI display)
inline const char* GetLfoTargetName(int32_t target) {
  static const char* kTargetNames[] = {
    "OFF",      // 0
    "DRY/WET",  // 1
    "TIME",     // 2
    "DIFFUSN",  // 3
    "LP DAMP",  // 4
    "IN GAIN",  // 5
    "TEXTURE",  // 6
    "GRN AMT",  // 7
    "GRN SZ",   // 8
    "GRN DNS",  // 9
    "GRN PTCH", // 10
    "GRN POS",  // 11
    "SFT AMT",  // 12
    "SFT PTCH", // 13
    "SFT SZ",   // 14
    "LFO2 SPD", // 15 (cross-modulation)
  };
  if (target < 0 || target >= LFO_TARGET_COUNT) {
    return "OFF";
  }
  return kTargetNames[target];
}

// String names for LFO waveforms (for UI display)
inline const char* GetLfoWaveformName(int32_t waveform) {
  static const char* kWaveformNames[] = {
    "SINE",
    "SAW",
    "RANDOM",
  };
  if (waveform < 0 || waveform >= LFO_WAVE_COUNT) {
    return "SINE";
  }
  return kWaveformNames[waveform];
}

}  // namespace clouds_revfx

#endif  // LFO_H
