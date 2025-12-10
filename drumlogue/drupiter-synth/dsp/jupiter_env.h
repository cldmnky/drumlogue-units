/**
 * @file jupiter_env.h
 * @brief ADSR Envelope Generator for Drupiter
 *
 * Based on Bristol envelope.c
 * Classic ADSR envelope with exponential curves
 */

#pragma once

#include <cstdint>
#include <cmath>

namespace dsp {

/**
 * @brief Jupiter-style ADSR Envelope Generator
 * 
 * Four-stage envelope: Attack, Decay, Sustain, Release
 * Exponential curves for analog-like behavior
 */
class JupiterEnvelope {
public:
    /**
     * @brief Envelope states
     */
    enum State {
        STATE_IDLE = 0,      // Not triggered
        STATE_ATTACK,        // Attack phase
        STATE_DECAY,         // Decay phase
        STATE_SUSTAIN,       // Sustain phase (holds at sustain level)
        STATE_RELEASE        // Release phase
    };

    /**
     * @brief Constructor
     */
    JupiterEnvelope();
    
    /**
     * @brief Destructor
     */
    ~JupiterEnvelope();

    /**
     * @brief Initialize envelope
     * @param sample_rate Sample rate in Hz
     */
    void Init(float sample_rate);
    
    /**
     * @brief Set attack time
     * @param time_sec Attack time in seconds
     */
    void SetAttack(float time_sec);
    
    /**
     * @brief Set decay time
     * @param time_sec Decay time in seconds
     */
    void SetDecay(float time_sec);
    
    /**
     * @brief Set sustain level
     * @param level Sustain level 0.0-1.0
     */
    void SetSustain(float level);
    
    /**
     * @brief Set release time
     * @param time_sec Release time in seconds
     */
    void SetRelease(float time_sec);
    
    /**
     * @brief Trigger note on (start attack phase)
     * @param velocity Note velocity 0.0-1.0
     */
    void NoteOn(float velocity = 1.0f);
    
    /**
     * @brief Trigger note off (start release phase)
     */
    void NoteOff();
    
    /**
     * @brief Get current envelope state
     * @return Current state
     */
    State GetState() const { return state_; }
    
    /**
     * @brief Check if envelope is active
     * @return True if not idle
     */
    bool IsActive() const { return state_ != STATE_IDLE; }
    
    /**
     * @brief Process one sample
     * @return Current envelope value 0.0-1.0
     */
    float Process();
    
    /**
     * @brief Reset envelope to idle state
     */
    void Reset();

private:
    float sample_rate_;
    State state_;
    float current_level_;       // Current envelope output (0.0-1.0)
    float target_level_;        // Target level for current stage
    float velocity_;            // Note velocity
    
    // ADSR parameters (in seconds)
    float attack_time_;
    float decay_time_;
    float sustain_level_;
    float release_time_;
    
    // Precalculated rates (increment per sample)
    float attack_rate_;
    float decay_rate_;
    float release_rate_;
    
    // Minimum/maximum times
    static constexpr float kMinTime = 0.001f;    // 1ms
    static constexpr float kMaxTime = 10.0f;     // 10 seconds
    
    /**
     * @brief Update rate calculations from time parameters
     */
    void UpdateRates();
    
    /**
     * @brief Calculate rate from time
     * @param time_sec Time in seconds
     * @return Rate (increment per sample)
     */
    float TimeToRate(float time_sec) const;
};

} // namespace dsp
