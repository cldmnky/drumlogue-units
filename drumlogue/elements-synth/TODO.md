# Elements-Synth TODO: Missing Features from Mutable Instruments Elements

This document tracks features from the original Mutable Instruments Elements that are not yet implemented in our drumlogue port.

---

## 🔴 High Priority (Core Sound Quality)

### 1. SVF (State Variable Filter) for Modal Modes
**Status:** ✅ IMPLEMENTED  
**Current:** Zero-delay feedback SVF (like stmlib)  
**Implementation:** `Mode` class now uses SVF with `g_`, `r_`, `h_` coefficients  

**Why it matters:**
- SVF is more stable at high Q values near Nyquist
- Better numeric precision for modal synthesis
- Elements uses `set_f_q<FREQUENCY_FAST>()` with optimized tan approximation

**Implementation:**
```cpp
// From stmlib/dsp/filter.h
class Svf {
    float g_, r_, h_;
    float state_1_, state_2_;
    
    template<FrequencyApproximation approximation>
    void set_f_q(float f, float resonance) {
        g_ = OnePole::tan<approximation>(f);
        r_ = 1.0f / resonance;
        h_ = 1.0f / (1.0f + r_ * g_ + g_ * g_);
    }
    
    float Process(float in) {
        float hp = (in - r_ * state_1_ - g_ * state_1_ - state_2_) * h_;
        float bp = g_ * hp + state_1_;
        state_1_ = g_ * hp + bp;
        float lp = g_ * bp + state_2_;
        state_2_ = g_ * bp + lp;
        return bp;  // bandpass for modal
    }
};
```

**Files to modify:**
- [ ] `dsp/resonator.h` - Replace `Mode` class with SVF-based implementation
- [ ] `dsp/dsp_core.h` - Add SVF class or include stmlib

---

### 2. Position Parameter Interpolation (Anti-Zipper)
**Status:** ✅ IMPLEMENTED  
**Current:** Per-sample smoothed interpolation with `previous_position_`  
**Implementation:** Exponential smoothing in `Process()` (~1ms time constant)  

**Why it matters:**
- Position parameter is extremely sensitive to zipper noise
- Elements interpolates position every sample

**Implementation:**
```cpp
// In Process():
float position_increment = (position_ - previous_position_) / size;
previous_position_ += position_increment;  // Per sample
```

**Files to modify:**
- [ ] `dsp/resonator.h` - Add `previous_position_` and per-sample interpolation

---

### 3. Clock Divider for Mode Updates
**Status:** ✅ IMPLEMENTED  
**Current:** First 24 modes @ every call, higher modes @ half rate  
**Implementation:** `clock_divider_` in `ComputeFilters()` with `(i & 1) == (clock_divider_ & 1)` test  

**Why it matters:**
- Significant CPU savings (especially with 32+ modes)
- Elements only updates subset of modes each audio block

**Implementation:**
```cpp
size_t clock_divider_ = 0;

void ComputeFilters() {
    ++clock_divider_;
    for (size_t i = 0; i < kNumModes; ++i) {
        // Update first 24 every time, higher modes alternating
        bool update = i <= 24 || ((i & 1) == (clock_divider_ & 1));
        if (update) {
            modes_[i].SetFrequencyAndQ(...);
        }
    }
}
```

**Files to modify:**
- [ ] `dsp/resonator.h` - Add clock divider logic to `Update()`

---

### 4. Stiffness Lookup Table
**Status:** ✅ IMPLEMENTED  
**Current:** `kStiffnessLUT[65]` with `GetStiffness()` interpolation  
**Implementation:** Dynamic stiffness accumulation in `ComputeFilters()` with fold-back prevention  

**Why it matters:**
- Elements calculates partials dynamically with accumulating stiffness
- Allows negative stiffness (partials converging instead of diverging)
- Much richer timbral control

**Implementation:**
```cpp
// Elements approach:
float stretch_factor = 1.0f;
for (size_t i = 0; i < num_modes; ++i) {
    float partial_frequency = harmonic * stretch_factor;
    stretch_factor += stiffness;  // Accumulates
    if (stiffness < 0.0f) stiffness *= 0.93f;  // Prevent fold-back
    harmonic += frequency_;
}
```

**Files to modify:**
- [ ] `dsp/dsp_core.h` - Add `lut_stiffness[257]` lookup table
- [ ] `dsp/resonator.h` - Replace preset tables with dynamic stiffness calculation

