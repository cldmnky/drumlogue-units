# Drupiter UI Design - Jupiter-8 Inspired Control Layout

**Version**: 2.0 Proposal  
**Date**: December 15, 2025  
**Constraint**: 24 parameters maximum (6 pages × 4 controls per page)

---

## Executive Summary

This document proposes a refined parameter layout for Drupiter that more closely mirrors the Roland Jupiter-8's legendary control surface. The Jupiter-8 has **60+ front panel controls**, while drumlogue provides only **24 parameters**. The strategy uses "hub controls" (mode selectors) to compress related parameters and prioritize the most musically essential controls.

### Design Philosophy

1. **Preserve Jupiter-8 workflow** - Keep oscillators, filter, and envelopes intuitive
2. **Hub controls for depth** - Use mode selectors to access related parameters
3. **Live performance priority** - Most-tweaked controls remain directly accessible
4. **Clear labeling** - Use Jupiter-8 terminology where possible

---

## Current vs. Proposed Layout Comparison

### Current Issues

- **MOD HUB** (Page 6) is cryptic - requires remembering 8 destinations
- **Missing HPF** - Jupiter-8 has a dedicated high-pass filter
- **Missing PWM controls** - Jupiter-8 has LFO and ENV-1 modulation of pulse width
- **Buried modulation** - LFO rate/depth hidden in hub selector
- **No VCF keyboard tracking control** - Jupiter-8 has dedicated slider
- **Missing VCO modulation routing** - Jupiter-8 has FM MOD and FREQ MOD sections

### Jupiter-8 Panel Breakdown

Looking at the actual Jupiter-8 panel, key sections are:

**Left Side:**
- Volume, Balance, Rate controls
- LFO (waveform, delay)

**Center:**
- VCO Modulator (LFO/ENV-1 to PWM and frequency)
- VCO-1 (range, waveform, pulse width)
- VCO-2 (range, waveform, tune, sync)

**Right Side:**
- HPF (high-pass filter cutoff)
- VCF (cutoff, resonance, env, LFO, keyboard follow)
- VCA (level)
- ENV-1 (filter) and ENV-2 (amp) with A/D/S/R

---

## Proposed Parameter Layout - Version 2.0

### Page 1: VCO-1 (Oscillator 1) ✓ Keep similar

| Param | Name | Type | Range | Description |
|-------|------|------|-------|-------------|
| 1 | **D1 RNG** | Strings | 16'/8'/4' | Oscillator 1 octave range |
| 2 | **D1 WAVE** | Strings | SAW/SQR/PLS/TRI | Waveform selection |
| 3 | **D1 PW** | Percent | 0-100% | Manual pulse width |
| 4 | **XMOD** | Percent | 0-100% | VCO2→VCO1 cross-modulation depth |

**Justification**: DCO-1 is fundamental to Jupiter-8 sound. Keep direct control.

---

### Page 2: VCO-2 & Sync ✓ Keep similar

| Param | Name | Type | Range | Description |
|-------|------|------|-------|-------------|
| 5 | **D2 RNG** | Strings | 16'/8'/4' | Oscillator 2 octave range |
| 6 | **D2 WAVE** | Strings | SAW/NOZ/PLS/SIN | Waveform (SAW/NOISE/PULSE/SINE) |
| 7 | **D2 TUNE** | Strings | -50¢ to +50¢ | Fine tuning (displays ±cents) |
| 8 | **SYNC** | Strings | OFF/SOFT/HARD | VCO1→VCO2 sync mode |

**Changes**:
- None (this page is already well-designed)

---

### Page 3: VCO Mix & VCF ⚠️ REVISED

| Param | Name | Type | Range | Description |
|-------|------|------|-------|-------------|
| 9 | **MIX** | Percent | 0-100% | VCO1 ← → VCO2 balance |
| 10 | **CUTOFF** | Percent | 0-100% | VCF cutoff frequency |
| 11 | **RESO** | Percent | 0-100% | VCF resonance/Q |
| 12 | **KEYFLW** | Percent | 0-100% | VCF keyboard tracking (50%=default) |

