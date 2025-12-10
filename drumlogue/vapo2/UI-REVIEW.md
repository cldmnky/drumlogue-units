# Vapo2 UI/UX Review

**Date**: December 10, 2025  
**Reviewer**: Code Review  
**Version**: 1.0.0 (feature/vapo2)

---

## Executive Summary

Overall, Vapo2 has a solid parameter layout with good organization across 6 pages. However, there are several inconsistencies and potential improvements that would enhance the end-user experience on the drumlogue hardware.

---

## Parameter Definition Analysis (header.c)

### ✅ What's Working Well

1. **Logical page organization** - Parameters are grouped sensibly:
   - Page 1-2: Oscillators
   - Page 3: Filter
   - Page 4-5: Envelopes
   - Page 6: Modulation/Output

2. **String parameters for banks** - Using `k_unit_param_type_strings` for bank selection provides readable labels

3. **Default values** - Most defaults create a playable initial sound

### ⚠️ Issues Found

#### 1. **OSC MODE Default Mismatch**

**header.c**: Default is `2` (Raw)
```c
{0, 2, 0, 2, k_unit_param_type_strings, 0, 0, 0, {"OSC MOD"}},
```

**vapo2_synth.h**: Also defaults to `2` ✓

**Issue**: Raw mode is the harshest, most aliased mode. New users may find this unpleasant. Consider defaulting to `0` (HiFi) for a more accessible first impression.

#### 2. **Parameter Naming Inconsistency**

| Parameter | header.c Name | README Name | Suggestion |
|-----------|---------------|-------------|------------|
| P_OSC_MODE | `OSC MOD` | `OSC MOD` | Should be `OSC MDE` or `MODE` (more readable) |
| P_LFO_RATE | `LFO RT` | `LFO RT` | Consider `LFO SPD` or `LFO HZ` |
| P_LFO_TO_MORPH | `LFO>MRP` | `LFO>MRP` | OK, but `MORPH` would be clearer if space allows |

#### 3. **OSC MIX Center Value Issue**

**header.c**:
```c
{0, 127, 64, 0, k_unit_param_type_none, 0, 0, 0, {"OSC MIX"}},
```

- `center = 64` but `default = 0`
- This means the encoder's center detent won't match the parameter's "balanced" state
- **Fix**: Either set `default = 64` or `center = 0`

#### 4. **SPACE Parameter Center Value**

```c
{0, 127, 0, 64, k_unit_param_type_none, 0, 0, 0, {"SPACE"}},
```

- `center = 0` but `default = 64`
- Similar issue as OSC MIX
- **Fix**: Set `center = 64` to match the "normal stereo" default

#### 5. **Envelope Time Display**

Envelope parameters (ATTACK, DECAY, RELEASE) show raw 0-127 values. Users have no indication of actual times.

**Recommendation**: Consider `k_unit_param_type_strings` to show time hints like "1ms", "100ms", "1s", "8s" at key positions.

---

## Parameter Handling Analysis (vapo2_synth.h)

### ✅ Strengths

1. **Immediate parameter updates** - Changes take effect on next render cycle
2. **Parameter bounds respected** - Bank switching validates indices
3. **Smooth morph modulation** - LFO to morph is properly clamped

### ⚠️ Issues Found

#### 1. **No Parameter Smoothing for Filter Cutoff**

**Current** (line ~233):
```cpp
const float cutoff_base = params_[P_FILTER_CUTOFF] / 127.0f;
```

**Problem**: Direct parameter read causes zipper noise when turning the cutoff knob quickly.

**Recommendation**: Add one-pole smoothing for cutoff:
```cpp
cutoff_smoothed_ += 0.01f * (target_cutoff - cutoff_smoothed_);
```

#### 2. **No Parameter Smoothing for OSC MIX**

Same issue - direct read causes audible stepping when crossfading between oscillators.

#### 3. **No Parameter Smoothing for SPACE**

Stereo width changes will cause audible clicks/pops.

#### 4. **Envelope Parameters Updated Every Frame**