---

### 5. CosineOscillator for Amplitude Modulation
**Status:** ✅ IMPLEMENTED  
**Current:** `CosineOscillator` class with `Init()`, `Start()`, `Next()`  
**Implementation:** Walking cosine for both center and aux (stereo) amplitude modulation  

**Why it matters:**
- Elements uses `CosineOscillator` to modulate mode amplitudes
- Provides physically accurate pickup position behavior
- Smoother stereo spread modulation

**Implementation:**
```cpp
// From stmlib/dsp/cosine_oscillator.h
class CosineOscillator {
    float y0_, y1_, iir_coefficient_;
    
    void Init(float position) {
        iir_coefficient_ = 2.0f * cosf(position * M_PI);
        y1_ = sinf(position * M_PI);
        y0_ = sinf(position * M_PI + M_PI_2);
    }
    
    float Next() {
        float temp = y0_;
        y0_ = iir_coefficient_ * y0_ - y1_;
        y1_ = temp;
        return temp;
    }
};
```

**Files to modify:**
- [ ] `dsp/dsp_core.h` - Add `CosineOscillator` class
- [ ] `dsp/resonator.h` - Use for amplitude calculation in `Process()`

---

## 🟡 Medium Priority (Extended Features)

### 6. Bowing Model (Sustained Excitation)
**Status:** ✅ IMPLEMENTED  
**Current:** Full banded waveguide bowing with BowTable friction model  
**Implementation:**  
- `BowTable()` function in `dsp_core.h`
- `BowedMode` class with delay line + bandpass filter
- 8 bowed modes (`kMaxBowedModes`) in Resonator
- `Process(excitation, bow_strength, ...)` API in Resonator
- Exciter provides `GetBowStrength()` for integration

**Why it matters:**
- Allows sustained, violin-like sounds
- Uses `BowTable()` friction function + delay lines per mode

**Files modified:**
- [x] `dsp/dsp_core.h` - Added `BowTable()`, `FastAbs()`, `DelayLine<>` template
- [x] `dsp/resonator.h` - Added `BowedMode` class, `bowed_modes_[]`, `bow_signal_`

---

### 7. Tube/Formant Filter (Blow Excitation)
**Status:** ✅ IMPLEMENTED  
**Current:** Full waveguide tube with reed model for breath sounds  
**Implementation:**
- `Tube` class in new `dsp/tube.h`
- Zero/pole filtering for formant character
- Reed-like nonlinearity for breath response
- Integrated into Exciter blow processing
- Blow frequency tracks pitch via `SetBlowFrequency()`

**Why it matters:**

- Creates vowel-like, breathy timbres
- Physical modeling of air column resonance

**Files modified:**

- [x] Created `dsp/tube.h` - Full waveguide tube implementation
- [x] `dsp/exciter.h` - Integrated tube processing in blow mode
- [x] `modal_synth.h` - Blow frequency set on NoteOn

---

### 8. Enhanced String Model (Dispersion)
**Status:** ✅ IMPLEMENTED  
**Current:** `DispersionFilter` class with 4-stage allpass cascade  
**Implementation:**
- `DispersionFilter` class with configurable 4-stage allpass network
- Frequency-dependent coefficient scaling (lower freqs = more dispersion)
- `SetDispersion()` method on String, MultiString, and ModalSynth
- Piano-like inharmonicity where high partials are sharper

**Why it matters:**

- Dispersion creates piano-like inharmonicity
- High partials become progressively sharper (stretch tuning)

**Files modified:**

- [x] `dsp/resonator.h` - Added `DispersionFilter` class, integrated into `String`
- [x] `modal_synth.h` - Added `SetDispersion()` method

---

### 9. Multi-String Mode (5 Detuned Strings)
**Status:** ✅ IMPLEMENTED  
**Current:** Full multi-string with 5 sympathetic strings  
**Implementation:**
- `MultiString` class with 5 `String` instances
- Detuning ratios: 1.0, 0.998, 1.002, 0.996, 1.004 (chorus effect)
- Stereo output with alternating pan
- Brightness/damping control for all strings
- `kMultiString` model in ModalSynth

**Why it matters:**

- Creates rich, 12-string guitar or piano-like resonance
- Sympathetic resonance adds depth

**Files modified:**

