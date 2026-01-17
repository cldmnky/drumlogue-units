/**
 * @file hub_control.h
 * @brief Hub control system for compressing multiple related parameters
 *
 * A hub control consists of:
 * - Selector parameter: chooses destination (0-N)
 * - Value parameter: sets value for selected destination
 *
 * This allows N parameters to be controlled with just 2 UI slots.
 * 
 * **Catch Behavior**: When you set a value for a destination, that modulation
 * value is "caught" and preserved. When switching to another destination that
 * has been caught, the hub value automatically adjusts to maintain the same
 * modulation output, preventing sudden jumps.
 * 
 * Example usage:
 * @code
 * static constexpr HubControl<8>::Destination kModDests[] = {
 *   {"LFO>PWM", "%",   0, 100, 0,  false},
 *   {"ENV>VCF", "%",   0, 100, 50, true},  // bipolar
 * };
 * HubControl<8> mod_hub{kModDests};
 * @endcode
 *
 * Usage notes:
 * 1. Populate each {@link Destination} so the hub knows how to clamp and format values.
 * 2. Call {@link SetDestination} when the selector parameter changes (e.g., user picks a new MOD target).
 * 3. Feed raw 0–100 slider values from the UI into {@link SetValue}. The hub stores the UI intent and
 *    converts it to the destination-specific range (visible via {@link GetValue}).
 * 4. Use {@link GetCurrentValueString} or {@link GetValueString} when rendering the parameter value; the
 *    returned pointer is stable and safe to cache, which prevents screen flicker.
 * 5. If you need the original UI slider position, read {@link GetOriginalValue}. This is distinct from the
 *    destination-clamped value returned by {@link GetValue}.
 * 6. For immediate DSP updates, prefer {@link SetValueAndGetClamped} or
 *    {@link SetValueForDestAndGetClamped} so you can act on the clamped destination value.
 *    to cache, which prevents screen flicker.
 * 7. In DSP code, read {@link GetValue} or {@link GetCurrentValue} to obtain the clamped value for the
 *    currently selected destination, or {@link GetValue} with a destination index for others.
 * 8. **Catch behavior**: Once you set a value for a destination, switching to it later will maintain
 *    that modulation level by automatically adjusting the hub value.
 */

#pragma once

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <algorithm>

