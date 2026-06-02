#pragma once

#include <stdint.h>
#include <math.h>
#include <stdlib.h>

inline float SafeDivide(float num, float denom) {
  if (fabsf(denom) < 1e-9f) return 0.0f;
  return num / denom;
}

inline float Clamp(float value, float min, float max) {
  if (value < min) return min;
  if (value > max) return max;
  return value;
}

inline float FlushDenormal(float x) {
  if (fabsf(x) < 1e-15f) return 0.0f;
  return x;
}

inline float clamp01(float value) {
  if (value < 0.0f) return 0.0f;
  if (value > 1.0f) return 1.0f;
  return value;
}

typedef struct { uint32_t knob; float seconds; } env_point_t;

float env_lookup_seconds(uint32_t value, const env_point_t *table, int n);

float env_time_to_samples_attack(uint32_t v);
float env_time_to_samples_hold(uint32_t v);
float env_time_to_samples_decay(uint32_t v);
float env_time_to_samples_release(uint32_t v);
float lfo_delay_to_samples(uint32_t v);

float env_level_at_sample(uint64_t sample,
    float atk_samples, float hold_samples, float dec_samples, float sus_level,
    uint64_t note_on);

float aux_env_level_at_sample(uint64_t sample,
    float delay_samples, float atk_samples, float hold_samples,
    float dec_samples, float sus_level, float rel_samples,
    uint64_t note_on, uint64_t note_off, float release_start_level);
