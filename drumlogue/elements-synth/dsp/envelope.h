/*
 * Multistage Envelope - Inspired by Mutable Instruments Elements
 * Supports ADSR, AD, AR, and looping modes with shaped segments
 * Part of Modal Synth for Drumlogue
 */

#pragma once

#include <algorithm>
#include "dsp_core.h"

namespace modal {

enum EnvelopeShape {
    ENV_SHAPE_LINEAR,
    ENV_SHAPE_EXPONENTIAL,
    ENV_SHAPE_QUARTIC
};

class MultistageEnvelope {
public:
    static constexpr int kMaxSegments = 6;
    
    MultistageEnvelope() {
        Init();
    }
    
    void Init() {
        value_ = 0.0f;
        start_value_ = 0.0f;
        phase_ = 0.0f;
        segment_ = 0;
        num_segments_ = 3;
        sustain_point_ = 2;
        loop_start_ = 0;
        loop_end_ = 0;
        hard_reset_ = true;
        gate_ = false;
        
        // Default ADSR
        SetADSR(0.001f, 0.1f, 0.7f, 0.3f);
    }
    
    void SetADSR(float attack, float decay, float sustain, float release) {
        num_segments_ = 3;
        sustain_point_ = 2;
        loop_start_ = loop_end_ = 0;
        
        level_[0] = 0.0f;
        level_[1] = 1.0f;
        level_[2] = sustain;
        level_[3] = 0.0f;
        
        // Convert times to phase increments
        time_[0] = TimeToIncrement(attack);
        time_[1] = TimeToIncrement(decay);
        time_[2] = TimeToIncrement(release);
        
        shape_[0] = ENV_SHAPE_QUARTIC;      // Fast attack curve
        shape_[1] = ENV_SHAPE_EXPONENTIAL;  // Natural decay
        shape_[2] = ENV_SHAPE_EXPONENTIAL;  // Natural release
    }
    
    void SetAD(float attack, float decay) {
        num_segments_ = 2;
        sustain_point_ = 0;  // No sustain
        loop_start_ = loop_end_ = 0;
        
        level_[0] = 0.0f;
        level_[1] = 1.0f;
        level_[2] = 0.0f;
        
        time_[0] = TimeToIncrement(attack);
        time_[1] = TimeToIncrement(decay);
        
        shape_[0] = ENV_SHAPE_LINEAR;
        shape_[1] = ENV_SHAPE_EXPONENTIAL;
    }
    
    void SetAR(float attack, float release) {
        num_segments_ = 2;
        sustain_point_ = 1;  // Hold at peak until gate off
        loop_start_ = loop_end_ = 0;
        
        level_[0] = 0.0f;
        level_[1] = 1.0f;
        level_[2] = 0.0f;
        
        time_[0] = TimeToIncrement(attack);
        time_[1] = TimeToIncrement(release);
        
        shape_[0] = ENV_SHAPE_LINEAR;
        shape_[1] = ENV_SHAPE_EXPONENTIAL;
    }
    
    void SetADLoop(float attack, float decay) {
        num_segments_ = 2;
        sustain_point_ = 0;
        loop_start_ = 0;
        loop_end_ = 2;  // Loop entire envelope
        
        level_[0] = 0.0f;
        level_[1] = 1.0f;
        level_[2] = 0.0f;
        
        time_[0] = TimeToIncrement(attack);
        time_[1] = TimeToIncrement(decay);
        
        shape_[0] = ENV_SHAPE_LINEAR;
        shape_[1] = ENV_SHAPE_LINEAR;
    }
    
    // Individual parameter setters
    void SetAttack(float t) { time_[0] = TimeToIncrement(t); }
    void SetDecay(float t) { time_[1] = TimeToIncrement(t); }
    void SetSustain(float s) { level_[2] = Clamp(s, 0.0f, 1.0f); }
    void SetRelease(float t) { time_[2] = TimeToIncrement(t); }
    
    void Trigger() {
        start_value_ = hard_reset_ ? level_[0] : value_;
        segment_ = 0;
        phase_ = 0.0f;
        gate_ = true;
    }
    
    void Gate(bool on) {
        if (on && !gate_) {
            Trigger();
        } else if (!on && gate_) {
            Release();
        }
        gate_ = on;
    }
    
    void Release() {
        if (sustain_point_ > 0 && segment_ < sustain_point_) {
            // Jump to release segment
            start_value_ = value_;
            segment_ = sustain_point_;
            phase_ = 0.0f;
        }
        gate_ = false;
    }
    
    float Process() {
        // Check if we've finished the current segment
        if (phase_ >= 1.0f) {
            start_value_ = level_[segment_ + 1];
            segment_++;
            phase_ = 0.0f;
            
            // Handle looping
            if (loop_end_ > 0 && segment_ >= loop_end_) {
                segment_ = loop_start_;
            }
        }
        
        bool done = segment_ >= num_segments_;
        bool sustained = (sustain_point_ > 0) && 
                        (segment_ == sustain_point_) && 
                        gate_;
        
        if (!sustained && !done) {
            phase_ += time_[segment_];
        }
        
        if (done) {
            value_ = level_[num_segments_];
        } else {
            // Apply envelope shape
            float t = ApplyShape(phase_, shape_[segment_]);
            value_ = start_value_ + (level_[segment_ + 1] - start_value_) * t;
        }
        
        return value_;
    }
    
    bool IsActive() const { return segment_ < num_segments_ || value_ > 0.001f; }
    float value() const { return value_; }
    
private:
    float TimeToIncrement(float time_seconds) {
        // Minimum time of 0.1ms, convert to phase increment per sample
        float t = std::max(0.0001f, time_seconds);
        return 1.0f / (t * kSampleRate);
    }
    
    float ApplyShape(float t, EnvelopeShape shape) {
        t = Clamp(t, 0.0f, 1.0f);
        switch (shape) {
            case ENV_SHAPE_LINEAR:
                return t;
            case ENV_SHAPE_EXPONENTIAL:
                // Attempt to approximate exponential curve
                return 1.0f - (1.0f - t) * (1.0f - t);
            case ENV_SHAPE_QUARTIC:
                // Fast attack curve (x^4 for steeper rise)
                return t * t * t * t;
            default:
                return t;
        }
    }
    
    float level_[kMaxSegments + 1];
    float time_[kMaxSegments];
    EnvelopeShape shape_[kMaxSegments];
    
    float value_;
    float start_value_;
    float phase_;
    int segment_;
    int num_segments_;
    int sustain_point_;
    int loop_start_;
    int loop_end_;
    bool hard_reset_;
    bool gate_;
};

} // namespace modal
