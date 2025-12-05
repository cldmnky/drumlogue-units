#include "clouds_fx.h"

#include <algorithm>
#include <cstdio>
#include <cstring>

// Preset names - must be at file scope for stable memory addresses
constexpr size_t kNumPresets = 8;
static const char * const kPresetNames[kNumPresets] = {
    "INIT",      // 0: Clean starting point
    "HALL",      // 1: Large concert hall
    "PLATE",     // 2: Bright plate reverb
    "SHIMMER",   // 3: Pitched reverb with shimmer
    "CLOUD",     // 4: Granular texture cloud
    "FREEZE",    // 5: For frozen/pad sounds
    "OCTAVER",   // 6: Pitch shifted reverb
    "AMBIENT",   // 7: Lush ambient wash
};

// Preset data: {DRY/WET, TIME, DIFFUSION, LP_DAMP, IN_GAIN, TEXTURE, 
//               GRAIN_AMT, GRAIN_SIZE, GRAIN_DENS, GRAIN_PITCH, GRAIN_POS, FREEZE,
//               SHIFT_AMT, SHIFT_PITCH, SHIFT_SIZE, reserved}
static const int32_t kPresets[kNumPresets][16] = {
    // 0: INIT - Clean starting point
    {100, 80, 80, 90, 50, 0, 0, 64, 64, 64, 64, 0, 0, 64, 64, 0},
    
    // 1: HALL - Large concert hall, long decay
    {120, 110, 100, 100, 40, 30, 0, 64, 64, 64, 64, 0, 0, 64, 64, 0},
    
    // 2: PLATE - Bright plate reverb
    {100, 70, 127, 127, 60, 0, 0, 64, 64, 64, 64, 0, 0, 64, 64, 0},
    
    // 3: SHIMMER - Pitched reverb with octave up
    {90, 100, 90, 80, 45, 40, 0, 64, 64, 64, 64, 0, 80, 88, 80, 0},
    
    // 4: CLOUD - Granular texture + reverb
    {80, 90, 90, 85, 50, 60, 80, 90, 70, 64, 64, 0, 0, 64, 64, 0},
    
    // 5: FREEZE - For frozen/pad sounds
    {100, 127, 100, 95, 30, 80, 60, 100, 50, 64, 64, 0, 0, 64, 64, 0},
    
    // 6: OCTAVER - Pitch shifted reverb
    {90, 85, 80, 90, 50, 20, 0, 64, 64, 64, 64, 0, 100, 52, 70, 0},
    
    // 7: AMBIENT - Lush ambient wash
    {140, 120, 110, 75, 35, 50, 40, 80, 40, 64, 64, 0, 30, 76, 90, 0},
};

// Static buffer for reverb delay lines (~64KB for 32768 * 2 bytes)
static uint16_t s_reverb_buffer[32768];

// Static buffer for diffuser delay lines (~8KB for 4096 * 2 bytes)
// Optimized from 16KB float to 8KB uint16_t
static uint16_t s_diffuser_buffer[4096];

// Static buffers for micro-granular processor (~16KB for 2048 * 2 * 4 bytes)
static float s_granular_buffer_l[clouds_revfx::kMicroGranularBufferSize];
static float s_granular_buffer_r[clouds_revfx::kMicroGranularBufferSize];

// Static buffer for pitch shifter (~16KB for 8192 * 2 bytes)
static uint16_t s_pitch_shifter_buffer[8192];

// Processing buffer for FloatFrame conversion
static constexpr size_t kMaxBlockSize = 64;
static clouds::FloatFrame s_process_buffer[kMaxBlockSize];

namespace {
constexpr uint8_t kActiveParams = 16;  // Number of active parameters
}  // namespace

