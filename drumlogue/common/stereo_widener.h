/**
 * @file stereo_widener.h
 * @brief NEON-optimized stereo widening utilities for drumlogue units
 *
 * Provides Mid/Side stereo processing inspired by Mutable Instruments Elements.
 * Can be used to add stereo spread to mono sources or enhance existing stereo.
 *
 * All batch processing functions are NEON-optimized for ARM Cortex-A7.
 *
 * Techniques included:
 * - Mid/Side encoding/decoding
 * - Stereo width control (0 = mono, 1 = full stereo, >1 = exaggerated)
 * - LFO-modulated stereo spread for animation
 */

#pragma once

#include <cstdint>
#include <cmath>

#ifdef USE_NEON
#include <arm_neon.h>
#endif

namespace common {

// ============================================================================
// NEON Helper Functions
// ============================================================================

#ifdef USE_NEON
/**
 * @brief NEON triangle LFO computation for 4 phases
 * @param phases 4 phase values in [0, 1)
 * @return 4 LFO values in [-1, 1]
 */
inline float32x4_t TriangleLfo4(float32x4_t phases) {
    // Triangle: phase > 0.5 ? 1 - phase : phase, then scale to [-1, 1]
    const float32x4_t half = vdupq_n_f32(0.5f);
    const float32x4_t one = vdupq_n_f32(1.0f);
    const float32x4_t four = vdupq_n_f32(4.0f);
    
    // Fold phase: result = phase > 0.5 ? (1 - phase) : phase
    uint32x4_t gt_half = vcgtq_f32(phases, half);
    float32x4_t folded = vbslq_f32(gt_half, vsubq_f32(one, phases), phases);
    
    // Scale to [-1, 1]: lfo = folded * 4 - 1
    return vsubq_f32(vmulq_f32(folded, four), one);
}
#endif

// ============================================================================
// StereoWidener - Simple Mid/Side width control
// ============================================================================

/**
 * @brief Simple stereo width control using Mid/Side processing
 *
 * Takes a center (mono) signal and a side (difference) signal,
 * applies width control, and outputs left/right stereo.
 */
class StereoWidener {
public:
    StereoWidener() : width_(0.5f) {}
    
    /**
     * @brief Set stereo width
     * @param width 0.0 = mono, 0.5 = normal stereo, 1.0 = wide, >1.0 = exaggerated
     */
    void SetWidth(float width) {
        width_ = (width < 0.0f) ? 0.0f : ((width > 2.0f) ? 2.0f : width);
    }
    
    float GetWidth() const { return width_; }
    
    /**
     * @brief Convert Mid/Side to Left/Right with width control
     * @param mid Center (mono) signal
     * @param side Side (difference) signal
     * @param out_left Output left channel
     * @param out_right Output right channel
     */
    inline void Process(float mid, float side, float& out_left, float& out_right) {
        float scaled_side = side * width_;
        out_left = mid + scaled_side;
        out_right = mid - scaled_side;
    }
    
    /**
     * @brief Convert stereo L/R to Mid/Side, apply width, convert back
     */
    inline void ProcessStereo(float in_left, float in_right, float& out_left, float& out_right) {
        float mid = (in_left + in_right) * 0.5f;
        float side = (in_left - in_right) * 0.5f;
        Process(mid, side, out_left, out_right);
    }
    
#ifdef USE_NEON
    /**
     * @brief NEON: Process 4 M/S samples to L/R
     */
    inline void Process4(const float* mid, const float* side, float* out_left, float* out_right) {
        float32x4_t mid_vec = vld1q_f32(mid);
        float32x4_t side_vec = vld1q_f32(side);
        float32x4_t width_vec = vdupq_n_f32(width_);
        
        float32x4_t scaled_side = vmulq_f32(side_vec, width_vec);
        vst1q_f32(out_left, vaddq_f32(mid_vec, scaled_side));
        vst1q_f32(out_right, vsubq_f32(mid_vec, scaled_side));
    }
    
