/**
 * @file unison_oscillator.h
 * @brief Unison oscillator with golden ratio detune spread
 * 
 * Creates supersaw/hoover effect by stacking multiple detuned oscillators.
 * Uses golden ratio (φ = 1.618...) for natural, non-beating detune distribution.
 * 
 * @author Drupiter Hoover Synthesis Team
 * @date 2025-12-25
 * @version 2.0.0-dev
 */

#pragma once

#include <cstdint>
#include <cmath>
#include "jupiter_dco.h"

namespace dsp {

/**
 * @brief Unison oscillator for hoover/supersaw sounds
 * 
 * Stacks multiple JupiterDCO oscillators with golden ratio detune spread.
 * The golden ratio (φ ≈ 1.618) creates natural, non-periodic detuning
 * that avoids beating and phasing artifacts.
 * 
 * Detune pattern (for 7 voices):
 *   Voice 0: Center (no detune)
 *   Voice 1: +detune * φ^0
 *   Voice 2: -detune * φ^0
 *   Voice 3: +detune * φ^1
 *   Voice 4: -detune * φ^1
 *   Voice 5: +detune * φ^2
 *   Voice 6: -detune * φ^2
 * 
 * This creates a rich, non-beating chorus effect ideal for hoover sounds.
 */
class UnisonOscillator {
public:
    static constexpr uint8_t kMaxVoices = 7;
    static constexpr float kGoldenRatio = 1.618033988749895f;
    
    /**
     * @brief Constructor
     */
    UnisonOscillator();
    
    /**
     * @brief Destructor
     */
    ~UnisonOscillator();
    
    /**
     * @brief Initialize unison oscillator
     * @param sample_rate Sample rate in Hz
     * @param num_voices Number of voices (3-7, must be odd for center voice)
     */
    void Init(float sample_rate, uint8_t num_voices = 7);
    
    /**
     * @brief Set base frequency for all voices
     * @param freq_hz Frequency in Hz (center pitch)
     */
    void SetFrequency(float freq_hz);
    
    /**
     * @brief Set waveform for all voices
     * @param waveform Waveform type
     */
    void SetWaveform(JupiterDCO::Waveform waveform);
    
    /**
     * @brief Set pulse width for all voices
     * @param pw Pulse width 0.0-1.0
     */
    void SetPulseWidth(float pw);
    
    /**
     * @brief Set detune amount
     * @param detune_cents Maximum detune in cents (typical: 5-20 cents)
     */
    void SetDetune(float detune_cents);
    
    /**
     * @brief Set stereo spread amount
     * @param spread 0.0-1.0 (0=mono, 1=full stereo)
     */
    void SetStereoSpread(float spread);
    
    /**
     * @brief Process one sample (stereo output)
     * @param left Output left channel
     * @param right Output right channel
     */
    inline void Process(float* left, float* right) {
        *left = 0.0f;
        *right = 0.0f;
        
        // Mix all voices with pan positioning
        for (uint8_t i = 0; i < num_voices_; i++) {
            float sample = oscillators_[i].Process();
            
            // Pan based on voice index (golden ratio spiral for natural spread)
            float pan = voice_pans_[i] * stereo_spread_;
            float pan_left = (1.0f - pan) * 0.5f + 0.5f;
            float pan_right = pan * 0.5f + 0.5f;
            
            *left += sample * pan_left;
            *right += sample * pan_right;
        }
        
        // Scale by voice count to prevent clipping
        float scale = 1.0f / sqrtf(static_cast<float>(num_voices_));
        *left *= scale;
        *right *= scale;
    }
    
    /**
     * @brief Reset oscillator phases (for hard sync)
     */
    void Reset();
    
    /**
     * @brief Get number of active voices
     */
    uint8_t GetNumVoices() const { return num_voices_; }
    
private:
    JupiterDCO oscillators_[kMaxVoices];  // Voice pool
    float voice_detunes_[kMaxVoices];     // Detune multipliers
    float voice_pans_[kMaxVoices];        // Pan positions (-1 to +1)
    
    uint8_t num_voices_;                  // Active voice count (3-7)
    float sample_rate_;                   // Sample rate
    float base_freq_;                     // Center frequency
    float detune_cents_;                  // Detune amount in cents
    float stereo_spread_;                 // Stereo width (0-1)
    
    /**
     * @brief Calculate detune ratios using golden ratio spread
     */
    void CalculateDetuneRatios();
    
    /**
     * @brief Calculate pan positions using golden ratio spiral
     */
    void CalculatePanPositions();
    
    /**
     * @brief Convert cents to frequency ratio
     * @param cents Detune in cents
     * @return Frequency ratio (1.0 = no detune)
     */
    static inline float CentsToRatio(float cents) {
        // 2^(cents/1200)
        return powf(2.0f, cents / 1200.0f);
    }
};

}  // namespace dsp
