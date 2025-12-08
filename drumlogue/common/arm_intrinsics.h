#pragma once

// Low-level ARM DSP intrinsics for Cortex-A7 (NEON capable) and Cortex-M4/M7.
// These wrap key instructions missing from standard CMSIS headers.

#include <stdint.h>

#ifndef DRUMLOGUE_ALWAYS_INLINE
#define DRUMLOGUE_ALWAYS_INLINE __attribute__((always_inline)) static inline
#endif

// Signed multiply returning the top 32 bits (Q31 * Q31 -> Q31).
DRUMLOGUE_ALWAYS_INLINE int32_t smmul(int32_t op1, int32_t op2) {
  int32_t result;
  __asm__ volatile ("smmul %0, %1, %2" : "=r" (result) : "r" (op1), "r" (op2));
  return result;
}

// Signed multiply-accumulate: op1 + (op2 * op3.low16 >> 16).
DRUMLOGUE_ALWAYS_INLINE int32_t smlawb(int32_t op1, int32_t op2, int32_t op3) {
  int32_t result;
  __asm__ volatile ("smlawb %0, %1, %2, %3" : "=r" (result) : "r" (op1), "r" (op2), "r" (op3));
  return result;
}

// Signed multiply-accumulate: op1 + (op2 * op3.high16 >> 16).
DRUMLOGUE_ALWAYS_INLINE int32_t smlawt(int32_t op1, int32_t op2, int32_t op3) {
  int32_t result;
  __asm__ volatile ("smlawt %0, %1, %2, %3" : "=r" (result) : "r" (op1), "r" (op2), "r" (op3));
  return result;
}

// Signed multiply word by low halfword: (op1 * op2.low16) >> 16.
DRUMLOGUE_ALWAYS_INLINE int32_t smulwb(int32_t op1, int32_t op2) {
  int32_t result;
  __asm__ volatile ("smulwb %0, %1, %2" : "=r" (result) : "r" (op1), "r" (op2));
  return result;
}

// Signed multiply word by high halfword: (op1 * op2.high16) >> 16.
DRUMLOGUE_ALWAYS_INLINE int32_t smulwt(int32_t op1, int32_t op2) {
  int32_t result;
  __asm__ volatile ("smulwt %0, %1, %2" : "=r" (result) : "r" (op1), "r" (op2));
  return result;
}

// Signed bitfield extract (sign-extends the extracted field).
DRUMLOGUE_ALWAYS_INLINE int32_t sbfx(int32_t op1, int32_t lsb, int32_t width) {
  int32_t result;
  __asm__ volatile ("sbfx %0, %1, %2, %3" : "=r" (result) : "r" (op1), "i" (lsb), "i" (width));
  return result;
}

// Unsigned bitfield extract.
DRUMLOGUE_ALWAYS_INLINE int32_t ubfx(int32_t op1, int32_t lsb, int32_t width) {
  int32_t result;
  __asm__ volatile ("ubfx %0, %1, %2, %3" : "=r" (result) : "r" (op1), "i" (lsb), "i" (width));
  return result;
}

// Unsigned saturate with arithmetic right shift.
DRUMLOGUE_ALWAYS_INLINE int32_t usat_asr(int32_t sat_bits, int32_t op1, int32_t shift) {
  int32_t result;
  __asm__ volatile ("usat %0, %1, %2, asr %3" : "=r" (result) : "i" (sat_bits), "r" (op1), "i" (shift));
  return result;
}

// Unsigned saturate with logical left shift.
DRUMLOGUE_ALWAYS_INLINE int32_t usat_lsl(int32_t sat_bits, int32_t op1, int32_t shift) {
  int32_t result;
  __asm__ volatile ("usat %0, %1, %2, lsl %3" : "=r" (result) : "i" (sat_bits), "r" (op1), "i" (shift));
  return result;
}

// Table branch helpers (useful for dense switch jump tables).
#define tbb(base, index) __asm__ volatile ("tbb [%0, %1]" : : "r" (base), "r" (index))
#define tbh(base, index) __asm__ volatile ("tbh [%0, %1, lsl #1]" : : "r" (base), "r" (index))
