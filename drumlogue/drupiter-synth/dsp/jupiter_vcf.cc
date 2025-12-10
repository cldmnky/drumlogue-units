/**
 * @file jupiter_vcf.cc
 * @brief Voltage Controlled Filter implementation
 *
 * Based on Bristol filter.c (Chamberlin state-variable filter)
 * Multi-mode filter with resonance
 */

#include "jupiter_vcf.h"
#include <algorithm>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace dsp {

JupiterVCF::JupiterVCF()
    : sample_rate_(48000.0f)
    , cutoff_hz_(1000.0f)
    , base_cutoff_hz_(1000.0f)
    , resonance_(0.0f)
    , mode_(MODE_LP12)
    , kbd_tracking_(0.5f)  // 50% keyboard tracking (Jupiter-8 typical)
    , lp_(0.0f)
    , bp_(0.0f)
    , hp_(0.0f)
    , f_(0.0f)
    , q_(1.0f)
    , lp2_(0.0f)
    , bp2_(0.0f)
{
}

JupiterVCF::~JupiterVCF() {
}

void JupiterVCF::Init(float sample_rate) {
    sample_rate_ = sample_rate;
    Reset();
    UpdateCoefficients();
}

void JupiterVCF::SetCutoff(float freq_hz) {
    base_cutoff_hz_ = ClampCutoff(freq_hz);
    cutoff_hz_ = base_cutoff_hz_;
    UpdateCoefficients();
}

void JupiterVCF::SetResonance(float resonance) {
    resonance_ = std::max(0.0f, std::min(resonance, 1.0f));
    UpdateCoefficients();
}

void JupiterVCF::SetMode(Mode mode) {
    mode_ = mode;
}

void JupiterVCF::SetKeyboardTracking(float amount) {
    kbd_tracking_ = std::max(0.0f, std::min(amount, 1.0f));
}

void JupiterVCF::ApplyKeyboardTracking(uint8_t note) {
    if (kbd_tracking_ > 0.0f) {
        // MIDI note 60 (C4) = no change
        // Each octave up doubles frequency
        float note_offset = (note - 60) / 12.0f;  // Octaves from C4
        float freq_mult = powf(2.0f, note_offset * kbd_tracking_);
        cutoff_hz_ = ClampCutoff(base_cutoff_hz_ * freq_mult);
        UpdateCoefficients();
    }
}

float JupiterVCF::Process(float input) {
    // Chamberlin state-variable filter
    // This is a simplified version - could be enhanced with oversampling
    
    lp_ = lp_ + f_ * bp_;
    hp_ = input - lp_ - q_ * bp_;
    bp_ = f_ * hp_ + bp_;
    
    // Gain compensation for resonance (prevents volume loss at high Q)
    // Jupiter-8 compensates gain to maintain consistent loudness
    float gain_comp = 1.0f + resonance_ * 0.5f;
    
    // Select output based on mode
    switch (mode_) {
        case MODE_LP12:
            return lp_ * gain_comp;
        
        case MODE_LP24: {
            // Second stage for 24dB/oct (cascaded)
            lp2_ = lp2_ + f_ * bp2_;
            float hp2 = lp_ - lp2_ - q_ * bp2_;
            bp2_ = f_ * hp2 + bp2_;
            return lp2_ * gain_comp;
        }
        
        case MODE_HP12:
            return hp_ * gain_comp;
        
        case MODE_BP12:
            return bp_ * gain_comp * 0.5f;  // BP needs less compensation
        
        default:
            return lp_;
    }
}

void JupiterVCF::Reset() {
    lp_ = 0.0f;
    bp_ = 0.0f;
    hp_ = 0.0f;
    lp2_ = 0.0f;
    bp2_ = 0.0f;
}

void JupiterVCF::UpdateCoefficients() {
    // Chamberlin filter coefficients
    // f = 2 * sin(pi * cutoff / samplerate)
    // q controls resonance feedback
    
    float cutoff_normalized = cutoff_hz_ / sample_rate_;
    
    // Clamp to prevent instability (Nyquist limit with safety margin)
    if (cutoff_normalized > 0.45f) {
        cutoff_normalized = 0.45f;
    }
    
    f_ = 2.0f * sinf(static_cast<float>(M_PI) * cutoff_normalized);
    
    // Enhanced Q coefficient for Jupiter-8 character
    // Jupiter-8 resonance goes into self-oscillation at maximum
    // Map resonance 0-1 to Q 1.0-0.005 (allows near-oscillation)
    // Use exponential curve for better control in usable range
    float res_squared = resonance_ * resonance_;
    q_ = 1.0f - (0.95f * res_squared + 0.05f * resonance_);
    
    // Prevent complete instability
    if (q_ < 0.005f) {
        q_ = 0.005f;
    }
}

float JupiterVCF::ClampCutoff(float freq) const {
    return std::max(20.0f, std::min(freq, 20000.0f));
}

} // namespace dsp
