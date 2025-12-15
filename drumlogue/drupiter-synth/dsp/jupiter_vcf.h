/**
 * @file jupiter_vcf.h
 * @brief Voltage Controlled Filter for Drupiter
 *
 * Based on Bristol filter.c (Chamberlin state-variable filter)
 * Multi-mode filter implementation
 */

#pragma once

#include <cstdint>
#include <cmath>

namespace dsp {

/**
 * @brief Jupiter-style Voltage Controlled Filter
 * 
 * Chamberlin state-variable filter providing multiple modes:
 * - LP12: 12dB/oct low-pass
 * - LP24: 24dB/oct low-pass (cascaded)
 * - HP12: 12dB/oct high-pass
 * - BP12: 12dB/oct band-pass
 */
class JupiterVCF {
public:
    // Oversampling factor (4x for stability and extended frequency range)
    static constexpr int kOversamplingFactor = 4;
    
    /**
     * @brief Filter modes
     */
    enum Mode {
        MODE_LP12 = 0,   // Low-pass 12dB/oct
        MODE_LP24 = 1,   // Low-pass 24dB/oct
        MODE_HP12 = 2,   // High-pass 12dB/oct
        MODE_BP12 = 3    // Band-pass 12dB/oct
    };

    /**
     * @brief Constructor
     */
    JupiterVCF();
    
    /**
     * @brief Destructor
     */
    ~JupiterVCF();

    /**
     * @brief Initialize filter
     * @param sample_rate Sample rate in Hz
     */
    void Init(float sample_rate);
    
    /**
     * @brief Set filter cutoff frequency
     * @param freq_hz Cutoff frequency in Hz (20-20000)
     */
    void SetCutoff(float freq_hz);

    /**
     * @brief Set modulated cutoff frequency without changing base cutoff
     * @param freq_hz Cutoff frequency in Hz (20-20000)
     */
    void SetCutoffModulated(float freq_hz);
    
    /**
     * @brief Set filter resonance
     * @param resonance Resonance amount 0.0-1.0
     */
    void SetResonance(float resonance);
    
    /**
     * @brief Set filter mode
     * @param mode Filter mode (LP12/LP24/HP12/BP12)
     */
    void SetMode(Mode mode);
    
    /**
     * @brief Set keyboard tracking amount
     * @param amount Tracking amount 0.0-1.0
     */
    void SetKeyboardTracking(float amount);
    
    /**
     * @brief Apply keyboard tracking for a note
     * @param note MIDI note number
     */
    void ApplyKeyboardTracking(uint8_t note);
    
    /**
     * @brief Process one sample
     * @param input Input sample
     * @return Filtered sample
     */
    float Process(float input);
    
    /**
     * @brief Reset filter state
     */
    void Reset();

private:
    float sample_rate_;
    float cutoff_hz_;
    float base_cutoff_hz_;     // Base cutoff without modulation
    float resonance_;
    Mode mode_;
    float kbd_tracking_;        // Keyboard tracking amount

    // Krajeski-style improved ladder filter states
    // 4 cascaded one-pole filters with delay elements for "compromise" poles
    float ota_s1_;              // State for stage 1
    float ota_s2_;              // State for stage 2
    float ota_s3_;              // State for stage 3
    float ota_s4_;              // State for stage 4
    float ota_d1_;              // Delay for stage 1 (z^-1)
    float ota_d2_;              // Delay for stage 2 (z^-1)
    float ota_d3_;              // Delay for stage 3 (z^-1)
    float ota_d4_;              // Delay for stage 4 (z^-1)
    float ota_y2_;              // LP12 output (after stage 2)
    float ota_y4_;              // LP24 output (after stage 4)
    float ota_a_;               // Filter coefficient (g) with polynomial correction
    float ota_res_k_;           // Resonance feedback gain with decoupling
    float ota_gain_comp_;       // Resonance gain compensation factor
    float ota_output_gain_;     // Output gain to compensate passband loss
    float ota_drive_;           // Input drive (unused in current impl)

    // Non-resonant 6dB high-pass (implemented as 1-pole low-pass + subtraction)
    float hp_lp_state_;
    float hp_a_;
    
    // Chamberlin state-variable filter states (for BP mode)
    float lp_;                  // Low-pass output
    float bp_;                  // Band-pass output
    float hp_;                  // High-pass output
    
    // Filter coefficients (SVF)
    float f_;                   // Frequency coefficient
    float q_;                   // Q coefficient (resonance)
    
    // For 24dB/oct mode (cascaded filters)
    float lp2_;
    float bp2_;
    
    /**
     * @brief Update filter coefficients from cutoff/resonance
     */
    void UpdateCoefficients();
    
    /**
     * @brief Clamp cutoff to valid range
     * @param freq Frequency to clamp
     * @return Clamped frequency
     */
    float ClampCutoff(float freq) const;
};

} // namespace dsp