**Current** (lines ~254-265):
```cpp
for (uint32_t v = 0; v < kNumVoices; v++) {
    voices_[v].amp_env.SetAttack(params_[P_AMP_ATTACK]);
    // ... etc
}
```

**Problem**: Updating all envelope coefficients for all voices every render call is wasteful and could cause discontinuities if changed during active notes.

**Recommendation**: Only update when parameter actually changes:
```cpp
if (params_dirty_[P_AMP_ATTACK]) {
    for (uint32_t v = 0; v < kNumVoices; v++) {
        voices_[v].amp_env.SetAttack(params_[P_AMP_ATTACK]);
    }
    params_dirty_[P_AMP_ATTACK] = false;
}
```

#### 5. **PPG Mode Updated Every Frame**

```cpp
const auto ppg_mode = static_cast<common::PpgMode>(params_[P_OSC_MODE]);
for (uint32_t v = 0; v < kNumVoices; v++) {
    voices_[v].osc_a.SetMode(ppg_mode);
    voices_[v].osc_b.SetMode(ppg_mode);
}
```

This is redundant - mode rarely changes but is set 750 times/second.

#### 6. **LFO Rate Updated Every Frame**

```cpp
lfo_.SetRate(params_[P_LFO_RATE]);
```

Same issue - should only update on change.

---

## Missing Features (UI Perspective)

### 1. **LFO Shape Parameter**

The LFO class supports multiple shapes (sine, triangle, saw, square, S&H), but there's no UI parameter to select them. Users are stuck with sine-only modulation.

**Recommendation**: Add LFO SHAPE parameter to Page 6, possibly replacing or augmenting existing params.

### 2. **Velocity to Filter Cutoff**

The synth receives velocity but it only affects amplitude. Many wavetable synths use velocity to modulate filter cutoff for expressive playing.

**Recommendation**: Add `VEL>FLT` parameter for velocity-to-filter scaling.

### 3. **Key Tracking for Filter**

No keyboard tracking for filter cutoff. Playing higher notes may sound dull while low notes are too bright.

**Recommendation**: Add `KEY TRK` parameter for filter tracking.

### 4. **Pitch Bend Range**

Pitch bend is hardcoded to ±2 semitones:
```cpp
const float base_note = static_cast<float>(voice.note) + pitch_bend_ * 2.0f;
```

**Recommendation**: Make pitch bend range a parameter (±2, ±7, ±12, ±24 semitones).

### 5. **Glide/Portamento**

No legato/portamento feature. Playing legato phrases jumps between notes.

### 6. **Osc B Fine Tune**

Osc B has octave offset but no fine detune. Classic wavetable sounds often use slight detuning between oscillators.

**Recommendation**: Add `B TUNE` parameter for fine detuning Osc B.

---

## String Value Improvements

### Current Bank Names Too Long

PPG bank names may be truncated on display:
- "UPPER_WT" → "UPPER_WT" (8 chars, OK)
- "RESONANT1" → likely truncated
- "RESONANT2" → likely truncated

**Recommendation**: Shorten to 6-7 char names:
```c
"UPPER", "RESNT1", "RESNT2", "MELLOW", "BRIGHT", "HARSH",
"CLIPPR", "SYNC", "PWM", "VOCAL1", "VOCAL2", "ORGAN",
"BELL", "ALIEN", "NOISE", "SPECAL"
```

### Mode Names

Current mode names are good ("HiFi", "LoFi", "Raw" - all 4 chars or less).

---

## Preset System

### Current State

- `num_presets = 8` in header.c
- `LoadPreset()` only stores index, doesn't load actual parameters
- `GetPresetData()` returns nullptr

**Issue**: Presets are declared but non-functional.

**Recommendation**: Either:
1. Implement proper preset storage/recall
2. Or set `num_presets = 0` to avoid user confusion

---

## Recommendations Summary

### High Priority (Affects Usability)

