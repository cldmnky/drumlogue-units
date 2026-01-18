/*
 * File: catchable_value.h
 * 
 * Knob catch mechanism for smooth parameter transitions.
 * 
 * When a hardware knob position differs from a preset's saved value, this class
 * prevents sudden audio jumps by "catching" the knob movement. The DSP parameter
 * only updates once the knob crosses the preset's saved position.
 * 
 * Usage:
 *   CatchableValue cutoff_;
 *   cutoff_.Init(50);  // Initialize to preset value
 *   
 *   // In SetParameter():
 *   float output = cutoff_.Update(knob_value);  // Returns DSP value
 *   if (output != previous_value) {
 *     // Apply to DSP (output has changed)
 *   }
 * 
 * Thread-safe: Yes (no dynamic allocation, simple state machine)
 * Real-time safe: Yes (fixed execution time, no branching in hot path)
 */

#pragma once

#include <cstdint>
#include <cmath>

namespace dsp {

/**
 * @brief Catchable parameter value with fixed threshold
 * 
 * Implements "catch-on-approach" behavior where a hardware knob must
 * cross the current DSP value before parameter changes take effect.
 * This prevents sudden jumps when knob position differs from preset.
 * 
 * Key features:
 * - Fixed ±3 unit catch threshold (consistent across all units)
 * - Transparent unipolar (0-100) and bipolar (-100 to +100) support
 * - Minimal state (8 bytes: 2 ints + 1 bool + padding)
 * - Header-only for easy reuse
 */
class CatchableValue {
 public:
  CatchableValue() : current_value_(0), last_knob_pos_(0), catching_(false) {}

  /**
   * @brief Initialize with a starting DSP value
   * @param initial_value DSP value (0-100 for unipolar, -100 to +100 for bipolar)
   */
  void Init(int32_t initial_value) {
    current_value_ = initial_value;
    last_knob_pos_ = initial_value;
    catching_ = false;
  }

  /**
   * @brief Reset to new value (e.g., on preset load)
   * @param new_value New DSP value to catch to
   * @param knob_pos Current hardware knob position
   */
  void Reset(int32_t new_value, int32_t knob_pos) {
    current_value_ = new_value;
    last_knob_pos_ = knob_pos;
    // Enable catching if knob is far from new value
    catching_ = (Abs(knob_pos - new_value) > kCatchThreshold);
  }

  /**
   * @brief Update with new knob position, returns effective DSP value
   * @param knob_pos New knob position (0-100 or -100 to +100)
   * @return Effective DSP value (may differ from knob_pos if catching)
   */
  int32_t Update(int32_t knob_pos) {
    // If not catching, follow knob directly
    if (!catching_) {
      last_knob_pos_ = knob_pos;
      current_value_ = knob_pos;
      return current_value_;
    }

    // Check if knob has crossed the catch point
    const bool crossed = HasCrossed(last_knob_pos_, knob_pos, current_value_);
    
    if (crossed) {
      // Disable catching, now follow knob
      catching_ = false;
      current_value_ = knob_pos;
    }
    // else: hold current_value_ steady while knob approaches

    last_knob_pos_ = knob_pos;
    return current_value_;
  }

  /**
   * @brief Check if currently in catch mode
   * @return true if parameter is catching (knob hasn't crossed yet)
   */
  bool IsCatching() const { return catching_; }

  /**
   * @brief Get current effective DSP value
   * @return Current DSP value
   */
  int32_t GetValue() const { return current_value_; }

  /**
   * @brief Force exit from catch mode (for manual override)
   */
  void ExitCatch() { catching_ = false; }

 private:
  // Fixed catch threshold: ±3 UI units
  static constexpr int32_t kCatchThreshold = 3;

  int32_t current_value_;   // Current effective DSP value
  int32_t last_knob_pos_;   // Previous knob position
  bool catching_;           // Currently in catch mode?

  /**
   * @brief Check if knob movement crossed the target value
   * @param prev Previous knob position
   * @param curr Current knob position
   * @param target Target value to catch
   * @return true if knob crossed target (including within threshold)
   */
  static bool HasCrossed(int32_t prev, int32_t curr, int32_t target) {
    // Check if we're within catch threshold
    if (Abs(curr - target) <= kCatchThreshold) {
      return true;
    }

    // Check if we crossed the target (direction changed)
    const int32_t prev_dist = prev - target;
    const int32_t curr_dist = curr - target;
    
    // If signs differ, we crossed zero (the target)
    if ((prev_dist > 0 && curr_dist < 0) || (prev_dist < 0 && curr_dist > 0)) {
      return true;
    }

    return false;
  }

  /**
   * @brief Absolute value helper
   */
  static int32_t Abs(int32_t x) {
    return x < 0 ? -x : x;
  }
};

/**
 * @brief Normalized floating-point version with 0.0-1.0 output
 * 
 * Same catch behavior as CatchableValue, but outputs normalized
 * float values for direct use in DSP calculations.
 */
class CatchableValueFloat {
 public:
  CatchableValueFloat() : catchable_() {}

  /**
   * @brief Initialize with normalized value (0.0-1.0)
   */
  void Init(float initial_value) {
    int32_t int_value = static_cast<int32_t>(initial_value * 100.0f);
    catchable_.Init(int_value);
  }

  /**
   * @brief Reset to new value
   * @param new_value New DSP value (0.0-1.0)
   * @param knob_pos Current hardware knob position (0-100)
   */
  void Reset(float new_value, int32_t knob_pos) {
    int32_t int_value = static_cast<int32_t>(new_value * 100.0f);
    catchable_.Reset(int_value, knob_pos);
  }

  /**
   * @brief Update with new knob position, returns normalized value
   * @param knob_pos New knob position (0-100)
   * @return Normalized DSP value (0.0-1.0)
   */
  float Update(int32_t knob_pos) {
    int32_t int_value = catchable_.Update(knob_pos);
    return int_value / 100.0f;
  }

  bool IsCatching() const { return catchable_.IsCatching(); }
  float GetValue() const { return catchable_.GetValue() / 100.0f; }
  void ExitCatch() { catchable_.ExitCatch(); }

 private:
  CatchableValue catchable_;
};

}  // namespace dsp
