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
    // 4x oversampling: run filter 4 times with same input
    // This allows stable operation at higher frequencies
    // Reference: "The only way around this is to oversample" - EarLevel Engineering
    
    float output = 0.0f;
    
    for (int i = 0; i < kOversamplingFactor; i++) {
        // Chamberlin state-variable filter
        // Using the correct two-integrator topology:
        // hp = input - lp - q*bp
        // bp = bp + f*hp
        // lp = lp + f*bp
        // where q = 1/Q (damping coefficient, lower = more resonance)
        
        // Calculate HP first
        hp_ = input - lp_ - q_ * bp_;
        
        // Update BP
        bp_ = bp_ + f_ * hp_;
        
        // Update LP
        lp_ = lp_ + f_ * bp_;
        
        // Clamp state variables to prevent runaway (stability measure)
        if (bp_ > 10.0f) bp_ = 10.0f;
        else if (bp_ < -10.0f) bp_ = -10.0f;
        if (lp_ > 10.0f) lp_ = 10.0f;
        else if (lp_ < -10.0f) lp_ = -10.0f;
    }
    
    // Gain compensation for resonance
    float gain_comp = 1.0f + resonance_ * 0.5f;
    
    // Select output based on mode (use final iteration's result)
    switch (mode_) {
        case MODE_LP12:
            output = lp_ * gain_comp;
            break;
        
        case MODE_LP24: {
            // Second stage for 24dB/oct (cascaded)
            // Also needs oversampling
            for (int i = 0; i < kOversamplingFactor; i++) {
                lp2_ = lp2_ + f_ * bp2_;
                float hp2 = lp_ - lp2_ - q_ * bp2_;
                bp2_ = f_ * hp2 + bp2_;
            }
            output = lp2_ * gain_comp;
            break;
        }
        
        case MODE_HP12:
            output = hp_ * gain_comp;
            break;
        
        case MODE_BP12:
            output = bp_ * gain_comp * 0.5f;  // BP needs less compensation
            break;
        
        default:
            output = lp_;
            break;
    }
    
    return output;
}

void JupiterVCF::Reset() {
    lp_ = 0.0f;
    bp_ = 0.0f;
    hp_ = 0.0f;
    lp2_ = 0.0f;
    bp2_ = 0.0f;
}

void JupiterVCF::UpdateCoefficients() {
    // Chamberlin filter coefficients with 4x oversampling
    // Standard Chamberlin: f = 2 * sin(pi * cutoff / samplerate)
    // With 4x oversampling, effective sample rate is 4x higher
    // This allows stable operation across full audio bandwidth
    
    // Calculate for oversampled rate (48kHz * 4 = 192kHz)
    float oversampled_rate = sample_rate_ * kOversamplingFactor;
    float cutoff_normalized = cutoff_hz_ / oversampled_rate;
    
    // Clamp to Nyquist limit at oversampled rate
    // At 192kHz effective rate, can safely reach 21.6kHz (0.45 * 48kHz)
    if (cutoff_normalized > 0.45f) {
        cutoff_normalized = 0.45f;
    }
    
    // Calculate f coefficient using oversampled rate
    // This gives much smaller f values, improving stability
    f_ = 2.0f * sinf(static_cast<float>(M_PI) * cutoff_normalized);
    
    // Damping coefficient: higher = less resonance
    // Map resonance 0-1 to q 2.0-0.05 (allow more resonance with oversampling)
    q_ = 2.0f - resonance_ * 1.95f;
    
    // Minimum damping to prevent self-oscillation
    if (q_ < 0.05f) {
        q_ = 0.05f;
    }
}

float JupiterVCF::ClampCutoff(float freq) const {
    return std::max(20.0f, std::min(freq, 20000.0f));
}

} // namespace dsp