| Issue | Fix |
|-------|-----|
| OSC MIX center mismatch | Set `center = 0` or `default = 64` |
| SPACE center mismatch | Set `center = 64` |
| No cutoff smoothing | Add one-pole filter for cutoff param |
| No mix smoothing | Add one-pole filter for OSC MIX |
| Redundant param updates | Add dirty flags, update only on change |

### Medium Priority (Polish)

| Issue | Fix |
|-------|-----|
| OSC MODE default | Consider defaulting to HiFi (0) |
| Bank names too long | Shorten to 6-7 chars |
| Missing LFO shape | Add LFO SHP parameter |
| Missing Osc B fine tune | Add B TUNE parameter |
| Non-functional presets | Implement or disable |

### Low Priority (Nice to Have)

| Feature | Description |
|---------|-------------|
| Velocity to filter | VEL>FLT parameter |
| Key tracking | KEY TRK parameter |
| Pitch bend range | Configurable ±2 to ±24 |
| Glide/portamento | GLIDE parameter |
| Envelope time display | Show ms/s hints |

---

## Code Quality Notes

### Positive

- Clean separation between header.c (metadata) and vapo2_synth.h (logic)
- Good use of enum for parameter indices
- Consistent parameter naming convention
- Voice allocation is well-implemented

### Areas for Improvement

- Parameter smoothing should be centralized (SmoothedParameter class)
- Dirty flag system would prevent redundant calculations
- Consider using a parameter descriptor struct for validation

---

## Conclusion

Vapo2 is musically functional and the parameter layout is reasonable for a drumlogue unit. The main issues are:

1. **Parameter center/default mismatches** that will confuse users with encoder detents
2. **Lack of parameter smoothing** causing audible artifacts
3. **Performance inefficiencies** from updating parameters every frame
4. **Missing expressive features** (LFO shape, detune, key tracking)

With the high-priority fixes, the synth would feel significantly more polished on the hardware.

---

## Appendix: Parameter Optimization Proposal

### The Challenge

The drumlogue SDK limits us to **24 parameters** (6 pages × 4 params). We're currently using all 24, but missing important features like LFO shape, Osc B detune, velocity routing, and key tracking.

### Solution: Multi-Function "Selector" Parameters

Combine related parameters using a **selector + value** pattern. One knob selects *what* to edit, another knob sets the *value*. This is common in hardware synths with limited controls.

---

### Proposed Page 6 Redesign: "MOD HUB"

**Current Page 6:**
| Param | Name | Function |
|-------|------|----------|
| 1 | LFO RT | LFO Speed |
| 2 | LFO>MRP | LFO to Morph depth |
| 3 | OSC MIX | Osc A/B balance |
| 4 | SPACE | Stereo width |

**Proposed Page 6 (Multi-function):**
| Param | Name | Function |
|-------|------|----------|
| 1 | MOD SEL | Selects which mod parameter to edit (0-7) |
| 2 | MOD VAL | Value for selected parameter |
| 3 | OSC MIX | Osc A/B balance (keep - used frequently) |
| 4 | SPACE | Stereo width (keep - used frequently) |

**MOD SEL Options (0-7):**
| Value | Display | Controls | Range |
|-------|---------|----------|-------|
| 0 | LFO SPD | LFO Rate | 0-127 (0.05-20Hz) |
| 1 | LFO SHP | LFO Shape | 0-5 (Sin/Tri/Saw+/Saw-/Sqr/S&H) |
| 2 | LFO>MRP | LFO to Morph | -64 to +63 |
| 3 | LFO>FLT | LFO to Filter | -64 to +63 |
| 4 | VEL>FLT | Velocity to Filter | 0-127 |
| 5 | KEY TRK | Filter Key Track | 0-127 (0-100%) |
| 6 | B TUNE | Osc B Fine Detune | -64 to +63 cents |
| 7 | PB RNG | Pitch Bend Range | 0-3 (±2/±7/±12/±24) |

**Implementation:**