- [x] `dsp/resonator.h` - Added `MultiString` class with 5 `String` instances
- [x] `modal_synth.h` - Added `kMultiString` model, routing in `Process()`

---

### 10. Additional Exciter Models
**Status:** ✅ IMPLEMENTED (Plectrum & Particles)  
**Current:** Sample, Granular, Noise, Plectrum, Particles  
**Implementation:**

- `STRIKE_MODE_PLECTRUM` - Guitar pick model with delayed release impulse
- `STRIKE_MODE_PARTICLES` - Random impulse train (rain/gravel effect)

#### 10a. Plectrum Model ✅

```cpp
// Implemented: Initial negative impulse, delay, positive release
// plectrum_delay_ controls timing based on timbre_
```

#### 10b. Particles Model ✅

```cpp
// Implemented: Random walk particle state with density-controlled timing
// particle_state_, particle_range_, particle_delay_
```

#### 10c. Sample Player (non-granular)

```cpp
// Already exists as STRIKE_MODE_SAMPLE
```

**Files modified:**

- [x] `dsp/exciter.h` - Added `STRIKE_MODE_PLECTRUM`, `STRIKE_MODE_PARTICLES`

---

### 11. Q Calculation: Frequency-Dependent
**Status:** ✅ IMPLEMENTED  
**Current:** `1.0f + partial_frequency * base_q` in `ComputeFilters()`  
**Implementation:** Higher partials get proportionally higher Q with progressive loss  

**Why it matters:**
- Higher frequency modes naturally need higher Q to ring
- More physically accurate

**Files to modify:**
- [ ] `dsp/resonator.h` - Change Q calculation in `Update()`

---

## 🟢 Lower Priority (Polish & Optimization)

### 12. 4-Decades Q Lookup Table
**Status:** ✅ IMPLEMENTED  
**Current:** `kQDecadesLUT[65]` with `GetQFromDamping()` interpolation  
**Implementation:** 4-decade logarithmic Q range (0.5 to 5000)  

**Files to modify:**
- [ ] `dsp/dsp_core.h` - Add `lut_4_decades[257]`
- [ ] `dsp/resonator.h` - Use for Q calculation

---

### 13. SVF Coefficient Lookup Tables
**Status:** ✅ IMPLEMENTED  
**Current:** `kSvfGLUT[129]` lookup table with `LookupSvfG()` interpolation  
**Implementation:**
- Pre-computed tan(π * f) for normalized frequencies [0, 0.49]
- 129-entry table with linear interpolation
- Replaces `FastTan()` in hot `ComputeFilters()` loop
- ~3x faster than polynomial approximation

**Files modified:**
- [x] `dsp/dsp_core.h` - Added `kSvfGLUT[129]` and `LookupSvfG()`
- [x] `dsp/resonator.h` - Updated `ComputeFilters()` and `UpdateMode()` to use lookup

---

### 14. Brightness Attenuation at Low Geometry
**Status:** ✅ IMPLEMENTED  
**Current:** 8th-power attenuation of brightness at low geometry  
**Implementation:** `brightness_attenuation ^= 8` applied in `ComputeFilters()`  

**Implementation:**
```cpp
float brightness_attenuation = 1.0f - geometry_;
brightness_attenuation *= brightness_attenuation;  // ^2
brightness_attenuation *= brightness_attenuation;  // ^4
brightness_attenuation *= brightness_attenuation;  // ^8
float brightness = brightness_ * (1.0f - 0.2f * brightness_attenuation);
```

**Files to modify:**
- [ ] `dsp/resonator.h` - Add brightness attenuation in `Update()`

---

### 15. Dynamic Mode Count Based on Frequency
**Status:** ✅ IMPLEMENTED  
**Current:** `ComputeFilters()` returns active mode count based on Nyquist  
**Implementation:** Modes with `partial_frequency >= 0.49` are skipped  

**Why it matters:**
- Low frequencies can use more modes (partials fit below Nyquist)
- High frequencies need fewer modes (save CPU)

**Implementation:**
```cpp
size_t num_modes = 0;
for (size_t i = 0; i < kMaxModes; ++i) {
    if (partial_frequency >= 0.49f) break;
    num_modes = i + 1;
}
```

**Files to modify:**
- [ ] `dsp/resonator.h` - Add runtime mode count calculation

---

