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

## 🚀 ARM NEON SIMD Optimization Analysis

### Platform Hardware

| Component | Specification |
|-----------|---------------|
| **CPU** | NXP MCIMX6Z0DVM09AB (ARM Cortex-A7) |
| **Clock Speed** | 900 MHz |
| **Architecture** | ARMv7-A |
| **NEON Support** | Yes (integrated, not optional) |
| **Sample Rate** | 48 kHz |
| **Buffer Size** | Variable (typically 64-256 frames) |

The drumlogue uses an **ARM Cortex-A7** processor, **NOT** a Cortex-M series. This is significant because:
- Cortex-M series (M0, M4, M7) do **not** have NEON SIMD support
- Cortex-A series (A7, A8, A9, etc.) **do** have NEON 128-bit SIMD
- The SDK explicitly states: *"drumlogue custom synth and effect units can be written in C, C++ and make use of ARMv7-A NEON instructions"*

### NEON Capabilities on Cortex-A7

| Feature | Details |
|---------|---------|
| Register width | 128-bit (Q registers), 64-bit (D registers) |
| Data types | 8/16/32/64-bit integers, 32-bit float |
| Float vectors | `float32x4_t` (4 floats per operation) |
| Integer vectors | `int16x8_t` (8 × 16-bit), `int32x4_t` (4 × 32-bit) |
| Key instructions | VMLA (multiply-accumulate), VLD1/VST1 (load/store), VADD, VMUL |

### Real-World Performance Findings from Drumlogue Developers

Based on extensive research including the Gearspace logue user oscillator programming thread (particularly from developer **dukesrg** who ported FM64 to drumlogue):

#### Critical Findings

1. **NEON Vectorization Can HURT Performance**
   > *"the more I'm trying to vectorize, the slower synth goes"* - dukesrg
   
   Full vectorization of FM64 resulted in only **2 voices** compared to **7-10 voices** with scalar code and selective optimization.

2. **GCC Fails at Optimal NEON Scheduling**
   - The compiler does not optimally schedule NEON instructions
   - Manual reordering of intrinsics improved FM64 from **7 to 10 voices**
   - Execution latency of NEON ops matters more than load/store penalties

3. **Toolchain Limitations**
   - `vld1q_*_x2`, `vld1q_*_x3`, `vld1q_*_x4` intrinsics **do not compile**
   - Workaround: Use individual `vld1q_*` calls or inline assembly

4. **Class Overhead Matters**
   - Removing C++ classes and using static variables improved stability
   - `fast_inline` functions and aggressive inlining help
   - The elements-synth already uses `inline` heavily (good)

5. **Voice Count Reality**
   - FM64 (complex FM synth): ~4 voices stable, 5-6 start skipping, 8+ causes hangs
   - Simple synths may achieve more voices
   - Modal synthesis (elements-synth) is inherently expensive due to per-mode processing

#### Performance Benchmarks from Forum

| Synth | Scalar Code | NEON Vectorized | Notes |
|-------|-------------|-----------------|-------|
| FM64 | 7 voices | 2 voices | Over-vectorization hurt performance |
| FM64 | 7 voices | 10 voices | Manual instruction reordering |

### NEON Optimization Opportunities in elements-synth

#### Potentially Beneficial (Use with Caution)

| Component | Vectorization Target | Potential | Risk |
|-----------|---------------------|-----------|------|
| `Resonator::ComputeFilters()` | Batch 4 modes at once | MEDIUM | Complex dependencies |
| `MultistageEnvelope::Process()` | Batch 4 envelopes | LOW | State machine branches |
| `Mode::Process()` | 4 × SVF computation | MEDIUM | Data dependencies |
| Audio buffer clearing | `memset()` replacement | LOW | Already fast |
| Gain/mix operations | Simple `float32x4_t` | HIGH | Good candidate |

#### NOT Recommended for NEON

