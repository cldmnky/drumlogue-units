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
    , ota_s1_(0.0f)
    , ota_s2_(0.0f)
    , ota_s3_(0.0f)
    , ota_s4_(0.0f)
    , ota_y2_(0.0f)
    , ota_y4_(0.0f)
    , ota_a_(0.0f)
    , ota_res_k_(0.0f)
    , ota_gain_comp_(1.0f)
    , ota_drive_(1.25f)
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
    // 4x oversampling: improves stability and reduces high-frequency artifacts.
    float output = 0.0f;

    // JP-8 style LP modes: 4-pole OTA cascade (IR3109 family), resonance on LP only.
    if (mode_ == MODE_LP12 || mode_ == MODE_LP24) {
        for (int i = 0; i < kOversamplingFactor; ++i) {
            // Feedback from final stage (previous sample/iteration) - stable with oversampling.
            float u = input - ota_res_k_ * ota_y4_;

            // Mild OTA-ish saturation on input (helps "Roland" character).
            // Keep it subtle to avoid obvious distortion.
            u = tanhf(u * ota_drive_) / tanhf(ota_drive_);

            // 4 cascaded TPT one-pole lowpass stages.
            // One-pole TPT (Zavalishin): v = (x - s) * a; y = v + s; s = y + v
            float v1 = (u - ota_s1_) * ota_a_;
            float y1 = v1 + ota_s1_;
            ota_s1_ = y1 + v1;

            float v2 = (y1 - ota_s2_) * ota_a_;
            float y2 = v2 + ota_s2_;
            ota_s2_ = y2 + v2;

            float v3 = (y2 - ota_s3_) * ota_a_;
            float y3 = v3 + ota_s3_;
            ota_s3_ = y3 + v3;

            float v4 = (y3 - ota_s4_) * ota_a_;
            float y4 = v4 + ota_s4_;
            ota_s4_ = y4 + v4;

            ota_y2_ = y2;
            ota_y4_ = y4;
        }

        output = (mode_ == MODE_LP12) ? ota_y2_ : ota_y4_;
        return output * ota_gain_comp_;
    }

    // HP mode: non-resonant 6dB high-pass.
    if (mode_ == MODE_HP12) {
        for (int i = 0; i < kOversamplingFactor; ++i) {
            float v = (input - hp_lp_state_) * hp_a_;
            float lp = v + hp_lp_state_;
            hp_lp_state_ = lp + v;
            output = input - lp;
        }
        return output;
    }

    // BP mode (and any unknown): keep existing Chamberlin SVF.
    for (int i = 0; i < kOversamplingFactor; i++) {
        hp_ = input - lp_ - q_ * bp_;
        bp_ = bp_ + f_ * hp_;
        lp_ = lp_ + f_ * bp_;

        if (bp_ > 10.0f) bp_ = 10.0f;
        else if (bp_ < -10.0f) bp_ = -10.0f;
        if (lp_ > 10.0f) lp_ = 10.0f;
        else if (lp_ < -10.0f) lp_ = -10.0f;
    }

    // For BP, keep compensation modest.
    const float gain_comp = 1.0f + resonance_ * 0.25f;
    return bp_ * gain_comp * 0.5f;
}

void JupiterVCF::Reset() {
    ota_s1_ = 0.0f;
    ota_s2_ = 0.0f;
    ota_s3_ = 0.0f;
    ota_s4_ = 0.0f;
    ota_y2_ = 0.0f;
    ota_y4_ = 0.0f;
    hp_lp_state_ = 0.0f;

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
    const float oversampled_rate = sample_rate_ * kOversamplingFactor;

    // Clamp cutoff for coefficient computation.
    float fc = cutoff_hz_;
    const float fc_max = 0.45f * oversampled_rate;
    if (fc > fc_max) fc = fc_max;
    if (fc < 20.0f) fc = 20.0f;

    // Coefficients for OTA cascade (TPT 1-pole).
    // g = tan(pi*fc/fs), a = g/(1+g)
    const float g = tanf(static_cast<float>(M_PI) * (fc / oversampled_rate));
    ota_a_ = g / (1.0f + g);

    // Resonance mapping: JP-8 IR3109 implementations are generally less eager to
    // full self-osc than many modern SVF models. Keep a conservative max.
    const float r = std::max(0.0f, std::min(resonance_, 1.0f));
    ota_res_k_ = (r * r) * 3.2f;  // square for finer low-res control

    // Gain compensation ("Q comp") to keep perceived loudness more stable.
    ota_gain_comp_ = 1.0f + r * 0.6f;

    // High-pass 6dB (non-resonant): share the same cutoff control.
    hp_a_ = ota_a_;

    // SVF coefficients (used for BP mode only).
    float cutoff_normalized = fc / oversampled_rate;
    if (cutoff_normalized > 0.45f) cutoff_normalized = 0.45f;
    f_ = 2.0f * sinf(static_cast<float>(M_PI) * cutoff_normalized);
    q_ = 2.0f - r * 1.95f;
    if (q_ < 0.05f) q_ = 0.05f;
}

float JupiterVCF::ClampCutoff(float freq) const {
    return std::max(20.0f, std::min(freq, 20000.0f));
}

} // namespace dsp
