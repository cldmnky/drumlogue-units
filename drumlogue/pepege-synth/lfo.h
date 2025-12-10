/**
 * @file lfo.h
 * @brief Low Frequency Oscillator for Pepege
 *
 * Simple LFO with morphable waveforms for modulation
 */

#pragma once

#include <cstdint>
#include <cmath>

/**
 * @brief LFO waveform shapes (for reference/strings)
 */
enum LFOShape {
    LFO_SINE = 0,
    LFO_TRIANGLE,
    LFO_SAW_UP,
    LFO_SAW_DOWN,
    LFO_SQUARE,
    LFO_SAMPLE_HOLD,
    LFO_NUM_SHAPES
};

/**
 * @brief Low Frequency Oscillator with morphable shapes
 */
class LFO {
public:
    LFO() {}
    ~LFO() {}
    
    void Init(float sample_rate) {
        sample_rate_ = sample_rate;
        inv_sample_rate_ = 1.0f / sample_rate;
        
        phase_ = 0.0f;
        phase_inc_ = 0.001f;
        shape_morph_ = 0.0f;
        
        // For sample & hold
        sh_value_ = 0.0f;
        prev_phase_ = 0.0f;
        
        // Simple noise state
        noise_state_ = 12345;
    }
    
    void Reset() {
        phase_ = 0.0f;
        sh_value_ = 0.0f;
        prev_phase_ = 0.0f;
    }
    
    /**
     * @brief Set LFO rate from 0-127 parameter
     * 
     * Maps to ~0.05Hz to ~20Hz exponentially
     */
    void SetRate(int32_t param) {
        const float min_freq = 0.05f;
        const float max_freq = 20.0f;
        const float norm = param / 127.0f;
        const float freq = min_freq * powf(max_freq / min_freq, norm);
        phase_inc_ = freq * inv_sample_rate_;
    }
    
    /**
     * @brief Set LFO shape morph position (0.0 to 5.0)
     * 
     * Smoothly morphs between shapes:
     * 0.0 = Sine, 1.0 = Triangle, 2.0 = Saw Up, 3.0 = Saw Down, 4.0 = Square, 5.0 = S&H
     */
    void SetShapeMorph(float morph) {
        shape_morph_ = morph;
    }
    
    /**
     * @brief Process one sample with shape morphing
     * @return LFO value (-1.0 to +1.0)
     */
    float Process() {
        // Advance phase
        prev_phase_ = phase_;
        phase_ += phase_inc_;
        if (phase_ >= 1.0f) {
            phase_ -= 1.0f;
            // Update S&H on phase wrap
            sh_value_ = RandomFloat();
        }
        
        // Calculate shape index and morph amount
        int shape_a = static_cast<int>(shape_morph_);
        if (shape_a >= LFO_NUM_SHAPES - 1) shape_a = LFO_NUM_SHAPES - 2;
        if (shape_a < 0) shape_a = 0;
        int shape_b = shape_a + 1;
        float morph_amt = shape_morph_ - static_cast<float>(shape_a);
        
        // Get values from both shapes and crossfade
        float val_a = GetShapeValue(shape_a);
        float val_b = GetShapeValue(shape_b);
        
        return val_a + (val_b - val_a) * morph_amt;
    }
    
private:
    float sample_rate_;
    float inv_sample_rate_;
    
    float phase_;
    float phase_inc_;
    float shape_morph_;
    
    // Sample & hold state
    float sh_value_;
    float prev_phase_;
    
    // Simple noise generator state
    uint32_t noise_state_;
    
    /**
     * @brief Get output value for a specific shape
     */
    float GetShapeValue(int shape) const {
        switch (shape) {
            case LFO_SINE:
                return sinf(phase_ * 6.28318530718f);
                
            case LFO_TRIANGLE:
                if (phase_ < 0.5f) {
                    return phase_ * 4.0f - 1.0f;
                } else {
                    return 3.0f - phase_ * 4.0f;
                }
                
            case LFO_SAW_UP:
                return phase_ * 2.0f - 1.0f;
                
            case LFO_SAW_DOWN:
                return 1.0f - phase_ * 2.0f;
                
            case LFO_SQUARE:
                return (phase_ < 0.5f) ? 1.0f : -1.0f;
                
            case LFO_SAMPLE_HOLD:
                return sh_value_;
                
            default:
                return 0.0f;
        }
    }
    
    /**
     * @brief Simple random float (-1 to +1)
     */
    float RandomFloat() {
        // Simple LCG
        noise_state_ = noise_state_ * 1103515245 + 12345;
        // Convert to float
        int32_t r = static_cast<int32_t>(noise_state_ >> 16) - 32768;
        return static_cast<float>(r) / 32768.0f;
    }
};