    /**
     * @brief NEON: Process 4 stereo L/R samples with width control
     */
    inline void ProcessStereo4(const float* in_left, const float* in_right, 
                               float* out_left, float* out_right) {
        float32x4_t l = vld1q_f32(in_left);
        float32x4_t r = vld1q_f32(in_right);
        float32x4_t half = vdupq_n_f32(0.5f);
        float32x4_t width_vec = vdupq_n_f32(width_);
        
        // L/R to M/S
        float32x4_t mid = vmulq_f32(vaddq_f32(l, r), half);
        float32x4_t side = vmulq_f32(vmulq_f32(vsubq_f32(l, r), half), width_vec);
        
        // M/S to L/R
        vst1q_f32(out_left, vaddq_f32(mid, side));
        vst1q_f32(out_right, vsubq_f32(mid, side));
    }
    
    /**
     * @brief NEON: Batch process entire buffer (optimal for drumlogue's 64-sample blocks)
     */
    void ProcessBatch(const float* mid, const float* side, 
                      float* out_left, float* out_right, size_t count) {
        float32x4_t width_vec = vdupq_n_f32(width_);
        
        size_t i = 0;
        for (; i + 4 <= count; i += 4) {
            float32x4_t mid_vec = vld1q_f32(&mid[i]);
            float32x4_t side_vec = vld1q_f32(&side[i]);
            float32x4_t scaled_side = vmulq_f32(side_vec, width_vec);
            vst1q_f32(&out_left[i], vaddq_f32(mid_vec, scaled_side));
            vst1q_f32(&out_right[i], vsubq_f32(mid_vec, scaled_side));
        }
        // Scalar tail
        for (; i < count; ++i) {
            float scaled_side = side[i] * width_;
            out_left[i] = mid[i] + scaled_side;
            out_right[i] = mid[i] - scaled_side;
        }
    }
    
    /**
     * @brief NEON: Batch process stereo L/R buffer with width (in-place capable)
     */
    void ProcessStereoBatch(float* left, float* right, size_t count) {
        float32x4_t width_vec = vdupq_n_f32(width_);
        float32x4_t half = vdupq_n_f32(0.5f);
        
        size_t i = 0;
        for (; i + 4 <= count; i += 4) {
            float32x4_t l = vld1q_f32(&left[i]);
            float32x4_t r = vld1q_f32(&right[i]);
            
            float32x4_t mid = vmulq_f32(vaddq_f32(l, r), half);
            float32x4_t side = vmulq_f32(vmulq_f32(vsubq_f32(l, r), half), width_vec);
            
            vst1q_f32(&left[i], vaddq_f32(mid, side));
            vst1q_f32(&right[i], vsubq_f32(mid, side));
        }
        // Scalar tail
        for (; i < count; ++i) {
            float mid = (left[i] + right[i]) * 0.5f;
            float side = (left[i] - right[i]) * 0.5f * width_;
            left[i] = mid + side;
            right[i] = mid - side;
        }
    }
#else
    // Scalar fallbacks
    void ProcessBatch(const float* mid, const float* side, 
                      float* out_left, float* out_right, size_t count) {
        for (size_t i = 0; i < count; ++i) {
            float scaled_side = side[i] * width_;
            out_left[i] = mid[i] + scaled_side;
            out_right[i] = mid[i] - scaled_side;
        }
    }
    
    void ProcessStereoBatch(float* left, float* right, size_t count) {
        for (size_t i = 0; i < count; ++i) {
            float mid = (left[i] + right[i]) * 0.5f;
            float side = (left[i] - right[i]) * 0.5f * width_;
            left[i] = mid + side;
            right[i] = mid - side;
        }
    }
#endif

private:
    float width_;
};

// ============================================================================
// AnimatedStereoWidener - LFO-modulated stereo spread
// ============================================================================

/**
 * @brief Animated stereo widener with LFO modulation
 *
 * Creates dynamic stereo movement by modulating the stereo field
 * with an internal LFO. Based on Elements' stereo spread technique.
 */
class AnimatedStereoWidener {
public:
    AnimatedStereoWidener() 
        : width_(0.5f)
        , lfo_rate_(0.5f / 48000.0f)
        , lfo_phase_(0.0f)
        , lfo_depth_(0.25f)
        , sample_rate_(48000.0f) {}
    