```cpp
// In header.c
{0, 7, 0, 0, k_unit_param_type_strings, 0, 0, 0, {"MOD SEL"}},
{0, 127, 64, 64, k_unit_param_type_none, 0, 0, 0, {"MOD VAL"}},

// String callback returns current selection name
const char* GetParameterStr(uint8_t id, int32_t value) {
    if (id == P_MOD_SELECT) {
        static const char* mod_names[] = {
            "LFO SPD", "LFO SHP", "LFO>MRP", "LFO>FLT",
            "VEL>FLT", "KEY TRK", "B TUNE", "PB RNG"
        };
        return mod_names[value];
    }
    // ...
}

// In SetParameter, route MOD_VAL to the selected target
void SetParameter(uint8_t id, int32_t value) {
    if (id == P_MOD_VALUE) {
        switch (params_[P_MOD_SELECT]) {
            case 0: lfo_rate_ = value; break;
            case 1: lfo_shape_ = value; break;
            case 2: lfo_to_morph_ = value - 64; break;
            // ... etc
        }
    }
}
```

---

### Alternative: Dedicated LFO Page

If the multi-function approach feels too complex, consider reorganizing pages:

**Option A: Merge Envelope Pages**

Combine amp and filter envelopes into one page using shared A/D/S/R controls with a toggle:

| Page 4 | Name | Function |
|--------|------|----------|
| 1 | ENV SEL | Amp / Filter select |
| 2 | ATTACK | Attack time (for selected env) |
| 3 | DECAY | Decay time |
| 4 | SUSTAIN | Sustain level |

Then use freed Page 5 for LFO:

| Page 5 (LFO) | Name | Function |
|--------------|------|----------|
| 1 | LFO SPD | Rate |
| 2 | LFO SHP | Shape |
| 3 | LFO>MRP | To Morph |
| 4 | LFO>FLT | To Filter |

**Downside**: Loses independent release times for amp/filter envelopes.

---

### Recommendation

**Go with the MOD HUB approach** (selector + value on Page 6). Reasons:

1. **No loss of current features** - OSC MIX and SPACE remain directly accessible
2. **Expandable** - Can add more mod destinations later
3. **Familiar pattern** - Many hardware synths use this (Microfreak, Digitone, etc.)
4. **Only 2 slots used** - Still have 2 quick-access params (MIX, SPACE)

**User Workflow:**
1. Turn MOD SEL to "LFO SHP"
2. Turn MOD VAL to select triangle
3. Turn MOD SEL to "LFO>FLT"  
4. Turn MOD VAL to set filter modulation depth
5. OSC MIX and SPACE always work directly without selection

---

### Visual Feedback Consideration

For the selector approach to work well, the display should show:
- **MOD SEL**: Shows the destination name ("LFO SPD", "VEL>FLT", etc.)
- **MOD VAL**: Shows the current value for the *selected* destination

This requires `GetParameterStr()` for MOD_VAL to be context-aware:

```cpp
const char* GetParameterStr(uint8_t id, int32_t value) {
    if (id == P_MOD_VALUE) {
        // Return value string based on current MOD_SELECT
        switch (params_[P_MOD_SELECT]) {
            case 1: // LFO Shape
                return lfo_shape_names[value];
            case 7: // PB Range
                return pb_range_names[value];
            // Others show numeric value (return nullptr)
        }
    }
}
```

---

## Implementation Plan

### Overview

This plan addresses all high-priority fixes plus the MOD HUB feature. Changes are organized into phases that can be implemented and tested incrementally.

---

### Phase 1: Fix Bipolar Parameter Ranges

**Goal**: Use proper signed ranges (-64 to +63) for bipolar parameters instead of 0-127 with implicit center at 64.

#### Parameters to Convert

| Parameter | Current Range | New Range | Notes |
|-----------|---------------|-----------|-------|
| A TUNE | -64 to +63 | -64 to +63 | ✅ Already correct |
| B OCT | -3 to +3 | -3 to +3 | ✅ Already correct |
| FLT ENV | -64 to +63 | -64 to +63 | ✅ Already correct |
| OSC MIX | 0-127 (center=64) | **-64 to +63** | -64=A only, 0=50/50, +63=B only |
| SPACE | 0-127 (center=64) | **-64 to +63** | -64=mono, 0=normal, +63=wide |

