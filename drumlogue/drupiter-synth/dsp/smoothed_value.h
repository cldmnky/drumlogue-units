/**
 * @file smoothed_value.h
 * @brief One-pole smoothed parameter value for zipper-free parameter changes
 *
 * Provides smooth interpolation for continuous parameters to eliminate
 * audible stepping/zipper noise when knobs are turned.
 */

#pragma once

#include <cstdint>

namespace dsp {

/**
 * @brief One-pole lowpass smoothed value
 *
 * Uses a simple one-pole filter to smoothly interpolate between
 * target values. Good for parameters like cutoff, mix, and level.
 */
class SmoothedValue {
public:
    SmoothedValue() : value_(0.0f), target_(0.0f), coef_(0.01f) {}
    
    /**
     * @brief Initialize with starting value and smoothing coefficient
     * @param initial Initial value
     * @param coef Smoothing coefficient (0.001 = slow, 0.1 = fast)
     */
    void Init(float initial = 0.0f, float coef = 0.01f) {
        value_ = target_ = initial;
        coef_ = coef;
    }
    
    /**
     * @brief Set the target value to smooth towards
     */
    void SetTarget(float target) {
        target_ = target;
    }
    
    /**
     * @brief Set value immediately without smoothing
     */
    void SetImmediate(float value) {
        value_ = target_ = value;
    }
    
    /**
     * @brief Process one sample of smoothing
     * @return Current smoothed value
     */
    float Process() {
        value_ += coef_ * (target_ - value_);
        return value_;
    }
    
    /**
     * @brief Get current value without processing
     */
    float GetValue() const {
        return value_;
    }
    
    /**
     * @brief Get target value
     */
    float GetTarget() const {
        return target_;
    }
    
    /**
     * @brief Check if value has reached target (within epsilon)
     */
    bool HasReachedTarget(float epsilon = 0.0001f) const {
        float diff = target_ - value_;
        return (diff > -epsilon && diff < epsilon);
    }
    
private:
    float value_;
    float target_;
    float coef_;
};

} // namespace dsp
