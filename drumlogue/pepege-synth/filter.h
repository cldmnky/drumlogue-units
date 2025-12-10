/**
 * @file filter.h
 * @brief State Variable Filter for Pepege
 *
 * 12dB/oct multimode filter (LP, HP, BP) with optional 24dB cascade
 */

#pragma once

#include <cstdint>
#include <cmath>

/**
 * @brief Filter types
 */
enum FilterType {
    FILTER_LP12 = 0,
    FILTER_LP24,
    FILTER_HP12,
    FILTER_BP12
};

/**
 * @brief State Variable Filter (Chamberlin topology)
 *
 * Efficient and stable 12dB/oct multimode filter
 */
class SVFilter {
public:
    SVFilter() {}
    ~SVFilter() {}
    
    void Init(float sample_rate) {
        sample_rate_ = sample_rate;
        inv_sample_rate_ = 1.0f / sample_rate;
        
        cutoff_ = 1.0f;
        resonance_ = 0.0f;
        type_ = FILTER_LP12;
        
        // State variables
        low_ = 0.0f;
        band_ = 0.0f;
        
        // For 24dB mode
        low2_ = 0.0f;
        band2_ = 0.0f;
        
        // Coefficient
        f_ = 1.0f;
        q_ = 0.5f;
    }
    
    void Reset() {
        low_ = 0.0f;
        band_ = 0.0f;
        low2_ = 0.0f;
        band2_ = 0.0f;
    }
    
    /**
     * @brief Set cutoff frequency (0.0 to 1.0)
     */
    void SetCutoff(float cutoff) {
        cutoff_ = cutoff;
        UpdateCoefficients();
    }
    
    /**
     * @brief Set resonance (0.0 to 1.0)
     */
    void SetResonance(float resonance) {
        resonance_ = resonance;
        UpdateCoefficients();
    }
    
    /**
     * @brief Set filter type
     */
    void SetType(FilterType type) {
        type_ = type;
    }
    
    /**
     * @brief Process one sample
     */
    float Process(float input) {
        // First stage
        const float high = input - low_ - q_ * band_;
        band_ += f_ * high;
        low_ += f_ * band_;
        
        float output;
        
        switch (type_) {
            case FILTER_LP12:
                output = low_;
                break;
                
            case FILTER_LP24:
                // Second stage for 24dB
                {
                    const float high2 = low_ - low2_ - q_ * band2_;
                    band2_ += f_ * high2;
                    low2_ += f_ * band2_;
                    output = low2_;
                }
                break;
                
            case FILTER_HP12:
                output = high;
                break;
                
            case FILTER_BP12:
                output = band_;
                break;
                
            default:
                output = low_;
        }
        
        return output;
    }
    
private:
    float sample_rate_;
    float inv_sample_rate_;
    
    float cutoff_;
    float resonance_;
    FilterType type_;
    
    // State variables (stage 1)
    float low_;
    float band_;
    
    // State variables (stage 2 for 24dB)
    float low2_;
    float band2_;
    
    // Coefficients
    float f_;   // Frequency coefficient
    float q_;   // Resonance/damping
    
    void UpdateCoefficients() {
        // Convert normalized cutoff (0-1) to frequency coefficient
        // Using approximation: f = 2 * sin(pi * fc / fs)
        // For stability at high frequencies, clamp
        const float fc = 20.0f + cutoff_ * cutoff_ * 20000.0f;  // Exponential mapping
        f_ = 2.0f * sinf(3.14159265f * fc * inv_sample_rate_);
        if (f_ > 0.99f) f_ = 0.99f;
        
        // Q from resonance (0 to self-oscillation)
        // q_ should be 0.5 (no resonance) to ~0.01 (high resonance)
        q_ = 0.5f - resonance_ * 0.48f;
        if (q_ < 0.02f) q_ = 0.02f;
    }
};
