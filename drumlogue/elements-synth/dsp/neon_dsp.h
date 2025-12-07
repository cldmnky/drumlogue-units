/*
 * ARM NEON SIMD DSP Utilities for Drumlogue
 * 
 * Optimized for ARM Cortex-A7 (drumlogue CPU: MCIMX6Z0DVM09AB @ 900MHz)
 * 
 * USAGE:
 *   Enable with: -DUSE_NEON in config.mk UDEFS
 *   Requires: ARMv7-A with NEON support (drumlogue SDK default)
 * 
 * NOTES:
 *   - Only targets simple parallel operations (gain, mix, stereo)
 *   - Feedback loops and recursive filters should remain scalar
 *   - Manual instruction interleaving for better latency hiding
 *   - GCC's auto-vectorization is suboptimal for Cortex-A7
 * 
 * Based on real-world drumlogue developer findings:
 *   - Over-vectorization can HURT performance
 *   - Manual scheduling improves results over auto-vectorization
 *   - Keep feedback-dependent DSP (filters, delays) scalar
 */

#pragma once

#include <stdint.h>

#ifdef USE_NEON
#include <arm_neon.h>
#endif

namespace modal {
namespace neon {

// ============================================================================
// Buffer Operations
// ============================================================================

/**
 * Clear a buffer to zero
 * NEON: 4 floats per iteration (128-bit)
 */
inline void ClearBuffer(float* buffer, uint32_t frames) {
#ifdef USE_NEON
    // Process 4 floats at a time with NEON
    float32x4_t zero = vdupq_n_f32(0.0f);
    uint32_t i = 0;
    
    // Main NEON loop (4 floats = 16 bytes per iteration)
    for (; i + 4 <= frames; i += 4) {
        vst1q_f32(&buffer[i], zero);
    }
    
    // Handle remaining samples
    for (; i < frames; ++i) {
        buffer[i] = 0.0f;
    }
#else
    // Scalar fallback
    for (uint32_t i = 0; i < frames; ++i) {
        buffer[i] = 0.0f;
    }
#endif
}

/**
 * Clear stereo buffers (L and R) to zero
 */
inline void ClearStereoBuffers(float* left, float* right, uint32_t frames) {
#ifdef USE_NEON
    float32x4_t zero = vdupq_n_f32(0.0f);
    uint32_t i = 0;
    
    // Interleaved loads/stores for better pipeline utilization
    for (; i + 4 <= frames; i += 4) {
        vst1q_f32(&left[i], zero);
        vst1q_f32(&right[i], zero);
    }
    
    for (; i < frames; ++i) {
        left[i] = 0.0f;
        right[i] = 0.0f;
    }
#else
    for (uint32_t i = 0; i < frames; ++i) {
        left[i] = 0.0f;
        right[i] = 0.0f;
    }
#endif
}

// ============================================================================
// Gain Operations
// ============================================================================

/**
 * Apply constant gain to a buffer (in-place)
 * out[i] = buffer[i] * gain
 */
inline void ApplyGain(float* buffer, float gain, uint32_t frames) {
#ifdef USE_NEON
    float32x4_t gain_vec = vdupq_n_f32(gain);
    uint32_t i = 0;
    
    // Process 8 samples at a time with manual interleaving
    // This helps hide NEON execution latency
    for (; i + 8 <= frames; i += 8) {
        float32x4_t a = vld1q_f32(&buffer[i]);
        float32x4_t b = vld1q_f32(&buffer[i + 4]);
        a = vmulq_f32(a, gain_vec);
        b = vmulq_f32(b, gain_vec);
        vst1q_f32(&buffer[i], a);
        vst1q_f32(&buffer[i + 4], b);
    }
    
    // Process remaining 4-sample block
    for (; i + 4 <= frames; i += 4) {
        float32x4_t samples = vld1q_f32(&buffer[i]);
        samples = vmulq_f32(samples, gain_vec);
        vst1q_f32(&buffer[i], samples);
    }
    
    // Handle remaining samples
    for (; i < frames; ++i) {
        buffer[i] *= gain;
    }
#else
    for (uint32_t i = 0; i < frames; ++i) {
        buffer[i] *= gain;
    }
#endif
}

/**
 * Apply gain and store to separate output buffer
 * out[i] = in[i] * gain
 */
inline void ApplyGainTo(const float* in, float* out, float gain, uint32_t frames) {
#ifdef USE_NEON
    float32x4_t gain_vec = vdupq_n_f32(gain);
    uint32_t i = 0;
    
    for (; i + 8 <= frames; i += 8) {
        float32x4_t a = vld1q_f32(&in[i]);
        float32x4_t b = vld1q_f32(&in[i + 4]);
        a = vmulq_f32(a, gain_vec);
        b = vmulq_f32(b, gain_vec);
        vst1q_f32(&out[i], a);
        vst1q_f32(&out[i + 4], b);
    }
    
    for (; i + 4 <= frames; i += 4) {
        float32x4_t samples = vld1q_f32(&in[i]);
        samples = vmulq_f32(samples, gain_vec);
        vst1q_f32(&out[i], samples);
    }
    
    for (; i < frames; ++i) {
        out[i] = in[i] * gain;
    }
#else
    for (uint32_t i = 0; i < frames; ++i) {
        out[i] = in[i] * gain;
    }
#endif
}

// ============================================================================
// Stereo Operations
// ============================================================================

/**
 * Convert Mid-Side to Left-Right stereo
 * left[i]  = mid[i] + side[i]
 * right[i] = mid[i] - side[i]
 * 
 * L/R channels are independent - perfect for NEON
 */
inline void MidSideToStereo(const float* mid, const float* side, 
                            float* left, float* right, uint32_t frames) {
#ifdef USE_NEON
    uint32_t i = 0;
    
    // Process 4 samples at a time
    for (; i + 4 <= frames; i += 4) {
        float32x4_t m = vld1q_f32(&mid[i]);
        float32x4_t s = vld1q_f32(&side[i]);
        float32x4_t l = vaddq_f32(m, s);  // L = M + S
        float32x4_t r = vsubq_f32(m, s);  // R = M - S
        vst1q_f32(&left[i], l);
        vst1q_f32(&right[i], r);
    }
    
    // Handle remaining samples
    for (; i < frames; ++i) {
        left[i] = mid[i] + side[i];
        right[i] = mid[i] - side[i];
    }
#else
    for (uint32_t i = 0; i < frames; ++i) {
        left[i] = mid[i] + side[i];
        right[i] = mid[i] - side[i];
    }
#endif
}

/**
 * Mix stereo buffers with independent gains
 * left[i]  = in_l[i] * gain_l
 * right[i] = in_r[i] * gain_r
 */
inline void StereoGain(const float* in_l, const float* in_r,
                       float* out_l, float* out_r,
                       float gain_l, float gain_r, uint32_t frames) {
#ifdef USE_NEON
    float32x4_t gl = vdupq_n_f32(gain_l);
    float32x4_t gr = vdupq_n_f32(gain_r);
    uint32_t i = 0;
    
    // Interleaved processing for better latency hiding
    for (; i + 4 <= frames; i += 4) {
        float32x4_t l = vld1q_f32(&in_l[i]);
        float32x4_t r = vld1q_f32(&in_r[i]);
        l = vmulq_f32(l, gl);
        r = vmulq_f32(r, gr);
        vst1q_f32(&out_l[i], l);
        vst1q_f32(&out_r[i], r);
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

/**
 * Interleave stereo buffers to output (L/R pairs)
 * out[i*2]   = left[i]
 * out[i*2+1] = right[i]
 * 
 * Used for final output to SDK
 */
inline void InterleaveStereo(const float* left, const float* right,
                             float* out, uint32_t frames) {
#ifdef USE_NEON
    uint32_t i = 0;
    
    // Process 4 samples at a time using zip (interleave) operations
    for (; i + 4 <= frames; i += 4) {
        float32x4_t l = vld1q_f32(&left[i]);
        float32x4_t r = vld1q_f32(&right[i]);
        
        // Interleave: [L0,L1,L2,L3] + [R0,R1,R2,R3] -> [L0,R0,L1,R1], [L2,R2,L3,R3]
        float32x4x2_t interleaved = vzipq_f32(l, r);
        
        vst1q_f32(&out[i * 2], interleaved.val[0]);
        vst1q_f32(&out[i * 2 + 4], interleaved.val[1]);
    }
    
    // Handle remaining samples
    for (; i < frames; ++i) {
        out[i * 2] = left[i];
        out[i * 2 + 1] = right[i];
    }
#else
    for (uint32_t i = 0; i < frames; ++i) {
        out[i * 2] = left[i];
        out[i * 2 + 1] = right[i];
    }
#endif
}

// ============================================================================
// Limiting / Clamping Operations
// ============================================================================

/**
 * Clamp buffer values to [-limit, +limit]
 * Prevents clipping/overflow in final output
 */
inline void ClampBuffer(float* buffer, float limit, uint32_t frames) {
#ifdef USE_NEON
    float32x4_t pos_limit = vdupq_n_f32(limit);
    float32x4_t neg_limit = vdupq_n_f32(-limit);
    uint32_t i = 0;
    
    for (; i + 4 <= frames; i += 4) {
        float32x4_t samples = vld1q_f32(&buffer[i]);
        // Clamp: max(min(x, pos_limit), neg_limit)
        samples = vminq_f32(samples, pos_limit);
        samples = vmaxq_f32(samples, neg_limit);
        vst1q_f32(&buffer[i], samples);
    }
    
    for (; i < frames; ++i) {
        float v = buffer[i];
        if (v > limit) v = limit;
        if (v < -limit) v = -limit;
        buffer[i] = v;
    }
#else
    for (uint32_t i = 0; i < frames; ++i) {
        float v = buffer[i];
        if (v > limit) v = limit;
        if (v < -limit) v = -limit;
        buffer[i] = v;
    }
#endif
}

/**
 * Clamp stereo buffers (both L and R)
 */
inline void ClampStereoBuffers(float* left, float* right, float limit, uint32_t frames) {
#ifdef USE_NEON
    float32x4_t pos_limit = vdupq_n_f32(limit);
    float32x4_t neg_limit = vdupq_n_f32(-limit);
    uint32_t i = 0;
    
    for (; i + 4 <= frames; i += 4) {
        float32x4_t l = vld1q_f32(&left[i]);
        float32x4_t r = vld1q_f32(&right[i]);
        
        l = vminq_f32(l, pos_limit);
        l = vmaxq_f32(l, neg_limit);
        r = vminq_f32(r, pos_limit);
        r = vmaxq_f32(r, neg_limit);
        
        vst1q_f32(&left[i], l);
        vst1q_f32(&right[i], r);
    }
    
    for (; i < frames; ++i) {
        float lv = left[i];
        float rv = right[i];
        if (lv > limit) lv = limit;
        if (lv < -limit) lv = -limit;
        if (rv > limit) rv = limit;
        if (rv < -limit) rv = -limit;
        left[i] = lv;
        right[i] = rv;
    }
#else
    ClampBuffer(left, limit, frames);
    ClampBuffer(right, limit, frames);
#endif
}

/**
 * Check for and replace NaN/Inf values with zero
 * Uses NEON comparison to detect invalid floats
 */
inline void SanitizeBuffer(float* buffer, uint32_t frames) {
#ifdef USE_NEON
    uint32_t i = 0;
    
    for (; i + 4 <= frames; i += 4) {
        float32x4_t samples = vld1q_f32(&buffer[i]);
        
        // NEON trick: NaN != NaN, so compare sample to itself
        // ceq returns all 1s for valid, all 0s for NaN
        uint32x4_t valid_mask = vceqq_f32(samples, samples);
        
        // Select: valid samples stay, NaN becomes 0
        float32x4_t zero = vdupq_n_f32(0.0f);
        samples = vbslq_f32(valid_mask, samples, zero);
        
        vst1q_f32(&buffer[i], samples);
    }
    
    // Handle remaining samples
    for (; i < frames; ++i) {
        if (buffer[i] != buffer[i]) {  // NaN check
            buffer[i] = 0.0f;
        }
    }
#else
    for (uint32_t i = 0; i < frames; ++i) {
        if (buffer[i] != buffer[i]) {
            buffer[i] = 0.0f;
        }
    }
#endif
}

/**
 * Combined sanitize (NaN removal) and clamp operation
 * Efficient single-pass protection for final output
 */
inline void SanitizeAndClamp(float* buffer, float limit, uint32_t frames) {
#ifdef USE_NEON
    float32x4_t pos_limit = vdupq_n_f32(limit);
    float32x4_t neg_limit = vdupq_n_f32(-limit);
    float32x4_t zero = vdupq_n_f32(0.0f);
    uint32_t i = 0;
    
    for (; i + 4 <= frames; i += 4) {
        float32x4_t samples = vld1q_f32(&buffer[i]);
        
        // Replace NaN with zero
        uint32x4_t valid_mask = vceqq_f32(samples, samples);
        samples = vbslq_f32(valid_mask, samples, zero);
        
        // Clamp to limits
        samples = vminq_f32(samples, pos_limit);
        samples = vmaxq_f32(samples, neg_limit);
        
        vst1q_f32(&buffer[i], samples);
    }
    
    for (; i < frames; ++i) {
        float v = buffer[i];
        if (v != v) v = 0.0f;  // NaN check
        if (v > limit) v = limit;
        if (v < -limit) v = -limit;
        buffer[i] = v;
    }
#else
    for (uint32_t i = 0; i < frames; ++i) {
        float v = buffer[i];
        if (v != v) v = 0.0f;
        if (v > limit) v = limit;
        if (v < -limit) v = -limit;
        buffer[i] = v;
    }
#endif
}

// ============================================================================
// Mixing Operations
// ============================================================================

/**
 * Add source buffer to destination (accumulate)
 * dest[i] += src[i]
 */
inline void Accumulate(const float* src, float* dest, uint32_t frames) {
#ifdef USE_NEON
    uint32_t i = 0;
    
    for (; i + 4 <= frames; i += 4) {
        float32x4_t s = vld1q_f32(&src[i]);
        float32x4_t d = vld1q_f32(&dest[i]);
        d = vaddq_f32(d, s);
        vst1q_f32(&dest[i], d);
    }
    
    for (; i < frames; ++i) {
        dest[i] += src[i];
    }
#else
    for (uint32_t i = 0; i < frames; ++i) {
        dest[i] += src[i];
    }
#endif
}

/**
 * Mix two buffers with gains: out = a*gain_a + b*gain_b
 */
inline void MixBuffers(const float* a, const float* b,
                       float* out, float gain_a, float gain_b, uint32_t frames) {
#ifdef USE_NEON
    float32x4_t ga = vdupq_n_f32(gain_a);
    float32x4_t gb = vdupq_n_f32(gain_b);
    uint32_t i = 0;
    
    for (; i + 4 <= frames; i += 4) {
        float32x4_t va = vld1q_f32(&a[i]);
        float32x4_t vb = vld1q_f32(&b[i]);
        
        // out = a*gain_a + b*gain_b
        float32x4_t result = vmulq_f32(va, ga);
        result = vmlaq_f32(result, vb, gb);  // Fused multiply-add
        
        vst1q_f32(&out[i], result);
    }
    
    for (; i < frames; ++i) {
        out[i] = a[i] * gain_a + b[i] * gain_b;
    }
#else
    for (uint32_t i = 0; i < frames; ++i) {
        out[i] = a[i] * gain_a + b[i] * gain_b;
    }
#endif
}

} // namespace neon
} // namespace modal
