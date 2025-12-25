# HubControl Usage Analysis - Drupiter Synth

## Summary

Drupiter's usage of `HubControl` is **correct and well-integrated**, with one potential optimization opportunity to move parameter-range conversion logic into `HubControl` itself.

---

## Current Implementation

### Positive Aspects ✓

1. **Proper initialization** (drupiter_synth.h:119)
   ```cpp
   HubControl<MOD_NUM_DESTINATIONS> mod_hub_{kModDestinations};
   ```
   - Correct template parameter count (17 destinations)
   - All destinations properly defined with min/max/unit/bipolar flags

2. **Correct SetValue/SetDestination flow** (drupiter_synth.cc:843-873)
   ```cpp
   case PARAM_MOD_HUB:
       v = clamp_u8_int32(value, 0, MOD_NUM_DESTINATIONS - 1);
       mod_hub_.SetDestination(v);
   
   case PARAM_MOD_AMT:
       v = clamp_u8_int32(value, 0, 100);
       mod_hub_.SetValue(v);  // UI sends 0-100, hub clamping handles it
   ```
   - UI always sends 0-100
   - Hub handles destination-specific range mapping
   - Values persist across destination switches

3. **Proper string display** (drupiter_synth.cc:1058-1076)
   ```cpp
   case PARAM_MOD_AMT: {
       uint8_t dest = mod_hub_.GetDestination();
       return mod_hub_.GetValueStringForDest(dest, value, modamt_buf, sizeof(modamt_buf));
   }
   ```
   - Returns stable pointers from HubControl
   - Caches by pointer identity (no flicker)
   - Fallback buffer only used for out-of-range values

4. **Proper preset save/load** (drupiter_synth.cc:1154-1165)
   ```cpp
   // Restore MOD HUB values from preset
   mod_hub_.SetDestination(current_preset_.params[PARAM_MOD_HUB]);
   for (uint8_t dest = 0; dest < MOD_NUM_DESTINATIONS; ++dest) {
       mod_hub_.SetValueForDest(dest, current_preset_.hub_values[dest]);
   }
   ```
   - Rebuilds hub state before applying parameters
   - Values persist across preset loads

5. **Correct DSP value reading** (drupiter_synth.cc:271-290)
   ```cpp
   const float lfo_pwm_depth = mod_hub_.GetValue(MOD_LFO_TO_PWM) / 100.0f;
   const float env_vcf_depth = (static_cast<int32_t>(mod_hub_.GetValue(MOD_ENV_TO_VCF)) - 50) / 50.0f;
   ```
   - Reads clamped values from hub
   - Performs range-specific math (e.g., bipolar division by 50)

---

## Optimization Opportunity 🎯

### Issue: Repeated Range-Conversion Math in DSP Code

Drupiter currently performs the same bipolar/unipolar conversion math in multiple places:

**Pattern 1: Unipolar (direct division by 100)**
```cpp
// Line 271-286: 10+ occurrences
const float lfo_pwm_depth = mod_hub_.GetValue(MOD_LFO_TO_PWM) / 100.0f;  // 0-100 → 0.0-1.0
const float vca_level = mod_hub_.GetValue(MOD_VCA_LEVEL) / 100.0f;
```

**Pattern 2: Bipolar (center=50, divide by 50)**
```cpp
// Line 276, 290: 3 occurrences
const float env_vcf_depth = (static_cast<int32_t>(mod_hub_.GetValue(MOD_ENV_TO_VCF)) - 50) / 50.0f;  // 0-100 → -1.0 to +1.0
const float env_pitch_depth = (static_cast<int32_t>(mod_hub_.GetValue(MOD_ENV_TO_PITCH)) - 50) / 50.0f * 12.0f;
```

**Pattern 3: Special-case ranges**
```cpp
// Line 301: Manual calculation for 0-50 cents range
const float unison_detune_cents = static_cast<float>(mod_hub_.GetValue(MOD_UNISON_DETUNE));
```

**Total: ~15 conversions scattered across Render()**

### Proposed Solution: Add Helper Methods to HubControl

Move these conversions **into HubControl** as optional helper methods:

```cpp
// In hub_control.h, add to HubControl class:

/**
 * @brief Get normalized float value [0.0, 1.0] for unipolar destinations
 * @param dest Destination index (must be 0-100 range)
 * @return Clamped value divided by 100.0f
 */
float GetValueNormalizedUnipolar(uint8_t dest) const {
  if (dest >= NUM_DESTINATIONS) return 0.0f;
  return static_cast<float>(GetValue(dest)) / 100.0f;
}

/**
 * @brief Get normalized float value [-1.0, +1.0] for bipolar destinations
 * @param dest Destination index (must be 0-100 range with center=50)
 * @return (clamped_value - 50) / 50.0f
 */
float GetValueNormalizedBipolar(uint8_t dest) const {
  if (dest >= NUM_DESTINATIONS) return 0.0f;
  int32_t val = static_cast<int32_t>(GetValue(dest));
  return static_cast<float>(val - 50) / 50.0f;
}

/**
 * @brief Get scaled value for destinations with specific ranges
 * @param dest Destination index
 * @param scale_factor Multiplier (e.g., 12.0f for semitones)
 * @return Normalized bipolar value × scale_factor
 */
float GetValueScaledBipolar(uint8_t dest, float scale_factor) const {
  return GetValueNormalizedBipolar(dest) * scale_factor;
}
```

