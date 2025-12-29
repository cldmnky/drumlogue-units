/*
 *  File: unit.cc
 *  @brief drumlogue SDK unit interface for Clouds-inspired reverb FX
 */

#include "unit.h"
#include "clouds_fx.h"

static CloudsFx s_clouds_fx;              // Clouds-style effect wrapper
static unit_runtime_desc_t s_runtime;     // Cached runtime descriptor

// ---- Callback entry points from drumlogue runtime ----------------------------------------------

__unit_callback int8_t unit_init(const unit_runtime_desc_t * desc) {
  if (!desc) {
    return k_unit_err_undef;
  }

  if (desc->target != unit_header.target) {
    return k_unit_err_target;
  }

  if (!UNIT_API_IS_COMPAT(desc->api)) {
    return k_unit_err_api_version;
  }

  s_runtime = *desc;  // cache runtime descriptor for future reference
  return s_clouds_fx.Init(desc);
}

__unit_callback void unit_teardown() {
  s_clouds_fx.Teardown();
}

__unit_callback void unit_reset() {
  s_clouds_fx.Reset();
}

__unit_callback void unit_resume() {
  s_clouds_fx.Resume();
}

__unit_callback void unit_suspend() {
  s_clouds_fx.Suspend();
}

__unit_callback void unit_render(const float * in, float * out, uint32_t frames) {
  s_clouds_fx.Process(in, out, frames, s_runtime.input_channels, s_runtime.output_channels);
}

__unit_callback void unit_set_param_value(uint8_t id, int32_t value) {
  s_clouds_fx.setParameter(id, value);
}

__unit_callback int32_t unit_get_param_value(uint8_t id) {
  return s_clouds_fx.getParameterValue(id);
}

__unit_callback const char * unit_get_param_str_value(uint8_t id, int32_t value) {
  return s_clouds_fx.getParameterStrValue(id, value);
}

__unit_callback const uint8_t * unit_get_param_bmp_value(uint8_t id, int32_t value) {
  return s_clouds_fx.getParameterBmpValue(id, value);
}

__unit_callback void unit_set_tempo(uint32_t tempo) {
  (void)tempo;  // tempo not used yet
}

// Preset callbacks removed - presets not supported on drumlogue for reverb/delay effects
// This is a known hardware limitation/bug in the drumlogue firmware
