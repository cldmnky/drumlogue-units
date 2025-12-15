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
 */

#pragma once

#include <cstdint>
#include <cstdio>
#include <cstring>

namespace common {

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
          strcmp(keys[i].unit, unit) == 0) {
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
    strncpy(keys[idx].unit, unit, 3);
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
      values_[i] = destinations[i].default_value;
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
   * @param value New value (must be within destination's min/max range)
   */
  void SetValue(int32_t value) {
    const Destination& dest = destinations_[current_dest_];
    // Clamp to valid range
    if (value < dest.min) value = dest.min;
    if (value > dest.max) value = dest.max;
    values_[current_dest_] = value;
  }
  
  /**
   * @brief Set value for specific destination (direct access)
   * @param dest Destination index
   * @param value New value
   */
  void SetValueForDest(uint8_t dest, int32_t value) {
    if (dest >= NUM_DESTINATIONS) return;
    
    const Destination& d = destinations_[dest];
    if (value < d.min) value = d.min;
    if (value > d.max) value = d.max;
    values_[dest] = value;
  }
  
  /**
   * @brief Get value for specific destination
   * @param dest Destination index
   * @return Value for that destination, or 0 if invalid index
   */
  int32_t GetValue(uint8_t dest) const {
    return (dest < NUM_DESTINATIONS) ? values_[dest] : 0;
  }
  
  /**
   * @brief Get value for current destination
   * @return Current destination's value
   */
  int32_t GetCurrentValue() const {
    return values_[current_dest_];
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
   * @return Pointer to buffer (for convenience)
   * 
   * Formats value according to destination's bipolar flag:
   * - Bipolar: "-50%" to "+50%" (centered)
   * - Unipolar: "0%" to "100%"
   */
  const char* GetValueString(char* buffer, size_t buf_size) const {
    return GetValueStringForDest(current_dest_, values_[current_dest_], 
                                  buffer, buf_size);
  }
  
  /**
   * @brief Get formatted value string for specific destination and value
   * @param dest Destination index
   * @param value Value to format
   * @param buffer Output buffer
   * @param buf_size Buffer size
   * @return Formatted string
   */
  const char* GetValueStringForDest(uint8_t dest, int32_t value,
                                     char* buffer, size_t buf_size) const {
    if (dest >= NUM_DESTINATIONS) {
      buffer[0] = '\0';
      return buffer;
    }
    
    const Destination& d = destinations_[dest];
    
    // If string array is explicitly provided, use it
    if (d.string_values != nullptr) {
      if (value >= d.min && value <= d.max) {
        return d.string_values[value - d.min];
      }
      buffer[0] = '\0';
      return buffer;
    }
    
    // Try to get cached strings for numeric ranges
    const char* const* cached = HubStringCache::GetStrings(
      d.min, d.max, d.value_unit, d.bipolar);
    
    if (cached != nullptr) {
      if (value >= d.min && value <= d.max) {
        return cached[value - d.min];
      }
      buffer[0] = '\0';
      return buffer;
    }
    
    // Fallback to dynamic formatting (for out-of-range values)
    if (d.bipolar) {
      // Display as ±: -100% to +100%
      int32_t range = d.max - d.min;
      int32_t center = d.min + range / 2;
      int32_t offset = value - center;
      int32_t percent = (offset * 100) / (range / 2);
      snprintf(buffer, buf_size, "%+d%s", percent, d.value_unit);
    } else {
      // Display as absolute value
      snprintf(buffer, buf_size, "%d%s", (int)value, d.value_unit);
    }
    
    return buffer;
  }
  
  /**
   * @brief Reset all values to defaults
   */
  void Reset() {
    for (uint8_t i = 0; i < NUM_DESTINATIONS; i++) {
      values_[i] = destinations_[i].default_value;
    }
    current_dest_ = 0;
  }
  
  /**
   * @brief Get number of destinations
   * @return NUM_DESTINATIONS
   */
  constexpr uint8_t GetNumDestinations() const {
    return NUM_DESTINATIONS;
  }
  
 private:
  const Destination* destinations_;  ///< Destination descriptors
  int32_t values_[NUM_DESTINATIONS]; ///< Current values for each destination
  uint8_t current_dest_;             ///< Currently selected destination index
};

}  // namespace common
