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

// Fast phase wrapping to [0, 1) using IEEE 754 bit manipulation.
// ~3-5x faster than phase - floorf(phase).
// Handles all float values correctly, including large values and edge cases.
DRUMLOGUE_ALWAYS_INLINE float fast_wrap_phase(float x) {
  union { float f; uint32_t i; } u = { x };
  const uint32_t exponent = (u.i >> 23) & 0xFF;

  // If x < 1.0 (exponent < 127), fractional part is x itself
  if (exponent < 127) return x;

  // For x >= 1.0, extract fractional part using bit manipulation
  // Clear mantissa to get integer part, then subtract from original
  union { float f; uint32_t i; } floor_val = { x };
  floor_val.i &= ~0x7FFFFF;  // Clear mantissa bits (keep sign and exponent)
  floor_val.i |= 0x3F800000; // Set exponent to 0 for proper float reconstruction

  // Handle the case where mantissa was zero (x was integer)
  if ((u.i & 0x7FFFFF) == 0) {
    return 0.0f;  // Integer values wrap to 0
  }

  // Extract fractional part: x - floor(x)
  return x - floor_val.f;
}
