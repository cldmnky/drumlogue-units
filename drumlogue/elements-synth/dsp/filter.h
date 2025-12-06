/*
 * Moog Ladder Filter - 4-pole resonant low-pass
 * Part of Modal Synth for Drumlogue
 */

#pragma once

#include "dsp_core.h"

namespace modal {

// ============================================================================
// 4-Pole Moog Ladder Filter
// ============================================================================

class MoogLadder {
public:
    MoogLadder() {
        Reset();
    }
    
    void Reset() {
        for (int i = 0; i < 4; ++i) stage_[i] = 0.0f;
        delay_[0] = delay_[1] = delay_[2] = delay_[3] = 0.0f;
    }
    
    void SetCutoff(float freq) {
        freq = Clamp(freq, 20.0f, kSampleRate * 0.45f);
        
        // Simplified Moog filter cutoff calculation
        float fc = freq / kSampleRate;
        g_ = 0.9892f * fc - 0.4324f * fc * fc - 0.1381f * fc * fc * fc + 0.0202f * fc * fc * fc * fc;
    }
    
    void SetResonance(float res) {
        res_ = Clamp(res, 0.0f, 1.0f) * 4.0f;  // 0 to 4 for self-oscillation threshold
    }
    
    float Process(float input) {
        // Protect against NaN input
        if (input != input) input = 0.0f;
        
        // Apply resonance (feedback from output to input)
        float feedback = res_ * delay_[3];
        float in = input - feedback;
        
        // Soft clip input to prevent runaway
        in = FastTanh(in);
        
        // 4 cascaded one-pole filters
        for (int i = 0; i < 4; ++i) {
            float new_stage = (in - stage_[i]) * g_ + stage_[i];
            delay_[i] = stage_[i];
            stage_[i] = new_stage;
            in = new_stage;
        }
        
        // Stability check (NaN != NaN trick)
        if (stage_[3] != stage_[3] || stage_[3] > 1e4f || stage_[3] < -1e4f) {
            Reset();
            return 0.0f;
        }
        
        return stage_[3];
    }
    
private:
    float stage_[4];
    float delay_[4];
    float g_ = 0.0f;
    float res_ = 0.0f;
};

} // namespace modal