namespace common {

// Stable empty string for consistent return pointer
static constexpr const char kEmptyString[] = "";

/**
 * @brief String cache for numeric parameter values
 * Pre-generates string arrays for common numeric ranges
 */
class HubStringCache {
 public:
  /**
   * @brief Get cached string array for a numeric range
   * @param min Minimum value (must be >= 0)
   * @param max Maximum value (must be <= 127)
   * @param unit Unit suffix (e.g., "%", "Hz", "ms")
   * @param bipolar True for ±display, false for absolute
   * @return Pointer to string array, or nullptr if not cacheable
   */
  static const char* const* GetStrings(int32_t min, int32_t max, 
                                       const char* unit, bool bipolar) {
    const int32_t range = max - min + 1;
    
    // Only cache reasonable ranges
    if (range <= 0 || range > 128 || min < 0 || max > 127) {
      return nullptr;
    }
    
    // Use function-local static for thread-safe lazy initialization
    static bool initialized = false;
    static constexpr int kStringSize = 12;
    static char storage[16][128][kStringSize];      // [cache_idx][value_idx][string]
    static const char* ptrs[16][128];     // [cache_idx][value_idx]
    static int cache_count = 0;
    
    const char* safe_unit = (unit != nullptr) ? unit : "";
    // Cache metadata
    struct CacheKey {
      int32_t min, max;
      char unit[4];
      bool bipolar;
    };
    static CacheKey keys[16];
    
    if (!initialized) {
      initialized = true;
      cache_count = 0;
    }
    
    // Check if already cached
    for (int i = 0; i < cache_count; i++) {
      if (keys[i].min == min && keys[i].max == max && 
          keys[i].bipolar == bipolar &&
          strcmp(keys[i].unit, safe_unit) == 0) {
        return ptrs[i];
      }
    }
    
    // Add to cache if space available
    if (cache_count >= 16) {
      return nullptr;  // Cache full
    }
    
    const int idx = cache_count;
    keys[idx].min = min;
    keys[idx].max = max;
    keys[idx].bipolar = bipolar;
    strncpy(keys[idx].unit, safe_unit, 3);
    keys[idx].unit[3] = '\0';
    
    // Generate strings
    for (int32_t i = 0; i < range; i++) {
      int32_t value = min + i;
      
      if (bipolar) {
        // Bipolar: center is middle of range
        int32_t center = min + range / 2;
        int32_t offset = value - center;
        int32_t half_range = range / 2;
        if (half_range == 0) {
          half_range = 1;
        }
        int32_t percent = (offset * 100) / half_range;
        if (percent > 999) {
          percent = 999;
        } else if (percent < -999) {
          percent = -999;
        }
        snprintf(storage[idx][i], kStringSize, "%+d%s", percent, safe_unit);
      } else {
        // Unipolar: direct value
        int32_t clamped_value = value;
        if (clamped_value > 9999) {
          clamped_value = 9999;
        } else if (clamped_value < -999) {
          clamped_value = -999;
        }
        snprintf(storage[idx][i], kStringSize, "%d%s", (int)clamped_value, safe_unit);
      }
      
      ptrs[idx][i] = storage[idx][i];
    }
    
    cache_count++;
    return ptrs[idx];
  }
};

/**
 * @brief Hub control system for parameter compression
 * 
 * Template parameter NUM_DESTINATIONS specifies how many destinations
 * this hub can control (typically 4-8).
 * 
 * Supports "catch" behavior: when switching destinations, the modulation
 * value is preserved by adjusting the hub value to maintain the same output.
 */
template<uint8_t NUM_DESTINATIONS>
class HubControl {
 public:
  /**
   * @brief Destination descriptor
   */
  struct Destination {
    const char* name;               ///< Short display name (e.g., "LFO>PWM")
    const char* value_unit;         ///< Unit suffix (e.g., "%", "Hz", "dB")
    int32_t min;                    ///< Minimum value
    int32_t max;                    ///< Maximum value
    int32_t default_value;          ///< Default/center value
    bool bipolar;                   ///< True = display as ±, false = 0-max
    const char* const* string_values; ///< Optional: Array of strings for enum values (NULL if numeric)
  };
  
  /**
   * @brief Construct hub control with destination descriptors
   * @param destinations Array of NUM_DESTINATIONS descriptors
   */
  explicit HubControl(const Destination* destinations) 
    : destinations_(destinations), current_dest_(0) {
    // Initialize all values to defaults
    for (uint8_t i = 0; i < NUM_DESTINATIONS; i++) {
      original_values_[i] = destinations[i].default_value;
      clamped_values_[i] = destinations[i].default_value;
      caught_values_[i] = destinations[i].default_value;
      caught_[i] = false;
    }
  }
  
  /**
   * @brief Set which destination is selected
   * @param dest Destination index (0 to NUM_DESTINATIONS-1)
   * 
   * Implements "catch" behavior: if the new destination has a caught value,
   * the hub value is adjusted to maintain the same modulation output.
   */
  void SetDestination(uint8_t dest) {
    if (dest >= NUM_DESTINATIONS || dest == current_dest_) return;
    
    current_dest_ = dest;
    
    // If new destination has been caught, adjust hub value to match caught modulation
    if (caught_[dest]) {
      // Calculate what hub value (0-100) would produce the caught modulation value
      const Destination& new_dest = destinations_[dest];
      int32_t caught_mod = caught_values_[dest];
      
      // Clamp caught value to destination's valid range
      if (caught_mod < new_dest.min) caught_mod = new_dest.min;
      if (caught_mod > new_dest.max) caught_mod = new_dest.max;
      
      // Convert back to 0-100 hub value
      int32_t range = new_dest.max - new_dest.min;
      int32_t hub_value;
      if (range > 0) {
        hub_value = static_cast<int32_t>(((caught_mod - new_dest.min) * 100.0f) / range + 0.5f);
      } else {
        hub_value = 0;
      }
      
      // Clamp to 0-100
      if (hub_value < 0) hub_value = 0;
      if (hub_value > 100) hub_value = 100;
      
      // Update the hub value and clamped value
      original_values_[dest] = hub_value;
      clamped_values_[dest] = caught_mod;
    }
  }
  
