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

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace dsp {

// Static wavetable initialization
float JupiterDCO::ramp_table_[kWavetableSize + 1];
float JupiterDCO::square_table_[kWavetableSize + 1];
float JupiterDCO::triangle_table_[kWavetableSize + 1];
bool JupiterDCO::tables_initialized_ = false;

JupiterDCO::JupiterDCO()
    : sample_rate_(48000.0f)
    , phase_(0.0f)
    , phase_inc_(0.0f)
    , base_freq_hz_(440.0f)
    , waveform_(WAVEFORM_RAMP)
    , pulse_width_(0.5f)
    , sync_enabled_(false)
    , fm_amount_(0.0f)
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
    phase_ = 0.0f;
    phase_inc_ = 0.0f;
    fm_amount_ = 0.0f;
}

void JupiterDCO::SetFrequency(float freq_hz) {
    base_freq_hz_ = freq_hz;
    phase_inc_ = freq_hz / sample_rate_;
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
        // FM modulation: ±1 octave range
        current_phase_inc *= (1.0f + fm_amount_ * 0.5f);
    }
    
    // Store last phase for sync detection
    last_phase_ = phase_;
    
    // Advance phase
    phase_ += current_phase_inc;
    if (phase_ >= 1.0f) {
        phase_ -= 1.0f;
    }
    
    // Generate waveform
    return GenerateWaveform();
}

bool JupiterDCO::DidWrap() const {
    // Detect if phase wrapped (for syncing slave oscillator)
    return phase_ < last_phase_;
}

float JupiterDCO::GenerateWaveform() {
    switch (waveform_) {
        case WAVEFORM_RAMP:
            return LookupWavetable(ramp_table_, phase_);
        
        case WAVEFORM_SQUARE:
            return LookupWavetable(square_table_, phase_);
        
        case WAVEFORM_PULSE: {
            // Bristol-style PWM: difference of two phased ramps
            // This provides better anti-aliasing than a simple threshold
            float ramp1 = LookupWavetable(ramp_table_, phase_);
            
            // Second ramp phase-shifted by pulse width
            float phase2 = phase_ + pulse_width_;
            if (phase2 >= 1.0f) {
                phase2 -= 1.0f;
            }
            float ramp2 = LookupWavetable(ramp_table_, phase2);
            
            // Difference gives PWM with smoother edges
            return ramp1 - ramp2;
        }
        
        case WAVEFORM_TRIANGLE:
            return LookupWavetable(triangle_table_, phase_);
        
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
        
        // Ramp/Sawtooth: -1 to +1 (positive-going)
        // Bristol uses rear-to-front table to make positive leading edge
        ramp_table_[i] = phase * 2.0f - 1.0f;
        
        // Square wave with slight decay on plateaus (Bristol style)
        // This adds warmth by simulating capacitor discharge
        if (phase < 0.5f) {
            // First half: start at +1 and decay slightly
            float decay = 1.0f - (phase * 0.04f);  // ~2% decay per half cycle
            square_table_[i] = decay;
        } else {
            // Second half: start at -1 and decay back toward 0
            float decay = 1.0f - ((phase - 0.5f) * 0.04f);
            square_table_[i] = -decay;
        }
        
        // Triangle wave: -1 to +1 to -1
        // Bristol uses this same approach
        if (phase < 0.5f) {
            triangle_table_[i] = phase * 4.0f - 1.0f;
        } else {
            triangle_table_[i] = 3.0f - phase * 4.0f;
        }
    }
    
    tables_initialized_ = true;
}

} // namespace dsp
