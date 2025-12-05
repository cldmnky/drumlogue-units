#include "clouds_fx.h"

#include <algorithm>
#include <cstdio>
#include <cstring>

// Simple plumbing layer: currently passthrough, ready for Clouds DSP integration.

namespace {
constexpr uint8_t kActiveParams = UNIT_PARAM_MAX;
}  // namespace

int8_t CloudsFx::Init(const unit_runtime_desc_t * desc) {
  if (!desc) {
    return k_unit_err_undef;
  }
  applyDefaults();
  preset_index_ = 0;
  (void)desc;
  return k_unit_err_none;
}

void CloudsFx::Teardown() {}

void CloudsFx::Reset() {
  applyDefaults();
}

void CloudsFx::Resume() {}

void CloudsFx::Suspend() {}

void CloudsFx::Process(const float * in, float * out, uint32_t frames, uint8_t in_ch, uint8_t out_ch) {
  if (!out || !frames || !out_ch) {
    return;
  }

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
}

void CloudsFx::setParameter(uint8_t id, int32_t value) {
  if (id >= kActiveParams) {
    return;
  }
  params_[id] = clampToParam(id, value);
}

int32_t CloudsFx::getParameterValue(uint8_t id) const {
  if (id >= kActiveParams) {
    return 0;
  }
  return params_[id];
}

const char * CloudsFx::getParameterStrValue(uint8_t id, int32_t value) {
  static char buf[32];
  (void)id;
  std::snprintf(buf, sizeof(buf), "%ld", static_cast<long>(value));
  return buf;
}

const uint8_t * CloudsFx::getParameterBmpValue(uint8_t id, int32_t value) {
  (void)id;
  (void)value;
  return nullptr;
}

void CloudsFx::LoadPreset(uint8_t idx) {
  if (idx >= unit_header.num_presets) {
    idx = 0;
  }
  preset_index_ = idx;
  applyDefaults();
}

uint8_t CloudsFx::getPresetIndex() const {
  return preset_index_;
}

const char * CloudsFx::getPresetName(uint8_t idx) {
  static const char * kPresets[] = {"INIT"};
  if (idx >= (sizeof(kPresets) / sizeof(kPresets[0]))) {
    idx = 0;
  }
  return kPresets[idx];
}

void CloudsFx::applyDefaults() {
  for (uint8_t i = 0; i < UNIT_PARAM_MAX; ++i) {
    params_[i] = unit_header.params[i].init;
  }
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