#### Changes Required

**header.c:**

```c
// OSC MIX: Change from 0-127 to bipolar
// Old: {0, 127, 64, 0, k_unit_param_type_none, 0, 0, 0, {"OSC MIX"}},
// New:
{-64, 63, 0, 0, k_unit_param_type_none, 0, 0, 0, {"OSC MIX"}},

// SPACE: Change from 0-127 to bipolar  
// Old: {0, 127, 0, 64, k_unit_param_type_none, 0, 0, 0, {"SPACE"}},
// New:
{-64, 63, 0, 0, k_unit_param_type_none, 0, 0, 0, {"SPACE"}},
```

**vapo2_synth.h:**

```cpp
// OSC MIX calculation
// Old: const float osc_mix = params_[P_OSC_MIX] / 127.0f;
// New:
const float osc_mix = (params_[P_OSC_MIX] + 64) / 127.0f;  // -64..+63 → 0..1

// SPACE calculation  
// Old: const float space = params_[P_SPACE] / 127.0f * 1.5f;
// New:
const float space = (params_[P_SPACE] + 64) / 127.0f * 1.5f;  // -64..+63 → 0..1.5
```

---

### Phase 2: Add Parameter Smoothing

**Goal**: Eliminate zipper noise on continuous parameters.

#### Add SmoothedValue Helper Class

Create `drumlogue/vapo2/smoothed_value.h`:

```cpp
class SmoothedValue {
public:
    void Init(float initial = 0.0f, float coef = 0.01f) {
        value_ = target_ = initial;
        coef_ = coef;
    }
    
    void SetTarget(float target) { target_ = target; }
    
    float Process() {
        value_ += coef_ * (target_ - value_);
        return value_;
    }
    
    float GetValue() const { return value_; }
    
private:
    float value_, target_, coef_;
};
```

#### Apply to Parameters

In `Vapo2Synth` class:

```cpp
// Member variables
SmoothedValue cutoff_smooth_;
SmoothedValue osc_mix_smooth_;
SmoothedValue space_smooth_;

// In Init()
cutoff_smooth_.Init(1.0f, 0.005f);   // Slower for filter
osc_mix_smooth_.Init(0.5f, 0.01f);
space_smooth_.Init(0.0f, 0.01f);

// In Render() - update targets from params
cutoff_smooth_.SetTarget(params_[P_FILTER_CUTOFF] / 127.0f);
osc_mix_smooth_.SetTarget((params_[P_OSC_MIX] + 64) / 127.0f);
space_smooth_.SetTarget((params_[P_SPACE] + 64) / 127.0f);

// Use smoothed values in processing
const float cutoff_base = cutoff_smooth_.Process();
const float osc_mix = osc_mix_smooth_.Process();
```

---

### Phase 3: Add Dirty Flags for Expensive Updates

**Goal**: Only recalculate envelope coefficients, LFO rate, etc. when parameters actually change.

#### Implementation

```cpp
// Member variable
uint32_t params_dirty_;  // Bitmask of changed params

// In SetParameter()
void SetParameter(uint8_t id, int32_t value) {
    if (id < P_NUM_PARAMS && params_[id] != value) {
        params_[id] = value;
        params_dirty_ |= (1u << id);
    }
}

// In Render() - check dirty flags before expensive operations
if (params_dirty_ & ((1u << P_AMP_ATTACK) | (1u << P_AMP_DECAY) | 
                     (1u << P_AMP_SUSTAIN) | (1u << P_AMP_RELEASE))) {
    for (uint32_t v = 0; v < kNumVoices; v++) {
        voices_[v].amp_env.SetAttack(params_[P_AMP_ATTACK]);
        voices_[v].amp_env.SetDecay(params_[P_AMP_DECAY]);
        voices_[v].amp_env.SetSustain(params_[P_AMP_SUSTAIN] / 127.0f);
        voices_[v].amp_env.SetRelease(params_[P_AMP_RELEASE]);
    }
}

// Clear dirty flags at end of Render()
params_dirty_ = 0;
```

