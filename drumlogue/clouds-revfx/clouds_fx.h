#ifndef CLOUDS_FX_H
#define CLOUDS_FX_H

#include "unit.h"

#include <array>
#include <cstdint>

#ifndef UNIT_PARAM_MAX
#define UNIT_PARAM_MAX 24
#endif

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
  static int32_t clampToParam(uint8_t id, int32_t value);

  std::array<int32_t, UNIT_PARAM_MAX> params_{};
  uint8_t preset_index_ = 0;
};

#endif  // CLOUDS_FX_H
