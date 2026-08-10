#pragma once

// Low-level ARM DSP intrinsics for Cortex-A7 (NEON capable) and Cortex-M4/M7.
// These wrap key instructions missing from standard CMSIS headers.

#include <stdint.h>

#ifndef DRUMLOGUE_ALWAYS_INLINE
#define DRUMLOGUE_ALWAYS_INLINE __attribute__((always_inline)) static inline
#endif

// Signed multiply returning the top 32 bits (Q31 * Q31 -> Q31).
DRUMLOGUE_ALWAYS_INLINE int32_t smmul(int32_t op1, int32_t op2) {
#if defined(UNIT_HOST_NATIVE) || defined(__aarch64__)
  return static_cast<int32_t>((static_cast<int64_t>(op1) * op2) >> 32);
#else
  int32_t result;
  __asm__ volatile ("smmul %0, %1, %2" : "=r" (result) : "r" (op1), "r" (op2));
  return result;
#endif
}

// Signed multiply-accumulate: op1 + (op2 * op3.low16 >> 16).
DRUMLOGUE_ALWAYS_INLINE int32_t smlawb(int32_t op1, int32_t op2, int32_t op3) {
#if defined(UNIT_HOST_NATIVE) || defined(__aarch64__)
  const int32_t low = static_cast<int16_t>(static_cast<uint32_t>(op3) & 0xffffu);
  return op1 + static_cast<int32_t>((static_cast<int64_t>(op2) * low) >> 16);
#else
  int32_t result;
  __asm__ volatile ("smlawb %0, %1, %2, %3" : "=r" (result) : "r" (op1), "r" (op2), "r" (op3));
  return result;
#endif
}

// Signed multiply-accumulate: op1 + (op2 * op3.high16 >> 16).
DRUMLOGUE_ALWAYS_INLINE int32_t smlawt(int32_t op1, int32_t op2, int32_t op3) {
#if defined(UNIT_HOST_NATIVE) || defined(__aarch64__)
  const int32_t high = static_cast<int16_t>((static_cast<uint32_t>(op3) >> 16) & 0xffffu);
  return op1 + static_cast<int32_t>((static_cast<int64_t>(op2) * high) >> 16);
#else
  int32_t result;
  __asm__ volatile ("smlawt %0, %1, %2, %3" : "=r" (result) : "r" (op1), "r" (op2), "r" (op3));
  return result;
#endif
}

// Signed multiply word by low halfword: (op1 * op2.low16) >> 16.
DRUMLOGUE_ALWAYS_INLINE int32_t smulwb(int32_t op1, int32_t op2) {
#if defined(UNIT_HOST_NATIVE) || defined(__aarch64__)
  const int32_t low = static_cast<int16_t>(static_cast<uint32_t>(op2) & 0xffffu);
  return static_cast<int32_t>((static_cast<int64_t>(op1) * low) >> 16);
#else
  int32_t result;
  __asm__ volatile ("smulwb %0, %1, %2" : "=r" (result) : "r" (op1), "r" (op2));
  return result;
#endif
}

// Signed multiply word by high halfword: (op1 * op2.high16) >> 16.
DRUMLOGUE_ALWAYS_INLINE int32_t smulwt(int32_t op1, int32_t op2) {
#if defined(UNIT_HOST_NATIVE) || defined(__aarch64__)
  const int32_t high = static_cast<int16_t>((static_cast<uint32_t>(op2) >> 16) & 0xffffu);
  return static_cast<int32_t>((static_cast<int64_t>(op1) * high) >> 16);
#else
  int32_t result;
  __asm__ volatile ("smulwt %0, %1, %2" : "=r" (result) : "r" (op1), "r" (op2));
  return result;
#endif
}

// Signed bitfield extract (sign-extends the extracted field).
DRUMLOGUE_ALWAYS_INLINE int32_t sbfx(int32_t op1, int32_t lsb, int32_t width) {
#if defined(UNIT_HOST_NATIVE) || defined(__aarch64__)
  const uint32_t mask = (width >= 32) ? UINT32_MAX : ((1u << width) - 1u);
  uint32_t value = (static_cast<uint32_t>(op1) >> lsb) & mask;
  if (width < 32 && (value & (1u << (width - 1))) != 0) value |= ~mask;
  return static_cast<int32_t>(value);
#else
  int32_t result;
  __asm__ volatile ("sbfx %0, %1, %2, %3" : "=r" (result) : "r" (op1), "i" (lsb), "i" (width));
  return result;
#endif
}

// Unsigned bitfield extract.
DRUMLOGUE_ALWAYS_INLINE int32_t ubfx(int32_t op1, int32_t lsb, int32_t width) {
#if defined(UNIT_HOST_NATIVE) || defined(__aarch64__)
  const uint32_t mask = (width >= 32) ? UINT32_MAX : ((1u << width) - 1u);
  return static_cast<int32_t>((static_cast<uint32_t>(op1) >> lsb) & mask);
#else
  int32_t result;
  __asm__ volatile ("ubfx %0, %1, %2, %3" : "=r" (result) : "r" (op1), "i" (lsb), "i" (width));
  return result;
#endif
}

// Unsigned saturate with arithmetic right shift.
DRUMLOGUE_ALWAYS_INLINE int32_t usat_asr(int32_t sat_bits, int32_t op1, int32_t shift) {
#if defined(UNIT_HOST_NATIVE) || defined(__aarch64__)
  int64_t shifted;
  if (shift >= 63) {
    shifted = (op1 < 0) ? -1 : 0;
  } else {
    shifted = static_cast<int64_t>(op1) >> shift;
  }
  const int64_t max_value = (static_cast<int64_t>(1) << sat_bits) - 1;
  if (shifted < 0) return 0;
  if (shifted > max_value) return static_cast<int32_t>(max_value);
  return static_cast<int32_t>(shifted);
#else
  int32_t result;
  __asm__ volatile ("usat %0, %1, %2, asr %3" : "=r" (result) : "i" (sat_bits), "r" (op1), "i" (shift));
  return result;
#endif
}

// Unsigned saturate with logical left shift.
DRUMLOGUE_ALWAYS_INLINE int32_t usat_lsl(int32_t sat_bits, int32_t op1, int32_t shift) {
#if defined(UNIT_HOST_NATIVE) || defined(__aarch64__)
  const int64_t max_value = (static_cast<int64_t>(1) << sat_bits) - 1;
  if (op1 <= 0) return 0;
  if (shift >= 63) return static_cast<int32_t>(max_value);
  const int64_t shifted = static_cast<int64_t>(op1) * (static_cast<int64_t>(1) << shift);
  if (shifted > max_value) return static_cast<int32_t>(max_value);
  return static_cast<int32_t>(shifted);
#else
  int32_t result;
  __asm__ volatile ("usat %0, %1, %2, lsl %3" : "=r" (result) : "i" (sat_bits), "r" (op1), "i" (shift));
  return result;
#endif
}

// Table branch helpers (useful for dense switch jump tables).
#define tbb(base, index) __asm__ volatile ("tbb [%0, %1]" : : "r" (base), "r" (index))
#define tbh(base, index) __asm__ volatile ("tbh [%0, %1, lsl #1]" : : "r" (base), "r" (index))