**Changes**:
- Renamed "OSC MIX" → **MIX** (shorter, clearer)
- **REMOVED**: VCF TYP (moved to Page 6 HUB)
- **ADDED**: **KEYFLW** (Jupiter-8 has this as a dedicated slider!)

**Justification**: Keyboard tracking is critical for musical filter response. On Jupiter-8, this is a dedicated front-panel slider. VCF type selection is less frequently changed, so it can be hub-controlled.

---

### Page 4: VCF Envelope (ENV-1) ✓ Keep

| Param | Name | Type | Range | Description |
|-------|------|------|-------|-------------|
| 13 | **F.ATK** | Percent | 0-100% | Filter envelope attack time |
| 14 | **F.DCY** | Percent | 0-100% | Filter envelope decay time |
| 15 | **F.SUS** | Percent | 0-100% | Filter envelope sustain level |
| 16 | **F.REL** | Percent | 0-100% | Filter envelope release time |

**Changes**: None (ADSR is sacred)

---

### Page 5: VCA Envelope (ENV-2) ✓ Keep

| Param | Name | Type | Range | Description |
|-------|------|------|-------|-------------|
| 17 | **A.ATK** | Percent | 0-100% | Amp envelope attack time |
| 18 | **A.DCY** | Percent | 0-100% | Amp envelope decay time |
| 19 | **A.SUS** | Percent | 0-100% | Amp envelope sustain level |
| 20 | **A.REL** | Percent | 0-100% | Amp envelope release time |

**Changes**: None (ADSR is sacred)

---

### Page 6: Modulation Hub ⚡ MAJOR REDESIGN

| Param | Name | Type | Range | Description |
|-------|------|------|-------|-------------|
| 21 | **LFO RT** | Percent | 0-100% | LFO rate (0.1Hz - 50Hz) |
| 22 | **MOD HUB** | Strings | 8 modes | Modulation destination selector |
| 23 | **MOD AMT** | Strings | Context | Amount for selected MOD destination |
| 24 | **EFFECT** | Strings | 4 modes | Output effect selector |

**MOD HUB Modes** (8 total):

| Mode | Display | MOD AMT Controls | Description |
|------|---------|------------------|-------------|
| 0 | **LFO→PWM** | 0-100% depth | LFO modulates pulse width (both VCOs) |
| 1 | **LFO→VCF** | 0-100% depth | LFO modulates filter cutoff |
| 2 | **LFO→VCO** | 0-100% depth | LFO modulates pitch (vibrato) |
| 3 | **ENV→PWM** | 0-100% depth | Filter ENV modulates pulse width |
| 4 | **ENV→VCF** | -100% to +100% | Filter ENV modulation (±4 octaves) |
| 5 | **HPF** | 0-100% cutoff | High-pass filter (Jupiter-8 feature!) |
| 6 | **VCF TYP** | LP12/LP24/HP12/BP | Filter type selection |
| 7 | **LFO DLY** | 0-100% time | LFO delay (fade-in time) |

**EFFECT Modes** (4 total):

| Mode | Display | Description |
|------|---------|-------------|
| 0 | **CHORUS** | Analog chorus (Jupiter-8 has built-in chorus) |
| 1 | **SPACE** | Stereo widening |
| 2 | **DRY** | Bypass all effects |
| 3 | **BOTH** | Chorus + Space combined |

