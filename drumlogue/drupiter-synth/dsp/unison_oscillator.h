/**
 * @file unison_oscillator.h
 * @brief Unison oscillator with golden ratio detune spread
 * 
 * Creates supersaw/hoover effect by stacking multiple detuned oscillators.
 * Uses golden ratio (φ = 1.618...) for natural, non-beating detune distribution.
 * 
 * @author Drupiter Hoover Synthesis Team
 * @date 2025-12-25
 * @version 2.0.0-dev
 */

#pragma once

#include <cstdint>
#include <cmath>
#include "jupiter_dco.h"

// Enable USE_NEON for ARM NEON optimizations
#ifdef __ARM_NEON
#define USE_NEON 1
#include <arm_neon.h>
#endif

namespace dsp {

/**
 * @brief Unison oscillator for hoover/supersaw sounds
 * 
 * Stacks multiple JupiterDCO oscillators with golden ratio detune spread.
 * The golden ratio (φ ≈ 1.618) creates natural, non-periodic detuning
 * that avoids beating and phasing artifacts.
 * 
 * Detune pattern (for 7 voices):
 *   Voice 0: Center (no detune)
 *   Voice 1: +detune * φ^0
 *   Voice 2: -detune * φ^0
 *   Voice 3: +detune * φ^1
 *   Voice 4: -detune * φ^1
 *   Voice 5: +detune * φ^2
 *   Voice 6: -detune * φ^2
 * 
 * This creates a rich, non-beating chorus effect ideal for hoover sounds.
 */
class UnisonOscillator {
public:
    static constexpr uint8_t kMaxVoices = 7;
    static constexpr float kGoldenRatio = 1.618033988749895f;
    
    /**
     * @brief Constructor
     */
    UnisonOscillator();
    
    /**
     * @brief Destructor
     */
    ~UnisonOscillator();
    
    /**
     * @brief Initialize unison oscillator
     * @param sample_rate Sample rate in Hz
     * @param num_voices Number of voices (3-7, must be odd for center voice)
     */
    void Init(float sample_rate, uint8_t num_voices = 7);
    
    /**
     * @brief Set base frequency for all voices
     * @param freq_hz Frequency in Hz (center pitch)
     */
    void SetFrequency(float freq_hz);
    
    /**
     * @brief Set waveform for all voices
     * @param waveform Waveform type
     */
    void SetWaveform(JupiterDCO::Waveform waveform);
    
    /**
     * @brief Set pulse width for all voices
     * @param pw Pulse width 0.0-1.0
     */
    void SetPulseWidth(float pw);
    
    /**
     * @brief Set detune amount
     * @param detune_cents Maximum detune in cents (typical: 5-20 cents)
     */
    void SetDetune(float detune_cents);
    
    /**
     * @brief Set stereo spread amount
     * @param spread 0.0-1.0 (0=mono, 1=full stereo)
     */
    void SetStereoSpread(float spread);
    
    /**
     * @brief Process one sample (stereo output)
     * @param left Output left channel
     * @param right Output right channel
     */
    inline void Process(float* left, float* right) {
#if defined(__ARM_NEON) && defined(USE_NEON)
        // NEON-optimized path: Process 4 oscillators at a time
        float samples[kMaxVoices];
        
        // Generate samples from all oscillators
        for (uint8_t i = 0; i < num_voices_; i++) {
            samples[i] = oscillators_[i].Process();
        }
        
        // NEON vectorized panning and mixing
        float32x4_t left_sum = vdupq_n_f32(0.0f);
        float32x4_t right_sum = vdupq_n_f32(0.0f);
        float32x4_t half = vdupq_n_f32(0.5f);
        float32x4_t spread = vdupq_n_f32(stereo_spread_);
        
        // Process 4 voices at a time
        uint8_t i = 0;
        for (; i + 4 <= num_voices_; i += 4) {
            // Load samples and pans
            float32x4_t samp = vld1q_f32(&samples[i]);
            float32x4_t pan = vld1q_f32(&voice_pans_[i]);
            
            // Apply stereo spread: pan *= stereo_spread_
            pan = vmulq_f32(pan, spread);
            
            // Calculate pan coefficients:
            // pan_left = (1.0 - pan) * 0.5 + 0.5
            // pan_right = pan * 0.5 + 0.5
            float32x4_t one_minus_pan = vsubq_f32(vdupq_n_f32(1.0f), pan);
            float32x4_t pan_left = vmlaq_f32(half, one_minus_pan, half);
            float32x4_t pan_right = vmlaq_f32(half, pan, half);
            
            // Apply panning and accumulate
            left_sum = vmlaq_f32(left_sum, samp, pan_left);
            right_sum = vmlaq_f32(right_sum, samp, pan_right);
        }
        
        // Horizontal add (sum all 4 lanes)
        float32x2_t left_pair = vadd_f32(vget_low_f32(left_sum), vget_high_f32(left_sum));
        float32x2_t right_pair = vadd_f32(vget_low_f32(right_sum), vget_high_f32(right_sum));
        *left = vget_lane_f32(vpadd_f32(left_pair, left_pair), 0);
        *right = vget_lane_f32(vpadd_f32(right_pair, right_pair), 0);
        
        // Process remaining voices (scalar)
        for (; i < num_voices_; i++) {
            float pan = voice_pans_[i] * stereo_spread_;
            float pan_left = (1.0f - pan) * 0.5f + 0.5f;
            float pan_right = pan * 0.5f + 0.5f;
            
            *left += samples[i] * pan_left;
            *right += samples[i] * pan_right;
        }
#else
        // Scalar fallback path
        *left = 0.0f;
        *right = 0.0f;
        
        // Mix all voices with pan positioning
        for (uint8_t i = 0; i < num_voices_; i++) {
            float sample = oscillators_[i].Process();
            
            // Pan based on voice index (golden ratio spiral for natural spread)
            float pan = voice_pans_[i] * stereo_spread_;
            float pan_left = (1.0f - pan) * 0.5f + 0.5f;
            float pan_right = pan * 0.5f + 0.5f;
            
            *left += sample * pan_left;
            *right += sample * pan_right;
        }
#endif
        
        // Scale by voice count to prevent clipping (same for both paths)
        float scale = 1.0f / sqrtf(static_cast<float>(num_voices_));
        *left *= scale;
        *right *= scale;
    }
    
    /**
     * @brief Reset oscillator phases (for hard sync)
     */
    void Reset();
    
    /**
     * @brief Get number of active voices
     */
    uint8_t GetNumVoices() const { return num_voices_; }
    
private:
    JupiterDCO oscillators_[kMaxVoices];  // Voice pool
    float voice_detunes_[kMaxVoices];     // Detune multipliers
    float voice_pans_[kMaxVoices];        // Pan positions (-1 to +1)
    
    uint8_t num_voices_;                  // Active voice count (3-7)
    float sample_rate_;                   // Sample rate
    float base_freq_;                     // Center frequency
    float detune_cents_;                  // Detune amount in cents
    float stereo_spread_;                 // Stereo width (0-1)
    
    /**
     * @brief Calculate detune ratios using golden ratio spread
     */
    void CalculateDetuneRatios();
    
    /**
     * @brief Calculate pan positions using golden ratio spiral
     */
    void CalculatePanPositions();
    
    /**
     * @brief Convert cents to frequency ratio
     * @param cents Detune in cents
     * @return Frequency ratio (1.0 = no detune)
     */
    static inline float CentsToRatio(float cents) {
        // 2^(cents/1200)
        return powf(2.0f, cents / 1200.0f);
    }
};

}  // namespace dsp