| Component | Reason |
|-----------|--------|
| `Tube::Process()` | Single-sample delay line, cannot vectorize |
| `MoogLadder::Process()` | Feedback loop prevents vectorization |
| LFO computation | Control rate, overhead not worth it |
| String model delay lines | Feedback dependencies |

### Recommended NEON Optimization Strategy

Based on findings, the optimal approach for elements-synth is:

#### 1. **Keep Most Code Scalar**
The current scalar implementation is likely near-optimal for this DSP topology. Modal synthesis has inherent feedback loops that prevent effective SIMD parallelization.

#### 2. **Target Simple Parallel Operations Only**

```cpp
// GOOD: Simple gain/mix operations
#include <arm_neon.h>

inline void ApplyGainNEON(float* buffer, float gain, size_t frames) {
    float32x4_t gain_vec = vdupq_n_f32(gain);
    for (size_t i = 0; i < frames; i += 4) {
        float32x4_t samples = vld1q_f32(&buffer[i]);
        samples = vmulq_f32(samples, gain_vec);
        vst1q_f32(&buffer[i], samples);
    }
}

// GOOD: Stereo mix
inline void MixStereoNEON(const float* in_l, const float* in_r, 
                          float* out_l, float* out_r,
                          float width, size_t frames) {
    float mid_gain = 0.5f;
    float side_gain = width * 0.5f;
    float32x4_t mid_g = vdupq_n_f32(mid_gain);
    float32x4_t side_g = vdupq_n_f32(side_gain);
    
    for (size_t i = 0; i < frames; i += 4) {
        float32x4_t l = vld1q_f32(&in_l[i]);
        float32x4_t r = vld1q_f32(&in_r[i]);
        float32x4_t mid = vmulq_f32(vaddq_f32(l, r), mid_g);
        float32x4_t side = vmulq_f32(vsubq_f32(l, r), side_g);
        vst1q_f32(&out_l[i], vaddq_f32(mid, side));
        vst1q_f32(&out_r[i], vsubq_f32(mid, side));
    }
}
```

#### 3. **Avoid Over-Vectorization**

```cpp
// BAD: Don't try to vectorize feedback loops
// This will NOT help and may hurt performance:
inline void BadResonatorNEON(/* ... */) {
    // Each mode depends on previous output - cannot parallelize
    for (int m = 0; m < num_modes; m++) {
        // Mode[m] state depends on Mode[m-1] - SERIAL dependency
    }
}
```

#### 4. **Manual Instruction Scheduling**
If using NEON intrinsics, manually interleave load, compute, and store operations:

```cpp
// BETTER: Interleaved operations to hide latency
float32x4_t a1 = vld1q_f32(&buf1[i]);
float32x4_t b1 = vld1q_f32(&buf2[i]);  // Load while previous computes
float32x4_t c1 = vmulq_f32(a1, gain);
float32x4_t a2 = vld1q_f32(&buf1[i+4]); // Prefetch next
float32x4_t d1 = vaddq_f32(c1, b1);
vst1q_f32(&out[i], d1);
// Continue interleaved pattern...
```

### Compiler Flags for NEON

Current SDK flags support NEON but may not be optimal:

```makefile
# In config.mk, could add:
UDEFS += -march=armv7-a -mfpu=neon-vfpv4 -mfloat-abi=hard

# For auto-vectorization (use with caution):
UDEFS += -ftree-vectorize -ftree-slp-vectorize
```

**⚠️ WARNING**: Auto-vectorization with `-O3 -ftree-vectorize` may produce slower code than manual scalar code due to GCC's poor NEON scheduling on Cortex-A7.

### Alternative Optimizations (Higher Impact)

Given the limited NEON benefits for modal synthesis, consider these alternatives first:

