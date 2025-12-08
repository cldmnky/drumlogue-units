#pragma once

/*
 * ARM NEON SIMD DSP Utilities for drumlogue (Cortex-A7, NEON).
 *
 * To bind functions into your unit's namespace, define NEON_DSP_NS before include:
 *   #define NEON_DSP_NS clouds_revfx
 *   #include "common/neon_dsp.h"
 * Otherwise the functions live in neon_dsp::neon.
 */

#include <stdint.h>

#ifdef USE_NEON
#include <arm_neon.h>
#endif

#ifndef NEON_DSP_NS
#define NEON_DSP_NS neon_dsp
#endif

namespace NEON_DSP_NS {
namespace neon {

// Buffer operations
inline void ClearBuffer(float* buffer, uint32_t frames) {
#ifdef USE_NEON
    float32x4_t zero = vdupq_n_f32(0.0f);
    uint32_t i = 0;
    for (; i + 4 <= frames; i += 4) {
        vst1q_f32(&buffer[i], zero);
    }
    for (; i < frames; ++i) buffer[i] = 0.0f;
#else
    for (uint32_t i = 0; i < frames; ++i) buffer[i] = 0.0f;
#endif
}

inline void ClearStereoBuffers(float* left, float* right, uint32_t frames) {
#ifdef USE_NEON
    float32x4_t zero = vdupq_n_f32(0.0f);
    uint32_t i = 0;
    for (; i + 4 <= frames; i += 4) {
        vst1q_f32(&left[i], zero);
        vst1q_f32(&right[i], zero);
    }
    for (; i < frames; ++i) { left[i] = 0.0f; right[i] = 0.0f; }
#else
    for (uint32_t i = 0; i < frames; ++i) { left[i] = 0.0f; right[i] = 0.0f; }
#endif
}

// Gain operations
inline void ApplyGain(float* buffer, float gain, uint32_t frames) {
#ifdef USE_NEON
    float32x4_t g = vdupq_n_f32(gain);
    uint32_t i = 0;
    for (; i + 8 <= frames; i += 8) {
        float32x4_t a = vld1q_f32(&buffer[i]);
        float32x4_t b = vld1q_f32(&buffer[i + 4]);
        vst1q_f32(&buffer[i], vmulq_f32(a, g));
        vst1q_f32(&buffer[i + 4], vmulq_f32(b, g));
    }
    for (; i + 4 <= frames; i += 4) {
        float32x4_t s = vld1q_f32(&buffer[i]);
        vst1q_f32(&buffer[i], vmulq_f32(s, g));
    }
    for (; i < frames; ++i) buffer[i] *= gain;
#else
    for (uint32_t i = 0; i < frames; ++i) buffer[i] *= gain;
#endif
}

inline void ApplyGainTo(const float* in, float* out, float gain, uint32_t frames) {
#ifdef USE_NEON
    float32x4_t g = vdupq_n_f32(gain);
    uint32_t i = 0;
    for (; i + 8 <= frames; i += 8) {
        float32x4_t a = vld1q_f32(&in[i]);
        float32x4_t b = vld1q_f32(&in[i + 4]);
        vst1q_f32(&out[i], vmulq_f32(a, g));
        vst1q_f32(&out[i + 4], vmulq_f32(b, g));
    }
    for (; i + 4 <= frames; i += 4) {
        float32x4_t s = vld1q_f32(&in[i]);
        vst1q_f32(&out[i], vmulq_f32(s, g));
    }
    for (; i < frames; ++i) out[i] = in[i] * gain;
#else
    for (uint32_t i = 0; i < frames; ++i) out[i] = in[i] * gain;
#endif
}

// Stereo operations
inline void MidSideToStereo(const float* mid, const float* side,
                            float* left, float* right, uint32_t frames) {
#ifdef USE_NEON
    uint32_t i = 0;
    for (; i + 4 <= frames; i += 4) {
        float32x4_t m = vld1q_f32(&mid[i]);
        float32x4_t s = vld1q_f32(&side[i]);
        vst1q_f32(&left[i],  vaddq_f32(m, s));
        vst1q_f32(&right[i], vsubq_f32(m, s));
    }
    for (; i < frames; ++i) { left[i] = mid[i] + side[i]; right[i] = mid[i] - side[i]; }
#else
    for (uint32_t i = 0; i < frames; ++i) { left[i] = mid[i] + side[i]; right[i] = mid[i] - side[i]; }
#endif
}

inline void StereoGain(const float* in_l, const float* in_r,
                       float* out_l, float* out_r,
                       float gain_l, float gain_r, uint32_t frames) {
#ifdef USE_NEON
    float32x4_t gl = vdupq_n_f32(gain_l);
    float32x4_t gr = vdupq_n_f32(gain_r);
    uint32_t i = 0;
    for (; i + 4 <= frames; i += 4) {
        float32x4_t l = vld1q_f32(&in_l[i]);
        float32x4_t r = vld1q_f32(&in_r[i]);
        vst1q_f32(&out_l[i], vmulq_f32(l, gl));
        vst1q_f32(&out_r[i], vmulq_f32(r, gr));
    }
    for (; i < frames; ++i) {
        out_l[i] = in_l[i] * gain_l;
        out_r[i] = in_r[i] * gain_r;
    }
#else
    for (uint32_t i = 0; i < frames; ++i) {
        out_l[i] = in_l[i] * gain_l;
        out_r[i] = in_r[i] * gain_r;
    }
#endif
}

inline void InterleaveStereo(const float* left, const float* right, float* out, uint32_t frames) {
#ifdef USE_NEON
    uint32_t i = 0;
    for (; i + 4 <= frames; i += 4) {
        float32x4_t l = vld1q_f32(&left[i]);
        float32x4_t r = vld1q_f32(&right[i]);
        float32x4x2_t interleaved = vzipq_f32(l, r);
        vst1q_f32(&out[i * 2],     interleaved.val[0]);
        vst1q_f32(&out[i * 2 + 4], interleaved.val[1]);
    }
    for (; i < frames; ++i) { out[i * 2] = left[i]; out[i * 2 + 1] = right[i]; }
#else
    for (uint32_t i = 0; i < frames; ++i) { out[i * 2] = left[i]; out[i * 2 + 1] = right[i]; }
#endif
}

// Limiting / clamping operations
inline void ClampBuffer(float* buffer, float limit, uint32_t frames) {
#ifdef USE_NEON
    float32x4_t pos = vdupq_n_f32(limit);
    float32x4_t neg = vdupq_n_f32(-limit);
    uint32_t i = 0;
    for (; i + 4 <= frames; i += 4) {
        float32x4_t s = vld1q_f32(&buffer[i]);
        s = vminq_f32(s, pos);
        s = vmaxq_f32(s, neg);
        vst1q_f32(&buffer[i], s);
    }
    for (; i < frames; ++i) {
        float v = buffer[i];
        if (v > limit) v = limit; else if (v < -limit) v = -limit;
        buffer[i] = v;
    }
#else
    for (uint32_t i = 0; i < frames; ++i) {
        float v = buffer[i];
        if (v > limit) v = limit; else if (v < -limit) v = -limit;
        buffer[i] = v;
    }
#endif
}

inline void ClampStereoBuffers(float* left, float* right, float limit, uint32_t frames) {
#ifdef USE_NEON
    float32x4_t pos = vdupq_n_f32(limit);
    float32x4_t neg = vdupq_n_f32(-limit);
    uint32_t i = 0;
    for (; i + 4 <= frames; i += 4) {
        float32x4_t l = vld1q_f32(&left[i]);
        float32x4_t r = vld1q_f32(&right[i]);
        l = vminq_f32(l, pos); l = vmaxq_f32(l, neg);
        r = vminq_f32(r, pos); r = vmaxq_f32(r, neg);
        vst1q_f32(&left[i], l);
        vst1q_f32(&right[i], r);
    }
    for (; i < frames; ++i) {
        float lv = left[i]; float rv = right[i];
        if (lv > limit) lv = limit; else if (lv < -limit) lv = -limit;
        if (rv > limit) rv = limit; else if (rv < -limit) rv = -limit;
        left[i] = lv; right[i] = rv;
    }
#else
    ClampBuffer(left, limit, frames);
    ClampBuffer(right, limit, frames);
#endif
}

inline void SanitizeBuffer(float* buffer, uint32_t frames) {
#ifdef USE_NEON
    uint32_t i = 0;
    float32x4_t zero = vdupq_n_f32(0.0f);
    for (; i + 4 <= frames; i += 4) {
        float32x4_t s = vld1q_f32(&buffer[i]);
        uint32x4_t valid = vceqq_f32(s, s); // NaN != NaN
        s = vbslq_f32(valid, s, zero);
        vst1q_f32(&buffer[i], s);
    }
    for (; i < frames; ++i) if (buffer[i] != buffer[i]) buffer[i] = 0.0f;
#else
    for (uint32_t i = 0; i < frames; ++i) if (buffer[i] != buffer[i]) buffer[i] = 0.0f;
#endif
}

inline void SanitizeAndClamp(float* buffer, float limit, uint32_t frames) {
#ifdef USE_NEON
    float32x4_t pos = vdupq_n_f32(limit);
    float32x4_t neg = vdupq_n_f32(-limit);
    float32x4_t zero = vdupq_n_f32(0.0f);
    uint32_t i = 0;
    for (; i + 4 <= frames; i += 4) {
        float32x4_t s = vld1q_f32(&buffer[i]);
        uint32x4_t valid = vceqq_f32(s, s);
        s = vbslq_f32(valid, s, zero);
        s = vminq_f32(s, pos);
        s = vmaxq_f32(s, neg);
        vst1q_f32(&buffer[i], s);
    }
    for (; i < frames; ++i) {
        float v = buffer[i];
        if (v != v) v = 0.0f;
        if (v > limit) v = limit; else if (v < -limit) v = -limit;
        buffer[i] = v;
    }
#else
    SanitizeBuffer(buffer, frames);
    ClampBuffer(buffer, limit, frames);
#endif
}

#ifdef USE_NEON
// NEON-optimized FastTanh for 4 values simultaneously
// tanh(x) ≈ x * (27 + x²) / (27 + 9*x²)
inline float32x4_t FastTanh4(float32x4_t x) {
    const float32x4_t k27 = vdupq_n_f32(27.0f);
    const float32x4_t k9 = vdupq_n_f32(9.0f);
    const float32x4_t kOne = vdupq_n_f32(1.0f);
    const float32x4_t kNegOne = vdupq_n_f32(-1.0f);
    const float32x4_t kFour = vdupq_n_f32(4.0f);
    const float32x4_t kNegFour = vdupq_n_f32(-4.0f);
    
    // x² 
    float32x4_t x2 = vmulq_f32(x, x);
    
    // numerator = x * (27 + x²)
    float32x4_t num = vmulq_f32(x, vaddq_f32(k27, x2));
    
    // denominator = 27 + 9*x²
    float32x4_t denom = vmlaq_f32(k27, k9, x2);  // 27 + 9*x²
    
    // Approximate division: num / denom
    // Use Newton-Raphson reciprocal for better accuracy
    float32x4_t recip = vrecpeq_f32(denom);
    recip = vmulq_f32(vrecpsq_f32(denom, recip), recip);  // One NR iteration
    float32x4_t result = vmulq_f32(num, recip);
    
    // Clamp to [-1, 1] for |x| > 4
    uint32x4_t gt4 = vcgtq_f32(x, kFour);
    uint32x4_t ltneg4 = vcltq_f32(x, kNegFour);
    result = vbslq_f32(gt4, kOne, result);
    result = vbslq_f32(ltneg4, kNegOne, result);
    
    return result;
}

// NEON-optimized soft clamp using tanh for stereo buffers
// Applies tanh(x * drive) * gain for smooth saturation
inline void SoftClampStereo(float* left, float* right, float drive, float gain, uint32_t frames) {
    float32x4_t drive_vec = vdupq_n_f32(drive);
    float32x4_t gain_vec = vdupq_n_f32(gain);
    uint32_t i = 0;
    for (; i + 4 <= frames; i += 4) {
        float32x4_t l = vld1q_f32(&left[i]);
        float32x4_t r = vld1q_f32(&right[i]);
        
        // Apply soft clipping: tanh(x * drive) * gain
        l = vmulq_f32(FastTanh4(vmulq_f32(l, drive_vec)), gain_vec);
        r = vmulq_f32(FastTanh4(vmulq_f32(r, drive_vec)), gain_vec);
        
        vst1q_f32(&left[i], l);
        vst1q_f32(&right[i], r);
    }
    // Scalar fallback for remaining samples
    for (; i < frames; ++i) {
        float lv = left[i] * drive;
        float rv = right[i] * drive;
        // Simple tanh approximation
        if (lv > 4.0f) lv = 1.0f; else if (lv < -4.0f) lv = -1.0f;
        else { float l2 = lv*lv; lv = lv * (27.0f + l2) / (27.0f + 9.0f * l2); }
        if (rv > 4.0f) rv = 1.0f; else if (rv < -4.0f) rv = -1.0f;
        else { float r2 = rv*rv; rv = rv * (27.0f + r2) / (27.0f + 9.0f * r2); }
        left[i] = lv * gain;
        right[i] = rv * gain;
    }
}
#endif

} // namespace neon
} // namespace NEON_DSP_NS

// NOTE: NEON_DSP_NS intentionally not undefined here.
// The including file is responsible for defining and undefining it.
// See drumlogue/clouds-revfx/dsp/neon_dsp.h for the pattern.