---

### Phase 4: Implement MOD HUB (Page 6 Redesign)

**Goal**: Replace LFO RT and LFO>MRP with MOD SEL + MOD VAL to unlock 8 new parameters.

#### 4.1 Update Parameter Enum

**vapo2_synth.h:**

```cpp
enum Vapo2Params {
    // Pages 1-5 unchanged...
    
    // Page 6: MOD HUB & Output
    P_MOD_SELECT = 20,   // Was P_LFO_RATE
    P_MOD_VALUE,         // Was P_LFO_TO_MORPH  
    P_OSC_MIX,
    P_SPACE,
    
    P_NUM_PARAMS
};

// MOD HUB destinations
enum ModDestination {
    MOD_LFO_RATE = 0,
    MOD_LFO_SHAPE,
    MOD_LFO_TO_MORPH,
    MOD_LFO_TO_FILTER,
    MOD_VEL_TO_FILTER,
    MOD_KEY_TRACK,
    MOD_OSC_B_DETUNE,
    MOD_PB_RANGE,
    MOD_NUM_DESTINATIONS
};
```

#### 4.2 Update header.c

```c
// Page 6: MOD HUB & Output
{0, 7, 0, 0, k_unit_param_type_strings, 0, 0, 0, {"MOD SEL"}},
{-64, 63, 0, 0, k_unit_param_type_strings, 0, 0, 0, {"MOD VAL"}},
{-64, 63, 0, 0, k_unit_param_type_none, 0, 0, 0, {"OSC MIX"}},
{-64, 63, 0, 0, k_unit_param_type_none, 0, 0, 0, {"SPACE"}},
```

#### 4.3 Add MOD HUB State Storage

```cpp
// Store values for all 8 mod destinations
int32_t mod_values_[MOD_NUM_DESTINATIONS];

// In Init()
mod_values_[MOD_LFO_RATE] = 40;      // Default LFO speed
mod_values_[MOD_LFO_SHAPE] = 0;      // Sine
mod_values_[MOD_LFO_TO_MORPH] = 0;   // No modulation
mod_values_[MOD_LFO_TO_FILTER] = 0;
mod_values_[MOD_VEL_TO_FILTER] = 0;
mod_values_[MOD_KEY_TRACK] = 0;
mod_values_[MOD_OSC_B_DETUNE] = 0;
mod_values_[MOD_PB_RANGE] = 0;       // ±2 semitones
```

#### 4.4 Route MOD_VALUE to Selected Destination

```cpp
void SetParameter(uint8_t id, int32_t value) {
    if (id == P_MOD_VALUE) {
        // Store in the currently selected mod destination
        int sel = params_[P_MOD_SELECT];
        if (sel >= 0 && sel < MOD_NUM_DESTINATIONS) {
            mod_values_[sel] = value;
            // Mark the relevant internal parameter as dirty
            switch (sel) {
                case MOD_LFO_RATE:
                    params_dirty_ |= (1u << P_MOD_SELECT); // Trigger LFO update
                    break;
                // ... etc
            }
        }
    } else if (id < P_NUM_PARAMS) {
        params_[id] = value;
        params_dirty_ |= (1u << id);
    }
}

int32_t GetParameter(uint8_t id) const {
    if (id == P_MOD_VALUE) {
        // Return the value for the currently selected destination
        int sel = params_[P_MOD_SELECT];
        if (sel >= 0 && sel < MOD_NUM_DESTINATIONS) {
            return mod_values_[sel];
        }
        return 0;
    }
    return (id < P_NUM_PARAMS) ? params_[id] : 0;
}
```

#### 4.5 Update GetParameterStr for MOD HUB

