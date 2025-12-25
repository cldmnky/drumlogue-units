# Implementation Checklist with Line References

## Phase 1: Value Separation (HIGH PRIORITY)

### File: drumlogue/common/hub_control.h

#### Change 1.1: Add dual value arrays (after line 165)
**Location:** Inside `HubControl` class, private section after `current_dest_`

```diff
  private:
    const Destination* destinations_;  ///< Destination descriptors
    int32_t values_[NUM_DESTINATIONS]; ///< Current values for each destination
+   int32_t original_values_[NUM_DESTINATIONS];  ///< Original 0-100 values from UI
+   int32_t clamped_values_[NUM_DESTINATIONS];   ///< Clamped to destination's range
    uint8_t current_dest_;             ///< Currently selected destination index
```

#### Change 1.2: Initialize dual arrays in constructor (around line 130)
**Location:** Inside `explicit HubControl(const Destination* destinations)` constructor

```diff
  explicit HubControl(const Destination* destinations) 
    : destinations_(destinations), current_dest_(0) {
    // Initialize all values to defaults
    for (uint8_t i = 0; i < NUM_DESTINATIONS; i++) {
      values_[i] = destinations[i].default_value;
+     original_values_[i] = destinations[i].default_value;
+     clamped_values_[i] = destinations[i].default_value;
    }
  }
```

#### Change 1.3: Rewrite SetValue() method (around line 140)
**Location:** Replace entire `SetValue()` method

```cpp
// OLD (BROKEN)
void SetValue(int32_t value) {
    const Destination& dest = destinations_[current_dest_];
    // Clamp to valid range
    if (value < dest.min) value = dest.min;
    if (value > dest.max) value = dest.max;
    values_[current_dest_] = value;
}

// NEW (FIXED)
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
```

#### Change 1.4: Rewrite SetValueForDest() method (around line 150)
**Location:** Replace entire `SetValueForDest()` method

```cpp
// OLD (BROKEN)
void SetValueForDest(uint8_t dest, int32_t value) {
    if (dest >= NUM_DESTINATIONS) return;
    
    const Destination& d = destinations_[dest];
    if (value < d.min) value = d.min;
    if (value > d.max) value = d.max;
    values_[dest] = value;
}

// NEW (FIXED)
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
```

#### Change 1.5: Update GetValue() to return clamped value (around line 160)
**Location:** Modify `GetValue()` return statement

```diff
  int32_t GetValue(uint8_t dest) const {
-   return (dest < NUM_DESTINATIONS) ? values_[dest] : 0;
+   return (dest < NUM_DESTINATIONS) ? clamped_values_[dest] : 0;
  }
```

#### Change 1.6: Update GetCurrentValue() (around line 164)
**Location:** Modify `GetCurrentValue()` return statement

```diff
  int32_t GetCurrentValue() const {
-   return values_[current_dest_];
+   return clamped_values_[current_dest_];
  }
```

#### Change 1.7: Update Reset() method (around line 270)
**Location:** Modify `Reset()` method

```diff
  void Reset() {
    for (uint8_t i = 0; i < NUM_DESTINATIONS; i++) {
      values_[i] = destinations_[i].default_value;
+     original_values_[i] = destinations_[i].default_value;
+     clamped_values_[i] = destinations_[i].default_value;
    }
    current_dest_ = 0;
  }
```

---

## Phase 2: String Pointer Normalization (HIGH PRIORITY)

### File: drumlogue/common/hub_control.h

#### Change 2.1: Add empty string constant (before class, around line 100)
**Location:** At top of file, before `HubControl` class definition

```cpp
namespace common {

// Stable empty string for consistent return pointer
static constexpr const char kEmptyString[] = "";

template<uint8_t NUM_DESTINATIONS>
class HubControl {
  // ...
```

#### Change 2.2: Rewrite GetValueStringForDest() method (around line 225)
**Location:** Replace entire `GetValueStringForDest()` method

```cpp
// OLD (BROKEN - multiple pointer sources)
const char* GetValueStringForDest(uint8_t dest, int32_t value,
                                   char* buffer, size_t buf_size) const {
    if (dest >= NUM_DESTINATIONS) {
      buffer[0] = '\0';
      return buffer;  // ← Different pointer each time!
    }
    
    const Destination& d = destinations_[dest];
    
    // If string array is explicitly provided, use it
    if (d.string_values != nullptr) {
      if (value >= d.min && value <= d.max) {
        return d.string_values[value - d.min];
      }
      buffer[0] = '\0';
      return buffer;  // ← Different pointer!
    }
    
    // ... rest of method
}

// NEW (FIXED - stable pointers only)
const char* GetValueStringForDest(uint8_t dest, int32_t value,
                                   char* buffer, size_t buf_size) const {
    if (dest >= NUM_DESTINATIONS) {
      return kEmptyString;  // ← STABLE pointer
    }
    
    const Destination& d = destinations_[dest];
    
    // Path 1: Enum/string values (highest priority)
    if (d.string_values != nullptr) {
      if (value >= d.min && value <= d.max) {
        return d.string_values[value - d.min];  // Static pointer ✓
      }
      return kEmptyString;  // ← STABLE pointer
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
        int32_t percent = (offset * 100) / (range / 2);
        snprintf(buffer, buf_size, "%+d%s", percent, d.value_unit);
      } else {
        snprintf(buffer, buf_size, "%d%s", (int)value, d.value_unit);
      }
      return buffer;
    }
    
    return kEmptyString;  // ← STABLE fallback
}
```

