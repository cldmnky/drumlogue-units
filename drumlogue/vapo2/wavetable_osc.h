/**
 * @file wavetable_osc.h
 * @brief Wavetable oscillator wrapper for Vapo2
 *
 * Provides two oscillator implementations:
 * 1. WavetableOsc - Uses integrated wavetable synthesis with anti-aliasing
 *    (Franck & Välimäki DAFX-12 technique)
 * 2. PpgWavetableOsc - PPG Wave 2.2/2.3 style oscillator with 8-bit character
 *
 * The PPG oscillator provides the classic lo-fi, stepped waveform sound
 * characteristic of the original PPG Wave synthesizers.
 */

#pragma once

#include <cstdint>
#include "../common/wavetable_osc.h"
#include "../common/ppg_osc.h"
#include "resources/wavetables.h"

// Re-export interpolation functions for compatibility
using common::InterpolateWaveLinear;
using common::InterpolateWaveHermite;

// Backwards compatibility alias
inline float InterpolateWave(const int16_t* table, int32_t index, float frac) {
    return common::InterpolateWaveLinear(table, index, frac);
}

/**
 * @brief Wavetable oscillator configured for integrated wavetables
 *
 * Wraps the common::WavetableOsc with PPG-specific bank handling.
 * Uses integrated wavetable synthesis for anti-aliased playback.
 */
class WavetableOsc {
public:
    WavetableOsc() {}
    ~WavetableOsc() {}
    
    void Init() {
        osc_.Init();
    }
    
    void Reset() {
        osc_.Reset();
    }
    
    /**
     * @brief Set phase directly (useful for hard sync)
     */
    void SetPhase(float phase) {
        osc_.SetPhase(phase);
    }
    
    /**
     * @brief Get current phase
     */
    float GetPhase() const {
        return osc_.GetPhase();
    }
    
    /**
     * @brief Process one sample
     * @param frequency Normalized frequency (freq_hz / sample_rate)
     * @param morph Morph position (0.0 to 1.0)
     * @param bank Wavetable bank index (0 to WT_NUM_BANKS-1)
     * @return Audio sample (-1.0 to +1.0)
     */
    float Process(float frequency, float morph, int bank) {
        return osc_.Process(frequency, morph, GetWavetableBank(bank));
    }
    
    /**
     * @brief Process with high quality (Hermite) interpolation
     */
    float ProcessHQ(float frequency, float morph, int bank) {
        return osc_.ProcessHQ(frequency, morph, GetWavetableBank(bank));
    }
    
private:
    // Use common oscillator with PPG wavetable dimensions
    common::WavetableOsc<WT_TABLE_SIZE, WT_WAVES_PER_BANK> osc_;
    
    /**
     * @brief Get wavetable bank pointer array
     * @param bank Bank index (0 to WT_NUM_BANKS-1)
     * @return Pointer to array of wave pointers
     */
    const int16_t* const* GetWavetableBank(int bank) const {
        // Bounds check
        if (bank < 0) bank = 0;
        if (bank >= WT_NUM_BANKS) bank = WT_NUM_BANKS - 1;
        
        // Return bank from the master wavetable_banks array
        return wavetable_banks[bank];
    }
};

/**
 * @brief PPG Wave style oscillator for Vapo2
 *
 * Provides the classic PPG Wave 2.2/2.3 sound with:
 * - 8-bit waveforms with antisymmetric mirroring
 * - Three interpolation modes for different character
 * - Wavetable sweep with key-wave interpolation
 *
 * This produces a lo-fi, stepped sound characteristic of the
 * original PPG Wave synthesizers.
 */
class PpgWavetableOsc {
public:
    PpgWavetableOsc() {}
    ~PpgWavetableOsc() {}
    
    void Init(float sample_rate) {
        osc_.Init(sample_rate);
        sample_rate_ = sample_rate;
    }
    
    void Reset() {
        osc_.Reset();
    }
    
    void SetPhase(float phase) {
        osc_.SetPhase(phase);
    }
    
    float GetPhase() const {
        return osc_.GetPhase();
    }
    
    /**
     * @brief Set oscillator frequency in Hz
     */
    void SetFrequency(float freq) {
        osc_.SetFrequency(freq);
    }
    
    /**
     * @brief Set frequency from normalized value
     * @param frequency Normalized frequency (freq_hz / sample_rate)
     */
    void SetNormalizedFrequency(float frequency) {
        osc_.SetFrequency(frequency * sample_rate_);
    }
    
    /**
     * @brief Set wave position within current wavetable
     * @param pos Position (0.0 to 1.0)
     */
    void SetWavePosition(float pos) {
        osc_.SetWavePosition(pos);
    }
    
    /**
     * @brief Set interpolation mode
     * @param mode 0=full interpolation, 1=sample only, 2=no interpolation
     */
    void SetMode(uint8_t mode) {
        osc_.SetMode(static_cast<common::PpgMode>(mode));
    }
    
    /**
     * @brief Set phase skew for timbral variation
     * @param skew Skew amount (0.0 to 1.0, 0.5 = no skew)
     */
    void SetSkew(float skew) {
        osc_.SetSkew(skew);
    }
    
    /**
     * @brief Load a wavetable from definition
     * @param waves_data Pointer to raw 64-sample wave data (8-bit unsigned)
     * @param wavetable_def Array of (wave_index, position) pairs, terminated by 0xFF
     */
    void LoadWavetable(const uint8_t* waves_data, const uint8_t* wavetable_def) {
        osc_.LoadWavetable(waves_data, wavetable_def);
    }
    
    /**
     * @brief Process one sample
     * @return Audio sample (-1.0 to +1.0)
     */
    float Process() {
        return osc_.Process();
    }
    
    /**
     * @brief Process one sample (compatibility interface)
     * @param frequency Normalized frequency (freq_hz / sample_rate)
     * @param morph Morph/wave position (0.0 to 1.0)
     * @param bank Unused (wavetable must be loaded with LoadWavetable)
     * @return Audio sample (-1.0 to +1.0)
     */
    float Process(float frequency, float morph, int bank) {
        (void)bank;  // PPG osc uses loaded wavetable
        SetNormalizedFrequency(frequency);
        SetWavePosition(morph);
        return osc_.Process();
    }
    
private:
    common::PpgOsc<61> osc_;  // 61-position wavetable (PPG standard)
    float sample_rate_;
};

