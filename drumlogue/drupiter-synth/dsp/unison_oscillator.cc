/**
 * @file unison_oscillator.cc
 * @brief Unison oscillator implementation
 * 
 * @author Drupiter Hoover Synthesis Team
 * @date 2025-12-25
 * @version 2.0.0-dev
 */

#include "unison_oscillator.h"
#include "../common/dsp_utils.h"
#include <cmath>
#include <cstring>

// Enable USE_NEON for ARM NEON optimizations
#ifdef __ARM_NEON
#define USE_NEON 1
#endif

#define NEON_DSP_NS drupiter
#include "../common/neon_dsp.h"

namespace dsp {

UnisonOscillator::UnisonOscillator()
    : num_voices_(7)
    , sample_rate_(48000.0f)
    , base_freq_(440.0f)
    , detune_cents_(10.0f)
    , stereo_spread_(0.7f) {
    memset(voice_detunes_, 0, sizeof(voice_detunes_));
    memset(voice_pans_, 0, sizeof(voice_pans_));
}

UnisonOscillator::~UnisonOscillator() {
}

void UnisonOscillator::Init(float sample_rate, uint8_t num_voices) {
    sample_rate_ = sample_rate;
    
    // Clamp to valid range (3-7 voices, must be odd)
    if (num_voices < 3) num_voices = 3;
    if (num_voices > kMaxVoices) num_voices = kMaxVoices;
    if ((num_voices & 1) == 0) num_voices++;  // Make odd
    num_voices_ = num_voices;
    
    // Initialize all oscillators
    for (uint8_t i = 0; i < kMaxVoices; i++) {
        oscillators_[i].Init(sample_rate);
        oscillators_[i].SetWaveform(JupiterDCO::WAVEFORM_SAW_PWM);  // Default to hoover waveform
        oscillators_[i].SetPulseWidth(0.5f);
    }
    
    // Calculate detune and pan patterns
    CalculateDetuneRatios();
    CalculatePanPositions();
    
    // Set initial frequency
    SetFrequency(base_freq_);
}

void UnisonOscillator::SetFrequency(float freq_hz) {
    base_freq_ = freq_hz;
    
    // Apply detuned frequencies to each voice
    for (uint8_t i = 0; i < num_voices_; i++) {
        float detuned_freq = base_freq_ * voice_detunes_[i];
        oscillators_[i].SetFrequency(detuned_freq);
    }
}

void UnisonOscillator::SetWaveform(JupiterDCO::Waveform waveform) {
    for (uint8_t i = 0; i < num_voices_; i++) {
        oscillators_[i].SetWaveform(waveform);
    }
}

void UnisonOscillator::SetPulseWidth(float pw) {
    for (uint8_t i = 0; i < num_voices_; i++) {
        oscillators_[i].SetPulseWidth(pw);
    }
}

void UnisonOscillator::SetDetune(float detune_cents) {
    detune_cents_ = detune_cents;
    CalculateDetuneRatios();
    SetFrequency(base_freq_);  // Reapply frequencies with new detune
}

void UnisonOscillator::SetStereoSpread(float spread) {
    stereo_spread_ = spread;
    if (stereo_spread_ < 0.0f) stereo_spread_ = 0.0f;
    if (stereo_spread_ > 1.0f) stereo_spread_ = 1.0f;
}

void UnisonOscillator::Reset() {
    // Reset all oscillator phases (for hard sync)
    for (uint8_t i = 0; i < num_voices_; i++) {
        oscillators_[i].ResetPhase();
    }
}

void UnisonOscillator::CalculateDetuneRatios() {
    // Golden ratio detune spread
    // Voice 0 is center (no detune)
    // Voices 1,2,3... alternate +/- with increasing golden ratio powers
    //
    // Example (7 voices):
    //   0: center (0 cents)
    //   1: +detune * φ^0 = +detune
    //   2: -detune * φ^0 = -detune
    //   3: +detune * φ^1 = +detune * 1.618
    //   4: -detune * φ^1 = -detune * 1.618
    //   5: +detune * φ^2 = +detune * 2.618
    //   6: -detune * φ^2 = -detune * 2.618
    
    voice_detunes_[0] = 1.0f;  // Center voice (no detune)
    
    float golden_power = 1.0f;
    for (uint8_t i = 1; i < num_voices_; i += 2) {
        // Positive detune
        float cents_plus = detune_cents_ * golden_power;
        voice_detunes_[i] = CentsToRatio(cents_plus);
        
        // Negative detune (if voice exists)
        if (i + 1 < num_voices_) {
            float cents_minus = -detune_cents_ * golden_power;
            voice_detunes_[i + 1] = CentsToRatio(cents_minus);
        }
        
        // Next golden ratio power
        golden_power *= kGoldenRatio;
    }
}

void UnisonOscillator::CalculatePanPositions() {
    // Pan voices using golden angle spiral (like sunflower seeds)
    // This creates natural stereo spread without periodic patterns
    //
    // Golden angle ≈ 137.5° creates optimal non-repeating distribution
    const float golden_angle = 2.0f * M_PI * (1.0f - 1.0f / kGoldenRatio);
    
    for (uint8_t i = 0; i < num_voices_; i++) {
        // Convert angle to pan position (-1 to +1)
        float angle = i * golden_angle;
        voice_pans_[i] = cosf(angle);  // Project onto horizontal axis
    }
    
    // Center voice always stays centered
    voice_pans_[0] = 0.0f;
}

}  // namespace dsp