    void Init(float sample_rate) {
        sample_rate_ = sample_rate;
        lfo_rate_ = 0.5f / sample_rate_;
        lfo_phase_ = 0.0f;
    }
    
    void SetWidth(float width) {
        width_ = (width < 0.0f) ? 0.0f : ((width > 2.0f) ? 2.0f : width);
    }
    
    void SetLfoRate(float rate_hz) {
        rate_hz = (rate_hz < 0.01f) ? 0.01f : ((rate_hz > 20.0f) ? 20.0f : rate_hz);
        lfo_rate_ = rate_hz / sample_rate_;
    }
    
    void SetLfoDepth(float depth) {
        lfo_depth_ = (depth < 0.0f) ? 0.0f : ((depth > 1.0f) ? 1.0f : depth);
    }
    
    float GetWidth() const { return width_; }
    float GetLfoValue() const {
        float lfo = lfo_phase_ > 0.5f ? 1.0f - lfo_phase_ : lfo_phase_;
        return lfo * 4.0f - 1.0f;
    }
    
    void Reset() { lfo_phase_ = 0.0f; }
    
    /**
     * @brief Process mono input to animated stereo (single sample)
     */
    inline void ProcessMono(float mono, float& out_left, float& out_right) {
        lfo_phase_ += lfo_rate_;
        if (lfo_phase_ >= 1.0f) lfo_phase_ -= 1.0f;
        
        float lfo = lfo_phase_ > 0.5f ? 1.0f - lfo_phase_ : lfo_phase_;
        lfo = lfo * 4.0f - 1.0f;
        
        float offset = width_ * lfo_depth_ * lfo;
        out_left = mono * (1.0f + offset);
        out_right = mono * (1.0f - offset);
    }
    
    /**
     * @brief Process M/S to L/R with animated width (single sample)
     */
    inline void Process(float mid, float side, float& out_left, float& out_right) {
        lfo_phase_ += lfo_rate_;
        if (lfo_phase_ >= 1.0f) lfo_phase_ -= 1.0f;
        
        float lfo = lfo_phase_ > 0.5f ? 1.0f - lfo_phase_ : lfo_phase_;
        lfo = lfo * 4.0f - 1.0f;
        
        float modulated_width = width_ * (1.0f + lfo * lfo_depth_);
        float scaled_side = side * modulated_width;
        
        out_left = mid + scaled_side;
        out_right = mid - scaled_side;
    }
    
#ifdef USE_NEON
    /**
     * @brief NEON: Batch process mono to animated stereo
     * @param mono Input mono buffer
     * @param out_left Output left buffer
     * @param out_right Output right buffer
     * @param count Number of samples
     */
    void ProcessMonoBatch(const float* mono, float* out_left, float* out_right, size_t count) {
        const float32x4_t one = vdupq_n_f32(1.0f);
        const float32x4_t width_depth = vdupq_n_f32(width_ * lfo_depth_);
        const float32x4_t rate_x4 = vdupq_n_f32(lfo_rate_ * 4.0f);
        
        // Create phase offsets for 4 samples: [0, 1, 2, 3] * rate
        float phase_arr[4] = {
            lfo_phase_,
            lfo_phase_ + lfo_rate_,
            lfo_phase_ + lfo_rate_ * 2.0f,
            lfo_phase_ + lfo_rate_ * 3.0f
        };
        float32x4_t phase_vec = vld1q_f32(phase_arr);
        
        size_t i = 0;
        for (; i + 4 <= count; i += 4) {
            // Wrap phases to [0, 1) using floor
            float32x4_t phase_floor = vcvtq_f32_s32(vcvtq_s32_f32(phase_vec));
            float32x4_t phase_wrapped = vsubq_f32(phase_vec, phase_floor);
            
            // Compute triangle LFO for 4 samples
            float32x4_t lfo = TriangleLfo4(phase_wrapped);
            
            // Compute stereo offset
            float32x4_t offset = vmulq_f32(width_depth, lfo);
            
            // Load mono samples
            float32x4_t mono_vec = vld1q_f32(&mono[i]);
            
            // Apply panning: L = mono * (1 + offset), R = mono * (1 - offset)
            vst1q_f32(&out_left[i], vmulq_f32(mono_vec, vaddq_f32(one, offset)));
            vst1q_f32(&out_right[i], vmulq_f32(mono_vec, vsubq_f32(one, offset)));
            
            // Advance phase by 4 samples
            phase_vec = vaddq_f32(phase_vec, rate_x4);
        }
        
        // Update phase state
        float phases[4];
        vst1q_f32(phases, phase_vec);
        lfo_phase_ = phases[0];
        // Wrap final phase
        while (lfo_phase_ >= 1.0f) lfo_phase_ -= 1.0f;
        
        // Scalar tail
        for (; i < count; ++i) {
            ProcessMono(mono[i], out_left[i], out_right[i]);
        }
    }
    
