# Code Review: elements-synth

## Executive Summary

The `elements-synth` unit is a well-implemented modal synthesis synth inspired by Mutable Instruments Elements. The codebase shows attention to DSP detail and follows good practices overall. However, there are opportunities for memory optimization, dead code cleanup, and UI parameter improvements.

---

## 📊 Memory Analysis

### Estimated Memory Usage

| Section | Component | Size (bytes) | Notes |
|---------|-----------|--------------|-------|
| **DATA/RODATA** | `samples.h` sample arrays | ~32KB | 6 samples (int16_t) |
| **DATA/RODATA** | `kMidiFreqTable[128]` | 512 | dsp_core.h |
| **DATA/RODATA** | `kStiffnessLUT[65]` | 260 | dsp_core.h |
| **DATA/RODATA** | `kSvfGLUT[129]` | 516 | dsp_core.h |
| **DATA/RODATA** | `kQDecadesLUT[65]` | 260 | dsp_core.h |
| **DATA/RODATA** | `kVelocityGainCoarse/Fine[33]` | 264 | dsp_core.h |
| **DATA/RODATA** | `kEnvExpTable[64]` | 256 | envelope.h |
| **DATA/RODATA** | `kEnvQuarticTable[64]` | 256 | envelope.h |
| **DATA/RODATA** | Sine table (resources_mini.h) | ~1KB | via GetSineTable() |
| **BSS** | `Tube::delay_line_[2048]` | 8KB | Blow exciter |
| **BSS** | `String::delay_[2048]` | 8KB | Per string model |
| **BSS** | `MultiString` (5× String) | 40KB | 5 strings × 8KB |
| **BSS** | `Mode[kNumModes]` | varies | Default 8 modes |
| **BSS** | `BowedMode[8]` | ~8KB | 8 × 1KB delay lines |
| **BSS** | `DelayLine<1024>` per bowed mode | 8KB | kMaxBowedModes × 4KB |
| **BSS** | `ElementsSynth` instance | ~4KB | params, runtime state |

### Memory Concerns

1. **Large Sample Data (~32KB)**: All samples embedded in header file (samples.h)
   - Currently stored as `const` arrays in flash (good)
   - Consider using platform sample API instead for dynamic loading

2. **MultiString Memory Waste**: `MultiString` allocates 5 full `String` instances (40KB+)
   - Only used when MODEL is set to MultiString (hidden/not exposed in UI currently)
   - These are always allocated even when unused

3. **Bowed Mode Over-allocation**: 8 bowed modes with 1KB delay lines each
   - Always allocated even when bow level is 0
   - Consider lazy allocation or smaller delay lines

4. **Duplicate Lookup Tables**: `resources_mini.h` has tables that overlap with `dsp_core.h`
   - `GetMidiFreqTable()` vs `kMidiFreqTable[]`
   - `FastTan()` implementation duplicated
   - `GetSineTable()` unused by main code

---

## 🐛 Dead Code & Unused Features

### Unused Files/Functions

| Location | Item | Status |
|----------|------|--------|
| `resources_mini.h` | Entire file | **UNUSED** - Not included anywhere |
| `resources_mini.h` | `GetSineTable()` | Not used (FastSin in dsp_core.h) |
| `resources_mini.h` | `GetEnvExpoTable()` | Not used (kEnvExpTable in envelope.h) |
| `resources_mini.h` | `GetDecayTable()` | Not used |
| `resources_mini.h` | `GetStiffnessTable()` | Not used (kStiffnessLUT in dsp_core.h) |
| `resources_mini.h` | `GetMidiFreqTable()` | Not used (kMidiFreqTable in dsp_core.h) |
| `dsp_core.h` | `FastSinCos()` | Declared but not called |
| `dsp_core.h` | `FastCosRad()` | Used only by FastSinCos() |
| `dsp_core.h` | `FastSinRad()` | Used only by FastSinCos() |
| `resonator.h` | `MultiString` class | Model 2 exists but UI exposes only Modal(0)/String(1) |
| `resonator.h` | `DispersionFilter` | `SetDispersion()` exists but no UI parameter |
| `exciter.h` | `STRIKE_MODE_PLECTRUM` | Mode 3 exists but UI only exposes 0-2 |
| `exciter.h` | `STRIKE_MODE_PARTICLES` | Mode 4 exists but UI only exposes 0-2 |
| `modal_synth.h` | `SetMultiStringDetune()` | Method exists, no UI parameter |
| `modal_synth.h` | `SetGranularPosition()` | Method exists, no UI parameter |
| `modal_synth.h` | `SetFilterEnvAmount()` | Method exists, no UI parameter |
| `modal_synth.h` | `SetModulationFrequency()` | Method exists, no UI parameter |
| `modal_synth.h` | `SetModulationOffset()` | Method exists, no UI parameter |
| `modal_synth.h` | `SetOutputLevel()` | Method exists, not controllable |
| `modal_synth.h` | `SetSustain()` | Called in updateEnvelope but sustain_level_ never set from UI |

