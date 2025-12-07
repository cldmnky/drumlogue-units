# Elementish Synth - Optimization TODO

## Current State (v1.0.0-pre1)

- **Modal Modes**: 8 (set via `NUM_MODES` in `config.mk`)
- **CPU Usage**: ~0.5% per mode at modal, working but occasional cracks at 8 modes
- **NEON**: Enabled for buffer operations only, not for modal processing

---

## Priority 1: Scalar Optimizations First (Low Risk)

Based on FM64 developer experience, **start with scalar optimizations before NEON**.
Over-vectorization on Cortex-A7 can actually slow things down!

### A. Coefficient Caching (Highest Priority)

Only recalculate filter coefficients when parameters actually change:

```cpp
// Track parameter changes
bool params_dirty_ = true;
float cached_geometry_ = 0.0f;
float cached_brightness_ = 0.0f;

void SetGeometry(float g) {
    if (geometry_ != g) {
        geometry_ = g;
        params_dirty_ = true;
    }
}

// In ComputeFilters():
if (!params_dirty_ && clock_divider_ > 4) {
    return cached_num_modes_;  // Skip recalculation entirely
}
```

### B. Clock Divider Tuning

Current implementation already has clock divider, but we can be more aggressive:

```cpp
// Current: Update first 24 modes every sample
bool update = i <= 24 || ((i & 1) == (clock_divider_ & 1));

// Optimized: More aggressive for high mode counts
bool update;
if (i <= 4) update = true;                           // Critical modes: every sample
else if (i <= 8) update = (clock_divider_ & 1) == 0; // Every 2 samples
else if (i <= 16) update = (clock_divider_ & 3) == 0; // Every 4 samples
else update = (clock_divider_ & 7) == 0;              // Every 8 samples
```

### C. Lookup Table for SVF G Coefficient

Replace polynomial approximation with lookup table:

```cpp
// Current: Polynomial in LookupSvfG()
// Optimized: 512-entry LUT with linear interpolation (2KB)
static const float svf_g_lut[512];  // Pre-computed tan(pi*f/sr)

inline float LookupSvfGFast(float frequency) {
    float idx = frequency * (511.0f / 24000.0f);  // 0-24kHz range
    int i = static_cast<int>(idx);
    float frac = idx - i;
    return svf_g_lut[i] + frac * (svf_g_lut[i+1] - svf_g_lut[i]);
}
```

---

## Priority 2: NEON Optimization (Careful Approach)

⚠️ **WARNING**: FM64 developer found that over-vectorization HURT performance on Cortex-A7!

### Background

The modal resonator is the CPU bottleneck. Each mode is an SVF (State Variable Filter) that processes samples sequentially due to feedback. However, **multiple modes can be processed in parallel** since they are independent until final summation.

### Current Implementation (`dsp/resonator.h`)

```cpp
// Current: Sequential mode processing
for (size_t i = 0; i < num_modes; ++i) {
    float s = modes_[i].Process(excitation);  // Feedback-dependent
    float amp = amplitudes.Next();
    float aux_amp = aux_amplitudes.Next();
    sum_center += s * amp;
    sum_side += s * aux_amp;
}
```

### NEON Strategy: Start Small, Measure Often

**Key learnings from FM64 development:**
1. GCC does NOT optimize NEON intrinsic ordering properly
2. Manual instruction reordering gave 7→10 voice improvement
3. Execution latency matters more than load/store on Cortex-A7

#### Implementation Plan (Incremental)

**Step 1**: Try NEON for amplitude calculation only (low risk):
```cpp
// Compute 4 amplitudes at once - simpler than full SVF
float32x4_t ComputeAmplitudesQuad(float position, int start_mode);
```

**Step 2**: If Step 1 helps, try NEON for horizontal sum:
```cpp
inline float HorizontalSum(float32x4_t v) {
    float32x2_t sum = vadd_f32(vget_low_f32(v), vget_high_f32(v));
    return vget_lane_f32(vpadd_f32(sum, sum), 0);
}
```

**Step 3**: Only if needed, try full SVF vectorization with SoA layout:
```cpp
// Structure of Arrays layout
float32x4_t state_1_[kNumModes/4];
float32x4_t state_2_[kNumModes/4];
// ... carefully ordered for Cortex-A7 pipeline
```

### Expected Speedup (Revised Estimates)

Based on FM64 experience:
- **Theoretical**: 4x for mode processing loop
- **Realistic on Cortex-A7**: 1.3-2x (due to pipeline/latency issues)
- **Risk**: Could be slower if done wrong!

### Files to Modify

