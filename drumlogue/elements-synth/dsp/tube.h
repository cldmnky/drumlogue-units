/*
 * Tube - Simple waveguide tube filter for blow excitation
 * Part of Modal Synth for Drumlogue
 * 
 * Based on Mutable Instruments Elements tube.cc
 * Implements:
 * - Waveguide delay line for tube resonance
 * - Zero/pole filtering for formant character
 * - Reed-like pressure response for breath sounds
 */

#pragma once

#include "dsp_core.h"

namespace modal {

// Tube delay line size (must be power of 2 for efficient modulo)
constexpr size_t kTubeDelaySize = 2048;

// ============================================================================
// Tube - Waveguide tube for wind instrument modeling
// Creates formant-like resonances when combined with breath/noise input
// ============================================================================

class Tube {
public:
    Tube() : delay_ptr_(0), zero_state_(0.0f), pole_state_(0.0f) {
        for (size_t i = 0; i < kTubeDelaySize; ++i) {
            delay_line_[i] = 0.0f;
        }
    }
    
    void Init() {
        zero_state_ = 0.0f;
        pole_state_ = 0.0f;
        delay_ptr_ = 0;
        for (size_t i = 0; i < kTubeDelaySize; ++i) {
            delay_line_[i] = 0.0f;
        }
    }
    
    // Process with inline mixing to input_output buffer
    // frequency: Tube resonant frequency
    // envelope: Breath envelope (0-1)
    // damping: Air column damping
    // timbre: Formant/brightness control
    // input_output: Input signal (noise/breath), mixed with tube output
    // gain: Output mixing gain
    void Process(
            float frequency,
            float envelope,
            float damping,
            float timbre,
            float* input_output,
            float gain,
            size_t size) {
        
        // Calculate delay in samples from frequency
        float delay = kSampleRate / frequency;
        
        // Ensure delay fits in buffer (octave fold if needed)
        while (delay >= static_cast<float>(kTubeDelaySize)) {
            delay *= 0.5f;
        }
        
        // Split into integer and fractional parts for linear interpolation
        int delay_integral = static_cast<int>(delay);
        float delay_fractional = delay - static_cast<float>(delay_integral);
        
        // Clamp envelope
        if (envelope > 1.0f) envelope = 1.0f;
        if (envelope < 0.0f) envelope = 0.0f;
        
        // Calculate filter coefficients
        // Damping affects overall energy loss
        float damp_factor = 3.6f - damping * 1.8f;
        
        // LPF coefficient controlled by timbre (formant position)
        float lpf_coefficient = frequency / kSampleRate * (1.0f + timbre * timbre * 256.0f);
        if (lpf_coefficient > 0.995f) lpf_coefficient = 0.995f;
        if (lpf_coefficient < 0.001f) lpf_coefficient = 0.001f;
        
        int32_t d = delay_ptr_;
        
        while (size--) {
            // Read breath input
            float breath = (*input_output) * damp_factor + 0.8f;
            
            // Read from delay line with linear interpolation
            int read_idx_a = (d + delay_integral) & (kTubeDelaySize - 1);
            int read_idx_b = (d + delay_integral + 1) & (kTubeDelaySize - 1);
            float a = delay_line_[read_idx_a];
            float b = delay_line_[read_idx_b];
            float in = a + (b - a) * delay_fractional;
            
            // Zero filter (high-pass characteristic for reed)
            float pressure_delta = -0.95f * (in * envelope + zero_state_) - breath;
            zero_state_ = in;
            
            // Reed response model (simplified Bernoulli effect)
            // Creates the characteristic "reed pinch" nonlinearity
            float reed = pressure_delta * -0.2f + 0.8f;
            float out = pressure_delta * reed + breath;
            
            // Soft clipping to prevent runaway
            if (out > 5.0f) out = 5.0f;
            if (out < -5.0f) out = -5.0f;
            
            // Write to delay line (with loss)
            delay_line_[d] = out * 0.5f;
            
            // Advance delay pointer (backwards)
            --d;
            if (d < 0) {
                d = kTubeDelaySize - 1;
            }
            
            // Low-pass filter for timbre control (pole)
            pole_state_ += lpf_coefficient * (out - pole_state_);
            
            // Mix tube output with original signal
            *input_output++ += gain * envelope * pole_state_;
        }
        
        delay_ptr_ = d;
    }
    
    // Simplified single-sample process for integration with Exciter
    float Process(float input, float frequency, float envelope, float damping, float timbre) {
        // Calculate delay
        float delay = kSampleRate / Clamp(frequency, 20.0f, 8000.0f);
        while (delay >= static_cast<float>(kTubeDelaySize)) {
            delay *= 0.5f;
        }
        
        int delay_integral = static_cast<int>(delay);
        float delay_fractional = delay - static_cast<float>(delay_integral);
        
        envelope = Clamp(envelope, 0.0f, 1.0f);
        
        float damp_factor = 3.6f - damping * 1.8f;
        float lpf_coefficient = frequency / kSampleRate * (1.0f + timbre * timbre * 256.0f);
        lpf_coefficient = Clamp(lpf_coefficient, 0.001f, 0.995f);
        
        float breath = input * damp_factor + 0.8f;
        
        // Read from delay with interpolation
        int read_idx_a = (delay_ptr_ + delay_integral) & (kTubeDelaySize - 1);
        int read_idx_b = (delay_ptr_ + delay_integral + 1) & (kTubeDelaySize - 1);
        float in = delay_line_[read_idx_a] + 
                   (delay_line_[read_idx_b] - delay_line_[read_idx_a]) * delay_fractional;
        
        // Zero filter
        float pressure_delta = -0.95f * (in * envelope + zero_state_) - breath;
        zero_state_ = in;
        
        // Reed model
        float reed = pressure_delta * -0.2f + 0.8f;
        float out = pressure_delta * reed + breath;
        out = Clamp(out, -5.0f, 5.0f);
        
        // Write to delay
        delay_line_[delay_ptr_] = out * 0.5f;
        --delay_ptr_;
        if (delay_ptr_ < 0) {
            delay_ptr_ = kTubeDelaySize - 1;
        }
        
        // Pole filter
        pole_state_ += lpf_coefficient * (out - pole_state_);
        
        // Stability check - flush denormals/NaN
        if (zero_state_ != zero_state_ || zero_state_ > 1e4f || zero_state_ < -1e4f) {
            zero_state_ = 0.0f;
        }
        if (pole_state_ != pole_state_ || pole_state_ > 1e4f || pole_state_ < -1e4f) {
            pole_state_ = 0.0f;
        }
        
        return envelope * pole_state_;
    }
    
    void Reset() {
        zero_state_ = 0.0f;
        pole_state_ = 0.0f;
        for (size_t i = 0; i < kTubeDelaySize; ++i) {
            delay_line_[i] = 0.0f;
        }
    }
    
private:
    int32_t delay_ptr_;
    float zero_state_;
    float pole_state_;
    float delay_line_[kTubeDelaySize];
};

} // namespace modal
