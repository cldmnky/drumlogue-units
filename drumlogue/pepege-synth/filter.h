/**
 * @file filter.h
 * @brief State Variable Filter for Pepege
 *
 * 12dB/oct multimode filter (LP, HP, BP) with optional 24dB cascade
 */

#pragma once

#include <cstdint>
#include <cmath>

/**
 * @brief Filter types
 */
enum FilterType {
    FILTER_LP12 = 0,
    FILTER_LP24,
    FILTER_HP12,
    FILTER_BP12
};

/**
 * @brief State Variable Filter (Chamberlin topology)
 *
 * Efficient and stable 12dB/oct multimode filter
 */
class SVFilter {
public:
    SVFilter() {}
    ~SVFilter() {}
    
    void Init(float sample_rate) {
        sample_rate_ = sample_rate;
        inv_sample_rate_ = 1.0f / sample_rate;
        
        cutoff_ = 1.0f;
        resonance_ = 0.0f;
        type_ = FILTER_LP12;
        
        // State variables
        low_ = 0.0f;
        band_ = 0.0f;
        
        // For 24dB mode
        low2_ = 0.0f;
        band2_ = 0.0f;
        
        // Coefficient
        f_ = 1.0f;
        q_ = 0.5f;
        dirty_ = true;  // Force the first UpdateCoefficients() to compute
    }
    
    void Reset() {
        low_ = 0.0f;
        band_ = 0.0f;
        low2_ = 0.0f;
        band2_ = 0.0f;
        dirty_ = true;  // Recompute coefficients after a reset
    }
    
    /**
     * @brief Set cutoff frequency (0.0 to 1.0)
     *
     * Only stores the value and marks the coefficients dirty. The relatively
     * expensive sinf() in UpdateCoefficients() is deferred so the caller can
     * run it at control rate instead of once per sample.
     */
    void SetCutoff(float cutoff) {
        if (cutoff != cutoff_) {
            cutoff_ = cutoff;
            dirty_ = true;
        }
    }
    
    /**
     * @brief Set resonance (0.0 to 1.0)
     *
     * Cheap store; defers the coefficient update like SetCutoff().
     */
    void SetResonance(float resonance) {
        if (resonance != resonance_) {
            resonance_ = resonance;
            dirty_ = true;
        }
    }
    
    /**
     * @brief Set filter type
     */
    void SetType(FilterType type) {
        type_ = type;
    }
    
    /**
     * @brief Recompute f_/q_ from the current cutoff and resonance
     *
     * No-op when neither value changed since the last update, so a static
     * cutoff costs no sinf() at all. Call this explicitly after SetCutoff()
     * when running at control rate (e.g. every 8th sample).
     */
    void UpdateCoefficients() {
        if (!dirty_) return;
        dirty_ = false;
        
        // Convert the normalized cutoff (0-1) to the frequency coefficient
        // f = 2 * sin(pi * fc / fs) that this Chamberlin topology tunes with.
        //
        // f has to stay below the stability limit, which the impulse response
        // puts at f = 1.56 in the worst case (no resonance, q = 0.5; the limit
        // rises with resonance), so the usable coefficient range ends at 1.45.
        //
        // Below the knee - the point where the original curve used to hit its
        // old 0.99 clamp - the mapping is unchanged, so every patch in the
        // lower range keeps its sound exactly. Above the knee the cutoff keeps
        // rising to 12.4kHz at the top of the knob instead of sitting on the
        // clamped coefficient: with the old clamp every cutoff above 0.63
        // collapsed onto the same f, so the top ~40% of the knob did nothing.
        const float knee = 0.6281f;                        // where f used to reach 0.99
        const float fc_knee = 20.0f + knee * knee * 20000.0f;   // ~7.9kHz
        const float fc_max = 0.25803f * sample_rate_;      // f = 1.45 at 48kHz
        float fc;
        if (cutoff_ <= knee) {
            fc = 20.0f + cutoff_ * cutoff_ * 20000.0f;
        } else {
            fc = fc_knee + (cutoff_ - knee) / (1.0f - knee) * (fc_max - fc_knee);
        }
        f_ = 2.0f * sinf(3.14159265f * fc * inv_sample_rate_);
        if (f_ > 1.45f) f_ = 1.45f;                        // Safety net for rounding
        
        // Q from resonance (0 to self-oscillation)
        // q_ should be 0.5 (no resonance) to ~0.01 (high resonance)
        q_ = 0.5f - resonance_ * 0.48f;
        if (q_ < 0.02f) q_ = 0.02f;
        
        // Damping of the LP24's second section: as much as a single resonance
        // peak wants (kSecondStageDamping), limited by this topology's
        // stability condition f^2 + 2*f*q < 4 so the cascade stays stable at
        // the top of the cutoff range (f reaches 1.45 there, where the limit
        // is ~0.65). Above roughly f = 1.2 the peak therefore grows a little
        // again, but only where the filter is nearly open.
        const float limit = (4.0f - f_ * f_) / (2.0f * f_);
        q2_ = fminf(kSecondStageDamping, limit * 0.95f);
    }
    
    /**
     * @brief Process one sample
     */
    float Process(float input) {
        // First stage
        const float high = input - low_ - q_ * band_;
        band_ += f_ * high;
        low_ += f_ * band_;
        
        float output;
        
        switch (type_) {
            case FILTER_LP12:
                output = low_;
                break;
                
            case FILTER_LP24:
                // Second stage for 24dB
                {
                    // A 24dB/oct filter is two 12dB/oct sections in series, but
                    // a real 4-pole - the PPG's SSM2044 - has ONE resonance
                    // peak. Feeding the same damping into both sections squares
                    // the peak: measured 2.00x the 2-pole's at every resonance
                    // setting, i.e. twice the dB boost. q2_ damps the second
                    // section as far as stability allows, which leaves the peak
                    // to the first section (measured ratio 1.00 up to f = 1.2)
                    // while keeping the 24dB/oct slope.
                    const float high2 = low_ - low2_ - q2_ * band2_;
                    band2_ += f_ * high2;
                    low2_ += f_ * band2_;
                    output = low2_;
                }
                break;
                
            case FILTER_HP12:
                output = high;
                break;
                
            case FILTER_BP12:
                output = band_;
                break;
                
            default:
                output = low_;
        }
        
        return output;
    }
    
private:
    /**
     * @brief Damping of the LP24's second section
     *
     * Chosen by measurement (see Process()): at 1.0 the cascade's resonance
     * peak matches the 2-pole's instead of squaring it.
     */
    static constexpr float kSecondStageDamping = 1.0f;

    float sample_rate_;
    float inv_sample_rate_;
    
    float cutoff_;
    float resonance_;
    FilterType type_;
    
    // State variables (stage 1)
    float low_;
    float band_;
    
    // State variables (stage 2 for 24dB)
    float low2_;
    float band2_;
    
    // Coefficients
    float f_;   // Frequency coefficient
    float q_;   // Resonance/damping (first section)
    float q2_;  // Damping of the second section in LP24 mode
    
    // Set when cutoff_/resonance_ changed and f_/q_ need recomputing
    bool dirty_ = true;  // Initialized so a not-yet-Init()'d instance still computes
};
