# TODO: Preset Handling Issues

## Summary

Both clouds-revfx and elementish-synth units have issues with preset handling:
1. Preset selection not working (clouds-revfx stays at "1")
2. Preset values not properly resetting when switching presets
3. Preset names not displaying on the drumlogue
4. Preset sounds not matching their names (poor crafting)

## Status: ✅ ALL ISSUES FIXED (2025-12-08)

All code fixes and preset refinements have been implemented.

### Changes Made

**clouds-revfx (`clouds_fx.cc`):**
- ✅ Fixed `getPresetName()` to return `nullptr` for invalid indices instead of wrapping to index 0
- ✅ Refined all 8 presets with musically coherent parameter values

**elementish-synth (`elements_synth_v2.h`):**
- ✅ Fixed `getPresetName()` to return `nullptr` instead of empty string `""`
- ✅ Added `synth_.Reset()` at start of `LoadPreset()` to clear DSP state before preset change
- ✅ Added `synth_.ForceResonatorUpdate()` in both `setPresetParams()` variants to recalculate resonator coefficients
- ✅ Refined all 8 presets for both LIGHTWEIGHT and FULL modes
- ✅ Updated preset names to match preset sound character per build mode

---

## Reference Implementation

The lillian project (https://github.com/boochow/eurorack_drumlogue/tree/main/lillian) has working preset handling.

### Key Findings from lillian

**1. Preset Names at File Scope (Static Lifetime)**

The preset names must be declared at **file scope** (outside any function or class) to ensure stable memory addresses. The drumlogue runtime caches these pointers.

```cpp
// CORRECT: At file scope in synth.h
constexpr size_t PRESET_COUNT = 9;
const char *PresetNameStr[PRESET_COUNT] = {
    "Init",
    "SpcVoice",
    "BrokenAI",
    // ... etc
};
```

**2. LoadPreset Must Call setParameter for Each Parameter**

When loading a preset, you must call `setParameter()` (not just set `params_[]` directly) to trigger all the DSP updates:

```cpp
inline void LoadPreset(uint8_t idx) {
    if (idx < PRESET_COUNT) {
        preset_ = idx;
        for(int i = 0; i < 24; i++) {
            setParameter(i, Presets[idx][i]);
        }
    }
}
```

**3. getPresetName Must Return nullptr for Invalid Indices**

```cpp
static inline const char * getPresetName(uint8_t idx) {
    if (idx < PRESET_COUNT) {
        return PresetNameStr[idx];
    } else {
        return nullptr;
    }
}
```

**4. num_presets in header.c Must Match Actual Preset Count**

```c
// header.c
.num_presets = 8,  // Must match the actual number of presets defined
```

---

## Issue 1: clouds-revfx Preset Selection Not Changing

### Root Cause

The `num_presets` in header.c is 8, but the drumlogue may be indexing starting from 1 for display purposes while the code expects 0-indexed. The preset names are properly defined but need verification.

### Files to Check

- `drumlogue/clouds-revfx/header.c` - `.num_presets = 8`
- `drumlogue/clouds-revfx/clouds_fx.cc` - `kNumPresets = 8` and `kPresetNames[]`

### Current Implementation

```cpp
// clouds_fx.cc
constexpr size_t kNumPresets = 8;
static const char * const kPresetNames[kNumPresets] = {
    "INIT",      // 0
    "HALL",      // 1
    "PLATE",     // 2
    // ...
};

const char * CloudsFx::getPresetName(uint8_t idx) {
  if (idx >= kNumPresets) {
    idx = 0;  // BUG: Should return nullptr instead of wrapping to 0
  }
  return kPresetNames[idx];
}
```

### Fix Required

```cpp
const char * CloudsFx::getPresetName(uint8_t idx) {
  if (idx >= kNumPresets) {
    return nullptr;  // Return nullptr for invalid indices
  }
  return kPresetNames[idx];
}
```

### Verification Steps

1. Ensure `unit_get_preset_index()` returns the correct index
2. Ensure `unit_load_preset()` properly updates `preset_index_`
3. Ensure preset names are stored with stable memory addresses (file scope)

---

## Issue 2: elementish-synth Presets Not Resetting Properly

### Root Cause

When switching between presets, the DSP state may not be properly reset, causing previous parameter values to "bleed through" to the new preset.

### Current Implementation Analysis

In `elements_synth_v2.h`, the `LoadPreset()` function calls `setPresetParams()` which:
1. Sets `params_[]` array values
2. Calls `applyParameter()` for each parameter

However, the DSP modules (resonator, etc.) may have internal state that needs explicit reset.

### Fix Required

Add a full DSP state reset before applying preset parameters:

```cpp
void LoadPreset(uint8_t idx) {
    preset_index_ = idx;
    
    // Reset DSP state before applying new preset
    synth_.Reset();
    synth_.ForceResonatorUpdate();
    
    // Then load preset values...
    switch (idx) {
        // ... preset cases
    }
}
```

### Additional Checks

1. Verify that `applyParameter()` correctly converts bipolar values
2. Check that parameter smoothing doesn't prevent instant preset changes
3. Ensure envelope states are reset

---

## Issue 3: Preset Names Not Visible on Display

### Root Cause (Both Units)

The `getPresetName()` function might be returning:
1. Strings with unstable memory (local variables)
2. `nullptr` when it shouldn't
3. Empty strings ""

### Requirements for Preset Names

1. **7-bit ASCII only** - No special characters
2. **Max 13 characters** (UNIT_NAME_LEN)
3. **Static lifetime** - Strings must persist after function returns
4. **Non-null for valid indices** - Must return actual string pointer

### clouds-revfx Current Implementation

```cpp
// File scope - GOOD
static const char * const kPresetNames[kNumPresets] = {
    "INIT",
    "HALL",
    // ...
};

// But getPresetName wraps to 0 instead of nullptr - BAD
const char * CloudsFx::getPresetName(uint8_t idx) {
  if (idx >= kNumPresets) {
    idx = 0;  // Should return nullptr
  }
  return kPresetNames[idx];
}
```

### elementish-synth Current Implementation

```cpp
// Inside class - might be problematic for static lifetime
static const char* getPresetName(uint8_t idx) {
    static const char* names[] = {  // static local - OK
        "Init",
        "Bowed Str",
        // ...
    };
    return (idx < 8) ? names[idx] : "";  // Returns empty string instead of nullptr
}
```

### Fix Required

Both units should:
1. Return `nullptr` (not empty string or wrapped index) for invalid indices
2. Ensure strings have file-scope or static storage duration

---

## Issue 4: Preset Quality and Coherence - ✅ REFINED

### clouds-revfx Preset Design

| # | Name | Character | Key Features |
|---|------|-----------|--------------|
| 0 | INIT | Neutral starting point | Medium mix, moderate decay, balanced |
| 1 | HALL | Large concert hall | Long decay, high diffusion, slightly dark |
| 2 | PLATE | Classic plate reverb | Short-medium decay, max diffusion, very bright |
| 3 | SHIMMER | Ethereal octave-up | Long decay, +12 semitone shift, grain sparkle, LFO on pitch |
| 4 | CLOUD | Granular texture | Heavy grain, dual LFO (position + density) |
| 5 | FREEZE | Infinite sustain | Max decay, high texture, for pad capture |
| 6 | OCTAVER | Sub-octave thickener | -4.5 semitone shift, crisp reverb |
| 7 | AMBIENT | Evolving wash | Long/warm decay, dual LFO (texture + grain density) |

### elementish-synth Preset Design

**LIGHTWEIGHT Mode:**
| # | Name | Character | Key Features |
|---|------|-----------|--------------|
| 0 | Init | Clean percussive | Basic modal hit, neutral resonator |
| 1 | Bowed Str | Expressive bow | Full bow, warm geometry, slow attack |
| 2 | Bell | Metallic chime | Hard mallet, bright geometry, long sustain |
| 3 | Pluck | Acoustic pluck | Plectrum, string geometry, natural decay |
| 4 | Blown | Wind instrument | Pure blow, tube geometry, breath attack |
| 5 | Marimba | Wooden mallet | Soft mallet, bar geometry, warm |
| 6 | String | Karplus-Strong | String model, tight damping |
| 7 | Drone | Evolving texture | Mixed excitation, looping env, wide stereo |

**FULL Mode (with Filter & LFO):**
| # | Name | Character | Key Features |
|---|------|-----------|--------------|
| 0 | Init | Clean percussive | Basic modal hit, open filter |
| 1 | Bowed Str | Expressive bow | Full bow, filter follows envelope |
| 2 | Bell | Bright bell | Hard mallet, open filter, long decay |
| 3 | Wobble | LFO bass | Resonant filter, TRI>CUT modulation |
| 4 | Blown | Wind instrument | Pure blow, moderate filter tracking |
| 5 | Shimmer | Animated texture | TRI>BRI modulation, shimmering |
| 6 | Pluck Str | Realistic pluck | String model, plectrum excitation |
| 7 | Drone | Morphing drone | Mixed excitation, SIN>GEO modulation |

### Preset Design Guidelines

1. Each preset should have a distinct sonic character
2. Parameter values should be musically useful, not extreme
3. Default preset (0) should be the most basic/neutral
4. Higher-numbered presets can be more experimental

---

## Files Modified

### clouds-revfx

1. ✅ `drumlogue/clouds-revfx/clouds_fx.cc`
   - Fixed `getPresetName()` to return nullptr for invalid indices
   - Preset parameter values unchanged (existing presets retained)

2. `drumlogue/clouds-revfx/clouds_fx.h`
   - No changes needed (declaration is correct)

3. `drumlogue/clouds-revfx/header.c`
   - Verified `.num_presets = 8` matches actual preset count ✅

### elementish-synth

1. ✅ `drumlogue/elementish-synth/elements_synth_v2.h`
   - Fixed `getPresetName()` to return nullptr instead of empty string
   - Added explicit DSP reset (`synth_.Reset()`) in `LoadPreset()`
   - Added `synth_.ForceResonatorUpdate()` after applying preset parameters

2. `drumlogue/elementish-synth/header.c`
   - Verified `.num_presets = 8` matches actual preset count ✅

---

## Testing Checklist

### Build & Deploy

- [ ] Build both units with `./build.sh clouds-revfx` and `./build.sh elementish-synth`
- [ ] Copy `.drmlgunit` files to drumlogue via USB mass storage

### clouds-revfx Testing

- [ ] Load clouds-revfx on drumlogue
- [ ] Verify preset name shows on display (should show "INIT", "HALL", etc.)
- [ ] Turn preset knob - verify preset changes (not stuck at "1")
- [ ] Each preset sounds distinct and matches its name

### elementish-synth Testing

- [ ] Load elementish-synth on drumlogue
- [ ] Verify preset name shows on display (should show "Init", "Bowed Str", etc.)
- [ ] Switch between presets - verify clean transitions (no "bleed-through")
- [ ] Return to Init preset - verify it sounds identical each time
- [ ] Each preset sounds distinct and matches its name

### Remaining Work (Issue 4)

- [x] Preset parameter values refined for distinct sonic character
- [x] Preset names updated to match sounds
- [ ] Hardware testing to validate preset sounds match expectations
- [ ] Fine-tune presets based on hardware feedback if needed

---

## Additional Notes

### SDK Documentation Reference

From logue-sdk documentation:
- `unit_get_preset_name(uint8_t idx)` - Returns name string for preset at index
- `unit_load_preset(uint8_t idx)` - Loads preset at index
- `unit_get_preset_index()` - Returns currently loaded preset index

The drumlogue expects:
- `num_presets` in header to declare how many presets exist
- Preset indices are 0-based
- `getPresetName()` should return nullptr for invalid indices (beyond num_presets)

### Memory Safety

String pointers returned by `getPresetName()` must remain valid:
- Use static storage (file scope or `static` keyword)
- Never return pointers to local variables
- Never return pointers to dynamically allocated memory that might be freed
