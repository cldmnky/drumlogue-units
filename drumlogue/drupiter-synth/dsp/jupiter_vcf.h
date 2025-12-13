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
    
    // Chamberlin state-variable filter states
    float lp_;                  // Low-pass output
    float bp_;                  // Band-pass output
    float hp_;                  // High-pass output
    
    // Filter coefficients
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
