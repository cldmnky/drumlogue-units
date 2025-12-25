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
 * 4. Use {@link GetValueString} when rendering the parameter value; the returned pointer is stable and safe
 *    to cache, which prevents screen flicker.
 * 5. In DSP code, read {@link GetValue} or {@link GetCurrentValue} to obtain the clamped value for the
 *    currently selected destination, or {@link GetValue} with a destination index for others.
 */

#pragma once

#include <cstdint>
#include <cstdio>
#include <cstring>

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
    static char storage[16][128][8];      // [cache_idx][value_idx][string]
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
        int32_t percent = (offset * 100) / (range / 2);
        snprintf(storage[idx][i], 8, "%+d%s", percent, unit);
      } else {
        // Unipolar: direct value
        snprintf(storage[idx][i], 8, "%d%s", (int)value, unit);
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
    }
  }
  
  /**
   * @brief Set which destination is selected
   * @param dest Destination index (0 to NUM_DESTINATIONS-1)
   */
  void SetDestination(uint8_t dest) {
    if (dest < NUM_DESTINATIONS) {
      current_dest_ = dest;
    }
  }
  
  /**
   * @brief Set value for current destination
   * @param value New value in 0-100 range (from UI)
   */
  void SetValue(int32_t value) {
    // Store normalized 0-100 value (from UI)
    if (value < 0) value = 0;
    if (value > 100) value = 100;
    original_values_[current_dest_] = value;
    
    // Calculate clamped version for destination's range
    const Destination& dest = destinations_[current_dest_];
    int32_t clamped = value;
    
    // Linear mapping from 0-100 to [min, max]
    int32_t range = dest.max - dest.min;
    clamped = dest.min + (value * range) / 100;
    
    // Safety clamp
    if (clamped < dest.min) clamped = dest.min;
    if (clamped > dest.max) clamped = dest.max;
    
    clamped_values_[current_dest_] = clamped;
  }
  
  /**
   * @brief Set value for specific destination (direct access)
   * @param dest Destination index
   * @param value New value in 0-100 range (from UI)
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
    int32_t clamped = d.min + (value * range) / 100;
    
    if (clamped < d.min) clamped = d.min;
    if (clamped > d.max) clamped = d.max;
    
    clamped_values_[dest] = clamped;
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
   * @brief Get value for current destination (clamped to destination's range)
   * @return Current destination's clamped value
   */
  int32_t GetCurrentValue() const {
    return clamped_values_[current_dest_];
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
    const char* const* cached = HubStringCache::GetStrings(
      d.min, d.max, d.value_unit, d.bipolar);
    
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
        snprintf(buffer, buf_size, "%+d%s", percent, d.value_unit);
      } else {
        snprintf(buffer, buf_size, "%d%s", (int)value, d.value_unit);
      }
      return buffer;
    }
    
    return kEmptyString;
  }
  
  /**
   * @brief Reset all values to defaults
   */
  void Reset() {
    for (uint8_t i = 0; i < NUM_DESTINATIONS; i++) {
      original_values_[i] = destinations_[i].default_value;
      clamped_values_[i] = destinations_[i].default_value;
    }
    current_dest_ = 0;
  }
  
  /**
   * @brief Get normalized float value [0.0, 1.0] for unipolar destinations
   * 
   * For destinations with 0-100 range and unipolar display.
   * Typical use: amplitude/depth modulations that range from 0 to full effect.
   * 
   * @param dest Destination index
   * @return Clamped value divided by 100.0f, returns 0.0f if dest invalid
   * 
   * @code
   * float lfo_depth = hub.GetValueNormalizedUnipolar(MOD_LFO_TO_PWM);  // 0.0 to 1.0
   * @endcode
   */
  float GetValueNormalizedUnipolar(uint8_t dest) const {
    if (dest >= NUM_DESTINATIONS) return 0.0f;
    return static_cast<float>(GetValue(dest)) / 100.0f;
  }

  /**
   * @brief Get normalized float value [-1.0, +1.0] for bipolar destinations
   * 
   * For destinations with 0-100 range but bipolar display (center=50).
   * Typical use: pitch modulations, filter FM, which can go up or down.
   * 
   * Conversion: (value - 50) / 50.0f yields [-1.0, +1.0] with center=0.0
   * 
   * @param dest Destination index
   * @return (clamped_value - 50) / 50.0f, returns 0.0f if dest invalid
   * 
   * @code
   * float env_fm = hub.GetValueNormalizedBipolar(MOD_ENV_TO_VCF);  // -1.0 to +1.0
   * @endcode
   */
  float GetValueNormalizedBipolar(uint8_t dest) const {
    if (dest >= NUM_DESTINATIONS) return 0.0f;
    int32_t val = static_cast<int32_t>(GetValue(dest));
    return static_cast<float>(val - 50) / 50.0f;
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
  
 private:
  const Destination* destinations_;        ///< Destination descriptors
  int32_t original_values_[NUM_DESTINATIONS];  ///< Original 0-100 values from UI
  int32_t clamped_values_[NUM_DESTINATIONS];   ///< Clamped to destination's range
  uint8_t current_dest_;                   ///< Currently selected destination index
};

}  // namespace common
