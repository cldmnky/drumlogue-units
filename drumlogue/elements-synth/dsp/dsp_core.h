/*
 * DSP Core - Basic building blocks for audio processing
 * Part of Modal Synth for Drumlogue
 */

#pragma once

#include <cstdint>
#include <cmath>

namespace modal {

// Configuration
constexpr int kNumModes = 8;  // Balance between richness and stability
constexpr float kSampleRate = 48000.0f;
constexpr float kTwoPi = 6.283185307179586f;
constexpr float kPi = 3.14159265358979f;

// ============================================================================
// Utility functions
// ============================================================================

inline float Clamp(float x, float lo, float hi) {
    return x < lo ? lo : (x > hi ? hi : x);
}

inline float SemitonesToRatio(float semitones) {
    return std::pow(2.0f, semitones / 12.0f);
}

inline float MidiToFrequency(float note) {
    return 440.0f * std::pow(2.0f, (note - 69.0f) / 12.0f);
}

// Fast tanh approximation
inline float FastTanh(float x) {
    float x2 = x * x;
    return x * (27.0f + x2) / (27.0f + 9.0f * x2);
}

// ============================================================================
// Noise Generator (xorshift)
// ============================================================================

class Noise {
public:
    Noise() : state_(12345) {}
    
    void Seed(uint32_t seed) { state_ = seed ? seed : 1; }
    
    float Next() {
        state_ ^= state_ << 13;
        state_ ^= state_ >> 17;
        state_ ^= state_ << 5;
        return static_cast<float>(static_cast<int32_t>(state_)) * (1.0f / 2147483648.0f);
    }
    
    // Filtered noise for smoother modulation
    float NextFiltered(float coeff) {
        float raw = Next();
        filtered_ = filtered_ * coeff + raw * (1.0f - coeff);
        return filtered_;
    }
    
private:
    uint32_t state_;
    float filtered_ = 0.0f;
};

// ============================================================================
// One-Pole Filter (for smoothing and simple filtering)
// ============================================================================

class OnePole {
public:
    OnePole() : state_(0.0f), coeff_(0.99f) {}
    
    void SetCoefficient(float c) { coeff_ = Clamp(c, 0.0f, 0.9999f); }
    void SetFrequency(float freq) {
        float w = kTwoPi * freq / kSampleRate;
        coeff_ = std::exp(-w);
    }
    
    float Process(float in) {
        // Protect against NaN input
        if (in != in) in = 0.0f;
        
        state_ = in + (state_ - in) * coeff_;
        // Stability check (NaN != NaN trick)
        if (state_ != state_) {
            state_ = 0.0f;
        }
        return state_;
    }
    
    float ProcessHighPass(float in) {
        // Protect against NaN input
        if (in != in) in = 0.0f;
        
        state_ = in + (state_ - in) * coeff_;
        // Stability check (NaN != NaN trick)
        if (state_ != state_) {
            state_ = 0.0f;
            return 0.0f;
        }
        return in - state_;
    }
    
    void Reset() { state_ = 0.0f; }
    float state() const { return state_; }
    
private:
    float state_;
    float coeff_;
};

// ============================================================================
// State Variable Filter (for exciter filtering)
// ============================================================================

class SVF {
public:
    SVF() : lp_(0), bp_(0), hp_(0), g_(0.1f), r_(1.0f) {}
    
    void SetFrequency(float freq) {
        freq = Clamp(freq, 20.0f, kSampleRate * 0.4f);  // More conservative limit
        float w = kPi * freq / kSampleRate;
        // Clamp to avoid tan() blowing up
        if (w > 1.5f) w = 1.5f;
        g_ = std::tan(w);
        // Safety clamp g_
        if (g_ > 10.0f) g_ = 10.0f;
        if (g_ < 0.001f) g_ = 0.001f;
    }
    
    void SetResonance(float q) {
        q = Clamp(q, 0.5f, 20.0f);
        r_ = 1.0f / q;
    }
    
    float ProcessLowPass(float in) {
        // Protect against NaN input
        if (in != in) in = 0.0f;
        
        hp_ = (in - lp_ - r_ * bp_) / (1.0f + g_ * (g_ + r_));
        bp_ += g_ * hp_;
        lp_ += g_ * bp_;
        // Stability check (NaN != NaN trick)
        if (lp_ != lp_ || lp_ > 1e4f || lp_ < -1e4f) {
            Reset();
            return 0.0f;
        }
        return lp_;
    }
    
    float ProcessBandPass(float in) {
        // Protect against NaN input
        if (in != in) in = 0.0f;
        
        hp_ = (in - lp_ - r_ * bp_) / (1.0f + g_ * (g_ + r_));
        bp_ += g_ * hp_;
        lp_ += g_ * bp_;
        // Stability check (NaN != NaN trick)
        if (bp_ != bp_ || bp_ > 1e4f || bp_ < -1e4f) {
            Reset();
            return 0.0f;
        }
        return bp_;
    }
    
    void Reset() { lp_ = bp_ = hp_ = 0.0f; }
    
private:
    float lp_, bp_, hp_;
    float g_, r_;
};

} // namespace modal