  /**
   * @brief Set value for current destination
   * @param value New value in 0-100 range (from UI)
   * 
   * Setting a value "catches" the destination, preserving its modulation
   * output when switching to other destinations.
   */
  void SetValue(int32_t value) {
    // Store normalized 0-100 value (from UI)
    if (value < 0) value = 0;
    if (value > 100) value = 100;
    original_values_[current_dest_] = value;
    
    // Calculate clamped version for destination's range
    const Destination& dest = destinations_[current_dest_];
    int32_t clamped = value;
    
    // Linear mapping from 0-100 to [min, max] using floating-point for precision
    int32_t range = dest.max - dest.min;
    if (range > 0) {
      clamped = dest.min + static_cast<int32_t>((value * range) / 100.0f + 0.5f);
    } else {
      clamped = dest.min;
    }
    
    // Safety clamp
    if (clamped < dest.min) clamped = dest.min;
    if (clamped > dest.max) clamped = dest.max;
    
    clamped_values_[current_dest_] = clamped;
    
    // Mark this destination as caught with its current modulation value
    caught_values_[current_dest_] = clamped;
    caught_[current_dest_] = true;
  }
  
  /**
   * @brief Set value for specific destination (direct access)
   * @param dest Destination index
   * @param value New value in 0-100 range (from UI)
   * 
   * Setting a value "catches" the destination, preserving its modulation
   * output when switching to other destinations.
   */
  void SetValueForDest(uint8_t dest, int32_t value) {
    if (dest >= NUM_DESTINATIONS) return;
    
    // value should be in 0-100 range (from UI)
    if (value < 0) value = 0;
    if (value > 100) value = 100;
    original_values_[dest] = value;
    
    // Calculate clamped version
    const Destination& d = destinations_[dest];
    int32_t range = d.max - d.min;
    int32_t clamped;
    if (range > 0) {
      clamped = d.min + static_cast<int32_t>((value * range) / 100.0f + 0.5f);
    } else {
      clamped = d.min;
    }
    
    if (clamped < d.min) clamped = d.min;
    if (clamped > d.max) clamped = d.max;
    
    clamped_values_[dest] = clamped;
    
    // Mark this destination as caught with its current modulation value
    caught_values_[dest] = clamped;
    caught_[dest] = true;
  }
  
  /**
   * @brief Get value for specific destination (clamped to destination's range)
   * @param dest Destination index
   * @return Clamped value for DSP use
   */
  int32_t GetValue(uint8_t dest) const {
    return (dest < NUM_DESTINATIONS) ? clamped_values_[dest] : 0;
  }

  /**
   * @brief Get original UI value (0-100) for a destination
   * @param dest Destination index
   * @return Original value, or 0 if invalid
   */
  int32_t GetOriginalValue(uint8_t dest) const {
    return (dest < NUM_DESTINATIONS) ? original_values_[dest] : 0;
  }
  
  /**
   * @brief Get value for current destination (clamped to destination's range)
   * @return Current destination's clamped value
   */
  int32_t GetCurrentValue() const {
    return clamped_values_[current_dest_];
  }

  /**
   * @brief Set value for current destination and return clamped value
   * @param value New value in 0-100 range (from UI)
   * @return Clamped destination value
   */
  int32_t SetValueAndGetClamped(int32_t value) {
    SetValue(value);
    return clamped_values_[current_dest_];
  }

  /**
   * @brief Set value for specific destination and return clamped value
   * @param dest Destination index
   * @param value New value in 0-100 range (from UI)
   * @return Clamped destination value
   */
  int32_t SetValueForDestAndGetClamped(uint8_t dest, int32_t value) {
    SetValueForDest(dest, value);
    return (dest < NUM_DESTINATIONS) ? clamped_values_[dest] : 0;
  }
  
  /**
   * @brief Get current destination index
   * @return Index of currently selected destination
   */
  uint8_t GetDestination() const { 
    return current_dest_; 
  }
  
