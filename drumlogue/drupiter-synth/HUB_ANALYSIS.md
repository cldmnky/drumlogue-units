# HubControl Analysis - Issues and Root Causes

## Overview
The `hub_control.h` implementation has critical issues causing:
1. **Parameter page flickering** - display updating too fast
2. **Values resetting when switching options** - state not persisting between selections

## Root Cause Analysis

### Issue #1: Parameter Values Resetting When Switching Options

**Problem:** When you change the MOD HUB destination (e.g., from `LFO>PWM` to `ENV>VCF`), the value for the newly selected destination resets to its default instead of retaining previously set values.

**Root Cause:** 
The `SetValue()` method in `HubControl::SetValue()` clamps to the **destination's range**, but the incoming value is always in **0-100 range** from the UI. When switching destinations with different ranges, this causes data loss:

```cpp
// In SetValue() - CURRENT IMPLEMENTATION (BROKEN)
void SetValue(int32_t value) {
    const Destination& dest = destinations_[current_dest_];
    // Clamp to valid range
    if (value < dest.min) value = dest.min;  // ← Can reset if incoming value is out of new dest's range
    if (value > dest.max) value = dest.max;
    values_[current_dest_] = value;
}
```

**Example scenario:**
1. User selects `LFO>PWM` (range 0-100), sets value to 75
2. User selects `S MODE` (range 0-2)
3. UI passes PARAM_MOD_AMT=75 (normalized 0-100)
4. `SetValue(75)` clamps 75 → 2 (max for S MODE)
5. `values_[dest]` gets 2 instead of retaining previous value

**Why this happens:**
The UI doesn't know about per-destination ranges. It always sends 0-100 normalized values. The hub needs to handle the conversion.

---

### Issue #2: Page Flickering During Parameter Selection

**Problem:** OSC and MOD HUB pages flicker when navigating/adjusting parameters.

**Root Causes:**

#### A) String Pointer Instability in GetValueStringForDest()
The method returns pointers to strings, but the source varies:
- Sometimes from static cached arrays
- Sometimes from dynamic `snprintf()` buffers
- UI may receive different pointers for the same logical value

```cpp
// PROBLEMATIC: Multiple sources, unstable pointers
const char* GetValueStringForDest(...) {
    if (d.string_values != nullptr) {
        return d.string_values[value - d.min];  // ← Static pointer
    }
    
    const char* const* cached = HubStringCache::GetStrings(...);
    if (cached != nullptr) {
        return cached[value - d.min];  // ← Cached pointer
    }
    
    // Fallback to dynamic formatting
    snprintf(buffer, buf_size, ...);  // ← Dynamic pointer
    return buffer;
}
```

The UI caches based on pointer identity, not value. Different pointers = forced UI update = flicker.

#### B) Race Condition in drupiter_synth.cc::GetParameterStr()
Multiple parameters share a single static buffer, causing overwrites:

```cpp
// CURRENT BROKEN IMPLEMENTATION
static char modamt_buf[16];    // Shared buffer

// Different cases might overwrite the same buffer during rapid queries
case PARAM_MOD_AMT:
    return mod_hub_.GetValueStringForDest(dest, value, modamt_buf, sizeof(modamt_buf));
```

When the UI queries multiple parameters in quick succession, buffer contents get corrupted or pointer addresses change mid-render.

---

### Issue #3: Inconsistent Value Clamping Between UI and DSP

**Problem:** Values display inconsistently and don't apply correctly to DSP.

**Current flow:**
1. UI sends 0-100 (normalized) → `SetParameter(PARAM_MOD_AMT, 75)`
2. `SetValue(75)` clamps to destination range → stored as `values_[dest]`
3. `GetValueStringForDest()` tries to format the clamped value
4. DSP retrieves via `GetValue(dest)` → receives clamped value

**Issue:** The original UI intention is lost. If user adjusted slider to 75 for a 0-50 range destination, they meant 75% of range (= 37.5), but it gets clamped to 50.

---

## Current Implementation Problems Summary

| Issue | Location | Symptom | Impact |
|-------|----------|---------|--------|
| Value loss on dest switch | `HubControl::SetValue()` | Values reset to defaults | Can't preserve settings |
| String pointer instability | `GetValueStringForDest()` | Multiple return paths | Flickering, UI confusion |
| Shared buffer corruption | `GetParameterStr()` | Static buffers overwrit | Garbage displays |
| Range mismatch | UI→Hub→DSP pipeline | 0-100 vs destination ranges | Incorrect DSP values |
| No value history | Hub structure | Switching loses previous values | Poor UX |

---

## Design Flaws

### 1. Bidirectional Value Transformation Missing
The hub doesn't properly convert between:
- **UI space:** 0-100 normalized
- **Destination space:** Individual min/max ranges

### 2. No Input Validation
`SetValue()` accepts pre-clamped values without knowing their origin range.

### 3. String Cache Contamination
HubStringCache uses function-local statics that can be shared across multiple hub instances.

### 4. Tight Coupling to Preset Storage
The `current_preset_.hub_values[]` array is updated in SetParameter but not read on GetValue() - data path is fragmented.

---

## Solution Strategy (Recommendations)

### Phase 1: Fix Value Persistence
- **Don't clamp in SetValue()**
- Store the **original 0-100 value** separately from the "current value"
- Let destination use the original value, apply its own clamping when needed

### Phase 2: Stabilize String Display
- **All values → same source string pointer**
- Use HubStringCache exclusively for numeric ranges
- Never mix snprintf() results with static arrays in same destination

### Phase 3: Sync Preset Storage
- Make `hub_values[]` the source of truth
- `GetValue()` should read from preset, not internal state
- Synchronize whenever a destination is written

### Phase 4: Decouple UI Range from Destination Range
- UI always sends 0-100
- Hub converts to destination's actual range at display/DSP time
- No data loss during range conversion