    /**
     * @brief NEON: Batch process M/S to animated L/R
     */
    void ProcessBatch(const float* mid, const float* side, 
                      float* out_left, float* out_right, size_t count) {
        const float32x4_t one = vdupq_n_f32(1.0f);
        const float32x4_t width_vec = vdupq_n_f32(width_);
        const float32x4_t depth_vec = vdupq_n_f32(lfo_depth_);
        const float32x4_t rate_x4 = vdupq_n_f32(lfo_rate_ * 4.0f);
        
        float phase_arr[4] = {
            lfo_phase_,
            lfo_phase_ + lfo_rate_,
            lfo_phase_ + lfo_rate_ * 2.0f,
            lfo_phase_ + lfo_rate_ * 3.0f
        };
        float32x4_t phase_vec = vld1q_f32(phase_arr);
        
        size_t i = 0;
        for (; i + 4 <= count; i += 4) {
            // Wrap phases
            float32x4_t phase_floor = vcvtq_f32_s32(vcvtq_s32_f32(phase_vec));
            float32x4_t phase_wrapped = vsubq_f32(phase_vec, phase_floor);
            
            // Triangle LFO
            float32x4_t lfo = TriangleLfo4(phase_wrapped);
            
            // Modulated width: width * (1 + lfo * depth)
            float32x4_t mod_width = vmulq_f32(width_vec, 
                vmlaq_f32(one, lfo, depth_vec));
            
            // Load M/S
            float32x4_t mid_vec = vld1q_f32(&mid[i]);
            float32x4_t side_vec = vld1q_f32(&side[i]);
            
            // Scale side and convert to L/R
            float32x4_t scaled_side = vmulq_f32(side_vec, mod_width);
            vst1q_f32(&out_left[i], vaddq_f32(mid_vec, scaled_side));
            vst1q_f32(&out_right[i], vsubq_f32(mid_vec, scaled_side));
            
            phase_vec = vaddq_f32(phase_vec, rate_x4);
        }
        
        // Update phase
        float phases[4];
        vst1q_f32(phases, phase_vec);
        lfo_phase_ = phases[0];
        while (lfo_phase_ >= 1.0f) lfo_phase_ -= 1.0f;
        
        // Scalar tail
        for (; i < count; ++i) {
            Process(mid[i], side[i], out_left[i], out_right[i]);
        }
    }
    