  /**
   * @brief Get destination name for display
   * @param dest Destination index
   * @return Display name string, or "" if invalid
   */
  const char* GetDestinationName(uint8_t dest) const {
    return (dest < NUM_DESTINATIONS) ? destinations_[dest].name : "";
  }
  
  /**
   * @brief Get current destination name
   * @return Display name of currently selected destination
   */
  const char* GetCurrentDestinationName() const {
    return destinations_[current_dest_].name;
  }
  
  /**
   * @brief Get formatted value string for current destination
   * @param buffer Output buffer for formatted string
   * @param buf_size Size of output buffer
   * @return Pointer to formatted string
   */
  const char* GetValueString(char* buffer, size_t buf_size) const {
    return GetValueStringForDest(current_dest_, clamped_values_[current_dest_], 
                                  buffer, buf_size);
  }

  /**
   * @brief Get formatted value string for current destination
   * @param buffer Output buffer (fallback only)
   * @param buf_size Buffer size
   * @return Stable pointer to formatted string
   */
  const char* GetCurrentValueString(char* buffer, size_t buf_size) const {
    return GetValueStringForDest(current_dest_, clamped_values_[current_dest_],
                                  buffer, buf_size);
  }
  
  /**
   * @brief Get formatted value string for specific destination and value
   * @param dest Destination index
   * @param value Value to format (clamped to destination's range)
   * @param buffer Output buffer (fallback only)
   * @param buf_size Buffer size
   * @return Stable pointer to formatted string
   */
  const char* GetValueStringForDest(uint8_t dest, int32_t value,
                                     char* buffer, size_t buf_size) const {
    if (dest >= NUM_DESTINATIONS) {
      return kEmptyString;
    }
    
    const Destination& d = destinations_[dest];
    
    // Path 1: Enum/string values (highest priority)
    if (d.string_values != nullptr) {
      if (value >= d.min && value <= d.max) {
        return d.string_values[value - d.min];  // Static pointer ✓
      }
      return kEmptyString;
    }
    
    // Path 2: Numeric values (use cache for ALL numeric)
    const char* safe_unit = (d.value_unit != nullptr) ? d.value_unit : "";
    const char* const* cached = HubStringCache::GetStrings(
      d.min, d.max, safe_unit, d.bipolar);
    
    if (cached != nullptr && value >= d.min && value <= d.max) {
      return cached[value - d.min];  // Cached pointer ✓
    }
    
    // Path 3: Out of range - only use buffer if needed
    if (value >= d.min && value <= d.max) {
      // This should be rare if cache is working
      if (d.bipolar) {
        int32_t range = d.max - d.min;
        int32_t center = d.min + range / 2;
        int32_t offset = value - center;
        int32_t half_range = range / 2;
        if (half_range == 0) {
          half_range = 1;
        }
        int32_t percent = (offset * 100) / half_range;
        snprintf(buffer, buf_size, "%+d%s", percent, safe_unit);
      } else {
        snprintf(buffer, buf_size, "%d%s", (int)value, safe_unit);
      }
      return buffer;
    }
    
    return kEmptyString;
  }
  
  /**
   * @brief Reset all values to defaults
   * 
   * Clears all caught states, returning to default behavior.
   */
  void Reset() {
    for (uint8_t i = 0; i < NUM_DESTINATIONS; i++) {
      original_values_[i] = destinations_[i].default_value;
      clamped_values_[i] = destinations_[i].default_value;
      caught_values_[i] = destinations_[i].default_value;
      caught_[i] = false;
    }
    current_dest_ = 0;
  }
  
  /**
   * @brief Get normalized float value [0.0, 1.0] for unipolar destinations
   * 
   * Maps the destination's actual range to [0.0, 1.0].
   * For example, a destination with range 0-50 will return 0.0 at min and 1.0 at max.
   * 
   * @param dest Destination index
   * @return Normalized value in [0.0, 1.0] range, returns 0.0f if dest invalid
   * 
   * @code
   * float lfo_depth = hub.GetValueNormalizedUnipolar(MOD_LFO_TO_PWM);  // 0.0 to 1.0
   * @endcode
   */
  float GetValueNormalizedUnipolar(uint8_t dest) const {
    if (dest >= NUM_DESTINATIONS) return 0.0f;
    const Destination& d = destinations_[dest];
    int32_t range = d.max - d.min;
    if (range <= 0) return 0.0f;
    int32_t value = GetValue(dest);
    return static_cast<float>(value - d.min) / range;
  }