int8_t CloudsFx::Init(const unit_runtime_desc_t * desc) {
  if (!desc) {
    return k_unit_err_undef;
  }
  
  // Verify sample rate is 48kHz as expected
  if (desc->samplerate != 48000) {
    return k_unit_err_samplerate;
  }
  
  applyDefaults();
  preset_index_ = 0;
  
  // Initialize reverb with static buffer
  reverb_.Init(s_reverb_buffer);
  reverb_initialized_ = true;
  
  // Initialize diffuser with static buffer
  diffuser_.Init(s_diffuser_buffer);
  diffuser_initialized_ = true;
  
  // Initialize micro-granular processor
  granular_.Init(s_granular_buffer_l, s_granular_buffer_r);
  granular_initialized_ = true;
  
  // Initialize pitch shifter
  pitch_shifter_.Init(s_pitch_shifter_buffer);
  pitch_shifter_initialized_ = true;
  
  // Initialize parameter smoothers
  initSmoothers();
  
  updateReverbParams();
  updateGranularParams();
  updatePitchShifterParams();
  
  return k_unit_err_none;
}

void CloudsFx::Teardown() {
  reverb_initialized_ = false;
  diffuser_initialized_ = false;
  granular_initialized_ = false;
  pitch_shifter_initialized_ = false;
}

void CloudsFx::Reset() {
  applyDefaults();
  if (reverb_initialized_) {
    reverb_.Clear();
  }
  if (diffuser_initialized_) {
    diffuser_.Clear();
  }
  if (granular_initialized_) {
    granular_.Clear();
  }
  if (pitch_shifter_initialized_) {
    pitch_shifter_.Clear();
  }
  updateReverbParams();
  updateGranularParams();
  updatePitchShifterParams();
}

void CloudsFx::Resume() {
  // Nothing special needed
}

void CloudsFx::Suspend() {
  // Clear reverb and diffuser tails when suspended
  if (reverb_initialized_) {
    reverb_.Clear();
  }
  if (diffuser_initialized_) {
    diffuser_.Clear();
  }
  if (pitch_shifter_initialized_) {
    pitch_shifter_.Clear();
  }
  // Don't clear granular buffer to preserve frozen content
}

void CloudsFx::Process(const float * in, float * out, uint32_t frames, uint8_t in_ch, uint8_t out_ch) {
  if (!out || !frames || !out_ch) {
    return;
  }
  
  if (!reverb_initialized_) {
    // Fallback to passthrough if reverb not initialized
    for (uint32_t i = 0; i < frames; ++i) {
      for (uint8_t ch = 0; ch < out_ch; ++ch) {
        const uint32_t out_idx = i * out_ch + ch;
        float sample = 0.0f;
        if (in && ch < in_ch) {
          sample = in[i * in_ch + ch];
        }
        out[out_idx] = sample;
      }
    }
    return;
  }
  
  // Update smoothed parameters once per block (efficient per-block smoothing)
  updateSmoothedParams();
  
  // Process in blocks
  uint32_t processed = 0;
  while (processed < frames) {
    uint32_t block_size = std::min(frames - processed, static_cast<uint32_t>(kMaxBlockSize));
    
    // Convert interleaved float input to FloatFrame array
    for (uint32_t i = 0; i < block_size; ++i) {
      uint32_t src_idx = (processed + i);
      if (in && in_ch >= 2) {
        s_process_buffer[i].l = in[src_idx * in_ch + 0];
        s_process_buffer[i].r = in[src_idx * in_ch + 1];
      } else if (in && in_ch == 1) {
        s_process_buffer[i].l = in[src_idx];
        s_process_buffer[i].r = in[src_idx];
      } else {
        s_process_buffer[i].l = 0.0f;
        s_process_buffer[i].r = 0.0f;
      }
    }
    
    // Process reverb (in-place) - always active as core effect
    reverb_.Process(s_process_buffer, block_size);
    
    // Process diffuser for texture (in-place, after reverb)
    // Skip if texture is 0 (CPU optimization)
    if (diffuser_initialized_ && diffuser_active_) {
      diffuser_.Process(s_process_buffer, block_size);
    }
    
    // Process micro-granular (in-place, after diffuser)
    // Skip if grain amount is 0 (CPU optimization)
    if (granular_initialized_ && granular_active_) {
      granular_.Process(s_process_buffer, block_size);
    }
    
    // Process pitch shifter (in-place, after granular)
    // Skip if shift amount is 0 (CPU optimization)
    if (pitch_shifter_initialized_ && pitch_shifter_active_) {
      pitch_shifter_.Process(s_process_buffer, block_size);
    }
    
    // Convert FloatFrame array back to interleaved float output
    for (uint32_t i = 0; i < block_size; ++i) {
      uint32_t dst_idx = (processed + i);
      if (out_ch >= 2) {
        out[dst_idx * out_ch + 0] = s_process_buffer[i].l;
        out[dst_idx * out_ch + 1] = s_process_buffer[i].r;
      } else {
        out[dst_idx] = (s_process_buffer[i].l + s_process_buffer[i].r) * 0.5f;
      }
    }
    
    processed += block_size;
  }
}