    /**
     * @brief NEON: Batch process stereo L/R with animated width (in-place)
     */
    void ProcessStereoBatch(float* left, float* right, size_t count) {
        const float32x4_t one = vdupq_n_f32(1.0f);
        const float32x4_t half = vdupq_n_f32(0.5f);
        const float32x4_t width_vec = vdupq_n_f32(width_);
        const float32x4_t depth_vec = vdupq_n_f32(lfo_depth_);
        const float32x4_t rate_x4 = vdupq_n_f32(lfo_rate_ * 4.0f);
        
        float phase_arr[4] = {
            lfo_phase_,
            lfo_phase_ + lfo_rate_,
            lfo_phase_ + lfo_rate_ * 2.0f,
            lfo_phase_ + lfo_rate_ * 3.0f
        };
        float32x4_t phase_vec = vld1q_f32(phase_arr);
        
        size_t i = 0;
        for (; i + 4 <= count; i += 4) {
            // Wrap phases
            float32x4_t phase_floor = vcvtq_f32_s32(vcvtq_s32_f32(phase_vec));
            float32x4_t phase_wrapped = vsubq_f32(phase_vec, phase_floor);
            
            // Triangle LFO
            float32x4_t lfo = TriangleLfo4(phase_wrapped);
            
            // Modulated width
            float32x4_t mod_width = vmulq_f32(width_vec, 
                vmlaq_f32(one, lfo, depth_vec));
            
            // Load L/R
            float32x4_t l = vld1q_f32(&left[i]);
            float32x4_t r = vld1q_f32(&right[i]);
            
            // L/R to M/S
            float32x4_t mid = vmulq_f32(vaddq_f32(l, r), half);
            float32x4_t side = vmulq_f32(vmulq_f32(vsubq_f32(l, r), half), mod_width);
            
            // M/S back to L/R
            vst1q_f32(&left[i], vaddq_f32(mid, side));
            vst1q_f32(&right[i], vsubq_f32(mid, side));
            
            phase_vec = vaddq_f32(phase_vec, rate_x4);
        }
        
        // Update phase
        float phases[4];
        vst1q_f32(phases, phase_vec);
        lfo_phase_ = phases[0];
        while (lfo_phase_ >= 1.0f) lfo_phase_ -= 1.0f;
        
        // Scalar tail
        for (; i < count; ++i) {
            lfo_phase_ += lfo_rate_;
            if (lfo_phase_ >= 1.0f) lfo_phase_ -= 1.0f;
            
            float lfo = lfo_phase_ > 0.5f ? 1.0f - lfo_phase_ : lfo_phase_;
            lfo = lfo * 4.0f - 1.0f;
            
            float mod_width = width_ * (1.0f + lfo * lfo_depth_);
            float mid = (left[i] + right[i]) * 0.5f;
            float side = (left[i] - right[i]) * 0.5f * mod_width;
            left[i] = mid + side;
            right[i] = mid - side;
        }
    }
#else
    // Scalar fallbacks
    void ProcessMonoBatch(const float* mono, float* out_left, float* out_right, size_t count) {
        for (size_t i = 0; i < count; ++i) {
            ProcessMono(mono[i], out_left[i], out_right[i]);
        }
    }
    
    void ProcessBatch(const float* mid, const float* side, 
                      float* out_left, float* out_right, size_t count) {
        for (size_t i = 0; i < count; ++i) {
            Process(mid[i], side[i], out_left[i], out_right[i]);
        }
    }
    