- `dsp/neon_dsp.h`: Add NEON helpers incrementally
- `dsp/resonator.h`: Add parallel processing path with `#ifdef USE_NEON`
- **Test after each change** - use bar count until audio corruption as metric

---

## Priority 3: Increase Mode Count

### Current Constraint

```cpp
// dsp/dsp_core.h
constexpr int kNumModes = NUM_MODES;  // Default: 8, max: 32
```

### Safe Mode Targets (Revised)

| Modes | CPU Est. | Use Case | Risk |
|-------|----------|----------|------|
| 8     | ~0.5%    | Current default (occasional cracks) | Low |
| 10    | ~0.6%    | First test after scalar optimizations | Low |
| 12    | ~0.75%   | Good balance | Medium |
| 16    | ~1.0%    | Rich harmonics, needs optimizations | Higher |

### Implementation

1. After scalar optimizations, test with 10 modes:

   ```makefile
   UDEFS = -DNUM_MODES=10 -DELEMENTS_LIGHTWEIGHT -DUSE_NEON
   ```

2. If stable, increase to 12:

   ```makefile
   UDEFS = -DNUM_MODES=12 -DELEMENTS_LIGHTWEIGHT -DUSE_NEON
   ```

---

## Priority 4: Additional Optimizations

### A. Avoid Large C++ Classes

From FM64 experience: Large Synth classes with many members can cause hangs on unit load.

**Workaround**: Use static variables instead of class members:

```cpp
// Instead of:
class Resonator {
    Mode modes_[32];  // Can cause issues
    // ...
};

// Use:
static Mode s_modes[32];  // Static allocation
class Resonator {
    // Reference static data
};
```

### B. Reduced-Precision Modes

For higher partials (modes 8+), use simplified SVF:
- Single-precision approximations
- Reduced stability checks (higher partials decay fast anyway)
- Potential 50% speedup for upper modes

### B. Modal Decimation

Process upper modes at lower sample rate:
- Modes 0-8: 48kHz (full rate)
- Modes 9-16: 24kHz (half rate, interpolate output)
- Modes 17+: 12kHz (quarter rate)

Requires careful anti-aliasing but significant CPU savings.

### C. Dynamic Mode Count

Adjust mode count based on fundamental frequency:
- Low notes (< 200Hz): All modes audible, use 16+
- Mid notes (200-1000Hz): 8-12 modes sufficient
- High notes (> 1000Hz): 4-8 modes (upper partials > Nyquist anyway)

```cpp
size_t GetDynamicModeCount(float frequency) {
    if (frequency < 200.0f) return 16;
    if (frequency < 500.0f) return 12;
    if (frequency < 1000.0f) return 8;
    return 6;
}
```

---

## Testing Strategy

### 0. Practical Hardware Testing (From FM64 Developer)

The FM64 developer uses a simple but effective metric:

> "I just load FM64 synth, put it on 6 bar cycle, select 16 voice polyphony and see how many bars it works until audio starts crackling."

**For Elementish Synth:**

1. Load unit on drumlogue
2. Set up 4-8 bar loop on sequencer
3. Hold chord or trigger multiple notes
4. Count bars until audio crackles/glitches
5. Compare bar count between optimizations

**If bar count decreases after an optimization, revert it!**

| Configuration | Bar Count | Notes |
|--------------|-----------|-------|
| 8 modes (baseline) | ? | Current implementation |
| 8 modes + scalar opt | ? | After coefficient caching |
| 10 modes + scalar opt | ? | First mode increase |
| 12 modes + scalar opt | ? | Target configuration |

### 1. Desktop Testing (Test Harness)

```bash
cd test/elementish-synth
mkdir build && cd build
cmake -DDSP_PROFILE=ON ..
make && ./elementish_test
```

Measure cycles per sample with different mode counts.

### 2. Hardware Testing

1. Build with profiling markers (toggle GPIO)
2. Measure with oscilloscope
3. Test with complex sequences (rapid notes, high velocity)

### 3. Audio Quality Testing

- A/B compare 8 modes vs 16 modes
- Listen for aliasing artifacts at high frequencies
- Verify stability at extreme parameter values

---

## References

### Original Elements Code

- `eurorack/elements/dsp/resonator.cc` - Original Resonator::Process()
- `eurorack/elements/dsp/resonator.h` - kMaxModes = 64
- `eurorack/stmlib/dsp/filter.h` - SVF implementation with FREQUENCY_FAST

### ARM NEON Resources