### 16. Configurable LFO Modulation Frequency
**Status:** ✅ IMPLEMENTED  
**Current:** `SetModulationFrequency()` and `SetModulationOffset()` methods  
**Implementation:** Default 0.5Hz, configurable 0.1-10Hz range  

**Files to modify:**
- [ ] `dsp/resonator.h` - Add `SetModulationFrequency()` method
- [ ] `elements_synth_v2.h` - Expose as parameter (or derive from existing param)

---

### 17. MultistageEnvelope
**Status:** ✅ IMPLEMENTED  
**Current:** Full `MultistageEnvelope` class with ADR, AD, AR, AD-Loop modes  
**Implementation:**
- `dsp/envelope.h` contains complete `MultistageEnvelope` class
- Supports ADSR, AD, AR, ADR (with 0 sustain), AD-Loop modes
- Filter envelope with faster times for pluck character
- Exponential attack/decay curves for musical response

**Files modified:**
- [x] `dsp/envelope.h` - Full `MultistageEnvelope` implementation
- [x] `modal_synth.h` - Uses `env_` and `filter_env_` MultistageEnvelope instances

---

### 18. Accent/Velocity Gain Tables
**Status:** ✅ IMPLEMENTED  
**Current:** `kVelocityGainCoarse[33]` and `kVelocityGainFine[33]` lookup tables  
**Implementation:**
- Coarse gain: Exponential velocity curve (0-1 range)
- Fine gain: Subtle accent modulation (0.5-1.5 range)
- `GetVelocityGain()` and `GetVelocityAccent()` with interpolation
- NoteOn uses exponential velocity curve for musical dynamics

**Files modified:**
- [x] `dsp/dsp_core.h` - Added velocity gain LUTs and lookup functions
- [x] `modal_synth.h` - NoteOn uses `GetVelocityGain()` instead of linear scaling

---

## 📋 Implementation Order Recommendation

### Phase 1: Core Sound Quality
1. SVF filters for modes (biggest sound improvement)
2. Position interpolation (eliminates zipper noise)
3. Stiffness LUT / dynamic partials
4. CosineOscillator for amplitudes

### Phase 2: Extended Features
5. Bowing model
6. Clock divider optimization
7. Enhanced string with dispersion
8. Missing exciter models

### Phase 3: Polish
9. Q lookup tables
10. Brightness attenuation
11. Dynamic mode count
12. MultistageEnvelope

---

## 📊 Feature Comparison Summary

| Feature | Elements | Ours | Status |
|---------|----------|------|--------|
| Sample Rate | 32kHz | 48kHz | ✅ Better |
| Max Modes | 64 | 32 | ✅ Adequate |
| Filter Type | SVF | SVF | ✅ Done |
| Position Interp | Per-sample | Per-sample | ✅ Done |
| Stiffness | LUT + dynamic | LUT + dynamic | ✅ Done |
| Amplitude Mod | CosineOsc | CosineOsc | ✅ Done |
| Clock Divider | Yes | Yes | ✅ Done |
| Q LUT | 4-decades | 4-decades | ✅ Done |
| Brightness Atten | Yes | Yes | ✅ Done |
| Dynamic Modes | Yes | Yes | ✅ Done |
| DampingFilter | 3-tap FIR | 3-tap FIR | ✅ Done |
| Bowing | 8 modes | 8 modes | ✅ Done |
| Tube/Formant | Yes | Yes | ✅ Done |
| Multi-String | 5 strings | 5 strings | ✅ Done |
| Plectrum | Yes | Yes | ✅ Done |
| Particles | Yes | Yes | ✅ Done |
| Dispersion | Allpass | 4-stage Allpass | ✅ Done |
| SVF Coef LUTs | Yes | 129-entry LUT | ✅ Done |
| Velocity LUTs | Yes | 33-entry LUT | ✅ Done |
| MultistageEnv | Yes | Yes | ✅ Done |

**🎉 ALL FEATURES COMPLETE! 🎉**

---

## References

- `eurorack/elements/dsp/resonator.cc` - Original resonator implementation
- `eurorack/elements/dsp/exciter.cc` - All exciter models
- `eurorack/elements/dsp/string.cc` - Enhanced string model
- `eurorack/elements/dsp/tube.cc` - Formant filter
- `eurorack/elements/resources.h` - All lookup tables
- `eurorack/stmlib/dsp/filter.h` - SVF implementation
- `eurorack/stmlib/dsp/cosine_oscillator.h` - Amplitude modulator