    void ProcessStereoBatch(float* left, float* right, size_t count) {
        for (size_t i = 0; i < count; ++i) {
            lfo_phase_ += lfo_rate_;
            if (lfo_phase_ >= 1.0f) lfo_phase_ -= 1.0f;
            
            float lfo = lfo_phase_ > 0.5f ? 1.0f - lfo_phase_ : lfo_phase_;
            lfo = lfo * 4.0f - 1.0f;
            
            float mod_width = width_ * (1.0f + lfo * lfo_depth_);
            float mid = (left[i] + right[i]) * 0.5f;
            float side = (left[i] - right[i]) * 0.5f * mod_width;
            left[i] = mid + side;
            right[i] = mid - side;
        }
    }
#endif

private:
    float width_;
    float lfo_rate_;
    float lfo_phase_;
    float lfo_depth_;
    float sample_rate_;
};

// ============================================================================
// Utility Functions
// ============================================================================

/** @brief Convert Left/Right to Mid/Side */
inline void LRtoMS(float left, float right, float& mid, float& side) {
    mid = (left + right) * 0.5f;
    side = (left - right) * 0.5f;
}

/** @brief Convert Mid/Side to Left/Right */
inline void MStoLR(float mid, float side, float& left, float& right) {
    left = mid + side;
    right = mid - side;
}

/** @brief Apply stereo width to L/R signal (in-place, single sample) */
inline void ApplyWidth(float& left, float& right, float width) {
    float mid = (left + right) * 0.5f;
    float side = (left - right) * 0.5f * width;
    left = mid + side;
    right = mid - side;
}

#ifdef USE_NEON
/** @brief NEON: Convert 4 L/R pairs to M/S */
inline void LRtoMS4(const float* left, const float* right, float* mid, float* side) {
    float32x4_t l = vld1q_f32(left);
    float32x4_t r = vld1q_f32(right);
    float32x4_t half = vdupq_n_f32(0.5f);
    vst1q_f32(mid, vmulq_f32(vaddq_f32(l, r), half));
    vst1q_f32(side, vmulq_f32(vsubq_f32(l, r), half));
}

/** @brief NEON: Convert 4 M/S pairs to L/R */
inline void MStoLR4(const float* mid, const float* side, float* left, float* right) {
    float32x4_t m = vld1q_f32(mid);
    float32x4_t s = vld1q_f32(side);
    vst1q_f32(left, vaddq_f32(m, s));
    vst1q_f32(right, vsubq_f32(m, s));
}

/**
 * @brief NEON: Batch apply stereo width (in-place)
 * @param left Left channel buffer (in/out)
 * @param right Right channel buffer (in/out)
 * @param width Width factor (0 = mono, 1 = unchanged, >1 = wider)
 * @param count Number of samples
 */
inline void ApplyWidthBatch(float* left, float* right, float width, size_t count) {
    float32x4_t width_vec = vdupq_n_f32(width);
    float32x4_t half = vdupq_n_f32(0.5f);
    
    size_t i = 0;
    for (; i + 4 <= count; i += 4) {
        float32x4_t l = vld1q_f32(&left[i]);
        float32x4_t r = vld1q_f32(&right[i]);
        
        float32x4_t mid = vmulq_f32(vaddq_f32(l, r), half);
        float32x4_t side = vmulq_f32(vmulq_f32(vsubq_f32(l, r), half), width_vec);
        
        vst1q_f32(&left[i], vaddq_f32(mid, side));
        vst1q_f32(&right[i], vsubq_f32(mid, side));
    }
    
    for (; i < count; ++i) {
        ApplyWidth(left[i], right[i], width);
    }
}

/**
 * @brief NEON: Batch convert L/R to M/S
 */
inline void LRtoMSBatch(const float* left, const float* right, 
                        float* mid, float* side, size_t count) {
    float32x4_t half = vdupq_n_f32(0.5f);
    
    size_t i = 0;
    for (; i + 4 <= count; i += 4) {
        float32x4_t l = vld1q_f32(&left[i]);
        float32x4_t r = vld1q_f32(&right[i]);
        vst1q_f32(&mid[i], vmulq_f32(vaddq_f32(l, r), half));
        vst1q_f32(&side[i], vmulq_f32(vsubq_f32(l, r), half));
    }
    
    for (; i < count; ++i) {
        mid[i] = (left[i] + right[i]) * 0.5f;
        side[i] = (left[i] - right[i]) * 0.5f;
    }
}

/**
 * @brief NEON: Batch convert M/S to L/R
 */
inline void MStoLRBatch(const float* mid, const float* side,
                        float* left, float* right, size_t count) {
    size_t i = 0;
    for (; i + 4 <= count; i += 4) {
        float32x4_t m = vld1q_f32(&mid[i]);
        float32x4_t s = vld1q_f32(&side[i]);
        vst1q_f32(&left[i], vaddq_f32(m, s));
        vst1q_f32(&right[i], vsubq_f32(m, s));
    }
    
    for (; i < count; ++i) {
        left[i] = mid[i] + side[i];
        right[i] = mid[i] - side[i];
    }
}
#else
// Scalar fallbacks
inline void ApplyWidthBatch(float* left, float* right, float width, size_t count) {
    for (size_t i = 0; i < count; ++i) {
        ApplyWidth(left[i], right[i], width);
    }
}

inline void LRtoMSBatch(const float* left, const float* right,
                        float* mid, float* side, size_t count) {
    for (size_t i = 0; i < count; ++i) {
        mid[i] = (left[i] + right[i]) * 0.5f;
        side[i] = (left[i] - right[i]) * 0.5f;
    }
}

inline void MStoLRBatch(const float* mid, const float* side,
                        float* left, float* right, size_t count) {
    for (size_t i = 0; i < count; ++i) {
        left[i] = mid[i] + side[i];
        right[i] = mid[i] - side[i];
    }
}
#endif

} // namespace common