```cpp
static const char* mod_dest_names[] = {
    "LFO SPD", "LFO SHP", "LFO>MRP", "LFO>FLT",
    "VEL>FLT", "KEY TRK", "B TUNE", "PB RNG"
};

static const char* lfo_shape_names[] = {
    "Sine", "Tri", "Saw+", "Saw-", "Square", "S&H"
};

static const char* pb_range_names[] = {
    "+/-2", "+/-7", "+/-12", "+/-24"
};

const char* GetParameterStr(uint8_t id, int32_t value) const {
    switch (id) {
        case P_MOD_SELECT:
            if (value >= 0 && value < MOD_NUM_DESTINATIONS) {
                return mod_dest_names[value];
            }
            break;
            
        case P_MOD_VALUE: {
            int sel = params_[P_MOD_SELECT];
            switch (sel) {
                case MOD_LFO_SHAPE:
                    if (value >= 0 && value < 6) return lfo_shape_names[value];
                    break;
                case MOD_PB_RANGE:
                    if (value >= 0 && value < 4) return pb_range_names[value];
                    break;
            }
            break;
        }
        // ... existing bank/filter type handling
    }
    return nullptr;
}
```

#### 4.6 Use MOD HUB Values in Render

```cpp
void Render(float* out, uint32_t frames) {
    // Read MOD HUB values
    const int32_t lfo_rate = mod_values_[MOD_LFO_RATE];
    const int32_t lfo_shape = mod_values_[MOD_LFO_SHAPE];
    const float lfo_to_morph = mod_values_[MOD_LFO_TO_MORPH] / 64.0f;
    const float lfo_to_filter = mod_values_[MOD_LFO_TO_FILTER] / 64.0f;
    const float vel_to_filter = mod_values_[MOD_VEL_TO_FILTER] / 127.0f;
    const float key_track = mod_values_[MOD_KEY_TRACK] / 127.0f;
    const float osc_b_detune = mod_values_[MOD_OSC_B_DETUNE] / 100.0f; // cents
    const int pb_range_idx = mod_values_[MOD_PB_RANGE];
    const float pb_semitones[] = {2.0f, 7.0f, 12.0f, 24.0f};
    const float pb_range = pb_semitones[pb_range_idx & 3];
    
    // Apply LFO shape
    lfo_.SetShape(static_cast<LFOShape>(lfo_shape));
    lfo_.SetRate(lfo_rate);
    
    // In voice processing loop:
    // ... apply vel_to_filter, key_track, osc_b_detune, pb_range ...
}
```

---

### Phase 5: Final Polish

#### 5.1 Shorten Bank Names

```cpp
static const char* ppg_bank_names[] = {
    "UPPER",  "RESNT1", "RESNT2", "MELLOW",
    "BRIGHT", "HARSH",  "CLIPPR", "SYNC",
    "PWM",    "VOCAL1", "VOCAL2", "ORGAN",
    "BELL",   "ALIEN",  "NOISE",  "SPECAL"
};
```

#### 5.2 Change OSC MODE Default

```c
// header.c - default to HiFi (0) instead of Raw (2)
{0, 2, 0, 0, k_unit_param_type_strings, 0, 0, 0, {"OSC MOD"}},
```

#### 5.3 Disable Non-Functional Presets

```c
// header.c
.num_presets = 0,  // Was 8
```

---

### Implementation Checklist

