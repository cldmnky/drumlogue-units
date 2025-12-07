/*
 * Simple TPT One-Pole Lowpass Filter
 * Part of Modal Synth for Drumlogue
 * 
 * Using a simple one-pole instead of Moog ladder for stability at high frequencies
 */

#pragma once

#include "dsp_core.h"

namespace modal {

// ============================================================================
// Simple One-Pole Lowpass Filter (stable for all frequencies)
// ============================================================================

class MoogLadder {
public:
    MoogLadder() {
        Reset();
    }
    
    void Reset() {
        state_ = 0.0f;
        g_ = 0.0f;
        g_target_ = 0.0f;
        initialized_ = false;
    }
    
    void SetCutoff(float freq) {
        // Clamp to safe range
        freq = Clamp(freq, 20.0f, kSampleRate * 0.49f);
        
        // Use fast tangent approximation for filter coefficient
        // g = tan(π * fc) where fc = freq / sample_rate
        float fc = freq / kSampleRate;
        g_target_ = FastTan(fc);
        
        // On first cutoff set, initialize g_ immediately (no smoothing)
        if (!initialized_) {
            g_ = g_target_;
            initialized_ = true;
        }
    }
    
    void SetResonance(float res) {
        // Not used in simple one-pole, but kept for API compatibility
        res_ = Clamp(res, 0.0f, 1.0f);
    }
    
    float Process(float input) {
        // Protect against NaN input
        if (input != input) input = 0.0f;
        
        // Smooth cutoff coefficient (one-pole lowpass) - prevents zipper noise
        g_ += (g_target_ - g_) * 0.005f;  // Faster smoothing
        
        // Pre-compute G = g/(1+g) for TPT stability
        float G = g_ / (1.0f + g_);
        
        // TPT one-pole lowpass: v = (in - state) * G, out = state + v
        float v = (input - state_) * G;
        float output = state_ + v;
        state_ = output + v;  // state = state + 2*v
        
        // Stability check
        if (state_ != state_ || state_ > 1e6f || state_ < -1e6f) {
            state_ = 0.0f;
            return 0.0f;
        }
        
        return output;
    }
    
private:
    float state_ = 0.0f;
    float g_ = 0.0f;
    float g_target_ = 0.0f;
    float res_ = 0.0f;  // Not used but kept for API
    bool initialized_ = false;
};

} // namespace modal