- [ARM NEON Intrinsics Reference](https://developer.arm.com/architectures/instruction-sets/intrinsics/)
- [NEON Programmer's Guide](https://developer.arm.com/documentation/den0018/a/)
- Drumlogue CPU: Cortex-A7 with NEON VFPv4

### Modal Synthesis Research

- "Accurate Sound Synthesis of 3D Object Collisions" - Zambon (2012)
- "A Coupled Resonant Filter Bank" - Poirot, DAFx 2023
- Stanford CCRMA: Digital State-Variable Filters

---

## Implementation Checklist

### Phase 1: Scalar Optimizations (Do First!)

- [x] Implement coefficient caching (skip recalc when params unchanged)
- [x] Add more aggressive clock divider for higher modes
- [ ] Create SVF G coefficient lookup table (512 entries) - *Already has 129-entry LUT, adequate*
- [ ] Test 10-mode configuration on hardware
- [ ] Measure bar count before audio issues (baseline metric)

### Phase 2: Structural Improvements

- [ ] Move large arrays to static storage if needed
- [ ] Consider dynamic mode count based on pitch
- [ ] Profile coefficient calculation vs filter processing time

### Phase 3: NEON (Only If Needed)

- [ ] Try NEON for amplitude calculation only (lowest risk)
- [ ] Try NEON horizontal sum helper
- [ ] **Only if above helps**: Try SoA layout for SVF
- [ ] Manual instruction reordering if using NEON
- [ ] Compare bar count metrics after each change

### Phase 4: Mode Count Increase

- [ ] Test 12-mode configuration after optimizations
- [ ] Test 16-mode configuration if 12 is stable
- [ ] Update README.md with recommended mode settings
- [ ] Document performance characteristics

---

## Notes

### Drumlogue CPU: NXP i.MX 6ULZ (MCIMX6Z0DVM09AB)

**Confirmed CPU details from teardown and datasheet:**

- **SoC**: NXP i.MX 6ULZ (MCIMX6Z0DVM09AB)
- **Core**: Single ARM Cortex-A7 @ 900 MHz
- **NEON**: VFPv4 with NEON Media Processing Engine (MPE)
  - 32x64-bit general-purpose registers
  - SIMD Media Processing Architecture
  - Integer ALU, Shift, MAC pipelines
  - Dual single-precision floating point (FADD, FMUL)
  - Load/store and permute pipeline
- **Caches**:
  - 32 KB L1 Instruction Cache
  - 32 KB L1 Data Cache
  - 128 KB unified L2 Cache
- **Memory**: Supports LPDDR2-800, DDR3-800, DDR3L-800 (16-bit)

**NOT an STM32!** The STM32 in the drumlogue handles other tasks (likely UI/sequencer),
but user units run on the i.MX 6ULZ ARM Cortex-A7.

Source: [NXP Datasheet IMX6ULZCEC](https://www.nxp.com/docs/en/data-sheet/IMX6ULZCEC.pdf)
Source: [Gearspace Thread - dukesrg teardown](https://gearspace.com/board/electronic-music-instruments-and-electronic-music-production/1306032-korg-logue-user-oscillator-programming-thread-88.html)

### Why Original Elements Uses 64 Modes

The original Mutable Instruments Elements runs on STM32F405 at 168MHz with:

- Optimized ARM assembly in some paths
- 32kHz sample rate (vs our 48kHz)
- Custom memory layout for cache efficiency

Our drumlogue runs at 48kHz on Cortex-A7 at 900MHz with different cache characteristics. The NEON unit can help, but we need to respect the real-time constraint.

### NEON Optimization Learnings from FM64 Developer (dukesrg)

Key insights from FM64 drumlogue development on the same hardware:

1. **Vectorization can hurt performance**: Over-vectorization on Cortex-A7 can actually slow things down due to execution latency vs load/store penalties

2. **Instruction reordering matters**: GCC does NOT properly reorder NEON intrinsics for optimal performance. Manual reordering gave significant speedup (7 to 10 voices on FM64)

3. **Avoid classes with many members**: Large Synth classes can cause hangs on unit load. Static variables may be more reliable.

4. **Toolchain limitations**: The SDK's GCC toolchain is older - some newer NEON intrinsics (like `vld1q_f32_x3`) are not available

5. **Performance threshold behavior**: When CPU is exceeded, drumlogue first skips audio, then can go completely silent (all tracks), requiring reboot

6. **Testing approach**: Use sequence playback to compare optimizations - counting bars before audio corruption/silence gives relative performance comparison

### NEON Caveats for Drumlogue

From drumlogue developer experience:

- Over-vectorization can **hurt** performance on Cortex-A7
- Memory bandwidth is limited
- Keep feedback-dependent DSP scalar where NEON overhead > benefit
- Test on actual hardware - simulator results may differ
- GCC fails to optimize NEON intrinsic ordering - manual ASM may be needed for best results
- No Cortex-A7 specific optimization guide exists (unlike A57/A72 which have detailed guides)