void CloudsFx::setParameter(uint8_t id, int32_t value) {
  if (id >= UNIT_PARAM_MAX) {
    return;
  }
  params_[id] = clampToParam(id, value);
  
  // Update smoother targets based on parameter type
  switch (id) {
    case PARAM_DRY_WET:
      smooth_dry_wet_.SetTarget(static_cast<float>(params_[id]) / 200.0f);
      break;
    case PARAM_TIME:
      smooth_time_.SetTarget(std::min(static_cast<float>(params_[id]) / 128.0f, 0.99f));
      break;
    case PARAM_DIFFUSION:
      smooth_diffusion_.SetTarget(static_cast<float>(params_[id]) / 127.0f * 0.75f);
      break;
    case PARAM_LP:
      smooth_lp_.SetTarget(0.3f + static_cast<float>(params_[id]) / 127.0f * 0.65f);
      break;
    case PARAM_INPUT_GAIN:
      smooth_input_gain_.SetTarget(static_cast<float>(params_[id]) / 127.0f * 0.5f);
      break;
    case PARAM_TEXTURE:
      smooth_texture_.SetTarget(static_cast<float>(params_[id]) / 127.0f);
      break;
    case PARAM_GRAIN_AMT:
      smooth_grain_amt_.SetTarget(static_cast<float>(params_[id]) / 127.0f);
      break;
    case PARAM_GRAIN_SIZE:
      smooth_grain_size_.SetTarget(static_cast<float>(params_[id]) / 127.0f);
      break;
    case PARAM_GRAIN_DENS:
      smooth_grain_density_.SetTarget(static_cast<float>(params_[id]) / 127.0f);
      break;
    case PARAM_GRAIN_PITCH:
      smooth_grain_pitch_.SetTarget(static_cast<float>(params_[id] - 64) * (24.0f / 64.0f));
      break;
    case PARAM_SHIFT_AMT:
      smooth_shift_amt_.SetTarget(static_cast<float>(params_[id]) / 127.0f);
      break;
    case PARAM_SHIFT_PITCH:
      smooth_shift_pitch_.SetTarget(static_cast<float>(params_[id] - 64) * (24.0f / 64.0f));
      break;
    case PARAM_FREEZE:
    case PARAM_GRAIN_POS:
    case PARAM_SHIFT_SIZE:
      // These don't need smoothing - update directly
      if (granular_initialized_) {
        if (id == PARAM_FREEZE) {
          granular_.set_freeze(params_[id] != 0);
        } else if (id == PARAM_GRAIN_POS) {
          granular_.set_position(static_cast<float>(params_[id]) / 127.0f);
        }
      }
      if (pitch_shifter_initialized_ && id == PARAM_SHIFT_SIZE) {
        pitch_shifter_.set_size(static_cast<float>(params_[id]) / 127.0f);
      }
      break;
    default:
      break;
  }
}

int32_t CloudsFx::getParameterValue(uint8_t id) const {
  if (id >= UNIT_PARAM_MAX) {
    return 0;
  }
  return params_[id];
}

const char * CloudsFx::getParameterStrValue(uint8_t id, int32_t value) {
  // No string-type parameters currently
  (void)id;
  (void)value;
  return nullptr;
}

const uint8_t * CloudsFx::getParameterBmpValue(uint8_t id, int32_t value) {
  (void)id;
  (void)value;
  return nullptr;
}

