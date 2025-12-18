/**
 * @file preset_manager.h
 * @brief Generic preset management system
 *
 * Handles preset storage, loading, validation, and naming.
 * Template parameter specifies the number of parameters per preset.
 *
 * Example usage:
 * @code
 * static constexpr PresetManager<24>::Preset kPresets[] = {
 *   {"Brass Lead", {1, 0, 50, 25, ...}},  // 24 params
 *   {"Fat Bass",   {0, 0, 50, 0,  ...}},
 * };
 * PresetManager<24> mgr{kPresets, 2};
 * @endcode
 */

#pragma once

#include <cstdint>
#include <cstring>

namespace common {

/**
 * @brief Preset management system
 * 
 * @tparam NUM_PARAMS Number of parameters in each preset
 */
template<uint8_t NUM_PARAMS>
class PresetManager {
 public:
  /**
   * @brief Preset data structure
   */
  struct Preset {
    char name[14];               ///< Preset name (13 chars + null, drumlogue limit)
    int32_t params[NUM_PARAMS];  ///< Parameter values
  };
  
  /**
   * @brief Construct preset manager with factory presets
   * @param factory_presets Array of factory presets (must be static/const)
   * @param num_presets Number of factory presets
   */
  PresetManager(const Preset* factory_presets, uint8_t num_presets)
    : factory_presets_(factory_presets),
      num_factory_presets_(num_presets),
      current_preset_idx_(0) {
    // Initialize current preset from factory preset 0
    if (num_presets > 0) {
      current_preset_ = factory_presets[0];
    }
  }
  
  /**
   * @brief Load preset by index
   * @param idx Preset index (0 to num_presets-1)
   * @return True if loaded successfully, false if invalid index
   */
  bool LoadPreset(uint8_t idx) {
    if (idx >= num_factory_presets_) {
      return false;  // Invalid index
    }
    
    current_preset_ = factory_presets_[idx];
    current_preset_idx_ = idx;
    return true;
  }
  
  /**
   * @brief Get preset name by index
   * @param idx Preset index
   * @return Preset name, or "Invalid" if index out of range
   */
  const char* GetPresetName(uint8_t idx) const {
    return (idx < num_factory_presets_) 
      ? factory_presets_[idx].name 
      : "Invalid";
  }
  
  /**
   * @brief Get current preset index
   * @return Index of currently loaded preset
   */
  uint8_t GetCurrentIndex() const {
    return current_preset_idx_;
  }
  
  /**
   * @brief Get current preset name
   * @return Name of currently loaded preset
   */
  const char* GetCurrentName() const {
    return current_preset_.name;
  }
  
  /**
   * @brief Get number of factory presets
   * @return Total number of presets
   */
  uint8_t GetNumPresets() const {
    return num_factory_presets_;
  }
  
  /**
   * @brief Get parameter value from current preset
   * @param param_id Parameter ID (0 to NUM_PARAMS-1)
   * @return Parameter value, or 0 if invalid ID
   */
  int32_t GetParam(uint8_t param_id) const {
    return (param_id < NUM_PARAMS) ? current_preset_.params[param_id] : 0;
  }
  
  /**
   * @brief Set parameter value in current preset
   * @param param_id Parameter ID
   * @param value New value
   * @return True if set successfully, false if invalid ID
   */
  bool SetParam(uint8_t param_id, int32_t value) {
    if (param_id >= NUM_PARAMS) {
      return false;
    }
    current_preset_.params[param_id] = value;
    return true;
  }
  
  /**
   * @brief Get read-only access to current preset
   * @return Reference to current preset
   */
  const Preset& GetCurrentPreset() const {
    return current_preset_;
  }
  
  /**
   * @brief Get read-write access to current preset
   * @return Reference to current preset
   * 
   * Warning: Direct modification doesn't update preset index.
   * Use SetParam() for individual parameter changes.
   */
  Preset& GetCurrentPresetMutable() {
    return current_preset_;
  }
  
  /**
   * @brief Copy current preset to another preset
   * @param dest Destination preset
   */
  void CopyCurrentTo(Preset& dest) const {
    dest = current_preset_;
  }
  
  /**
   * @brief Restore current preset to factory default
   * @return True if restored, false if no presets available
   */
  bool RestoreToFactory() {
    return LoadPreset(current_preset_idx_);
  }
  
  /**
   * @brief Validate preset data (check parameter ranges)
   * @param preset Preset to validate
   * @param min_values Array of minimum values for each parameter
   * @param max_values Array of maximum values for each parameter
   * @return True if all parameters are within valid ranges
   */
  bool ValidatePreset(const Preset& preset, 
                      const int32_t* min_values,
                      const int32_t* max_values) const {
    for (uint8_t i = 0; i < NUM_PARAMS; i++) {
      if (preset.params[i] < min_values[i] || 
          preset.params[i] > max_values[i]) {
        return false;
      }
    }
    return true;
  }
  
  /**
   * @brief Get factory preset by index (read-only)
   * @param idx Preset index
   * @return Pointer to factory preset, or nullptr if invalid index
   */
  const Preset* GetFactoryPreset(uint8_t idx) const {
    return (idx < num_factory_presets_) ? &factory_presets_[idx] : nullptr;
  }
  
 private:
  const Preset* factory_presets_;    ///< Factory preset array (static)
  uint8_t num_factory_presets_;      ///< Number of factory presets
  uint8_t current_preset_idx_;       ///< Current preset index
  Preset current_preset_;            ///< Current preset (mutable copy)
};

}  // namespace common
