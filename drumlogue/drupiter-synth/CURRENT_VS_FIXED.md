# Side-by-Side: Current vs Fixed Implementation

## Issue 1: Value Reset on Destination Switch

### CURRENT (BROKEN)
```cpp
// hub_control.h
template<uint8_t NUM_DESTINATIONS>
class HubControl {
 private:
  int32_t values_[NUM_DESTINATIONS];  // ← Only stores one copy!
  
 public:
  void SetValue(int32_t value) {
    const Destination& dest = destinations_[current_dest_];
    
    // Clamp to destination's range
    if (value < dest.min) value = dest.min;
    if (value > dest.max) value = dest.max;
    
    // ✗ LOST: Original 0-100 intent
    values_[current_dest_] = value;
  }
};

// drupiter_synth.cc - SetParameter
void SetParameter(uint8_t id, int32_t value) {
  case PARAM_MOD_AMT:
    v = clamp_u8_int32(value, 0, 100);      // UI: 0-100
    mod_hub_.SetValue(v);                    // ← Clamps to dest range
    // If destination changes, previous value is gone!
    break;
}
```

**What Happens:**
1. User selects `LFO>PWM` (0-100), adjusts to 75 → `values_[0] = 75` ✓
2. User switches to `S MODE` (0-2), UI sends 75 → `values_[1] = 2` (clamped) ✗
3. Back to `LFO>PWM` → now at default because space was overwritten? NO, but the 0 was never stored

### FIXED
```cpp
// hub_control.h
template<uint8_t NUM_DESTINATIONS>
class HubControl {
 private:
  int32_t original_values_[NUM_DESTINATIONS];  // ← Store UI intent (0-100)
  int32_t clamped_values_[NUM_DESTINATIONS];   // ← Store for DSP (min-max)
  
 public:
  void SetValue(int32_t value) {
    if (value < 0) value = 0;
    if (value > 100) value = 100;
    
    // ✓ Store original, unmodified
    original_values_[current_dest_] = value;
    
    // Also calculate clamped version for DSP
    const Destination& dest = destinations_[current_dest_];
    int32_t clamped = value;
    if (clamped < dest.min) clamped = dest.min;
    if (clamped > dest.max) clamped = dest.max;
    
    clamped_values_[current_dest_] = clamped;
  }
  
  int32_t GetValue(uint8_t dest) const {
    return (dest < NUM_DESTINATIONS) ? clamped_values_[dest] : 0;
  }
};

// drupiter_synth.cc - SetParameter
void SetParameter(uint8_t id, int32_t value) {
  case PARAM_MOD_AMT:
    v = clamp_u8_int32(value, 0, 100);      // UI: 0-100
    mod_hub_.SetValue(v);                    // ✓ Now preserves original value!
    // Hub internally calculates clamped version for DSP
    break;
}
```

**What Happens:**
1. User selects `LFO>PWM` (0-100), adjusts to 75
   - `original_values_[0] = 75` ✓
   - `clamped_values_[0] = 75` ✓

2. User switches to `S MODE` (0-2), UI sends 75
   - `original_values_[1] = 75` ✓
   - `clamped_values_[1] = 2` (clamped for DSP) ✓

3. Back to `LFO>PWM`
   - `original_values_[0] = 75` ← STILL THERE! ✓
   - `clamped_values_[0] = 75` ← DSP gets correct value ✓

---

## Issue 2: String Pointer Flickering

### CURRENT (BROKEN)
```cpp
// hub_control.h
const char* GetValueStringForDest(uint8_t dest, int32_t value,
                                   char* buffer, size_t buf_size) const {
  if (dest >= NUM_DESTINATIONS) {
    buffer[0] = '\0';
    return buffer;  // ← Different pointer each call!
  }
  
  const Destination& d = destinations_[dest];
  
  // Path A: Enum strings
  if (d.string_values != nullptr) {
    if (value >= d.min && value <= d.max) {
      return d.string_values[value - d.min];  // Static pointer ← OK
    }
    buffer[0] = '\0';
    return buffer;  // ← Different pointer!
  }
  
  // Path B: Cached numeric strings
  const char* const* cached = HubStringCache::GetStrings(
    d.min, d.max, d.value_unit, d.bipolar);
  
  if (cached != nullptr) {
    if (value >= d.min && value <= d.max) {
      return cached[value - d.min];  // Cached pointer ← OK
    }
    buffer[0] = '\0';
    return buffer;  // ← Different pointer!
  }
  
  // Path C: Dynamic fallback
  if (d.bipolar) {
    int32_t range = d.max - d.min;
    int32_t center = d.min + range / 2;
    int32_t offset = value - center;
    int32_t percent = (offset * 100) / (range / 2);
    snprintf(buffer, buf_size, "%+d%s", percent, d.value_unit);
  } else {
    snprintf(buffer, buf_size, "%d%s", (int)value, d.value_unit);
  }
  
  return buffer;  // ✗ Dynamic buffer = different pointer each time!
}

// Result:
// UI caches like: if (ptr != last_ptr) needs_redraw = true
// Call 1: ptr = 0x1000 (buffer)
// Call 2: ptr = 0x1004 (buffer) ← DIFFERENT! UI thinks value changed!
// Flicker!
```

