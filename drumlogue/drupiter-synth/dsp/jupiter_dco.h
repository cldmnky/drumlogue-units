/**
 * @file jupiter_dco.h
 * @brief Digital Controlled Oscillator for Drupiter
 *
 * Based on Bristol junodco.c (Juno-style DCO used in Jupiter-8)
 * Implements phase accumulator with multiple waveforms
 */

#pragma once

#include <cstdint>
#include <cmath>

namespace dsp {

/**
 * @brief Jupiter-style Digital Controlled Oscillator
 * 
 * Features:
 * - Multiple waveforms (Ramp, Square, Pulse, Triangle)
 * - Pulse width modulation
 * - Hard sync capability
 * - FM cross-modulation input
 */
class JupiterDCO {
public:
    /**
     * @brief Waveform types
     */
    enum Waveform {
        WAVEFORM_SAW = 0,       // Sawtooth
        WAVEFORM_SQUARE = 1,    // Square wave (50% duty)
        WAVEFORM_PULSE = 2,     // Pulse wave (variable width)
        WAVEFORM_TRIANGLE = 3,  // Triangle wave
        WAVEFORM_SINE = 4,      // Sine wave
        WAVEFORM_NOISE = 5      // White noise
    };

    /**
     * @brief Constructor
     */
    JupiterDCO();
    
    /**
     * @brief Destructor
     */
    ~JupiterDCO();

    /**
     * @brief Initialize oscillator
     * @param sample_rate Sample rate in Hz (48000 for drumlogue)
     */
    void Init(float sample_rate);
    
    /**
     * @brief Set oscillator frequency
     * @param freq_hz Frequency in Hz
     */
    void SetFrequency(float freq_hz);
    
    /**
     * @brief Set waveform type
     * @param waveform Waveform selection
     */
    void SetWaveform(Waveform waveform);
    
    /**
     * @brief Set pulse width (for PULSE waveform)
     * @param pw Pulse width 0.0-1.0 (0.5 = square)
     */
    void SetPulseWidth(float pw);
    
    /**
     * @brief Enable/disable hard sync from another oscillator
     * @param enabled True to enable sync
     */
    void SetSyncEnabled(bool enabled);
    
    /**
     * @brief Apply FM modulation
     * @param fm_amount FM modulation amount (-1.0 to 1.0)
     */
    void ApplyFM(float fm_amount);
    
    /**
     * @brief Reset phase (for sync)
     */
    void ResetPhase();
    
    /**
     * @brief Get current phase (for syncing other oscillators)
     * @return Current phase 0.0-1.0
     */
    float GetPhase() const { return phase_; }
    
    /**
     * @brief Check if oscillator phase wrapped (for sync)
     * @return True if phase wrapped on last Process() call
     */
    bool DidWrap() const;
    
    /**
     * @brief Process one sample
     * @return Audio sample (-1.0 to 1.0)
     */
    float Process();

private:
    float sample_rate_;
    float phase_;              // Current phase (0.0-1.0)
    float phase_inc_;          // Phase increment per sample
    float base_freq_hz_;       // Base frequency
    float max_freq_hz_;        // Cached Nyquist-guarded max frequency
    Waveform waveform_;
    float pulse_width_;        // Pulse width (0.0-1.0)
    bool sync_enabled_;
    float fm_amount_;          // Current FM modulation
    float drift_phase_;        // Slow analog-style drift phase
    int drift_counter_;        // Counter for drift update
    float current_drift_;      // Current drift value
    uint32_t noise_seed_;      // For subtle drift noise
    uint32_t noise_seed2_;     // For noise waveform
    
    float last_phase_;         // For sync detection
    
    // Wavetable size (power of 2 for efficiency)
    static constexpr int kWavetableSize = 2048;
    static constexpr int kWavetableMask = kWavetableSize - 1;
    
    // Wavetables (static, shared across instances)
    static float ramp_table_[kWavetableSize + 1];     // +1 for interpolation
    static float square_table_[kWavetableSize + 1];
    static float triangle_table_[kWavetableSize + 1];
    static bool tables_initialized_;
    
    /**
     * @brief Initialize wavetables (called once)
     */
    static void InitWavetables();
    
    /**
     * @brief Generate waveform for given phase with bandlimiting
     * @param phase Current phase (0.0-1.0)
     * @param phase_inc Phase increment per sample (for polyBLEP)
     * @return Waveform value at given phase
     */
    float GenerateWaveform(float phase, float phase_inc);
    
    /**
     * @brief Wavetable lookup with linear interpolation
     * @param table Wavetable to read from
     * @param phase Phase 0.0-1.0
     * @return Interpolated value
     */
    float LookupWavetable(const float* table, float phase);
};

} // namespace dsp
