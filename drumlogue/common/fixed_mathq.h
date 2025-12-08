#pragma once

// Fixed-point helpers (Q formats) geared for drumlogue Cortex-A7 builds.
// Minimal dependencies: uses arm_intrinsics for fast multiply and bitfield ops.

#include <stdint.h>
#include "arm_intrinsics.h"

#ifndef q31_t
typedef int32_t q31_t;
#endif
#ifndef q15_t
typedef int16_t q15_t;
#endif
#ifndef q7_t
typedef int8_t q7_t;
#endif

#define q7_to_q31(q)  ((q31_t)(q) << 24)
#define q31_to_q7(q)  ((q7_t)((q31_t)(q) >> 24))

#define q11_to_q31(q) ((q31_t)(q) << 20)
#define q31_to_q11(q) ((q15_t)((q31_t)(q) >> 20))

#define q15_to_q31(q) ((q31_t)(q) << 16)
#define q31_to_q15(q) ((q15_t)((q31_t)(q) >> 16))

#define q7_to_f32_c   0.0078125f
#define q11_to_f32_c  0.00048828125f

#define q7_to_f32(q)  ((float)(q) * q7_to_f32_c)
#define q11_to_f32(q) ((float)(q) * q11_to_f32_c)
#define f32_to_q7(f)  ((q7_t)usat_asr(8, (int32_t)((f) * ((1 << 7) - 1)), 0))

// Q31 linear interpolation: x0 + frac * (x1 - x0). frac in [0, 0x7FFFFFFF].
DRUMLOGUE_ALWAYS_INLINE q31_t linintq31(q31_t frac, q31_t x0, q31_t x1) {
  return x0 + smmul(frac, (x1 - x0) << 1);
}

// Q15 linear interpolation with Q15 fraction.
DRUMLOGUE_ALWAYS_INLINE q15_t linintq15(q15_t frac, q15_t x0, q15_t x1) {
  return x0 + (q15_t)(((int32_t)frac * (x1 - x0)) >> 15);
}

// Clamp helpers.
DRUMLOGUE_ALWAYS_INLINE q31_t clipmaxq31(q31_t x, q31_t m) { return (x > m) ? m : x; }
DRUMLOGUE_ALWAYS_INLINE q31_t clipminq31(q31_t m, q31_t x) { return (x < m) ? m : x; }
DRUMLOGUE_ALWAYS_INLINE q31_t clipminmaxq31(q31_t mn, q31_t x, q31_t mx) {
  return (x > mx) ? mx : (x < mn) ? mn : x;
}