| Phase | Task | File(s) | Priority | Status |
|-------|------|---------|----------|--------|
| 1.1 | Convert OSC MIX to -64..+63 | header.c, vapo2_synth.h | HIGH | ✅ Done |
| 1.2 | Convert SPACE to -64..+63 | header.c, vapo2_synth.h | HIGH | ✅ Done |
| 2.1 | Create SmoothedValue class | smoothed_value.h (new) | HIGH | ✅ Done |
| 2.2 | Apply smoothing to cutoff | vapo2_synth.h | HIGH | ✅ Done |
| 2.3 | Apply smoothing to OSC MIX | vapo2_synth.h | HIGH | ✅ Done |
| 2.4 | Apply smoothing to SPACE | vapo2_synth.h | HIGH | ✅ Done |
| 3.1 | Add dirty flags mechanism | vapo2_synth.h | MEDIUM | ✅ Done |
| 3.2 | Guard envelope updates | vapo2_synth.h | MEDIUM | ✅ Done |
| 3.3 | Guard LFO/mode updates | vapo2_synth.h | MEDIUM | ✅ Done |
| 4.1 | Update param enum for MOD HUB | vapo2_synth.h | HIGH | ✅ Done |
| 4.2 | Update header.c Page 6 | header.c | HIGH | ✅ Done |
| 4.3 | Add mod_values_ storage | vapo2_synth.h | HIGH | ✅ Done |
| 4.4 | Implement SetParameter routing | vapo2_synth.h | HIGH | ✅ Done |
| 4.5 | Implement GetParameter routing | vapo2_synth.h | HIGH | ✅ Done |
| 4.6 | Update GetParameterStr | vapo2_synth.h | HIGH | ✅ Done |
| 4.7 | Use MOD HUB values in Render | vapo2_synth.h | HIGH | ✅ Done |
| 4.8 | Implement velocity to filter | vapo2_synth.h | MEDIUM | ✅ Done |
| 4.9 | Implement key tracking | vapo2_synth.h | MEDIUM | ✅ Done |
| 4.10 | Implement Osc B detune | vapo2_synth.h | MEDIUM | ✅ Done |
| 4.11 | Implement variable PB range | vapo2_synth.h | LOW | ✅ Done |
| 5.1 | Shorten bank names | vapo2_synth.h | LOW | ✅ Done |
| 5.2 | Change OSC MODE default | header.c | LOW | ✅ Done |
| 5.3 | Disable presets | header.c | LOW | ✅ Done |

**Build Status**: ✅ Successful (30KB, Dec 10 2025)

---

### Testing Plan

#### Phase 1 Tests

- [ ] OSC MIX: Verify -64 = Osc A only, 0 = 50/50, +63 = Osc B only
- [ ] SPACE: Verify -64 = mono, 0 = normal stereo, +63 = wide
- [ ] Encoder center detent aligns with 0 value

#### Phase 2 Tests
- [ ] Turn cutoff knob quickly - no zipper noise
- [ ] Turn OSC MIX quickly - smooth crossfade
- [ ] Turn SPACE quickly - no clicks

#### Phase 3 Tests
- [ ] CPU usage reduced when not turning knobs
- [ ] Parameters still respond immediately to changes

#### Phase 4 Tests
- [ ] MOD SEL cycles through all 8 destinations
- [ ] MOD VAL updates the selected destination
- [ ] Changing MOD SEL shows the stored value for that destination
- [ ] LFO shape changes waveform audibly
- [ ] LFO>FLT modulates filter
- [ ] VEL>FLT responds to velocity
- [ ] KEY TRK makes higher notes brighter
- [ ] B TUNE detunes Osc B
- [ ] PB RNG changes pitch bend range

---

### Final Parameter Layout

| Page | Param 1 | Param 2 | Param 3 | Param 4 |
|------|---------|---------|---------|---------|
| 1 | A BANK | A MORPH | A OCT | A TUNE |
| 2 | B BANK | B MORPH | B OCT | OSC MOD |
| 3 | CUTOFF | RESO | FLT ENV | FLT TYP |
| 4 | ATTACK | DECAY | SUSTAIN | RELEASE |
| 5 | F.ATK | F.DCY | F.SUS | F.REL |
| 6 | **MOD SEL** | **MOD VAL** | OSC MIX | SPACE |

**MOD SEL Options:**
| Sel | Name | MOD VAL Range | Description |
|-----|------|---------------|-------------|
| 0 | LFO SPD | 0-127 | LFO rate (0.05-20Hz) |
| 1 | LFO SHP | 0-5 | Sine/Tri/Saw+/Saw-/Sqr/S&H |
| 2 | LFO>MRP | -64..+63 | LFO to morph depth |
| 3 | LFO>FLT | -64..+63 | LFO to filter depth |
| 4 | VEL>FLT | 0-127 | Velocity to filter |
| 5 | KEY TRK | 0-127 | Filter key tracking |
| 6 | B TUNE | -64..+63 | Osc B detune (cents) |
| 7 | PB RNG | 0-3 | Pitch bend ±2/7/12/24 |

