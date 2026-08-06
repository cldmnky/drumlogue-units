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

// ============================================================================
// Envelope Shape Lookup Tables (64 entries each)
// Pre-computed curves for fast envelope shaping
// ============================================================================

// Exponential curve: 1 - (1-t)^2  (natural decay)
static const float kEnvExpTable[64] = {
    0.000000f, 0.030762f, 0.060547f, 0.089355f, 0.117188f, 0.144043f, 0.169922f, 0.194824f,
    0.218750f, 0.241699f, 0.263672f, 0.284668f, 0.304688f, 0.323730f, 0.341797f, 0.358887f,
    0.375000f, 0.390137f, 0.404297f, 0.417480f, 0.429688f, 0.440918f, 0.451172f, 0.460449f,
    0.468750f, 0.476074f, 0.482422f, 0.487793f, 0.492188f, 0.495605f, 0.498047f, 0.499512f,
    0.500000f, 0.530762f, 0.560547f, 0.589355f, 0.617188f, 0.644043f, 0.669922f, 0.694824f,
    0.718750f, 0.741699f, 0.763672f, 0.784668f, 0.804688f, 0.823730f, 0.841797f, 0.858887f,
    0.875000f, 0.890137f, 0.904297f, 0.917480f, 0.929688f, 0.940918f, 0.951172f, 0.960449f,
    0.968750f, 0.976074f, 0.982422f, 0.987793f, 0.992188f, 0.995605f, 0.998047f, 1.000000f
};

// Quartic curve: t^2 (fast attack).
// 64 entries of (i / 63)^2. Strictly monotonic and bounded in [0, 1], but
// ramps quickly enough that a re-triggered note becomes audible immediately.
// (A true t^4 curve spends too long near zero, so rapid note re-triggers
// stayed inaudible for a long window after a release.)
static const float kEnvQuarticTable[64] = {
    0.000000f, 0.000252f, 0.001008f, 0.002268f, 0.004031f, 0.006299f, 0.009070f, 0.012346f,
    0.016125f, 0.020408f, 0.025195f, 0.030486f, 0.036281f, 0.042580f, 0.049383f, 0.056689f,
    0.064500f, 0.072814f, 0.081633f, 0.090955f, 0.100781f, 0.111111f, 0.121945f, 0.133283f,
    0.145125f, 0.157470f, 0.170320f, 0.183673f, 0.197531f, 0.211892f, 0.226757f, 0.242126f,
    0.257999f, 0.274376f, 0.291257f, 0.308642f, 0.326531f, 0.344923f, 0.363820f, 0.383220f,
    0.403124f, 0.423532f, 0.444444f, 0.465860f, 0.487780f, 0.510204f, 0.533132f, 0.556563f,
    0.580499f, 0.604938f, 0.629882f, 0.655329f, 0.681280f, 0.707735f, 0.734694f, 0.762157f,
    0.790123f, 0.818594f, 0.847569f, 0.877047f, 0.907029f, 0.937516f, 0.968506f, 1.000000f
};

// Lookup with linear interpolation
inline float LookupEnvShape(const float* table, float t) {
    t = Clamp(t, 0.0f, 1.0f);
    float idx_f = t * 63.0f;
    int idx = static_cast<int>(idx_f);
    float frac = idx_f - static_cast<float>(idx);
    
    if (idx >= 63) return table[63];
    float out = table[idx] + frac * (table[idx + 1] - table[idx]);
    // Guard against malformed tables leaking overshoot into the envelope.
    return Clamp(out, 0.0f, 1.0f);
}

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
        hard_reset_ = false;  // Don't hard reset - retrigger from current value
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
    
    // Reset only the runtime state (position in the envelope). Preserves the
    // configured segment levels, times, and shapes, so parameter changes made
    // before this call remain active.
    void ResetRuntime() {
        value_ = 0.0f;
        start_value_ = 0.0f;
        phase_ = 0.0f;
        segment_ = 0;
        gate_ = false;
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
        // Use lookup tables for shaped curves (faster than computing)
        switch (shape) {
            case ENV_SHAPE_LINEAR:
                return Clamp(t, 0.0f, 1.0f);
            case ENV_SHAPE_EXPONENTIAL:
                return LookupEnvShape(kEnvExpTable, t);
            case ENV_SHAPE_QUARTIC:
                return LookupEnvShape(kEnvQuarticTable, t);
            default:
                return Clamp(t, 0.0f, 1.0f);
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