### Refactored Drupiter Code

**Before (15 conversions):**
```cpp
const float lfo_pwm_depth = mod_hub_.GetValue(MOD_LFO_TO_PWM) / 100.0f;
const float lfo_vcf_depth = mod_hub_.GetValue(MOD_LFO_TO_VCF) / 100.0f;
const float lfo_vco_depth = mod_hub_.GetValue(MOD_LFO_TO_VCO) / 100.0f;
const float env_pwm_depth = mod_hub_.GetValue(MOD_ENV_TO_PWM) / 100.0f;
const float env_vcf_depth = (static_cast<int32_t>(mod_hub_.GetValue(MOD_ENV_TO_VCF)) - 50) / 50.0f;
const float env_pitch_depth = (static_cast<int32_t>(mod_hub_.GetValue(MOD_ENV_TO_PITCH)) - 50) / 50.0f * 12.0f;
// ... etc
```

**After (with helpers):**
```cpp
const float lfo_pwm_depth = mod_hub_.GetValueNormalizedUnipolar(MOD_LFO_TO_PWM);
const float lfo_vcf_depth = mod_hub_.GetValueNormalizedUnipolar(MOD_LFO_TO_VCF);
const float lfo_vco_depth = mod_hub_.GetValueNormalizedUnipolar(MOD_LFO_TO_VCO);
const float env_pwm_depth = mod_hub_.GetValueNormalizedUnipolar(MOD_ENV_TO_PWM);
const float env_vcf_depth = mod_hub_.GetValueNormalizedBipolar(MOD_ENV_TO_VCF);
const float env_pitch_depth = mod_hub_.GetValueScaledBipolar(MOD_ENV_TO_PITCH, 12.0f);
// ... etc
```

### Benefits

| Aspect | Current | With Helpers |
|--------|---------|-------------|
| **Clarity** | Raw math scattered | Intent clear from method name |
| **Maintainability** | Change 1 conversion = edit 5 places | Change once in hub_control.h |
| **Consistency** | Manual casts/divisions | Unified implementation |
| **Readability** | `(val - 50) / 50.0f` | `GetValueNormalizedBipolar(dest)` |
| **Correctness** | Error-prone | Type-safe, returns float |

---

## Destination-Specific Conversions

Some destinations have **non-standard ranges** that already need special handling. These should remain in Drupiter (not move to HubControl):

| Destination | Current Code | Why It Stays in Drupiter |
|-------------|--------------|-------------------------|
| `MOD_UNISON_DETUNE` | Direct float cast (0-50 range) | Domain-specific: cents → ratio |
| `MOD_SYNTH_MODE` | `allocator_.SetMode()` call | Requires state management, not just math |
| `MOD_VCF_TYPE` / `MOD_LFO_WAVE` | Direct enum cast | Destination is already enum, no conversion |
| `MOD_ENV_TO_PITCH` | Scaled by 12.0f for semitones | Could use `GetValueScaledBipolar()` helper |

---

## Recommendation

**Add the three helper methods to `HubControl`** to simplify DSP value extraction:

1. `GetValueNormalizedUnipolar(dest)` → 0.0 to 1.0
2. `GetValueNormalizedBipolar(dest)` → -1.0 to +1.0  
3. `GetValueScaledBipolar(dest, scale)` → bipolar × scale

**Benefits for all future drumlogue units:**
- Consistent conversion semantics
- Reduced code duplication
- Self-documenting parameter flow
- Easier to audit and maintain

---

## Other Observations

### Minor: Unused Template Parameter Check

`GetNumDestinations()` is marked `constexpr` but always returns compile-time constant `NUM_DESTINATIONS`. This is fine for API consistency, but the method is never called. Can be kept for API completeness.

### Minor: Duplicate Clamping

Drupiter calls `clamp_u8_int32()` before `SetValue()`, but `SetValue()` also clamps internally:
```cpp
v = clamp_u8_int32(value, 0, 100);  // Pre-clamp to 0-100
mod_hub_.SetValue(v);               // Clamps again internally
```

This is redundant but harmless (defensive programming). Could remove the pre-clamp, but it's not a problem.

---

## Conclusion

✅ **HubControl integration is correct and production-ready.**  
🎯 **Add helper methods for unipolar/bipolar normalization to reduce code duplication.**
