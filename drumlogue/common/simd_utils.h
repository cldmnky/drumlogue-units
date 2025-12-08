#pragma once

// NEON SIMD helpers for drumlogue (Cortex-A7 with NEON).
// Focused on common audio buffer operations: load/store, MAC, clamp, (de)interleave.
// NOTE: This header requires NEON support; it will not compile without USE_NEON.

#ifndef USE_NEON
#error "simd_utils.h requires USE_NEON to be defined"
#endif

#include <arm_neon.h>
#include <stdint.h>

#ifndef DRUMLOGUE_ALWAYS_INLINE
#define DRUMLOGUE_ALWAYS_INLINE __attribute__((always_inline)) static inline
#endif

DRUMLOGUE_ALWAYS_INLINE float32x4_t simd_load4(const float *src) {
  return vld1q_f32(src);
}

DRUMLOGUE_ALWAYS_INLINE void simd_store4(float *dst, float32x4_t v) {
  vst1q_f32(dst, v);
}

// acc + a * b
DRUMLOGUE_ALWAYS_INLINE float32x4_t simd_muladd4(float32x4_t a, float32x4_t b, float32x4_t acc) {
  return vmlaq_f32(acc, a, b);
}

// a * b
DRUMLOGUE_ALWAYS_INLINE float32x4_t simd_mul4(float32x4_t a, float32x4_t b) {
  return vmulq_f32(a, b);
}

// a + b
DRUMLOGUE_ALWAYS_INLINE float32x4_t simd_add4(float32x4_t a, float32x4_t b) {
  return vaddq_f32(a, b);
}

// acc += a * b (in-place MAC for buffer processing)
DRUMLOGUE_ALWAYS_INLINE void simd_mac4_inplace(float32x4_t a, float32x4_t b, float32x4_t *acc) {
  *acc = vmlaq_f32(*acc, a, b);
}

// acc + a * s (scalar broadcast)
DRUMLOGUE_ALWAYS_INLINE float32x4_t simd_muladd4_scalar(float32x4_t a, float s, float32x4_t acc) {
  return vmlaq_f32(acc, a, vdupq_n_f32(s));
}

// In-place gain: dst[0:4] *= gain
DRUMLOGUE_ALWAYS_INLINE void simd_gain4(float *dst, float gain) {
  float32x4_t v = vld1q_f32(dst);
  v = vmulq_f32(v, vdupq_n_f32(gain));
  vst1q_f32(dst, v);
}

DRUMLOGUE_ALWAYS_INLINE float32x4_t simd_clamp4(float32x4_t v, float32x4_t lo, float32x4_t hi) {
  return vminq_f32(hi, vmaxq_f32(lo, v));
}

// Soft clip using a light tanh-like polynomial: y = x * (1 - x^2 * c) with small c.
// Works best for |x| <= ~3; higher inputs still clamp.
DRUMLOGUE_ALWAYS_INLINE float32x4_t simd_softclip4(float32x4_t x) {
  const float32x4_t c = vdupq_n_f32(0.1f);
  float32x4_t x2 = vmulq_f32(x, x);
  float32x4_t t = vmlsq_f32(vdupq_n_f32(1.0f), x2, c); // 1 - c*x^2
  float32x4_t y = vmulq_f32(x, t);
  const float32x4_t limit = vdupq_n_f32(3.0f);
  return simd_clamp4(y, vnegq_f32(limit), limit);
}

// Sum of squares for 4-lane vector, useful for RMS/energy.
DRUMLOGUE_ALWAYS_INLINE float simd_sum_squares4(float32x4_t v) {
  float32x4_t sq = vmulq_f32(v, v);
  float32x2_t sum2 = vadd_f32(vget_low_f32(sq), vget_high_f32(sq));
  sum2 = vpadd_f32(sum2, sum2);
  return vget_lane_f32(sum2, 0);
}

// Deinterleave stereo float buffer (LRLR...) to separate L and R.
DRUMLOGUE_ALWAYS_INLINE void simd_deinterleave_stereo(const float *src, float *dstL, float *dstR, uint32_t frames) {
  uint32_t i = 0;
  for (; i + 4 <= frames; i += 4) {
    float32x4x2_t lr = vld2q_f32(src + (i << 1));
    vst1q_f32(dstL + i, lr.val[0]);
    vst1q_f32(dstR + i, lr.val[1]);
  }
  for (; i < frames; ++i) {
    dstL[i] = src[(i << 1)];
    dstR[i] = src[(i << 1) + 1];
  }
}

// Interleave stereo L/R into LRLR...
DRUMLOGUE_ALWAYS_INLINE void simd_interleave_stereo(const float *srcL, const float *srcR, float *dst, uint32_t frames) {
  uint32_t i = 0;
  for (; i + 4 <= frames; i += 4) {
    float32x4x2_t lr = {vld1q_f32(srcL + i), vld1q_f32(srcR + i)};
    vst2q_f32(dst + (i << 1), lr);
  }
  for (; i < frames; ++i) {
    dst[(i << 1)]     = srcL[i];
    dst[(i << 1) + 1] = srcR[i];
  }
}
