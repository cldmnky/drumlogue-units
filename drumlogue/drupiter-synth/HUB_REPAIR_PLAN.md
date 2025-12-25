# HubControl Repair Plan

## Executive Summary

The `hub_control.h` system has 4 critical issues causing flickering and value loss:

1. **Value Persistence:** Values reset when switching between destinations with different ranges
2. **String Pointer Instability:** Multiple sources of string pointers cause UI flicker
3. **Buffer Race Conditions:** Shared static buffers get overwritten during rapid queries
4. **Range Mismatch:** UI sends 0-100 but destinations have different ranges (0-2, 0-50, etc)

## Detailed Repair Strategy

### Phase 1: Separate Value Spaces (HIGH PRIORITY)

**Problem:** `SetValue(75)` clamps immediately to destination's range, losing the original intent.

**Solution:** Store two values per destination:
- `original_value_` (0-100, from UI) - what user actually adjusted
- `clamped_value_` (min-max, for destination) - what DSP uses

```cpp
// NEW STRUCTURE
template<uint8_t NUM_DESTINATIONS>
class HubControl {
 private:
  int32_t original_values_[NUM_DESTINATIONS];  // 0-100 from UI (preserved)
  int32_t clamped_values_[NUM_DESTINATIONS];   // min-max for each dest (for DSP)
};
```

**Benefits:**
- Switching destinations preserves UI slider position
- User adjusts slider to 75%, switching destinations remembers 75%
- DSP gets properly clamped values for its range

**Implementation:**
```cpp
void SetValue(int32_t value) {
    // Store original 0-100 value
    original_values_[current_dest_] = value;  // No clamping!
    
    // Calculate clamped value for DSP
    const Destination& dest = destinations_[current_dest_];
    int32_t clamped = value;
    if (clamped < dest.min) clamped = dest.min;
    if (clamped > dest.max) clamped = dest.max;
    clamped_values_[current_dest_] = clamped;
}

int32_t GetValue(uint8_t dest) const {
    return (dest < NUM_DESTINATIONS) ? clamped_values_[dest] : 0;
}

// For UI display: use original 0-100
int32_t GetOriginalValue(uint8_t dest) const {
    return (dest < NUM_DESTINATIONS) ? original_values_[dest] : 0;
}
```

---

### Phase 2: Stabilize String Pointers (HIGH PRIORITY)

**Problem:** `GetValueStringForDest()` returns different pointer types based on destination type.

**Solution:** Normalize all return paths to use HubStringCache exclusively.

**Current problematic flow:**
```cpp
// Case A: Static enum strings
if (d.string_values != nullptr) {
    return d.string_values[value - d.min];  // ← Static pointer
}

// Case B: Cached numeric strings
const char* const* cached = HubStringCache::GetStrings(...);
if (cached != nullptr) {
    return cached[value - d.min];  // ← Cached pointer
}

// Case C: Dynamic fallback
snprintf(buffer, buf_size, ...);
return buffer;  // ← Dynamic pointer (DIFFERENT EACH TIME!)
```

**Fixed approach:**
```cpp
const char* GetValueStringForDest(uint8_t dest, int32_t value,
                                   char* buffer, size_t buf_size) const {
    if (dest >= NUM_DESTINATIONS) {
        return "";  // Empty string constant
    }
    
    const Destination& d = destinations_[dest];
    
    // Path 1: Enum/string destinations → use provided array
    if (d.string_values != nullptr) {
        if (value >= d.min && value <= d.max) {
            return d.string_values[value - d.min];  // Static, stable
        }
        return "";  // Return empty string, not nullptr
    }
    
    // Path 2: Numeric destinations → ALWAYS use HubStringCache
    const char* const* cached = HubStringCache::GetStrings(
        d.min, d.max, d.value_unit, d.bipolar);
    
    if (cached != nullptr && value >= d.min && value <= d.max) {
        return cached[value - d.min];  // Cached, stable
    }
    
    // Path 3: Only use fallback for truly out-of-range values
    // But still cache the result
    if (value >= d.min && value <= d.max) {
        // Should never reach here if HubStringCache works
        snprintf(buffer, buf_size, "%d%s", (int)value, d.value_unit);
        return buffer;
    }
    
    return "";  // Safe fallback
}
```

**Benefits:**
- All in-range values get stable pointers (static or cached)
- No dynamic buffer returns for normal values
- UI sees same pointer for same destination+value = no flicker

---

### Phase 3: Fix Buffer Management (MEDIUM PRIORITY)

**Problem:** `GetParameterStr()` shares buffers across different parameters.

**Solution:** Use per-parameter buffers with smart caching.

**Current code (broken):**
```cpp
const char* GetParameterStr(uint8_t id, int32_t value) {
    static char modamt_buf[16];  // ← Shared by multiple params
    
    case PARAM_MOD_AMT:
        return mod_hub_.GetValueStringForDest(..., modamt_buf, ...);
    // modamt_buf gets overwritten by other cases!
}
```

**Fixed approach:**
```cpp
const char* GetParameterStr(uint8_t id, int32_t value) {
    // Separate buffer for each parameter that needs one
    static char modamt_buf[16];
    
    switch (id) {
        case PARAM_MOD_AMT: {
            uint8_t dest = mod_hub_.GetDestination();
            const char* result = mod_hub_.GetValueStringForDest(
                dest, value, modamt_buf, sizeof(modamt_buf));
            return result;  // Always returns stable pointer now
        }
        
        // Other parameters don't use modamt_buf
        case PARAM_MOD_HUB:
            return mod_hub_.GetDestinationName(value < MOD_NUM_DESTINATIONS ? value : 0);
        
        default:
            return nullptr;
    }
}
```

