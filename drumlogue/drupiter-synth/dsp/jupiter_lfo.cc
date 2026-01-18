/**
 * @file jupiter_lfo.cc
 * @brief Low Frequency Oscillator implementation
 *
 * Based on Bristol lfo.c
 * Multi-waveform LFO with delay envelope
 */

#include "jupiter_lfo.h"
#include <algorithm>
#include <cmath>

namespace dsp {

JupiterLFO::JupiterLFO()
    : sample_rate_(48000.0f)
    , phase_(0.0f)
    , phase_inc_(0.0f)
    , waveform_(WAVEFORM_TRIANGLE)
    , key_trigger_(true)  // JP-8 authenticity: key trigger enabled by default
    , delay_phase_(1.0f)  // Start with no delay (fully on)
    , delay_inc_(0.0f)
    , delay_time_(0.0f)
    , sh_value_(0.0f)
    , rand_seed_(12345)
{
}

JupiterLFO::~JupiterLFO() {
}

void JupiterLFO::Init(float sample_rate) {
    sample_rate_ = sample_rate;
    Reset();
}

void JupiterLFO::SetFrequency(float freq_hz) {
    freq_hz = ClampFrequency(freq_hz);
    phase_inc_ = freq_hz / sample_rate_;
}

void JupiterLFO::SetWaveform(Waveform waveform) {
    waveform_ = waveform;
}

void JupiterLFO::SetDelay(float delay_sec) {
    delay_time_ = std::max(0.0f, std::min(delay_sec, 10.0f));
    if (delay_time_ > 0.0f) {
        delay_inc_ = 1.0f / (delay_time_ * sample_rate_);
    } else {
        delay_inc_ = 0.0f;
        delay_phase_ = 1.0f;  // No delay
    }
}

void JupiterLFO::SetKeyTrigger(bool enable) {
    key_trigger_ = enable;
}

void JupiterLFO::Trigger() {
    // Reset phase only if key trigger is enabled (JP-8 feature)
    if (key_trigger_) {
        phase_ = 0.0f;
    }
    // Always reset delay envelope on trigger
    if (delay_time_ > 0.0f) {
        delay_phase_ = 0.0f;
    }
}

void JupiterLFO::Reset() {
    phase_ = 0.0f;
    delay_phase_ = 1.0f;  // Start with LFO fully on
    sh_value_ = 0.0f;
}

float JupiterLFO::Process() {
    // Update phase
    phase_ += phase_inc_;
    if (phase_ >= 1.0f) {
        phase_ -= 1.0f;
    }
    
    // Generate waveform
    float output = GenerateWaveform();
    
    // Apply delay envelope if active
    if (delay_phase_ < 1.0f) {
        delay_phase_ += delay_inc_;
        if (delay_phase_ > 1.0f) {
            delay_phase_ = 1.0f;
        }
        output *= delay_phase_;
    }
    
    return output;
}

float JupiterLFO::GenerateWaveform() {
    switch (waveform_) {
        case WAVEFORM_TRIANGLE:
            // Triangle: -1 to +1 to -1
            if (phase_ < 0.5f) {
                return phase_ * 4.0f - 1.0f;  // 0 to 0.5 => -1 to +1
            } else {
                return 3.0f - phase_ * 4.0f;  // 0.5 to 1 => +1 to -1
            }
        
        case WAVEFORM_RAMP:
            // Ramp/Sawtooth: -1 to +1
            return phase_ * 2.0f - 1.0f;
        
        case WAVEFORM_SQUARE:
            // Square wave
            return (phase_ < 0.5f) ? -1.0f : 1.0f;
        
        case WAVEFORM_SAMPLE_HOLD:
            // Sample & Hold: update on phase wrap
            if (phase_ < phase_inc_) {
                sh_value_ = GenerateRandom();
            }
            return sh_value_;
        
        default:
            return 0.0f;
    }
}

float JupiterLFO::GenerateRandom() {
    // Simple LCG random number generator
    rand_seed_ = (rand_seed_ * 1103515245 + 12345) & 0x7FFFFFFF;
    return (static_cast<float>(rand_seed_) / static_cast<float>(0x7FFFFFFF)) * 2.0f - 1.0f;
}

float JupiterLFO::ClampFrequency(float freq) const {
    return std::max(kMinFreq, std::min(freq, kMaxFreq));
}

} // namespace dsp