void CloudsFx::LoadPreset(uint8_t idx) {
  if (idx >= kNumPresets) {
    idx = 0;
  }
  preset_index_ = idx;
  
  // Load preset values using setParameter (like Lilian does)
  // This ensures proper parameter handling and smoother updates
  for (uint8_t i = 0; i < 16; ++i) {
    setParameter(i, kPresets[idx][i]);
  }
  
  // Update DSP modules
  if (reverb_initialized_) {
    updateReverbParams();
  }
  if (granular_initialized_) {
    updateGranularParams();
  }
  if (pitch_shifter_initialized_) {
    updatePitchShifterParams();
  }
}

uint8_t CloudsFx::getPresetIndex() const {
  return preset_index_;
}

const char * CloudsFx::getPresetName(uint8_t idx) {
  if (idx >= kNumPresets) {
    idx = 0;
  }
  return kPresetNames[idx];
}

void CloudsFx::applyDefaults() {
  for (uint8_t i = 0; i < UNIT_PARAM_MAX; ++i) {
    params_[i] = unit_header.params[i].init;
  }
}

void CloudsFx::initSmoothers() {
  // Initialize smoothers with default values and coefficient
  // Coefficient of 0.05 = ~20 samples to settle (~0.4ms @ 48kHz)
  // Lower values = slower/smoother transitions
  constexpr float kSmoothCoeff = 0.05f;
  
  // Reverb smoothers
  smooth_dry_wet_.Init(static_cast<float>(params_[PARAM_DRY_WET]) / 200.0f, kSmoothCoeff);
  smooth_time_.Init(std::min(static_cast<float>(params_[PARAM_TIME]) / 128.0f, 0.99f), kSmoothCoeff);
  smooth_diffusion_.Init(static_cast<float>(params_[PARAM_DIFFUSION]) / 127.0f * 0.75f, kSmoothCoeff);
  smooth_lp_.Init(0.3f + static_cast<float>(params_[PARAM_LP]) / 127.0f * 0.65f, kSmoothCoeff);
  smooth_input_gain_.Init(static_cast<float>(params_[PARAM_INPUT_GAIN]) / 127.0f * 0.5f, kSmoothCoeff);
  smooth_texture_.Init(static_cast<float>(params_[PARAM_TEXTURE]) / 127.0f, kSmoothCoeff);
  
  // Granular smoothers
  smooth_grain_amt_.Init(static_cast<float>(params_[PARAM_GRAIN_AMT]) / 127.0f, kSmoothCoeff);
  smooth_grain_size_.Init(static_cast<float>(params_[PARAM_GRAIN_SIZE]) / 127.0f, kSmoothCoeff);
  smooth_grain_density_.Init(static_cast<float>(params_[PARAM_GRAIN_DENS]) / 127.0f, kSmoothCoeff);
  smooth_grain_pitch_.Init(static_cast<float>(params_[PARAM_GRAIN_PITCH] - 64) * (24.0f / 64.0f), kSmoothCoeff);
  
  // Pitch shifter smoothers
  smooth_shift_amt_.Init(static_cast<float>(params_[PARAM_SHIFT_AMT]) / 127.0f, kSmoothCoeff);
  smooth_shift_pitch_.Init(static_cast<float>(params_[PARAM_SHIFT_PITCH] - 64) * (24.0f / 64.0f), kSmoothCoeff);
}

