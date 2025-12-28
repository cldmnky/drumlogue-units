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
        WAVEFORM_SAW_PWM = 4,   // PWM Sawtooth (Hoover sound) - NEW!
        WAVEFORM_SINE = 5,      // Sine wave (moved from 4)
        WAVEFORM_NOISE = 6      // White noise (moved from 5)
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

    /**
     * @brief Multiple oscillator state for vectorized processing
     */
    struct MultiOscState {
        float phase[4];           // Current phase for 4 oscillators
        float phase_inc[4];       // Phase increment per sample
        float pulse_width[4];     // Pulse width for each oscillator
        Waveform waveform[4];     // Waveform type for each oscillator
        float fm_amount[4];       // FM modulation amount
        bool sync_enabled[4];     // Sync enable flags
        float last_phase[4];      // For sync detection
    };

    /**
     * @brief Process multiple oscillators simultaneously (NEON optimized)
     * 
     * Processes up to 4 oscillators in parallel using NEON SIMD operations.
     * Useful for unison modes and multi-oscillator effects.
     * 
     * @param states Array of oscillator states (up to 4)
     * @param num_osc Number of oscillators to process (1-4)
     * @param outputs Output buffer for each oscillator
     * @param frames Number of samples to process
     * @param sync_trigger Optional sync trigger phase (0.0-1.0)
     */
    static void ProcessMultipleOscillators(MultiOscState* states, int num_osc,
                                          float* outputs[4], uint32_t frames,
                                          float sync_trigger = -1.0f);

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
    static float sine_table_[kWavetableSize + 1];     // Fast sine lookup
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
    static float LookupWavetable(const float* table, float phase);

    /**
     * @brief Generate waveform for multiple oscillator processing
     * @param phase Current phase (0.0-1.0)
     * @param phase_inc Phase increment per sample
     * @param waveform Waveform type
     * @param pulse_width Pulse width for pulse waves
     * @return Waveform value
     */
    static float GenerateWaveformForMulti(float phase, float phase_inc,
                                         Waveform waveform, float pulse_width);

    /**
     * @brief Get wavetable pointer for waveform type
     * @param waveform Waveform type
     * @return Pointer to wavetable
     */
    static const float* GetWavetable(Waveform waveform);

#ifdef NEON_DCO
    /**
     * @brief NEON-optimized processing for 4 oscillators
     */
    static void ProcessMultipleOscillatorsNEON(MultiOscState* state,
                                              float* outputs[4], uint32_t frames,
                                              float sync_trigger);
#endif

};

} // namespace dsp