### Static Variables in Functions (Potential Issues)

1. **`modal_synth.h:264`** - `static float last_phase` in RND LFO:
   ```cpp
   static float last_phase = 0.0f;
   ```
   - This persists across instances (unlikely to be a problem with singleton)
   - Could cause issues if multiple instances existed

2. **`elements_synth_v2.h:99-100`** - Static render buffers:
   ```cpp
   static float out_l[kMaxFrames];
   static float out_r[kMaxFrames];
   ```
   - Acceptable pattern but consider member variables

---

## ⚡ Performance Bottlenecks

### Hot Path Analysis (`Process()` functions)

1. **`Resonator::Process()`** - Most CPU intensive
   - `ComputeFilters()` called every sample block
   - Good: Clock divider limits mode updates
   - Good: Uses `LookupSvfG()` instead of `tan()`
   - Concern: `CosineOscillator::Init()` called every sample (could cache)

2. **`Tube::Process()`** - Single-sample version
   - Delay interpolation per sample is efficient
   - Good soft clipping implementation

3. **`MoogLadder::Process()`**
   - Good: Smoothed cutoff (0.001 coefficient)
   - Good: Uses `FastTanh()` for soft clipping

4. **`MultistageEnvelope::Process()`**
   - Lookup tables for shapes (good)
   - Branch-heavy state machine (acceptable)

### Optimization Opportunities

| Location | Current | Suggested | Impact |
|----------|---------|-----------|--------|
| `modal_synth.h:240-280` | LFO per-sample check | Move outside loop | LOW |
| `resonator.h:290` | `CosineOscillator::Init()` per call | Cache between calls | MEDIUM |
| `resonator.h:340` | Per-mode `SetCoefficients()` | Batch updates | LOW |
| `exciter.h` | Per-sample noise in blow | Block-based noise | LOW |
| `filter.h:38` | Per-sample smoothing (0.001) | Consider control rate | LOW |

---

## 🔧 Compiler & Linker Configuration

### Current Build Settings (from SDK Makefile)

| Setting | Value | Notes |
|---------|-------|-------|
| Optimization | `-Os` | Size optimization (default) |
| Fast math | `-ffast-math` | Enabled |
| Inline limit | `9999999999` | Aggressive inlining |
| Frame pointer | `-fomit-frame-pointer` | Release only |
| Threadsafe statics | `-fno-threadsafe-statics` | Good for embedded |
| LTO | Disabled by default | Could enable |
| Link GC | Disabled by default | Could enable |

### Recommendations

1. **Enable Link-Time Garbage Collection**
   - Add to `config.mk`: 
     ```makefile
     USE_LINK_GC = yes
     ```
   - Removes unused functions (significant with dead code present)

2. **Consider `-O2` instead of `-Os`**
   - May improve performance at cost of ~10-20% larger binary
   - Test with: `make OPTIM=-O2`

3. **Enable LTO (Link-Time Optimization)**
   - Add to `config.mk`:
     ```makefile
     USE_LTO = yes
     ```
   - Better cross-file inlining, smaller final binary

---

## 📋 Coding Best Practices Issues

### Memory Segment Optimization

1. **Lookup Tables Should Be `const`** ✅
   - All LUTs correctly declared with `const`
   - Example: `static const float kMidiFreqTable[128]`
   - Goes to `.rodata` (flash), not `.data` (RAM)