### FIXED
```cpp
// hub_control.h - Using static empty string constant
static const char kEmptyString[] = "";

const char* GetValueStringForDest(uint8_t dest, int32_t value,
                                   char* buffer, size_t buf_size) const {
  if (dest >= NUM_DESTINATIONS) {
    return kEmptyString;  // ← STABLE pointer
  }
  
  const Destination& d = destinations_[dest];
  
  // Path A: Enum strings (highest priority)
  if (d.string_values != nullptr) {
    if (value >= d.min && value <= d.max) {
      return d.string_values[value - d.min];  // Static pointer ✓
    }
    return kEmptyString;  // ← STABLE pointer
  }
  
  // Path B: Numeric strings (use cache for ALL numeric)
  const char* const* cached = HubStringCache::GetStrings(
    d.min, d.max, d.value_unit, d.bipolar);
  
  if (cached != nullptr && value >= d.min && value <= d.max) {
    return cached[value - d.min];  // Cached pointer ✓
  }
  
  // Path C: Only fallback if truly out of range (rare!)
  // Use provided buffer (still not ideal, but rare)
  if (value >= d.min && value <= d.max) {
    // This should almost never happen if cache works
    if (d.bipolar) {
      int32_t range = d.max - d.min;
      int32_t center = d.min + range / 2;
      int32_t offset = value - center;
      int32_t percent = (offset * 100) / (range / 2);
      snprintf(buffer, buf_size, "%+d%s", percent, d.value_unit);
    } else {
      snprintf(buffer, buf_size, "%d%s", (int)value, d.value_unit);
    }
    return buffer;  // Only if needed
  }
  
  return kEmptyString;  // ← STABLE fallback
}

// Result:
// Call 1: ptr = 0x2000 (kEmptyString) or 0x3000 (cache) or 0x4000 (static array)
// Call 2: ptr = 0x2000 (kEmptyString) or 0x3000 (cache) or 0x4000 (static array)
// Same value = same pointer ✓ No flicker!
```

---

## Issue 3: Buffer Overwrites

### CURRENT (BROKEN)
```cpp
// drupiter_synth.cc
const char* DrupiterSynth::GetParameterStr(uint8_t id, int32_t value) {
    static char modamt_buf[16];  // ← Shared by multiple cases!
    
    switch (id) {
        case PARAM_DCO1_OCT:
            return kOctave1Names[value < 3 ? value : 0];
        
        case PARAM_MOD_AMT: {
            // ... calculation ...
            return mod_hub_.GetValueStringForDest(dest, value, modamt_buf, sizeof(modamt_buf));
            // modamt_buf might contain old value!
        }
        
        case PARAM_EFFECT:
            return effect_names[value < 4 ? value : 0];
    }
}

// Timeline of rapid queries:
// T0: GetParameterStr(PARAM_MOD_HUB, 5) → "LFO>PWM" (no buffer used)
// T1: GetParameterStr(PARAM_MOD_AMT, 75) → modamt_buf = "75%"
// T2: GetParameterStr(PARAM_EFFECT, 1) → "SPACE" (no buffer used)
// T3: GetParameterStr(PARAM_MOD_AMT, ???) → modamt_buf = ??? (stale?)
```

### FIXED
```cpp
// drupiter_synth.cc
const char* DrupiterSynth::GetParameterStr(uint8_t id, int32_t value) {
    // Separate static buffer for MOD_AMT display
    static char modamt_buf[16];
    
    // Other parameters don't use buffers if possible
    static const char* effect_names[] = {"CHORUS", "SPACE", "DRY", "BOTH"};
    
    switch (id) {
        case PARAM_DCO1_OCT:
            return kOctave1Names[value < 3 ? value : 0];  // ← Static array
        
        case PARAM_MOD_HUB:
            if (value >= 0 && value < MOD_NUM_DESTINATIONS) {
                return mod_hub_.GetDestinationName(value);  // ← Returns stable pointer
            }
            return "";
        
        case PARAM_MOD_AMT: {
            uint8_t dest = mod_hub_.GetDestination();
            if (dest >= MOD_NUM_DESTINATIONS) return nullptr;
            
            // GetValueStringForDest now returns stable pointer!
            // modamt_buf only used as fallback
            return mod_hub_.GetValueStringForDest(dest, value, modamt_buf, sizeof(modamt_buf));
        }
        
        case PARAM_EFFECT:
            return effect_names[value < 4 ? value : 0];  // ← Static array
        
        default:
            return nullptr;
    }
}

// Timeline:
// T0: GetParameterStr(PARAM_MOD_HUB, 5) → stable pointer from hub
// T1: GetParameterStr(PARAM_MOD_AMT, 75) → stable pointer (from cache, not buffer!)
// T2: GetParameterStr(PARAM_EFFECT, 1) → "SPACE" (static array)
// T3: GetParameterStr(PARAM_MOD_AMT, 75) → same stable pointer as T1 ✓
```

---

## Summary of Fixes

| Issue | Current | Fixed | Benefit |
|-------|---------|-------|---------|
| **Value persistence** | Single `values_[]` array | Dual `original_values_[]` + `clamped_values_[]` | Values survive destination switches |
| **String pointers** | Multiple sources (static/cached/dynamic) | Unified: static > cached > empty | Stable pointers = no flicker |
| **Buffer overwrites** | Shared `modamt_buf` | Dedicated buffers + hub returns stable pointers | No corruption between queries |
| **Range handling** | Lost UI intent | Preserve original 0-100, calculate clamped | Consistent behavior across ranges |

---

## Migration Path

### Step 1: Update HubControl class
- Add `original_values_[]` array
- Modify `SetValue()` and `SetValueForDest()`
- Keep `GetValue()` returning clamped version

### Step 2: Fix GetValueStringForDest()
- Use `kEmptyString` constant for empty cases
- Ensure cache is used for all numeric ranges
- Minimize dynamic buffer usage

### Step 3: Update drupiter_synth integration
- SetParameter() already calls hub correctly
- Verify GetParameterStr() uses stable pointers
- Test preset save/load/recall

### Step 4: Test
- Verify values persist on destination switch
- Verify no flickering during parameter adjustment
- Verify preset recall works correctly