**Changes from Current**:
- **ADDED**: Direct **LFO RT** control (no longer buried in hub)
- **REDESIGNED**: MOD HUB with Jupiter-8 specific routings
- **ADDED**: **ENV→VCF** mode (currently fixed, now controllable)
- **ADDED**: **HPF** mode (Jupiter-8's dedicated high-pass filter)
- **ADDED**: **LFO DLY** mode (Jupiter-8 has LFO delay with dedicated slider)
- **SIMPLIFIED**: EFFECT selector replaces individual CHORUS/SPACE params

---

## Detailed Modulation Hub Implementation

### Why a Hub Control?

The Jupiter-8 has **11 dedicated modulation controls**:
- LFO waveform
- LFO delay
- PWM MOD (LFO/ENV-1 to pulse width)
- FREQ MOD (LFO to VCO frequency)
- VCF LFO mod
- VCF ENV mod
- VCF keyboard follow
- HPF cutoff

We can fit **3 controls** on Page 6, so we use:
- **Direct LFO Rate** (most commonly tweaked)
- **Hub Selector** (8 destinations)
- **Hub Amount** (context-sensitive value)

### Hub Display Strategy

When **MOD HUB** is turned, the drumlogue display shows:
```
MOD: LFO→PWM    (mode name)
```

When **MOD AMT** is turned, display shows context:
```
PWM: 45%        (if hub is on LFO→PWM)
HPF: 2.5kHz     (if hub is on HPF)
VCF: LP24       (if hub is on VCF TYP)
```

This gives users clear feedback about what they're controlling.

---

## Jupiter-8 Fidelity Improvements

### What We Gain

1. **LFO Rate** - Direct control (was buried in hub)
2. **HPF** - Jupiter-8's signature high-pass filter now accessible
3. **Keyboard Follow** - Dedicated control for musical filter tracking
4. **PWM Modulation** - LFO and ENV can modulate pulse width
5. **ENV→VCF Amount** - Dedicated control (currently fixed)
6. **LFO Delay** - Fade-in effect for vibrato/modulation
7. **Clear modulation routing** - Named after Jupiter-8 sections

### What We Still Can't Do

Due to 24-param limit, these Jupiter-8 features remain unavailable:
- LFO waveform selection (could fix to triangle)
- Separate VCO1/VCO2 pulse width (use common PW)
- Independent VCO levels (use MIX balance instead)
- Bender range controls
- Second LFO
- Poly/unison modes (drumlogue is mono anyway)

---

## Implementation Notes

### Code Changes Required

1. **header.c**: Update parameter descriptors for Page 3 and Page 6
2. **unit.cc**: Implement hub control logic with context-aware value handling
3. **drupiter_synth.cc**: Add HPF, LFO delay envelope, PWM modulation routing
4. **Display strings**: Context-sensitive MOD AMT display

### String Value Examples

```c
// MOD HUB selector (param 22)
const char* mod_hub_names[] = {
    "LFO>PWM",    // LFO modulates pulse width
    "LFO>VCF",    // LFO modulates filter cutoff
    "LFO>VCO",    // LFO modulates pitch (vibrato)
    "ENV>PWM",    // Envelope modulates pulse width
    "ENV>VCF",    // Envelope modulates VCF (with ± range)
    "HPF",        // High-pass filter cutoff
    "VCF TYP",    // Filter type selector
    "LFO DLY"     // LFO delay time
};

// EFFECT selector (param 24)
const char* effect_names[] = {
    "CHORUS",     // Analog chorus only
    "SPACE",      // Stereo widening only
    "DRY",        // Bypass all effects
    "BOTH"        // Chorus + Space
};
```

### Parameter Value Encoding

For bipolar parameters like **ENV→VCF**:
- 0-49 = negative modulation (filter closes with envelope)
- 50 = center (no modulation)
- 51-100 = positive modulation (filter opens with envelope)

Display as: `-100%` to `+100%` with center at `0%`

---

## User Experience Improvements

### Before (Current Design)

**Scenario**: User wants to add vibrato

1. Go to Page 6
2. Turn MOD SEL to mode 2 (LFO rate)
3. Adjust MOD VAL
4. Turn MOD SEL to mode 0 (LFO→VCO depth)
5. Adjust MOD VAL again
6. **Result**: Confusing, requires remembering mode numbers

### After (Proposed Design)

**Scenario**: User wants to add vibrato

1. Go to Page 6
2. Adjust **LFO RT** (direct control)
3. Turn **MOD HUB** to "LFO>VCO"
4. Adjust **MOD AMT** (display shows "VCO: 35%")
5. **Result**: Clear, named controls, direct feedback

---

## Additional Suggestions

### 1. LFO Waveform Selection

**Option A**: Fix to triangle wave (most versatile for modulation)  
**Option B**: Add to MOD HUB as mode 8 (sacrifice one current mode)  
**Recommendation**: Fix to triangle, saves parameter space

### 2. Sub-Oscillator

Jupiter-8 doesn't have a dedicated sub-osc, but Jupiter-6 does. Consider:
- Use VCO1 at 16' range as sub-bass
- OR: Add -1 octave mode to VCO2

### 3. Arpeggiator

Jupiter-8 has a built-in arpeggiator. For drumlogue:
- **Not feasible**: Requires polyphony and note memory
- **Alternative**: Use drumlogue's external arp mode

### 4. Preset Names

Rename presets to classic Jupiter-8 patch types:
1. **Brass 1** (current: Brass Lead)
2. **String 1** (current: String Pad)
3. **Bass 1** (current: Fat Bass)
4. **Lead 1** (current: Sync Lead)
5. **Pad 1** (current: Mod Pad)
6. **FX 1** (current: Space Lead)

---

## Visual Reference - Parameter Map

```
┌─────────────────────────────────────────────────────────┐
│  DRUPITER - Jupiter-8 Inspired Synth for Drumlogue     │
├─────────────────────────────────────────────────────────┤
│                                                         │
│  PAGE 1: VCO-1                                          │
│  ┌──────┬──────┬──────┬──────┐                         │
│  │ D1   │ D1   │ D1   │ XMOD │                         │
│  │ RNG  │ WAVE │ PW   │      │                         │
│  └──────┴──────┴──────┴──────┘                         │
│                                                         │
│  PAGE 2: VCO-2                                          │
│  ┌──────┬──────┬──────┬──────┐                         │
│  │ D2   │ D2   │ D2   │ SYNC │                         │
│  │ RNG  │ WAVE │ TUNE │      │                         │
│  └──────┴──────┴──────┴──────┘                         │
│                                                         │
│  PAGE 3: MIX & VCF                                      │
│  ┌──────┬──────┬──────┬──────┐                         │
│  │ MIX  │CUTOFF│ RESO │KEYFLW│  ← NEW: Keyboard track │
│  └──────┴──────┴──────┴──────┘                         │
│                                                         │
│  PAGE 4: ENV-1 (Filter)                                │
│  ┌──────┬──────┬──────┬──────┐                         │
│  │F.ATK │F.DCY │F.SUS │F.REL │                         │
│  └──────┴──────┴──────┴──────┘                         │
│                                                         │
│  PAGE 5: ENV-2 (Amp)                                   │
│  ┌──────┬──────┬──────┬──────┐                         │
│  │A.ATK │A.DCY │A.SUS │A.REL │                         │
│  └──────┴──────┴──────┴──────┘                         │
│                                                         │
│  PAGE 6: MODULATION ⚡                                  │
│  ┌──────┬──────┬──────┬──────┐                         │
│  │ LFO  │ MOD  │ MOD  │EFFECT│  ← NEW: Direct LFO     │
│  │ RT   │ HUB  │ AMT  │      │         + Hub system   │
│  └──────┴──────┴──────┴──────┘                         │
│         ↓                                               │
│    [LFO>PWM, LFO>VCF, LFO>VCO, ENV>PWM,               │
│     ENV>VCF, HPF, VCF TYP, LFO DLY]                    │
│                                                         │
└─────────────────────────────────────────────────────────┘
```

---

## Implementation Priority

### Phase 1: Core Improvements (High Priority)
- [ ] Add **KEYFLW** parameter (Page 3, param 12)
- [ ] Replace **VCF TYP** with hub mode
- [ ] Implement **LFO RT** direct control (Page 6, param 21)
- [ ] Redesign **MOD HUB** with 8 named modes

### Phase 2: Modulation Routing (Medium Priority)
- [x] Implement **LFO→PWM** modulation path
- [x] Implement **ENV→PWM** modulation path
- [x] Add **HPF** (high-pass filter)
- [x] Add **LFO DLY** (delay envelope for LFO fade-in)

### Phase 3: Polish (Low Priority)
- [x] Context-sensitive MOD AMT display strings
- [x] EFFECT hub with 4 modes (Chorus/Space/Dry/Both)
- [x] Update preset names to Jupiter-8 style
- [x] Add bipolar ENV→VCF range (±100%)

---

## Conclusion

This redesign brings Drupiter significantly closer to the Jupiter-8 experience while respecting the 24-parameter limit. The key innovations are:

1. **Direct LFO Rate** - No longer buried in a menu
2. **Keyboard Follow** - Essential for musical filter response
3. **Smart Hub Control** - Access 8 modulation targets efficiently
4. **HPF Access** - Jupiter-8's signature high-pass filter
5. **Named Routing** - Clear modulation paths (LFO→PWM, ENV→VCF, etc.)

The hub control approach lets advanced users access Jupiter-8 features while keeping the most-used controls immediately accessible. This is the optimal balance between depth and usability within drumlogue's constraints.

---

**Next Steps**: Review this proposal, then begin implementation starting with Phase 1 changes to `header.c` and hub control logic in `unit.cc`.

---

# Appendix: Common Utilities Proposal

## Shared Code Opportunities

After analyzing all units (drupiter-synth, pepege-synth, elementish-synth, clouds-revfx), several patterns emerge that should be abstracted into `drumlogue/common/`:

---

## 1. Hub Control System (`hub_control.h`)

### Problem
Multiple units implement "hub" controls (mode selector + value) differently:
- **Drupiter**: MOD HUB with 8 destinations
- **Pepege**: MOD HUB with 8 destinations
- Both have duplicated string lookup logic
- No type safety or validation

### Proposed Solution

```cpp
// drumlogue/common/hub_control.h
#pragma once

#include <cstdint>

namespace common {

/**
 * @brief Hub control system for compressing multiple related parameters
 * 
 * A hub control consists of:
 * - Selector parameter: chooses destination (0-N)
 * - Value parameter: sets value for selected destination
 * 
 * This allows N parameters to be controlled with just 2 UI slots.
 */
template<uint8_t NUM_DESTINATIONS>
class HubControl {
 public:
  struct Destination {
    const char* name;        // Short display name (e.g., "LFO>PWM")
    const char* value_unit;  // Unit suffix (e.g., "%", "Hz", "dB")
    int32_t min;             // Minimum value
    int32_t max;             // Maximum value
    int32_t default_value;   // Default/center value
    bool bipolar;            // True = display as ±, false = 0-max
  };
  
  HubControl(const Destination* destinations) 
    : destinations_(destinations), current_dest_(0) {
    for (uint8_t i = 0; i < NUM_DESTINATIONS; i++) {
      values_[i] = destinations[i].default_value;
    }
  }
  
  // Set which destination is selected (0-N)
  void SetDestination(uint8_t dest) {
    if (dest < NUM_DESTINATIONS) {
      current_dest_ = dest;
    }
  }
  
  // Set value for current destination
  void SetValue(int32_t value) {
    const Destination& dest = destinations_[current_dest_];
    if (value >= dest.min && value <= dest.max) {
      values_[current_dest_] = value;
    }
  }
  
  // Get value for specific destination
  int32_t GetValue(uint8_t dest) const {
    return (dest < NUM_DESTINATIONS) ? values_[dest] : 0;
  }
  
  // Get current destination index
  uint8_t GetDestination() const { return current_dest_; }
  
  // Get destination name for display
  const char* GetDestinationName(uint8_t dest) const {
    return (dest < NUM_DESTINATIONS) ? destinations_[dest].name : "";
  }
  
  // Get formatted value string for display
  const char* GetValueString(char* buffer, size_t buf_size) const {
    const Destination& dest = destinations_[current_dest_];
    int32_t value = values_[current_dest_];
    
    if (dest.bipolar) {
      // Display as ±: -100% to +100%
      int32_t range = dest.max - dest.min;
      int32_t centered = value - (dest.min + range / 2);
      int32_t percent = (centered * 100) / (range / 2);
      snprintf(buffer, buf_size, "%+d%s", percent, dest.value_unit);
    } else {
      // Display as absolute value
      snprintf(buffer, buf_size, "%d%s", value, dest.value_unit);
    }
    return buffer;
  }
  
 private:
  const Destination* destinations_;
  int32_t values_[NUM_DESTINATIONS];
  uint8_t current_dest_;
};

}  // namespace common
```

### Usage Example (Drupiter)

```cpp
// In drupiter_synth.h
#include "common/hub_control.h"

// Define destinations
static constexpr common::HubControl<8>::Destination kModDestinations[] = {
  {"LFO>PWM", "%",   0, 100, 0,  false},  // LFO to pulse width
  {"LFO>VCF", "%",   0, 100, 0,  false},  // LFO to filter
  {"LFO>VCO", "%",   0, 100, 0,  false},  // LFO vibrato
  {"ENV>PWM", "%",   0, 100, 0,  false},  // ENV to pulse width
  {"ENV>VCF", "%",   0, 100, 50, true},   // ENV to filter (bipolar!)
  {"HPF",     "Hz",  0, 100, 0,  false},  // High-pass filter
  {"VCF TYP", "",    0, 3,   1,  false},  // Filter type (enum)
  {"LFO DLY", "ms",  0, 100, 0,  false}   // LFO delay
};

class DrupiterSynth {
 private:
  common::HubControl<8> mod_hub_{kModDestinations};
};

// In SetParameter()
case PARAM_MOD_HUB:
  mod_hub_.SetDestination(value);
  break;
case PARAM_MOD_AMT:
  mod_hub_.SetValue(value);
  break;

// In GetParameterStr()
case PARAM_MOD_HUB:
  return mod_hub_.GetDestinationName(value);
case PARAM_MOD_AMT:
  static char buf[16];
  return mod_hub_.GetValueString(buf, sizeof(buf));
```

**Benefits**:
- Type-safe destination management
- Automatic range validation
- Consistent string formatting
- Reusable across all units
- Bipolar range support (±100%)

---

## 2. Preset Management (`preset_manager.h`)

### Problem
Every unit implements preset loading differently:
- Preset struct definitions duplicated
- No standard preset validation
- Inconsistent preset name handling

### Proposed Solution

```cpp
// drumlogue/common/preset_manager.h
#pragma once

#include <cstdint>
#include <cstring>

namespace common {

/**
 * @brief Generic preset management system
 * 
 * Handles preset storage, loading, and validation.
 * Template parameter is the number of parameters.
 */
template<uint8_t NUM_PARAMS>
class PresetManager {
 public:
  struct Preset {
    char name[14];               // 13 chars + null (drumlogue limit)
    int32_t params[NUM_PARAMS];  // Parameter values
  };
  
  PresetManager(const Preset* factory_presets, uint8_t num_presets)
    : factory_presets_(factory_presets),
      num_factory_presets_(num_presets),
      current_preset_idx_(0) {}
  
  // Load preset by index
  bool LoadPreset(uint8_t idx, Preset& dest) {
    if (idx >= num_factory_presets_) {
      return false;  // Invalid index
    }
    
    dest = factory_presets_[idx];
    current_preset_idx_ = idx;
    return true;
  }
  
  // Get preset name
  const char* GetPresetName(uint8_t idx) const {
    return (idx < num_factory_presets_) 
      ? factory_presets_[idx].name 
      : "Invalid";
  }
  
  // Get current preset index
  uint8_t GetCurrentIndex() const {
    return current_preset_idx_;
  }
  
  // Get number of factory presets
  uint8_t GetNumPresets() const {
    return num_factory_presets_;
  }
  
  // Validate preset data (check ranges, etc.)
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
  
 private:
  const Preset* factory_presets_;
  uint8_t num_factory_presets_;
  uint8_t current_preset_idx_;
};

}  // namespace common
```

### Usage Example

```cpp
// In drupiter_synth.h
#include "common/preset_manager.h"

static constexpr common::PresetManager<24>::Preset kFactoryPresets[] = {
  {"Brass Lead", {1, 0, 50, 25, ...}},  // 24 params
  {"Fat Bass",   {0, 0, 50, 0,  ...}},
  // ...
};

class DrupiterSynth {
 private:
  common::PresetManager<24> preset_mgr_{kFactoryPresets, 6};
  common::PresetManager<24>::Preset current_preset_;
};

// In LoadPreset()
void LoadPreset(uint8_t idx) {
  if (preset_mgr_.LoadPreset(idx, current_preset_)) {
    // Apply all parameters
    for (uint8_t i = 0; i < 24; i++) {
      SetParameter(i, current_preset_.params[i]);
    }
  }
}
```

---

## 3. Parameter String Formatter (`param_format.h`)

### Problem
Every unit has duplicated string formatting logic for:
- Percentage values
- Frequency values (Hz/kHz)
- Time values (ms/s)
- Decibel values
- Bipolar ranges (±)

### Proposed Solution

```cpp
// drumlogue/common/param_format.h
#pragma once

#include <cstdio>
#include <cstdint>

namespace common {

/**
 * @brief Standard parameter value formatting utilities
 */
class ParamFormat {
 public:
  // Format as percentage: "50%"
  static const char* Percent(char* buf, size_t size, int32_t value, 
                             int32_t min = 0, int32_t max = 100) {
    snprintf(buf, size, "%d%%", value);
    return buf;
  }
  
  // Format as bipolar percentage: "-50%" to "+50%"
  static const char* BipolarPercent(char* buf, size_t size, int32_t value,
                                    int32_t min, int32_t max) {
    int32_t center = (min + max) / 2;
    int32_t offset = value - center;
    int32_t percent = (offset * 100) / (max - center);
    snprintf(buf, size, "%+d%%", percent);
    return buf;
  }
  
  // Format as frequency: "440Hz" or "4.4kHz"
  static const char* Frequency(char* buf, size_t size, float freq_hz) {
    if (freq_hz >= 1000.0f) {
      snprintf(buf, size, "%.1fkHz", freq_hz / 1000.0f);
    } else {
      snprintf(buf, size, "%.0fHz", freq_hz);
    }
    return buf;
  }
  
  // Format as time: "50ms" or "1.5s"
  static const char* Time(char* buf, size_t size, float time_ms) {
    if (time_ms >= 1000.0f) {
      snprintf(buf, size, "%.1fs", time_ms / 1000.0f);
    } else {
      snprintf(buf, size, "%.0fms", time_ms);
    }
    return buf;
  }
  
  // Format as decibels: "-12dB"
  static const char* Decibels(char* buf, size_t size, float db) {
    snprintf(buf, size, "%+.1fdB", db);
    return buf;
  }
  
  // Format as semitones/cents: "+7" or "-12¢"
  static const char* Pitch(char* buf, size_t size, float cents) {
    if (cents >= 100.0f || cents <= -100.0f) {
      // Show semitones
      int semitones = static_cast<int>(cents / 100.0f);
      snprintf(buf, size, "%+dst", semitones);
    } else {
      // Show cents
      snprintf(buf, size, "%+.0fc", cents);
    }
    return buf;
  }
  
  // Format octave range: "16'" / "8'" / "4'"
  static const char* OctaveRange(int32_t octave) {
    static const char* ranges[] = {"16'", "8'", "4'"};
    if (octave >= 0 && octave <= 2) {
      return ranges[octave];
    }
    return "??";
  }
};

}  // namespace common
```

### Usage Example

```cpp
// In GetParameterStr()
case PARAM_VCF_CUTOFF: {
  static char buf[16];
  float freq = cutoff_to_frequency(value);
  return common::ParamFormat::Frequency(buf, sizeof(buf), freq);
}

case PARAM_DCO2_TUNE: {
  static char buf[16];
  float cents = (value - 50) * 4.0f;  // ±200 cents
  return common::ParamFormat::Pitch(buf, sizeof(buf), cents);
}

case PARAM_ENV_VCF_AMT: {
  static char buf[16];
  return common::ParamFormat::BipolarPercent(buf, sizeof(buf), value, 0, 100);
}
```

---

## 4. MIDI Helper (`midi_helper.h`)

### Problem
Note-to-frequency conversion duplicated in every synth unit.

### Proposed Solution

```cpp
// drumlogue/common/midi_helper.h
#pragma once

#include <cstdint>
#include <cmath>

namespace common {

/**
 * @brief MIDI utility functions
 */
class MidiHelper {
 public:
  // Convert MIDI note to frequency (A4 = 440Hz)
  static constexpr float NoteToFreq(uint8_t note) {
    // f = 440 * 2^((note - 69) / 12)
    return 440.0f * powf(2.0f, (note - 69) / 12.0f);
  }
  
  // Convert MIDI velocity (0-127) to 0.0-1.0 float
  static constexpr float VelocityToFloat(uint8_t velocity) {
    return velocity / 127.0f;
  }
  
  // Convert pitch bend (0-16383, center=8192) to semitones
  static float PitchBendToSemitones(uint16_t bend, float range_semitones = 2.0f) {
    const float centered = (bend - 8192.0f) / 8192.0f;  // -1.0 to +1.0
    return centered * range_semitones;
  }
  
  // Get note name from MIDI note number
  static const char* NoteName(uint8_t note) {
    static const char* names[] = {
      "C", "C#", "D", "D#", "E", "F", 
      "F#", "G", "G#", "A", "A#", "B"
    };
    return names[note % 12];
  }
  
  // Get octave from MIDI note number (C4 = note 60)
  static int8_t NoteOctave(uint8_t note) {
    return (note / 12) - 1;
  }
};

}  // namespace common
```

---

## 5. Value Smoother (`smoother.h`)

### Problem
Parameter smoothing for anti-zipper duplicated across units.

### Proposed Solution

Already exists in `drupiter-synth/dsp/smoothed_value.h` but should be moved to common:

```bash
mv drumlogue/drupiter-synth/dsp/smoothed_value.h drumlogue/common/
```

Then all units can use:
```cpp
#include "common/smoothed_value.h"
```

---

## 6. Proposed Common Structure

```
drumlogue/common/
├── arm_intrinsics.h      (existing - ARM NEON helpers)
├── dsp_utils.h           (existing - DSP math)
├── fixed_mathq.h         (existing - fixed-point math)
├── neon_dsp.h            (existing - NEON DSP)
├── ppg_osc.h             (existing - PPG wavetable)
├── simd_utils.h          (existing - SIMD utilities)
├── stereo_widener.h      (existing - stereo effects)
├── wavetable_osc.h       (existing - wavetable synth)
│
├── hub_control.h         (NEW - hub parameter system)
├── preset_manager.h      (NEW - preset management)
├── param_format.h        (NEW - parameter string formatting)
├── midi_helper.h         (NEW - MIDI utilities)
└── smoothed_value.h      (MOVED from drupiter-synth/dsp/)
```

---

## Implementation Plan

### Phase 1: Core Utilities (Highest Value)
1. **hub_control.h** - Immediate benefit for drupiter and pepege
2. **param_format.h** - Clean up string formatting across all units
3. **smoothed_value.h** - Move from drupiter to common

### Phase 2: Structure (Medium Priority)
4. **preset_manager.h** - Standardize preset handling
5. **midi_helper.h** - Consolidate MIDI math

### Phase 3: Refactor Existing Units (Low Priority)
6. Update drupiter-synth to use common utilities
7. Update pepege-synth to use common utilities
8. Update elementish-synth to use common utilities
9. Update clouds-revfx to use common utilities

---

## Benefits Summary

**Code Reuse**: 
- Eliminate ~200 lines of duplicated code per unit
- Reduce bugs from copy-paste errors

**Consistency**:
- All units format strings the same way
- All units handle presets the same way
- All hub controls work identically

**Maintainability**:
- Fix once, benefit everywhere
- Easier to onboard new contributors
- Standard patterns reduce cognitive load

**Testing**:
- Common utilities can have unit tests
- Desktop test harnesses validate shared code

---

**Recommendation**: Start with `hub_control.h` and `param_format.h` as these provide immediate value for the Drupiter UI redesign and are broadly applicable to all units.
