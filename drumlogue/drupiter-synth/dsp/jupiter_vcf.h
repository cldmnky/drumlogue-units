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
#include "../../common/dsp_utils.h"

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
    // Oversampling factor (2x is sufficient with improved ladder topology)
    static constexpr int kOversamplingFactor = 2;
    
    // Lookup table sizes
    static constexpr int kTanhTableSize = 512;        // Tanh[-4,4] with 512 entries
    static constexpr int kKbdTrackingTableSize = 128;  // One entry per MIDI note
    
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
     */
    void Reset();

private:
    // Lookup table initialization flag
    bool tables_initialized_;   // Track if lookup tables are ready
    
    float sample_rate_;
    float cutoff_hz_;
    float base_cutoff_hz_;     // Base cutoff without modulation
    float resonance_;
    Mode mode_;
    float kbd_tracking_;        // Keyboard tracking amount
    bool coefficients_dirty_;   // Flag to batch coefficient updates

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
    float ota_g_;               // Filter coefficient (g) with polynomial correction
    float ota_res_k_;           // Resonance feedback gain with decoupling
    float ota_gain_comp_;       // Resonance gain compensation factor
    float ota_output_gain_;     // Output gain to compensate passband loss

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
    
    /**
     * @brief Lookup-based tanh approximation (optimized)
     * @param x Input value
     * @return tanh(x) approximation from lookup table
     */
    float TanhLookup(float x) const;

private:
    // Lookup tables for optimization
    static float s_tanh_table[kTanhTableSize];
    static float s_kbd_tracking_table[kKbdTrackingTableSize];
    static bool s_tables_initialized;
    
    /**
     * @brief Initialize lookup tables (called from Init())
     */
    void InitializeLookupTables();
    
    /**
     * @brief Flush denormal numbers conditionally
     * @param x Value to check and flush
     * @return Zero if denormal, original value otherwise
     */
    static inline float FlushDenormalConditional(float x) {
        return (fabsf(x) < 1e-15f) ? 0.0f : x;
    }
};

} // namespace dsp
