/**
 * @file jupiter_lfo.h
 * @brief Low Frequency Oscillator for Drupiter
 *
 * Based on Bristol lfo.c
 * Multi-waveform LFO with delay envelope
 */

#pragma once

#include <cstdint>
#include <cmath>

namespace dsp {

/**
 * @brief Jupiter-style Low Frequency Oscillator
 * 
 * Features:
 * - Multiple waveforms (Triangle, Ramp, Square, Sample & Hold)
 * - Variable rate (0.1Hz to ~50Hz)
 * - Delay envelope (fade-in from 0)
 * - Retriggerable on note-on
 */
class JupiterLFO {
public:
    /**
     * @brief LFO waveforms
     */
    enum Waveform {
        WAVEFORM_TRIANGLE = 0,   // Triangle wave
        WAVEFORM_RAMP = 1,       // Ramp/sawtooth wave
        WAVEFORM_SQUARE = 2,     // Square wave
        WAVEFORM_SAMPLE_HOLD = 3 // Random sample & hold
    };

    /**
     * @brief Constructor
     */
    JupiterLFO();
    
    /**
     * @brief Destructor
     */
    ~JupiterLFO();

    /**
     * @brief Initialize LFO
     * @param sample_rate Sample rate in Hz
     */
    void Init(float sample_rate);
    
    /**
     * @brief Set LFO frequency
     * @param freq_hz Frequency in Hz (0.1 - 50Hz typical)
     */
    void SetFrequency(float freq_hz);
    
    /**
     * @brief Set waveform type
     * @param waveform Waveform selection
     */
    void SetWaveform(Waveform waveform);
    
    /**
     * @brief Set delay time (fade-in duration)
     * @param delay_sec Delay time in seconds (0-10s)
     */
    void SetDelay(float delay_sec);
    
    /**
     * @brief Set key trigger enable/disable
     * @param enable If true, LFO phase resets on note-on
     */
    void SetKeyTrigger(bool enable);
    
    /**
     * @brief Get key trigger state
     * @return true if key trigger is enabled
     */
    bool GetKeyTrigger() const { return key_trigger_; }
    
    /**
     * @brief Trigger LFO (reset delay envelope and optionally phase)
     */
    void Trigger();
    
    /**
     * @brief Reset LFO phase and delay
     */
    void Reset();
    
    /**
     * @brief Process one sample
     * @return LFO value -1.0 to +1.0 (scaled by delay envelope)
     */
    float Process();

private:
    float sample_rate_;
    float phase_;               // Current phase (0.0-1.0)
    float phase_inc_;           // Phase increment per sample
    Waveform waveform_;
    bool key_trigger_;          // If true, reset phase on Trigger()
    
    // Delay envelope
    float delay_phase_;         // Delay envelope phase (0.0-1.0)
    float delay_inc_;           // Delay increment per sample
    float delay_time_;          // Delay time in seconds
    
    // Sample & hold
    float sh_value_;            // Current S&H value
    uint32_t rand_seed_;        // Random number generator seed
    
    // Frequency limits
    static constexpr float kMinFreq = 0.1f;    // 0.1 Hz
    static constexpr float kMaxFreq = 50.0f;   // 50 Hz
    
    /**
     * @brief Generate waveform for current phase
     * @return Waveform value -1.0 to +1.0
     */
    float GenerateWaveform();
    
    /**
     * @brief Generate random value for S&H
     * @return Random value -1.0 to +1.0
     */
    float GenerateRandom();
    
    /**
     * @brief Clamp frequency to valid range
     * @param freq Frequency to clamp
     * @return Clamped frequency
     */
    float ClampFrequency(float freq) const;
};

} // namespace dsp