**Benefits:**
- Each parameter has its own static buffer
- No overwrites during rapid parameter queries
- Simpler to debug (clear separation)

---

### Phase 4: Improve Preset Sync (MEDIUM PRIORITY)

**Problem:** `preset_.hub_values[]` and `mod_hub_.values_[]` can get out of sync.

**Solution:** Make preset the source of truth.

**Current fragmented approach:**
```cpp
void SetParameter(uint8_t id, int32_t value) {
    case PARAM_MOD_AMT:
        mod_hub_.SetValue(v);  // Updates internal state
        
        // Then separately update preset
        uint8_t dest = current_preset_.params[PARAM_MOD_HUB];
        int32_t actual = mod_hub_.GetValue(dest);
        current_preset_.hub_values[dest] = actual;  // ← Late sync!
        break;
}
```

**Fixed approach:**
```cpp
void SetParameter(uint8_t id, int32_t value) {
    case PARAM_MOD_HUB:
        v = clamp_u8_int32(value, 0, MOD_NUM_DESTINATIONS - 1);
        mod_hub_.SetDestination(v);
        current_preset_.params[id] = v;
        return;
    
    case PARAM_MOD_AMT:
        v = clamp_u8_int32(value, 0, 100);
        uint8_t dest = current_preset_.params[PARAM_MOD_HUB];
        
        // Write directly to preset
        current_preset_.hub_values[dest] = v;  // Premature write
        
        // THEN update hub from preset
        mod_hub_.SetDestination(dest);
        mod_hub_.SetValue(v);
        return;
}
```

**Loading from preset:**
```cpp
void LoadPreset(uint8_t preset_id) {
    // Load preset values
    memcpy(&current_preset_, &kPresets[preset_id], sizeof(Preset));
    
    // Rebuild hub from preset data
    for (uint8_t i = 0; i < MOD_NUM_DESTINATIONS; i++) {
        mod_hub_.SetValueForDest(i, current_preset_.hub_values[i]);
    }
    mod_hub_.SetDestination(current_preset_.params[PARAM_MOD_HUB]);
    
    // Also update DSP...
}
```

**Benefits:**
- Single source of truth (preset)
- Consistent state across save/load
- Easier to debug
- Proper preset recall

---

## Implementation Order

1. **Phase 1** (Value Separation) - Fix value loss on destination switch
   - Modify HubControl to store original_values_[]
   - Update SetValue() and SetValueForDest()
   - Update GetValue() to return clamped version

2. **Phase 2** (String Pointers) - Fix flickering
   - Refactor GetValueStringForDest() to normalize return paths
   - Ensure HubStringCache is used for all numeric destinations
   - Test with UI to verify pointer stability

3. **Phase 3** (Buffer Management) - Fix corruption
   - Separate static buffers in GetParameterStr()
   - Verify no overwrites during rapid queries

4. **Phase 4** (Preset Sync) - Ensure consistency
   - Update LoadPreset() to rebuild hub
   - Update SetParameter() to sync with preset
   - Test preset save/load/recall

---

## Testing Strategy

### Unit Tests
```cpp
// Test 1: Value persistence on destination switch
hub.SetDestination(0);  // LFO>PWM (0-100)
hub.SetValue(75);
assert(hub.GetValue(0) == 75);

hub.SetDestination(1);  // S MODE (0-2)
hub.SetValue(50);      // UI sends 50%
assert(hub.GetValue(1) == 2);  // Clamped to max

hub.SetDestination(0);  // Switch back
assert(hub.GetValue(0) == 75);  // ← MUST be preserved!

// Test 2: String pointer stability
const char* p1 = hub.GetValueStringForDest(0, 75, buf, 16);
const char* p2 = hub.GetValueStringForDest(0, 75, buf, 16);
assert(p1 == p2);  // Same pointer = no flicker

// Test 3: Preset sync
preset.hub_values[0] = 75;
hub.LoadFromPreset(preset);
assert(hub.GetValue(0) == 75);
assert(hub.GetDestination() == preset.params[PARAM_MOD_HUB]);
```

### Hardware Tests
1. **Value Persistence:** Adjust MOD HUB parameter, switch destination, switch back → value preserved
2. **No Flicker:** Adjust parameters smoothly, no screen flickering
3. **Preset Recall:** Save preset with specific hub values, power off/on, reload → values match
4. **Range Consistency:** All destinations use slider proportionally (75% slider = 75% of range)

---

## Files to Modify

1. `drumlogue/common/hub_control.h` - Core fix
2. `drumlogue/drupiter-synth/drupiter_synth.cc` - Update SetParameter, GetParameterStr
3. `drumlogue/drupiter-synth/drupiter_synth.h` - Remove mutable buffers if needed
4. `drumlogue/drupiter-synth/presets.h` - If preset structure changes

## Expected Outcomes

- ✓ No flickering when adjusting parameters
- ✓ Values persist when switching between MOD HUB destinations
- ✓ All destinations respond proportionally to the same slider position
- ✓ Preset save/load/recall works correctly
- ✓ String display is stable and consistent

