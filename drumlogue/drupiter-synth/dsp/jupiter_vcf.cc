/**
 * @file jupiter_vcf.cc
 * @brief Voltage Controlled Filter implementation
 *
 * Jupiter-8 style IR3109 4-pole OTA cascade filter.
 * Based on Krajeski/Stilson improved ladder with TPT topology.
 * 
 * Key characteristics:
 * - 4 cascaded one-pole lowpass filters
 * - Global resonance feedback (pre-saturation)
 * - Q compensation to maintain passband gain
 * - Cutoff-resonance decoupling via polynomial correction
 */

#include "jupiter_vcf.h"
#include <algorithm>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Thermal voltage for transistor saturation modeling
static constexpr float kThermal = 1.0f;  // Normalized (original is 0.000025)

namespace dsp {

JupiterVCF::JupiterVCF()
    : sample_rate_(48000.0f)
    , cutoff_hz_(1000.0f)
    , base_cutoff_hz_(1000.0f)
    , resonance_(0.0f)
    , mode_(MODE_LP12)
    , kbd_tracking_(0.5f)  // 50% keyboard tracking (Jupiter-8 typical)
    , ota_s1_(0.0f)
    , ota_s2_(0.0f)
    , ota_s3_(0.0f)
    , ota_s4_(0.0f)
    , ota_d1_(0.0f)
    , ota_d2_(0.0f)
    , ota_d3_(0.0f)
    , ota_d4_(0.0f)
    , ota_y2_(0.0f)
    , ota_y4_(0.0f)
    , ota_a_(0.0f)
    , ota_res_k_(0.0f)
    , ota_gain_comp_(0.0f)
    , ota_output_gain_(1.0f)
    , ota_drive_(1.0f)
    , hp_lp_state_(0.0f)
    , hp_a_(0.0f)
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

void JupiterVCF::SetCutoffModulated(float freq_hz) {
    cutoff_hz_ = ClampCutoff(freq_hz);
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
    // JP-8 style LP modes: 4-pole OTA cascade (IR3109 family).
    // Using Krajeski-style improved ladder filter with:
    // - Compromise poles at z = -0.3 for cutoff-resonance decoupling
    // - Tanh saturation for analog character
    // - Gain compensation to maintain passband level
    
    if (mode_ == MODE_LP12 || mode_ == MODE_LP24) {
        float output = 0.0f;
        
        for (int i = 0; i < kOversamplingFactor; ++i) {
            // Resonance feedback from 4th stage with gain compensation
            // gComp helps maintain passband level at high resonance
            float feedback = ota_res_k_ * (ota_s4_ - ota_gain_comp_ * input);
            
            // Input stage with soft saturation (transistor-like nonlinearity)
            float u = input - feedback;
            
            // Soft clipping (tanh approximation) for input stage
            // This creates the characteristic "warmth" of analog filters
            if (u > 1.5f) u = 1.5f;
            else if (u < -1.5f) u = -1.5f;
            else {
                float u2 = u * u;
                u = u * (1.0f - u2 * 0.1111f);  // tanh approx: x - x³/9
            }
            
            // 4 cascaded one-pole sections using Krajeski-style improved coefficients
            // Each pole: y[n] = g * (0.3/1.3 * x[n] + 1/1.3 * x[n-1] - y[n-1]) + y[n-1]
            // The 0.3/1.3 and 1/1.3 terms create the "compromise" zero at z = -0.3
            // that decouples cutoff from resonance
            
            const float g = ota_a_;
            const float a1 = 0.3f / 1.3f;
            const float a2 = 1.0f / 1.3f;
            
            // Stage 1
            float y1_new = g * (a1 * u + a2 * ota_d1_ - ota_s1_) + ota_s1_;
            ota_d1_ = u;
            ota_s1_ = y1_new;
            
            // Stage 2
            float y2_new = g * (a1 * y1_new + a2 * ota_d2_ - ota_s2_) + ota_s2_;
            ota_d2_ = y1_new;
            ota_s2_ = y2_new;
            ota_y2_ = y2_new;  // LP12 output tap
            
            // Stage 3
            float y3_new = g * (a1 * y2_new + a2 * ota_d3_ - ota_s3_) + ota_s3_;
            ota_d3_ = y2_new;
            ota_s3_ = y3_new;
            
            // Stage 4
            float y4_new = g * (a1 * y3_new + a2 * ota_d4_ - ota_s4_) + ota_s4_;
            ota_d4_ = y3_new;
            ota_s4_ = y4_new;
            ota_y4_ = y4_new;  // LP24 output tap
            
            // Soft clipping on output to prevent blow-ups
            float y = ota_y4_;
            float y3 = y * y * y;
            output = y - y3 * 0.1666667f;  // Band-limited sigmoid: y - y³/6
        }

        // Select 12dB or 24dB output with gain compensation
        float result = (mode_ == MODE_LP12) ? ota_y2_ : output;
        return result * ota_output_gain_;
    }

    // HP mode: non-resonant 6dB high-pass (simple DC blocker style)
    if (mode_ == MODE_HP12) {
        for (int i = 0; i < kOversamplingFactor; ++i) {
            float v = (input - hp_lp_state_) * hp_a_;
            float lp = v + hp_lp_state_;
            hp_lp_state_ = lp + v;
            hp_ = input - lp;
        }
        return hp_;
    }

    // BP mode: Chamberlin SVF bandpass
    for (int i = 0; i < kOversamplingFactor; i++) {
        hp_ = input - lp_ - q_ * bp_;
        bp_ = bp_ + f_ * hp_;
        lp_ = lp_ + f_ * bp_;

        // Clamp to prevent blowup
        if (bp_ > 10.0f) bp_ = 10.0f;
        else if (bp_ < -10.0f) bp_ = -10.0f;
        if (lp_ > 10.0f) lp_ = 10.0f;
        else if (lp_ < -10.0f) lp_ = -10.0f;
    }

    return bp_ * (1.0f + resonance_ * 0.5f);
}

void JupiterVCF::Reset() {
    ota_s1_ = 0.0f;
    ota_s2_ = 0.0f;
    ota_s3_ = 0.0f;
    ota_s4_ = 0.0f;
    ota_y2_ = 0.0f;
    ota_y4_ = 0.0f;
    ota_d1_ = 0.0f;
    ota_d2_ = 0.0f;
    ota_d3_ = 0.0f;
    ota_d4_ = 0.0f;
    hp_lp_state_ = 0.0f;

    lp_ = 0.0f;
    bp_ = 0.0f;
    hp_ = 0.0f;
    lp2_ = 0.0f;
    bp2_ = 0.0f;
}

void JupiterVCF::UpdateCoefficients() {
    // Jupiter-8/Krajeski style filter coefficient calculation
    // 
    // Key improvements:
    // 1. Better frequency mapping that doesn't over-attenuate at low cutoffs
    // 2. Cutoff-resonance decoupling via polynomial corrections
    // 3. Proper gain compensation to maintain passband level
    
    // Calculate for oversampled rate (48kHz * 4 = 192kHz)
    const float oversampled_rate = sample_rate_ * kOversamplingFactor;

    // Clamp cutoff frequency to reasonable range
    // Minimum 80Hz to prevent over-resonant bass rumble
    // Maximum 0.45 * Nyquist for stability
    float fc = cutoff_hz_;
    const float fc_min = 80.0f;
    const float fc_max = 0.45f * oversampled_rate;
    if (fc < fc_min) fc = fc_min;
    if (fc > fc_max) fc = fc_max;

    // Normalized cutoff: fc / sample_rate
    const float wc = 2.0f * static_cast<float>(M_PI) * fc / oversampled_rate;
    
    // Krajeski-style g coefficient with polynomial correction
    // This provides better tuning accuracy across the frequency range
    const float wc2 = wc * wc;
    const float wc3 = wc2 * wc;
    const float wc4 = wc3 * wc;
    ota_a_ = 0.9892f * wc - 0.4342f * wc2 + 0.1381f * wc3 - 0.0202f * wc4;
    
    // Clamp g to prevent instability
    if (ota_a_ < 0.0f) ota_a_ = 0.0f;
    if (ota_a_ > 1.0f) ota_a_ = 1.0f;

    // Resonance mapping
    // res_k = 4 * r for maximum self-oscillation at r = 1.0
    // Use polynomial correction for resonance to match cutoff changes (decoupling)
    const float r = std::max(0.0f, std::min(resonance_, 1.0f));
    
    // Krajeski resonance correction: compensate resonance for cutoff changes
    // gRes = r * (1.0029 + 0.0526*wc - 0.926*wc² + 0.0218*wc³)
    const float res_correction = 1.0029f + 0.0526f * wc - 0.926f * wc2 + 0.0218f * wc3;
    ota_res_k_ = 4.0f * r * res_correction;
    
    // Limit resonance to prevent self-oscillation going out of control
    if (ota_res_k_ > 3.8f) ota_res_k_ = 3.8f;

    // Gain compensation to maintain passband level
    // At high resonance, the passband can drop by up to 12dB
    // Compensate with 0.5 factor at max resonance (6dB compensation)
    ota_gain_comp_ = 0.5f * r;  // Used in feedback: (state[4] - gComp * input)
    
    // Output gain to compensate for resonance-induced passband drop
    // More aggressive compensation at higher resonance
    ota_output_gain_ = 1.0f + r * 1.5f;

    // High-pass 6dB (non-resonant): share the same base cutoff
    // Use simpler TPT coefficient
    const float g_hp = tanf(static_cast<float>(M_PI) * (fc / oversampled_rate));
    hp_a_ = g_hp / (1.0f + g_hp);

    // SVF coefficients (used for BP mode only)
    float cutoff_normalized = fc / oversampled_rate;
    if (cutoff_normalized > 0.45f) cutoff_normalized = 0.45f;
    f_ = 2.0f * sinf(static_cast<float>(M_PI) * cutoff_normalized);
    q_ = 2.0f - r * 1.95f;
    if (q_ < 0.05f) q_ = 0.05f;
}

float JupiterVCF::ClampCutoff(float freq) const {
    // Minimum 80Hz to prevent muddy resonance
    // Maximum 20kHz (will be further limited by Nyquist in UpdateCoefficients)
    return std::max(80.0f, std::min(freq, 20000.0f));
}

} // namespace dsp