void CloudsFx::updateSmoothedParams() {
  // Process all smoothers and update DSP modules
  // This is called once per audio block for efficient smoothing
  
  // Update reverb with smoothed values
  if (reverb_initialized_) {
    reverb_.set_amount(smooth_dry_wet_.Process());
    reverb_.set_time(smooth_time_.Process());
    reverb_.set_diffusion(smooth_diffusion_.Process());
    reverb_.set_lp(smooth_lp_.Process());
    reverb_.set_input_gain(smooth_input_gain_.Process());
  }
  
  // Update diffuser with smoothed texture - track active state for bypass
  if (diffuser_initialized_) {
    float texture = smooth_texture_.Process();
    diffuser_.set_amount(texture);
    diffuser_active_ = (texture > 0.001f);
  }
  
  // Update granular with smoothed values - track active state for bypass
  if (granular_initialized_) {
    float grain_amt = smooth_grain_amt_.Process();
    granular_.set_amount(grain_amt);
    granular_.set_size(smooth_grain_size_.Process());
    granular_.set_density(smooth_grain_density_.Process());
    granular_.set_pitch(smooth_grain_pitch_.Process());
    granular_active_ = (grain_amt > 0.001f);
  }
  
  // Update pitch shifter with smoothed values - track active state for bypass
  if (pitch_shifter_initialized_) {
    float shift_amt = smooth_shift_amt_.Process();
    pitch_shifter_.set_amount(shift_amt);
    pitch_shifter_.set_pitch(smooth_shift_pitch_.Process());
    pitch_shifter_active_ = (shift_amt > 0.001f);
  }
}

void CloudsFx::updateReverbParams() {
  // Direct update for initialization - smoothing happens in updateSmoothedParams()
  float dry_wet = static_cast<float>(params_[PARAM_DRY_WET]) / 200.0f;
  reverb_.set_amount(dry_wet);
  
  float time = static_cast<float>(params_[PARAM_TIME]) / 128.0f;
  time = std::min(time, 0.99f);
  reverb_.set_time(time);
  
  float diffusion = static_cast<float>(params_[PARAM_DIFFUSION]) / 127.0f;
  reverb_.set_diffusion(diffusion * 0.75f);
  
  float lp = static_cast<float>(params_[PARAM_LP]) / 127.0f;
  reverb_.set_lp(0.3f + lp * 0.65f);
  
  float input_gain = static_cast<float>(params_[PARAM_INPUT_GAIN]) / 127.0f;
  reverb_.set_input_gain(input_gain * 0.5f);
  
  if (diffuser_initialized_) {
    float texture = static_cast<float>(params_[PARAM_TEXTURE]) / 127.0f;
    diffuser_.set_amount(texture);
  }
}

void CloudsFx::updateGranularParams() {
  // Direct update for initialization - smoothing happens in updateSmoothedParams()
  float grain_amt = static_cast<float>(params_[PARAM_GRAIN_AMT]) / 127.0f;
  granular_.set_amount(grain_amt);
  
  float grain_size = static_cast<float>(params_[PARAM_GRAIN_SIZE]) / 127.0f;
  granular_.set_size(grain_size);
  
  float grain_density = static_cast<float>(params_[PARAM_GRAIN_DENS]) / 127.0f;
  granular_.set_density(grain_density);
  
  float grain_pitch = static_cast<float>(params_[PARAM_GRAIN_PITCH] - 64) * (24.0f / 64.0f);
  granular_.set_pitch(grain_pitch);
  
  float grain_pos = static_cast<float>(params_[PARAM_GRAIN_POS]) / 127.0f;
  granular_.set_position(grain_pos);
  
  granular_.set_freeze(params_[PARAM_FREEZE] != 0);
}

void CloudsFx::updatePitchShifterParams() {
  // Direct update for initialization - smoothing happens in updateSmoothedParams()
  float shift_amt = static_cast<float>(params_[PARAM_SHIFT_AMT]) / 127.0f;
  pitch_shifter_.set_amount(shift_amt);
  
  float shift_pitch = static_cast<float>(params_[PARAM_SHIFT_PITCH] - 64) * (24.0f / 64.0f);
  pitch_shifter_.set_pitch(shift_pitch);
  
  float shift_size = static_cast<float>(params_[PARAM_SHIFT_SIZE]) / 127.0f;
  pitch_shifter_.set_size(shift_size);
}

int32_t CloudsFx::clampToParam(uint8_t id, int32_t value) {
  if (id >= UNIT_PARAM_MAX) {
    return value;
  }
  const unit_param_t & p = unit_header.params[id];
  if (value < p.min) {
    return p.min;
  }
  if (value > p.max) {
    return p.max;
  }
  return value;
}