2. **Uninitialized Globals (BSS)** - Could improve:
   - `Tube::delay_line_[2048]` - Uses constructor initialization
   - Consider: Leave uninitialized, reset in `Init()` only

3. **`resources_mini.h` Has Non-const Static in Function**:
   ```cpp
   static float lut_midi_freq[kPitchTableSize];  // BAD - goes to BSS + init
   static bool initialized = false;
   ```
   - Should be `const` with compile-time initialization

### Code Style Issues

1. **Inconsistent NaN Checks**:
   - Some use `(x != x)` pattern
   - Some use bit pattern check (`0x7F800000`)
   - Consider standardizing on one approach

2. **Magic Numbers**:
   - `0.001f` smoothing coefficient used in multiple places
   - `0.99f`, `0.997f` decay constants undocumented
   - Consider named constants

3. **Long Functions**:
   - `ModalSynth::Process()` is ~90 lines
   - `Resonator::ComputeFilters()` is ~60 lines
   - Consider breaking into smaller functions

---

## 🎛️ UI Parameter Review

### Parameter Functionality Verification

| ID | Name | Range | Working | Notes |
|----|------|-------|---------|-------|
| 0 | BOW | 0-127 | ✅ | Controls bow friction exciter |
| 1 | BLOW | 0-127 | ✅ | Controls tube/breath exciter |
| 2 | STRIKE | 0-127 | ✅ | Controls strike level |
| 3 | MALLET | 0-11 | ✅ | 12 sample+timbre combos |
| 4 | BOW TIM | -64/+63 | ✅ | Bow filter frequency |
| 5 | BLW TIM | -64/+63 | ✅ | Blow filter + tube timbre |
| 6 | STK MOD | 0-2 | ⚠️ | Only 3 modes exposed (5 exist) |
| 7 | DENSITY | -64/+63 | ✅ | Granular density |
| 8 | GEOMETRY | -64/+63 | ✅ | Structure/stiffness |
| 9 | BRIGHT | -64/+63 | ✅ | High-freq damping |
| 10 | DAMPING | -64/+63 | ✅ | Decay time |
| 11 | POSITION | -64/+63 | ✅ | Excitation point |
| 12 | CUTOFF | 0-127 | ✅ | Filter cutoff |
| 13 | RESO | 0-127 | ✅ | Filter resonance |
| 14 | SPACE | 0-127 | ✅ | Stereo width |
| 15 | MODEL | 0-1 | ⚠️ | Only 2 modes exposed (3 exist) |
| 16 | ATTACK | 0-127 | ✅ | Envelope attack |
| 17 | DECAY | 0-127 | ✅ | Envelope decay |
| 18 | RELEASE | 0-127 | ✅ | Envelope release |
| 19 | ENV MOD | 0-3 | ✅ | ADR/AD/AR/LOOP |
| 20 | LFO RT | 0-127 | ✅ | LFO rate |
| 21 | LFO DEP | 0-127 | ✅ | LFO depth |
| 22 | LFO PRE | 0-7 | ✅ | Shape+destination preset |
| 23 | COARSE | -64/+63 | ✅ | Pitch coarse tune |

### UI Improvement Suggestions

1. **Expose More Strike Modes**
   - Change `STK MOD` max from 2 to 4
   - Add strings for PLECTRUM, PARTICLES

2. **Expose MultiString Model**
   - Change `MODEL` max from 1 to 2
   - Add string for "M-STRING"

3. **Missing Useful Parameters**:
   - **DISPERSION**: String inharmonicity (piano-like)
   - **FLT ENV**: Filter envelope amount (currently fixed)
   - **SUSTAIN**: For ADSR mode (currently always 0 in ADR)