  /**
   * @brief Get normalized float value [-1.0, +1.0] for bipolar destinations
   * 
   * Maps the destination's range to [-1.0, +1.0] with the default/center value at 0.0.
   * For example, a bipolar destination with range 0-100 and default 50 will return:
   * - 0.0 at the center (50)
   * - -1.0 at minimum (0) 
   * - +1.0 at maximum (100)
   * 
   * @param dest Destination index
   * @return Normalized value in [-1.0, +1.0] range, returns 0.0f if dest invalid
   * 
   * @code
   * float env_fm = hub.GetValueNormalizedBipolar(MOD_ENV_TO_VCF);  // -1.0 to +1.0
   * @endcode
   */
  float GetValueNormalizedBipolar(uint8_t dest) const {
    if (dest >= NUM_DESTINATIONS) return 0.0f;
    const Destination& d = destinations_[dest];
    int32_t value = GetValue(dest);
    int32_t center = d.default_value;
    
    // Calculate the maximum deviation from center
    int32_t max_deviation = std::max(center - d.min, d.max - center);
    if (max_deviation <= 0) return 0.0f;
    
    return static_cast<float>(value - center) / max_deviation;
  }

  /**
   * @brief Get scaled bipolar value [-scale, +scale]
   * 
   * Convenience method for bipolar destinations that need range scaling.
   * Equivalent to GetValueNormalizedBipolar(dest) × scale_factor.
   * 
   * @param dest Destination index
   * @param scale_factor Multiplier (e.g., 12.0f for semitones, 100.0f for cents)
   * @return Normalized bipolar value × scale_factor, returns 0.0f if dest invalid
   * 
   * @code
   * float pitch_mod = hub.GetValueScaledBipolar(MOD_ENV_TO_PITCH, 12.0f);  // ±12 semitones
   * float detune = hub.GetValueScaledBipolar(MOD_LFO_DETUNE, 50.0f);       // ±50 cents
   * @endcode
   */
  float GetValueScaledBipolar(uint8_t dest, float scale_factor) const {
    return GetValueNormalizedBipolar(dest) * scale_factor;
  }

  /**
   * @brief Get number of destinations
   * @return NUM_DESTINATIONS
   */
  constexpr uint8_t GetNumDestinations() const {
    return NUM_DESTINATIONS;
  }
  
  /**
   * @brief Check if a destination has been caught
   * @param dest Destination index
   * @return True if the destination has a preserved modulation value
   */
  bool IsCaught(uint8_t dest) const {
    return (dest < NUM_DESTINATIONS) ? caught_[dest] : false;
  }
  
  /**
   * @brief Clear caught state for a specific destination
   * @param dest Destination index
   * 
   * The destination will revert to using the current hub value.
   */
  void ClearCaught(uint8_t dest) {
    if (dest < NUM_DESTINATIONS) {
      caught_[dest] = false;
      caught_values_[dest] = destinations_[dest].default_value;
    }
  }
  
  /**
   * @brief Clear all caught states
   * 
   * All destinations will revert to using the current hub value.
   */
  void ClearAllCaught() {
    for (uint8_t i = 0; i < NUM_DESTINATIONS; i++) {
      caught_[i] = false;
      caught_values_[i] = destinations_[i].default_value;
    }
  }
  
 private:
  const Destination* destinations_;        ///< Destination descriptors
  int32_t original_values_[NUM_DESTINATIONS];  ///< Original 0-100 values from UI
  int32_t clamped_values_[NUM_DESTINATIONS];   ///< Clamped to destination's range
  int32_t caught_values_[NUM_DESTINATIONS];    ///< Caught modulation values for catch behavior
  bool caught_[NUM_DESTINATIONS];              ///< Whether each destination has been caught
  uint8_t current_dest_;                   ///< Currently selected destination index
};

}  // namespace common
