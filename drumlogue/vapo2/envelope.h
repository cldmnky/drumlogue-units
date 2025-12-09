/**
 * @file envelope.h
 * @brief ADSR envelope generator for Vapo2
 *
 * Analog-style exponential curves with configurable attack/decay/release times.
 * Uses true RC-circuit exponential curves like classic analog synthesizers:
 * - Attack: Overshoots to ~1.63x target for punchy response
 * - Decay/Release: Exponential fall toward target with curvature control
 */

#pragma once

#include <cstdint>
#include <cmath>

/**
 * @brief Envelope stages
 */
enum EnvelopeStage {
    ENV_IDLE = 0,
    ENV_ATTACK,
    ENV_DECAY,
    ENV_SUSTAIN,
    ENV_RELEASE
};

/**
 * @brief ADSR envelope generator with true analog-style exponential curves
 * 
 * Models the charging/discharging behavior of an RC circuit.
 * Attack overshoots to create punchy transients (like Minimoog/Prophet).
 * Decay and Release use exponential curves for natural sound.
 */
class ADSREnvelope {
public:
    ADSREnvelope() {}
    ~ADSREnvelope() {}
    
    void Init(float sample_rate) {
        sample_rate_ = sample_rate;
        inv_sample_rate_ = 1.0f / sample_rate;
        
        // Calculate exponential coefficients for default times
        SetAttack(5);    // Fast attack
        SetDecay(40);    // Medium decay
        SetSustain(0.6f);
        SetRelease(30);  // Medium release
        
        stage_ = ENV_IDLE;
        value_ = 0.0f;
        
        // Curvature controls (adjustable for different character)
        // Higher values = more curved/snappy, lower = more linear
        attack_curve_ = 0.3f;  // Slight curve for attack (more linear = punchier)
        decay_curve_ = 3.0f;   // Strong curve for decay (exponential fall)
        release_curve_ = 3.0f; // Strong curve for release
    }
    
    void Reset() {
        stage_ = ENV_IDLE;
        value_ = 0.0f;
    }
    
    /**
     * @brief Set attack time from 0-127 parameter value
     */
    void SetAttack(int32_t param) {
        // Convert 0-127 to time in seconds (1ms to 8s, exponential mapping)
        float time = ParameterToTime(param, 0.001f, 8.0f);
        // Calculate coefficient for exponential rise
        // We target a value slightly above 1.0 to overshoot, creating punch
        attack_coef_ = CalcCoef(time, attack_curve_);
    }
    
    /**
     * @brief Set decay time from 0-127 parameter value
     */
    void SetDecay(int32_t param) {
        float time = ParameterToTime(param, 0.005f, 12.0f);
        decay_coef_ = CalcCoef(time, decay_curve_);
    }
    
    /**
     * @brief Set sustain level (0.0 to 1.0)
     */
    void SetSustain(float level) {
        sustain_level_ = level < 0.0f ? 0.0f : (level > 1.0f ? 1.0f : level);
    }
    
    /**
     * @brief Set release time from 0-127 parameter value
     */
    void SetRelease(int32_t param) {
        float time = ParameterToTime(param, 0.005f, 12.0f);
        release_coef_ = CalcCoef(time, release_curve_);
    }
    
    /**
     * @brief Trigger gate on/off
     */
    void Gate(bool on) {
        if (on) {
            stage_ = ENV_ATTACK;
        } else {
            if (stage_ != ENV_IDLE) {
                stage_ = ENV_RELEASE;
            }
        }
    }
    
    /**
     * @brief Process one sample using true exponential curves
     * @param gate Current gate state (for sustain)
     * @return Envelope value (0.0 to 1.0)
     * 
     * Attack: value = 1 - (1-value) * coef  (exponential rise toward overshoot)
     * Decay:  value = sustain + (value - sustain) * coef  (exponential fall)
     * Release: value = value * coef  (exponential fall to zero)
     */
    float Process(bool gate) {
        switch (stage_) {
            case ENV_IDLE:
                value_ = 0.0f;
                break;
                
            case ENV_ATTACK: {
                // Exponential approach to overshoot target (~1.5x)
                // This creates the "punch" of analog envelopes
                const float overshoot_target = 1.0f + attack_curve_;
                value_ = overshoot_target - (overshoot_target - value_) * attack_coef_;
                
                if (value_ >= 1.0f) {
                    value_ = 1.0f;
                    stage_ = ENV_DECAY;
                }
                break;
            }
                
            case ENV_DECAY: {
                // Exponential decay toward sustain level
                value_ = sustain_level_ + (value_ - sustain_level_) * decay_coef_;
                
                // Check if we've reached sustain (within small epsilon)
                if (value_ <= sustain_level_ + 0.0001f) {
                    value_ = sustain_level_;
                    stage_ = ENV_SUSTAIN;
                }
                break;
            }
                
            case ENV_SUSTAIN:
                value_ = sustain_level_;
                if (!gate) {
                    stage_ = ENV_RELEASE;
                }
                break;
                
            case ENV_RELEASE: {
                // Exponential decay toward zero
                value_ *= release_coef_;
                
                // Cutoff at very low level to avoid denormals
                if (value_ < 0.0001f) {
                    value_ = 0.0f;
                    stage_ = ENV_IDLE;
                }
                break;
            }
        }
        
        return value_;
    }
    
    /**
     * @brief Check if envelope is active (not idle)
     */
    bool IsActive() const {
        return stage_ != ENV_IDLE;
    }
    
    EnvelopeStage GetStage() const {
        return stage_;
    }
    
    float GetValue() const {
        return value_;
    }

private:
    float sample_rate_;
    float inv_sample_rate_;
    
    // Exponential coefficients (computed from time constants)
    float attack_coef_;
    float decay_coef_;
    float release_coef_;
    float sustain_level_;
    
    // Curve shape parameters
    float attack_curve_;
    float decay_curve_;
    float release_curve_;
    
    EnvelopeStage stage_;
    float value_;
    
    /**
     * @brief Convert 0-127 parameter to time in seconds
     * 
     * Exponential mapping for perceptually linear control
     */
    float ParameterToTime(int32_t param, float min_time, float max_time) {
        if (param <= 0) return min_time;
        if (param >= 127) return max_time;
        const float norm = param / 127.0f;
        // Attempt at more perceptually linear: use pow curve
        return min_time * powf(max_time / min_time, norm * norm * 0.8f + norm * 0.2f);
    }
    
    /**
     * @brief Calculate exponential coefficient for given time and curve
     * 
     * The coefficient determines how quickly we approach the target.
     * coef = exp(-1 / (time * sample_rate * curve_factor))
     * 
     * This models an RC circuit where the time constant tau = R*C
     * After one time constant, the value reaches ~63.2% of target.
     */
    float CalcCoef(float time_seconds, float curve) {
        if (time_seconds <= 0.0f) return 0.0f;
        // Number of samples for this time
        float samples = time_seconds * sample_rate_;
        // Curve factor affects how many time constants fit in the time
        // Higher curve = faster initial movement, slower tail
        float tau = samples / curve;
        if (tau < 1.0f) tau = 1.0f;
        return expf(-1.0f / tau);
    }
};
