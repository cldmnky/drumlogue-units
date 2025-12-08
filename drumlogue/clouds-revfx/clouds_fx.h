#ifndef CLOUDS_FX_H
#define CLOUDS_FX_H

#include "unit.h"
#include "clouds/dsp/frame.h"
#include "reverb.h"
#include "diffuser.h"
#include "micro_granular.h"
#include "pitch_shifter.h"
#include "lfo.h"

#include <array>
#include <cstdint>

#ifndef UNIT_PARAM_MAX
#define UNIT_PARAM_MAX 24
#endif

// Parameter indices matching header.c
enum CloudsParams {
  PARAM_DRY_WET = 0,
  PARAM_TIME,
  PARAM_DIFFUSION,
  PARAM_LP,
  // Page 2
  PARAM_INPUT_GAIN,
  PARAM_TEXTURE,     // Diffuser amount
  PARAM_GRAIN_AMT,   // Granular mix amount
  PARAM_GRAIN_SIZE,  // Grain size
  // Page 3 - Granular controls
  PARAM_GRAIN_DENS,  // Grain density
  PARAM_GRAIN_PITCH, // Grain pitch shift
  PARAM_GRAIN_POS,   // Grain buffer position
  PARAM_FREEZE,      // Freeze toggle
  // Page 4 - Pitch shifter
  PARAM_SHIFT_AMT,   // Pitch shifter amount
  PARAM_SHIFT_PITCH, // Pitch shift semitones
  PARAM_SHIFT_SIZE,  // Pitch shifter window size
  PARAM_SHIFT_BLANK, // Reserved
  // Page 5 - LFO1
  PARAM_LFO1_ASSIGN, // LFO1 target parameter
  PARAM_LFO1_SPEED,  // LFO1 speed
  PARAM_LFO1_DEPTH,  // LFO1 modulation depth
  PARAM_LFO1_WAVE,   // LFO1 waveform
  // Page 6 - LFO2
  PARAM_LFO2_ASSIGN, // LFO2 target parameter
  PARAM_LFO2_SPEED,  // LFO2 speed
  PARAM_LFO2_DEPTH,  // LFO2 modulation depth
  PARAM_LFO2_WAVE,   // LFO2 waveform
  // ... rest are blank
};

// One-pole lowpass filter for parameter smoothing
// Prevents zipper noise when parameters change
class ParamSmoother {
 public:
  ParamSmoother() : value_(0.0f), target_(0.0f) {}
  
  void Init(float initial_value, float coefficient = 0.01f) {
    value_ = initial_value;
    target_ = initial_value;
    coeff_ = coefficient;
  }
  
  void SetTarget(float target) {
    target_ = target;
  }
  
  // Update and return smoothed value (call once per sample or per block)
  float Process() {
    value_ += coeff_ * (target_ - value_);
    return value_;
  }
  
  // Get current smoothed value without updating
  float value() const { return value_; }
  
  // Get target value
  float target() const { return target_; }
  
  // Check if smoothing is essentially complete
  bool IsSettled() const {
    float diff = target_ - value_;
    return (diff > -0.0001f && diff < 0.0001f);
  }
  
 private:
  float value_;
  float target_;
  float coeff_;  // Smoothing coefficient (0.01 = ~100 samples to settle)
};

class CloudsFx {
 public:
  int8_t Init(const unit_runtime_desc_t * desc);
  void Teardown();
  void Reset();
  void Resume();
  void Suspend();
  void Process(const float * in, float * out, uint32_t frames, uint8_t in_ch, uint8_t out_ch);

  void setParameter(uint8_t id, int32_t value);
  int32_t getParameterValue(uint8_t id) const;
  const char * getParameterStrValue(uint8_t id, int32_t value);
  const uint8_t * getParameterBmpValue(uint8_t id, int32_t value);

  void LoadPreset(uint8_t idx);
  uint8_t getPresetIndex() const;
  static const char * getPresetName(uint8_t idx);

 private:
  void applyDefaults();
  void initSmoothers();
  void updateSmoothedParams();  // Call per-block to update smoothed parameters
  void updateReverbParams();
  void updateGranularParams();
  void updatePitchShifterParams();
  void updateLfoParams();
  void applyLfoModulation();  // Apply LFO modulation to target parameters
  static int32_t clampToParam(uint8_t id, int32_t value);

  std::array<int32_t, UNIT_PARAM_MAX> params_{};
  uint8_t preset_index_ = 0;
  
  // Parameter smoothers to prevent zipper noise
  ParamSmoother smooth_dry_wet_;
  ParamSmoother smooth_time_;
  ParamSmoother smooth_diffusion_;
  ParamSmoother smooth_lp_;
  ParamSmoother smooth_input_gain_;
  ParamSmoother smooth_texture_;
  ParamSmoother smooth_grain_amt_;
  ParamSmoother smooth_grain_size_;
  ParamSmoother smooth_grain_density_;
  ParamSmoother smooth_grain_pitch_;
  ParamSmoother smooth_shift_amt_;
  ParamSmoother smooth_shift_pitch_;
  
  // Cached bypass flags for CPU optimization
  // These skip processing when effects are disabled
  bool diffuser_active_ = false;
  bool granular_active_ = false;
  bool pitch_shifter_active_ = false;
  
  clouds_revfx::Reverb reverb_;
  clouds_revfx::Diffuser48k diffuser_;
  clouds_revfx::MicroGranular granular_;
  clouds_revfx::PitchShifter pitch_shifter_;
  bool reverb_initialized_ = false;
  bool diffuser_initialized_ = false;
  bool granular_initialized_ = false;
  bool pitch_shifter_initialized_ = false;
  
  // LFO instances for parameter modulation
  clouds_revfx::Lfo lfo1_;
  clouds_revfx::Lfo lfo2_;
  bool lfo1_initialized_ = false;
  bool lfo2_initialized_ = false;
  
  // Base parameter values (before LFO modulation)
  // Used to restore original values when LFO target changes
  std::array<float, UNIT_PARAM_MAX> base_param_values_{};
  
  // Pre-allocated temp buffers for NEON output processing
  // Avoids per-call stack allocation of 2KB+ in Process()
  static constexpr size_t kMaxTempBlockSize = 64;
  float temp_l_[kMaxTempBlockSize];
  float temp_r_[kMaxTempBlockSize];
  float temp_mono_[kMaxTempBlockSize];
};

#endif  // CLOUDS_FX_H
