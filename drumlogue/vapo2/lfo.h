/**
 * @file lfo.h
 * @brief Low Frequency Oscillator for Vapo2
 *
 * Simple LFO with multiple waveforms for modulation
 */

#pragma once

#include <cstdint>
#include <cmath>

/**
 * @brief LFO waveform shapes
 */
enum LFOShape {
    LFO_SINE = 0,
    LFO_TRIANGLE,
    LFO_SAW_UP,
    LFO_SAW_DOWN,
    LFO_SQUARE,
    LFO_SAMPLE_HOLD
};

/**
 * @brief Low Frequency Oscillator
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
        shape_ = LFO_SINE;
        
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
     * @brief Set LFO shape
     */
    void SetShape(LFOShape shape) {
        shape_ = shape;
    }
    
    /**
     * @brief Process one sample
     * @return LFO value (-1.0 to +1.0)
     */
    float Process() {
        // Advance phase
        prev_phase_ = phase_;
        phase_ += phase_inc_;
        if (phase_ >= 1.0f) {
            phase_ -= 1.0f;
        }
        
        float output = 0.0f;
        
        switch (shape_) {
            case LFO_SINE:
                // Sine approximation using parabola
                {
                    float p = phase_ * 2.0f - 1.0f;  // -1 to +1
                    output = 1.0f - p * p * 2.0f;    // Parabola approximation
                    // Better sine approximation
                    output = sinf(phase_ * 6.28318530718f);
                }
                break;
                
            case LFO_TRIANGLE:
                if (phase_ < 0.5f) {
                    output = phase_ * 4.0f - 1.0f;
                } else {
                    output = 3.0f - phase_ * 4.0f;
                }
                break;
                
            case LFO_SAW_UP:
                output = phase_ * 2.0f - 1.0f;
                break;
                
            case LFO_SAW_DOWN:
                output = 1.0f - phase_ * 2.0f;
                break;
                
            case LFO_SQUARE:
                output = (phase_ < 0.5f) ? 1.0f : -1.0f;
                break;
                
            case LFO_SAMPLE_HOLD:
                // Update S&H on phase wrap
                if (phase_ < prev_phase_) {
                    sh_value_ = RandomFloat();
                }
                output = sh_value_;
                break;
        }
        
        return output;
    }
    
private:
    float sample_rate_;
    float inv_sample_rate_;
    
    float phase_;
    float phase_inc_;
    LFOShape shape_;
    
    // Sample & hold state
    float sh_value_;
    float prev_phase_;
    
    // Simple noise generator state
    uint32_t noise_state_;
    
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
