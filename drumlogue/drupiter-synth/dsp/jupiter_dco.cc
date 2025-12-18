/**
 * @file jupiter_dco.cc
 * @brief Digital Controlled Oscillator implementation
 *
 * Based on Bristol junodco.c
 * Wavetable-based oscillator with multiple waveforms
 */

#include "jupiter_dco.h"
#include <algorithm>
#include <cmath>

#ifdef DEBUG
#include <cstdio>
#endif

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace dsp {

namespace {

// Keep oscillator safely below Nyquist (0.5), leaving headroom for BLEP transition width and processing.
// 0.48f = 0.5f - 0.02f guard for BLEP ringing and processing headroom (not for modulation overshoot, which is clamped).
constexpr float kMaxPhaseIncrement = 0.48f;
constexpr float kFmModRange = 1.0f;  // Exponential FM: fm_amount=±1 -> ±1 octave

// PolyBLEP (polynomial band-limited step) to smooth discontinuities.
// t = current phase [0,1), dt = phase increment (controls transition width).
inline float PolyBlep(float t, float dt) {
    if (dt <= 0.0f) {
        return 0.0f;
    }
    if (dt > 1.0f) {  // Should be clamped upstream; keep as safety for bad inputs.
        dt = 1.0f;
    }
    
    if (t < dt) {
        t /= dt;
        return t + t - t * t - 1.0f;
    }
    if (t > 1.0f - dt) {
        t = (t - 1.0f) / dt;
        return t * t + t + t + 1.0f;
    }
    return 0.0f;
}

inline float WrapPhase(float phase) {
    // Efficient wrapping to [0, 1) using floorf
    return phase - floorf(phase);
}

}  // namespace

// Static wavetable initialization
float JupiterDCO::ramp_table_[kWavetableSize + 1];
float JupiterDCO::square_table_[kWavetableSize + 1];
float JupiterDCO::triangle_table_[kWavetableSize + 1];
float JupiterDCO::sine_table_[kWavetableSize + 1];
bool JupiterDCO::tables_initialized_ = false;

JupiterDCO::JupiterDCO()
    : sample_rate_(48000.0f)
    , phase_(0.0f)
    , phase_inc_(0.0f)
    , base_freq_hz_(440.0f)
    , max_freq_hz_(sample_rate_ * kMaxPhaseIncrement)
    , waveform_(WAVEFORM_SAW)
    , pulse_width_(0.5f)
    , sync_enabled_(false)
    , fm_amount_(0.0f)
    , drift_phase_(0.0f)
    , drift_counter_(0)
    , current_drift_(0.0f)
    , noise_seed_(0x41594E31)  // "AYN1"
    , noise_seed2_(0x4A503842) // "JP8B"
    , last_phase_(0.0f)
{
    if (!tables_initialized_) {
        InitWavetables();
    }
}

JupiterDCO::~JupiterDCO() {
}

void JupiterDCO::Init(float sample_rate) {
    sample_rate_ = sample_rate;
    max_freq_hz_ = sample_rate_ * kMaxPhaseIncrement;
    phase_ = 0.0f;
    base_freq_hz_ = 440.0f;
    phase_inc_ = base_freq_hz_ / sample_rate_;
    fm_amount_ = 0.0f;
    drift_phase_ = 0.0f;
    drift_counter_ = 0;
    current_drift_ = 0.0f;
    noise_seed_ = 0x41594E31;
    noise_seed2_ = 0x4A503842;
}

void JupiterDCO::SetFrequency(float freq_hz) {
    float clamped = freq_hz;
    if (clamped < 0.0f) clamped = 0.0f;
    if (clamped > max_freq_hz_) clamped = max_freq_hz_;
    base_freq_hz_ = clamped;
    phase_inc_ = clamped / sample_rate_;
}

void JupiterDCO::SetWaveform(Waveform waveform) {
    waveform_ = waveform;
}

void JupiterDCO::SetPulseWidth(float pw) {
    pulse_width_ = std::max(0.01f, std::min(pw, 0.99f));
}

void JupiterDCO::SetSyncEnabled(bool enabled) {
    sync_enabled_ = enabled;
}

void JupiterDCO::ApplyFM(float fm_amount) {
    fm_amount_ = fm_amount;
}

void JupiterDCO::ResetPhase() {
    phase_ = 0.0f;
}

float JupiterDCO::Process() {
    // Apply FM modulation to phase increment
    float current_phase_inc = phase_inc_;
    if (fm_amount_ != 0.0f) {
        // Exponential FM: map fm_amount to octave-scaled multiplier
        float fm_scale = exp2f(fm_amount_ * kFmModRange);
        current_phase_inc *= fm_scale;
    }
    
    // Analog-style slow drift: Update drift amount at a slow rate (~100 Hz)
    // to avoid rapid pitch wobbling that confuses the tuner
    if (++drift_counter_ >= 480) {  // Update every ~10ms at 48kHz
        drift_counter_ = 0;
        drift_phase_ += 0.01f;  // ~1 Hz drift LFO
        if (drift_phase_ >= 1.0f) {
            drift_phase_ -= 1.0f;
        }
        noise_seed_ = noise_seed_ * 1664525u + 1013904223u;
        float noise = ((noise_seed_ >> 9) & 0x7FFFFF) / float(0x7FFFFF) - 0.5f;
        // Reduced drift: ~±0.005% (was ±0.05%) for better tuning stability
        current_drift_ = 0.00003f * sinf(drift_phase_ * 2.0f * static_cast<float>(M_PI)) + 0.00002f * noise;
    }
    current_phase_inc *= (1.0f + current_drift_);
    
    
    // Protect against aliasing when FM tries to push past Nyquist.
    current_phase_inc = std::max(0.0f, std::min(current_phase_inc, kMaxPhaseIncrement));
    
    // Store last phase for sync detection
    last_phase_ = phase_;
    
    // Generate waveform from current phase (pre-increment)
    float sample = GenerateWaveform(phase_, current_phase_inc);
    
    // Advance phase
    phase_ += current_phase_inc;
    if (phase_ >= 1.0f) {
        phase_ -= 1.0f;
    }
    
    return sample;
}

bool JupiterDCO::DidWrap() const {
    // Detect if phase wrapped (for syncing slave oscillator)
    return phase_ < last_phase_;
}

float JupiterDCO::GenerateWaveform(float phase, float phase_inc) {
    const float dt = std::min(phase_inc, 1.0f);
    
    switch (waveform_) {
        case WAVEFORM_SAW:
        {
            float value = LookupWavetable(ramp_table_, phase);
            value -= PolyBlep(phase, dt);
            return value;
        }
        
        case WAVEFORM_SQUARE:
        {
            float value = LookupWavetable(square_table_, phase);
            value += PolyBlep(phase, dt);                           // rising edge at 0
            value -= PolyBlep(WrapPhase(phase + 0.5f), dt);         // falling edge at 0.5
            return value;
        }
        
        case WAVEFORM_PULSE: {
            // Comparator-style PWM for Jupiter character
            float value = (phase < pulse_width_) ? 1.0f : -1.0f;
            value += PolyBlep(phase, dt);                            // rising edge at reset
            value -= PolyBlep(WrapPhase(phase + (1.0f - pulse_width_)), dt);  // falling edge at PW
            return value;
        }
        
        case WAVEFORM_TRIANGLE:
            return LookupWavetable(triangle_table_, phase);

        case WAVEFORM_SINE:
            // Use wavetable for fast sine (avoids expensive sinf() call)
            return LookupWavetable(sine_table_, phase);

        case WAVEFORM_NOISE:
            // Simple white noise (VCO2 on JP-8 has NOISE instead of SQUARE).
            noise_seed2_ = noise_seed2_ * 1664525u + 1013904223u;
            return ((noise_seed2_ >> 9) & 0x7FFFFF) / float(0x7FFFFF) * 2.0f - 1.0f;
        
        default:
            return 0.0f;
    }
}

float JupiterDCO::LookupWavetable(const float* table, float phase) {
    // Linear interpolation
    float table_pos = phase * kWavetableSize;
    int index = static_cast<int>(table_pos);
    float frac = table_pos - index;
    
    // Interpolate between samples
    return table[index] + (table[index + 1] - table[index]) * frac;
}

void JupiterDCO::InitWavetables() {
    for (int i = 0; i <= kWavetableSize; ++i) {
        float phase = static_cast<float>(i) / kWavetableSize;
        
        // Ramp/Sawtooth: Jupiter-8 style descending saw (+1 to -1)
        // This matches the Roland/Jupiter sawtooth direction
        ramp_table_[i] = 1.0f - phase * 2.0f;
        
        // Square wave: perfect symmetry for zero DC offset
        // Must be exactly ±1.0 to avoid pitch drift
        square_table_[i] = (phase < 0.5f) ? 1.0f : -1.0f;
        
        // Triangle wave: -1 to +1 to -1
        // Bristol uses this same approach
        if (phase < 0.5f) {
            triangle_table_[i] = phase * 4.0f - 1.0f;
        } else {
            triangle_table_[i] = 3.0f - phase * 4.0f;
        }
        
        // Sine wave: Critical optimization - avoids sinf() in Process()
        // Pre-compute sin(2*PI*phase) for all table entries
        sine_table_[i] = sinf(phase * 2.0f * static_cast<float>(M_PI));
    }
    
    tables_initialized_ = true;
}

} // namespace dsp