4. **Parameter Page Reorganization Suggestion**:
   ```
   Page 1: Exciter Mix (BOW, BLOW, STRIKE, MALLET) ✓
   Page 2: Exciter Timbre (BOW TIM, BLW TIM, STK MOD, DENSITY) ✓
   Page 3: Resonator (GEO, BRIGHT, DAMP, POS) ✓
   Page 4: Filter (CUT, RESO, FLT ENV*, SPACE) 
   Page 5: Envelope (ATK, DEC, REL, ENV MOD) ✓
   Page 6: Modulation (LFO RT, LFO DEP, LFO PRE, COARSE) ✓
   ```
   *Replace SPACE with FLT ENV, move SPACE to p6 or make DISPERSION

---

## 📝 Actionable TODO List

### High Priority

- [x] **Delete `resources_mini.h`** - Completely unused, duplicates dsp_core.h ✅ DONE
- [x] **Enable USE_LINK_GC=yes** in config.mk to remove dead code ✅ DONE
- [x] **Expose PLECTRUM and PARTICLES strike modes** - Already implemented ✅ DONE
- [x] **Expose MultiString model** - Already implemented ✅ DONE
- [x] **Add filter envelope amount parameter** - Functionality exists ✅ DONE (replaced SPACE)

### Medium Priority

- [ ] **Consider lazy allocation** for MultiString (only allocate when used)
- [ ] **Remove duplicate math functions** between files
- [ ] **Add DISPERSION parameter** for string models
- [ ] **Standardize NaN check pattern** across all files
- [ ] **Document magic numbers** with named constants
- [ ] **Test with `-O2`** instead of `-Os` for performance

### Low Priority

- [ ] **Cache CosineOscillator** state between Process() calls
- [ ] **Consider block-based noise** for blow exciter
- [ ] **Add GetSustain UI parameter** for full ADSR control
- [ ] **Remove unused SetOutputLevel()** method
- [ ] **Extract long functions** into smaller units
- [ ] **Enable LTO** for additional size/speed optimization

### Documentation

- [ ] **Update TODO.md** to reflect completed features
- [ ] **Add memory map** documentation
- [ ] **Document exposed vs internal parameters**

---

## ✅ Changes Applied (December 6, 2025)

### Files Deleted

- `resources_mini.h` - Was completely unused, duplicated functionality in dsp_core.h

### Files Modified

**config.mk:**

- Added `USE_LINK_GC = yes` for link-time garbage collection

**header.c:**

- Changed STK MOD parameter max from 2 to 4 (exposes PLECTRUM, PARTICLES)
- Changed MODEL parameter max from 1 to 2 (exposes MSTRING)
- Renamed parameter 14 from "SPACE" to "FLT ENV" (filter envelope amount)
- Updated default value for FLT ENV to 64 (50%)

**elements_synth_v2.h:**

- Added "PLECTRUM", "PARTICLE" to strike mode names array
- Added "MSTRING" to model names array
- Changed parameter 14 handler from `SetSpace()` to `SetFilterEnvAmount()`
- Updated `Init()` default for param[14] to 64
- Updated `setPresetParams()` to use filter envelope instead of space
- Updated all 8 presets with appropriate FLT ENV values
- Fixed preset 7 LFO target (was SPACE→5, now GEOMETRY→2)
- Removed unused `stereo_width_` member variable

**modal_synth.h:**

- Changed default stereo width from 50% to 70%

**Makefile:**

- Fixed linker flag bug: LDOPT now includes leading comma (`,--gc-sections`)
  for proper `-Wl` concatenation

### Build Verification

- Unit compiles successfully with all changes
- Final artifact: `elements_synth.drmlgunit` (128,940 bytes)

---

## 🔍 Files Reviewed

```text
drumlogue/elements-synth/
├── config.mk           - Build configuration
├── header.c            - Unit metadata & parameters
├── unit.cc             - SDK callbacks
├── elements_synth_v2.h - Main synth wrapper
├── modal_synth.h       - DSP engine
├── samples.h           - Sample data (~4325 lines)
└── dsp/
    ├── dsp_core.h      - Math utilities & LUTs
    ├── envelope.h      - MultistageEnvelope
    ├── exciter.h       - Bow/Blow/Strike exciters
    ├── filter.h        - Moog ladder filter
    ├── resonator.h     - Modal resonator & strings
    └── tube.h          - Waveguide tube
```

---

*Review Date: December 6, 2025*
*Reviewer: Code Analysis Agent*