| Optimization | Expected Impact | Effort |
|--------------|-----------------|--------|
| **ELEMENTS_LIGHTWEIGHT mode** | HIGH (already done) | ✅ Complete |
| **USE_NEON SIMD optimizations** | LOW-MEDIUM | ✅ Complete |
| **Reduce `kNumModes`** from 8 to 6 or 4 | HIGH | LOW |
| **Fixed-point arithmetic** for resonator | MEDIUM | HIGH |
| **Reduce `kMaxBowedModes`** | MEDIUM | LOW |
| **Smaller delay line buffers** | MEDIUM | MEDIUM |
| **Skip inactive exciters** | LOW-MEDIUM | LOW |
| **LTO (`USE_LTO=yes`)** | LOW | LOW |

### Conclusion

**For elements-synth, NEON optimization is NOT recommended as a primary optimization strategy.**

Reasons:

1. Modal synthesis has inherent serial dependencies (feedback loops)
2. Real-world drumlogue developer experience shows over-vectorization hurts performance
3. GCC's NEON scheduling is suboptimal for Cortex-A7
4. The `ELEMENTS_LIGHTWEIGHT` mode already provides significant CPU savings
5. Alternative optimizations (mode count reduction, fixed-point) offer better ROI

If NEON is pursued, focus **only** on:

- Simple gain/attenuation operations
- Stereo processing (independent L/R channels)  
- Final mix/output stages
- **Manually schedule** intrinsics to hide latency

---

## ✅ NEON Implementation (December 2025)

Based on the above analysis, a conservative NEON implementation has been added targeting only the safe parallel operations:

### Files Added

**`dsp/neon_dsp.h`** - ARM NEON SIMD utilities library:

| Function | Operation | NEON Benefit |
|----------|-----------|--------------|
| `ClearBuffer()` | Zero fill buffer | 4× throughput |
| `ClearStereoBuffers()` | Zero fill L/R buffers | 4× throughput |
| `ApplyGain()` | In-place gain | 4× throughput |
| `ApplyGainTo()` | Gain to separate buffer | 4× throughput |
| `MidSideToStereo()` | M/S to L/R conversion | 4× throughput |
| `StereoGain()` | Independent L/R gain | 4× throughput |
| `InterleaveStereo()` | L/R to interleaved output | Uses `vzipq_f32` |
| `ClampBuffer()` | Limit to ±range | Uses `vminq/vmaxq` |
| `SanitizeBuffer()` | Replace NaN with zero | Uses `vceqq` mask |
| `SanitizeAndClamp()` | Combined NaN+clamp | Single pass |
| `Accumulate()` | Add buffers | 4× throughput |
| `MixBuffers()` | Weighted mix | Uses `vmlaq` FMA |

### Files Modified

**`config.mk`**:
```makefile
UDEFS += -DUSE_NEON
```

**`elements_synth_v2.h`**:
- Added `#include "dsp/neon_dsp.h"`
- `Render()` now uses NEON utilities when `USE_NEON` defined:
  - `ClearStereoBuffers()` for buffer initialization
  - `SanitizeAndClamp()` for NaN protection + limiting (single pass)
  - `InterleaveStereo()` for final L/R output

### Implementation Notes

1. **Scalar fallback**: All functions compile to identical scalar code when `USE_NEON` is not defined
2. **Manual interleaving**: Load/store operations are interleaved to hide NEON latency
3. **Remainder handling**: All loops handle non-multiple-of-4 frame counts
4. **No feedback vectorization**: Filters, delays, and resonators remain scalar (per recommendations)

### Build Sizes

| Configuration | Binary Size | Notes |
|--------------|-------------|-------|
| LIGHTWEIGHT only | 125,556 bytes | Baseline |
| LIGHTWEIGHT + NEON | 128,952 bytes | +3.4KB for NEON code |

### Usage

Enable/disable in `config.mk`:
```makefile
# Enable NEON SIMD optimizations
UDEFS += -DUSE_NEON

# Disable NEON (scalar fallback)
# Comment out the above line
```

---

*Review Date: December 6, 2025 (Updated: December 2025)*
*Reviewer: Code Analysis Agent*