---

## Phase 3: Buffer Management (MEDIUM PRIORITY)

### File: drumlogue/drupiter-synth/drupiter_synth.cc

#### Change 3.1: Update GetParameterStr() (around line 1039)
**Location:** Modify GetParameterStr() method

```cpp
// OLD (BROKEN - shared buffer)
const char* DrupiterSynth::GetParameterStr(uint8_t id, int32_t value) {
    static const char* effect_names[] = {"CHORUS", "SPACE", "DRY", "BOTH"};
    
    // Use separate buffers for each parameter to avoid race conditions
    static char tune_buf[16];      // For PARAM_DCO2_TUNE
    static char modamt_buf[16];    // For PARAM_MOD_AMT
    
    // ... switch cases ...
}

// NEW (FIXED - stable pointers from hub)
const char* DrupiterSynth::GetParameterStr(uint8_t id, int32_t value) {
    static const char* effect_names[] = {"CHORUS", "SPACE", "DRY", "BOTH"};
    
    // Buffer only needed if hub fallback is used (rare)
    static char modamt_buf[16];
    
    switch (id) {
        // ... existing cases ...
        
        case PARAM_MOD_HUB:
            if (value >= 0 && value < MOD_NUM_DESTINATIONS) {
                return mod_hub_.GetDestinationName(value);  // ← Stable pointer
            }
            return "";
        
        case PARAM_MOD_AMT: {
            uint8_t dest = mod_hub_.GetDestination();
            if (dest >= MOD_NUM_DESTINATIONS) {
                return "";
            }
            // Hub now returns stable pointers (buffer is fallback only)
            return mod_hub_.GetValueStringForDest(dest, value, modamt_buf, sizeof(modamt_buf));
        }
        
        // ... rest of cases ...
    }
}
```

---

## Phase 4: Preset Synchronization (MEDIUM PRIORITY)

### File: drumlogue/drupiter-synth/drupiter_synth.cc

#### Change 4.1: Update SetParameter() MOD_AMT case (around line 833)
**Location:** Modify PARAM_MOD_AMT case in SetParameter()

```cpp
// OLD (BROKEN - fragmented sync)
case PARAM_MOD_AMT:
    v = clamp_u8_int32(value, 0, 100);
    mod_hub_.SetValue(v);
    
    {
        uint8_t dest = current_preset_.params[PARAM_MOD_HUB];
        if (dest < MOD_NUM_DESTINATIONS) {
            int32_t actual_value = mod_hub_.GetValue(dest);
            current_preset_.hub_values[dest] = actual_value;
            // ... mode switching logic ...
        }
    }
    return;

// NEW (FIXED - preset-driven)
case PARAM_MOD_AMT: {
    v = clamp_u8_int32(value, 0, 100);
    uint8_t dest = current_preset_.params[PARAM_MOD_HUB];
    
    // Write to preset first (source of truth)
    if (dest < MOD_NUM_DESTINATIONS) {
        current_preset_.hub_values[dest] = v;
    }
    
    // THEN update hub
    mod_hub_.SetValue(v);
    
    // Apply specific destinations
    if (dest < MOD_NUM_DESTINATIONS) {
        int32_t actual_value = mod_hub_.GetValue(dest);
        
        switch (dest) {
            case MOD_SYNTH_MODE:
                if (actual_value <= 2 && !allocator_.IsAnyVoiceActive()) {
                    current_mode_ = static_cast<dsp::SynthMode>(actual_value);
                    allocator_.SetMode(current_mode_);
                }
                break;
            case MOD_UNISON_DETUNE:
                allocator_.SetUnisonDetune(actual_value);
                break;
            default:
                break;
        }
    }
    return;
}
```

#### Change 4.2: Update LoadPreset() (around line 1190)
**Location:** Find LoadPreset() and add hub rebuild

```cpp
void DrupiterSynth::LoadPreset(uint8_t preset_id) {
    if (preset_id >= 6) return;
    
    const Preset& preset = kPresets[preset_id];
    memcpy(&current_preset_, &preset, sizeof(Preset));
    current_preset_idx_ = preset_id;
    
    // Rebuild hub from preset data
    for (uint8_t i = 0; i < MOD_NUM_DESTINATIONS; i++) {
        mod_hub_.SetValueForDest(i, current_preset_.hub_values[i]);
    }
    mod_hub_.SetDestination(current_preset_.params[PARAM_MOD_HUB]);
    
    // Apply all parameters
    for (uint8_t i = 0; i < PARAM_COUNT; i++) {
        SetParameter(i, current_preset_.params[i]);
    }
}
```

---

## Testing Checklist

- [ ] **Phase 1 Test:** Adjust value, switch destination, switch back → value preserved
- [ ] **Phase 2 Test:** Adjust parameter → no screen flickering
- [ ] **Phase 3 Test:** Rapid parameter changes → no garbage text
- [ ] **Phase 4 Test:** Save preset, power off/on, recall → values match

---

## Files Modified Summary

| File | Changes | Lines | Priority |
|------|---------|-------|----------|
| `hub_control.h` | Add arrays, rewrite methods | ~60 | HIGH |
| `drupiter_synth.cc` | Fix SetParameter, GetParameterStr, LoadPreset | ~40 | MEDIUM |
| `drupiter_synth.h` | May remove mutable buffers | ~5 | LOW |

**Total Estimated Changes:** ~105 lines
**Estimated Implementation Time:** 2-3 hours including testing

