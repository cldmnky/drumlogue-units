#pragma once

// Lightweight DSP helpers shared across units.

#include <math.h>
#include <stdint.h>

#ifndef DRUMLOGUE_ALWAYS_INLINE
#define DRUMLOGUE_ALWAYS_INLINE __attribute__((always_inline)) static inline
#endif

DRUMLOGUE_ALWAYS_INLINE float clampf(float x, float mn, float mx) {
  return (x < mn) ? mn : (x > mx) ? mx : x;
}

DRUMLOGUE_ALWAYS_INLINE float lerp(float a, float b, float t) {
  return a + (b - a) * t;
}

// Simple crossfade between a and b with t in [0,1].
DRUMLOGUE_ALWAYS_INLINE float crossfade(float a, float b, float t) {
  return (1.0f - t) * a + t * b;
}

// One-pole dezipper for parameter smoothing.
typedef struct dezipper {
  float z;
  float coef;
} dezipper_t;

DRUMLOGUE_ALWAYS_INLINE void dezipper_init(dezipper_t *d, float initial, float time_ms, float sample_rate) {
  d->z = initial;
  const float alpha = time_ms <= 0.f ? 1.f : expf(-1000.f / (time_ms * sample_rate));
  d->coef = 1.f - alpha;
}

DRUMLOGUE_ALWAYS_INLINE float dezipper_process(dezipper_t *d, float target) {
  d->z += d->coef * (target - d->z);
  return d->z;
}
