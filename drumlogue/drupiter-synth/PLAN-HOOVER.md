# Hoover Synthesis Implementation Plan for Drupiter-Synth

**Project**: Add Hoover synthesis capabilities to Drupiter  
**Target**: Authentic Roland Alpha Juno "Hoover" sound recreation  
**Status**: Planning Phase  
**Created**: 2025-12-25

---

## Table of Contents

1. [Overview](#overview)
2. [What is Hoover Synthesis](#what-is-hoover-synthesis)
3. [Current State Analysis](#current-state-analysis)
4. [Missing Features](#missing-features)
5. [Implementation Phases](#implementation-phases)
6. [Technical Specifications](#technical-specifications)
7. [Parameter Mapping](#parameter-mapping)
8. [CPU/Memory Considerations](#cpumemory-considerations)
9. [Testing Strategy](#testing-strategy)
10. [Success Criteria](#success-criteria)
11. [References](#references)

---

## Overview

The "Hoover" is a distinctive synthesizer sound that became iconic in 1990s rave, techno, and jungle music. Originating from the Roland Alpha Juno synthesizer (1986), it's characterized by thick, chorused, evolving timbres created through multiple detuned oscillators with pulse width modulation.

This plan outlines the features needed to transform Drupiter from a Jupiter-8 style synthesizer into a Hoover-capable instrument while maintaining backward compatibility with existing presets.

**Key Goals:**
- Enable authentic Alpha Juno "Hoover" sound recreation
- Maintain existing Jupiter-8 functionality
- Optimize for ARM Cortex-A7 performance constraints
- Keep within 24-parameter drumlogue limit

---

## What is Hoover Synthesis

### Historical Context

The Hoover sound was created on the Roland Alpha Juno (1986) and popularized in:
- **"Mentasm"** - Joey Beltram (1991)
- **"Dominator"** - Human Resource (1991)
- **"Charly"** - The Prodigy (1992)

The sound is sometimes called "Mentasm" or the "Dominator sound" after these seminal tracks.

### Polyphonic Extension

In addition to hoover synthesis, this plan includes a **polyphonic mode** to make Drupiter a full-featured synthesizer capable of:
- **Monophonic mode:** Current single-voice operation (one MIDI note at a time)
- **Polyphonic mode:** Multiple notes simultaneously (4 voices, configurable at compile-time)
- **Unison mode:** Single note with multiple detuned copies (Phase 1 implementation)

Both polyphonic and unison modes are configurable via compile-time flags (`-DVOICES=4` or `-DVOICES=7`), allowing flexible deployment to different drumlogue versions or use cases.

### Core Characteristics

#### 1. **PWM Sawtooth** (Unique to Alpha Juno)
- Sawtooth waveform with pulse width modulation
- Most synths only have PWM on pulse/square waves
- Creates distinctive harmonic movement

#### 2. **Dual PWM Oscillators (Phase-Aligned)**
- Sawtooth PWM + Pulse PWM simultaneously
- Both waveforms from single oscillator (phase-locked)
- Critical alignment creates the signature sound

#### 3. **Sub Oscillator**
- Square wave, 2 octaves below main pitch
- Adds bass weight and power

#### 4. **Heavy Chorus/Unison**
- Multiple detuned voices (3-7+)
- Wide stereo spread
- Time-varying modulation
- Creates "thick" evolving texture

#### 5. **Modulation**
- **LFO → PWM**: Triangle LFO modulating pulse width
- **Pitch Envelope**: Aggressive pitch drop at note start (optional)
- **Filter Sweeps**: Dynamic filter movement

#### 6. **Processing**
- Heavy chorus/ensemble effect
- Moderate to high resonance filter
- Often with reverb/space

---

## Synth Mode Architecture

### Three Operating Modes

The enhanced Drupiter will support three distinct synthesis modes:

#### 1. **Monophonic Mode** (Current - Preserved)
- Single voice, one MIDI note at a time
- Voice priority: Last note (typical synthesizer behavior)
- No note stealing
- CPU usage: Minimal
- **Use case:** Bass, leads, monophonic sequences
- **Default mode:** Maintains backward compatibility

#### 2. **Polyphonic Mode** (New)
- Multiple simultaneous notes (4 voices, configurable: `#define NUM_VOICES 4`)
- Voice allocation: Round-robin, oldest-note stealing, or first-available
- Shared filter and envelope per voice (classic subtractive synth)
- Per-voice pitch, amplitude, and envelope state
- **Use case:** Chords, pads, complex progressions
- **Compile-time configuration:** `-DNUM_VOICES=4`
- **CPU usage:** ~4× single voice (4 DCO pairs, 4 filter/envelope instances)

#### 3. **Unison Mode** (New - Phase 1)
- Single note with multiple detuned copies (3-7 voices, configurable: `#define UNISON_VOICES 5`)
- Voice allocation: Not applicable (all voices play same note)
- Unison voices share pitch, detune spread, and panning
- **Use case:** Thick, evolving leads and hoover sounds
- **Compile-time configuration:** `-DUNISON_VOICES=5`
- **CPU usage:** 5-7× single voice (5-7 detuned DCO pairs)

### Build Configuration

```makefile
# config.mk additions

# Synthesis mode selection (mutually exclusive)
# DRUPITER_MODE = MONOPHONIC (default, minimal CPU)
# DRUPITER_MODE = POLYPHONIC (4-voice polyphony)
# DRUPITER_MODE = UNISON (thick, detuned unison)

# Voice count configuration
UDEFS += -DDRUPITER_MODE=UNISON
UDEFS += -DUNISON_VOICES=5
UDEFS += -DPOLYPHONIC_VOICES=4

# Example: Build for unison with 5 voices
# make UDEFS="-DDRUPITER_MODE=UNISON -DUNISON_VOICES=5"
```

### CPU Budget Comparison

| Mode | Oscillators | Filters | Envelopes | Est. CPU |
|------|-------------|---------|-----------|----------|
| Monophonic | 2 | 1 | 2 | ~20% |
| Polyphonic (4v) | 8 | 4 | 8 | ~60-75% |
| Unison (5v) | 10 | 1 | 2 | ~50-65% |
| Unison (7v) | 14 | 1 | 2 | ~65-80% |

### MIDI Behavior by Mode

**Monophonic:**
- `NoteOn`: Always accepted, triggers envelope
- `NoteOff`: Releases voice if note matches, otherwise ignored
- Behavior: Last-note-priority (traditional synth)

**Polyphonic:**
- `NoteOn`: Allocates next available voice (round-robin)
- `NoteOff`: Releases matching voice by note number
- Behavior: 4 independent voices with per-voice pitch and envelopes

**Unison:**
- `NoteOn`: Triggers unison voices (all play same pitch)
- `NoteOff`: Releases all voices together
- Behavior: Single "super-voice" with detuned copies

---

### ✅ Existing Features (Jupiter-8 Architecture)

Drupiter already has:

**Oscillators:**
- ✅ Dual DCOs (DCO1 + DCO2)
- ✅ Multiple waveforms: SAW, SQUARE, PULSE, TRIANGLE
- ✅ Pulse Width Modulation (PWM) on PULSE waveform
- ✅ Octave shifting (16', 8', 4', 2')
- ✅ Fine tune/detune per oscillator
- ✅ Cross-modulation (DCO2 → DCO1 FM)
- ✅ Hard sync (DCO1 → DCO2)

**Modulation:**
- ✅ LFO with 4 waveforms (TRI, RAMP, SQR, S&H)
- ✅ LFO rate control (0.1Hz - 50Hz)
- ✅ LFO delay envelope
- ✅ LFO → PWM modulation (`MOD_LFO_TO_PWM`)
- ✅ LFO → VCF modulation
- ✅ LFO → VCO modulation (vibrato)
- ✅ Dual ADSR envelopes (VCF + VCA)
- ✅ ENV → PWM modulation
- ✅ ENV → VCF modulation (bipolar ±4 octaves)

**Filter:**
- ✅ Multi-mode VCF (LP12, LP24, HP12, BP12)
- ✅ Resonance control (self-oscillation capable)
- ✅ Keyboard tracking (50% default)
- ✅ High-pass filter

**Effects:**
- ✅ Stereo widener/chorus (`stereo_widener.h`)
- ✅ Effect modes: CHORUS, SPACE, DRY, BOTH

**Architecture:**
- ✅ ARM NEON SIMD optimization support
- ✅ PolyBLEP anti-aliasing
- ✅ Preset system (6 factory presets)
- ✅ MOD HUB routing system
- ✅ Desktop test harness framework

---

## Missing Features

### Missing Critical Features

#### **0. Polyphonic/Unison Architecture** 🔴 PRIORITY 1 (Foundation)

**Current State:**
- Monophonic only (single voice, one note at a time)
- `gate_`, `current_note_`, `current_velocity_` are scalar values
- No voice allocation or management

**Problem:**
- Cannot play polyphonic chords
- Cannot leverage synth architecture for richer textures
- Unison mode requires voice infrastructure

**Implementation Needed:**

```cpp
// New file: dsp/voice_allocator.h/cc
enum SynthMode {
    SYNTH_MODE_MONOPHONIC = 0,
    SYNTH_MODE_POLYPHONIC,
    SYNTH_MODE_UNISON
};

struct Voice {
    bool active;
    uint8_t note;
    uint8_t velocity;
    float pitch_hz;
    JupiterDCO dco1, dco2;
    JupiterVCF vcf;
    JupiterEnvelope env_vcf, env_vca;
    // ... per-voice state ...
};

class VoiceAllocator {
public:
    void Init(SynthMode mode, uint8_t max_voices);
    Voice* AllocateVoice(uint8_t note, uint8_t velocity);
    void ReleaseVoice(uint8_t note);
    void RenderVoices(float* out, uint32_t frames);
    
private:
    Voice voices_[DRUPITER_MAX_VOICES];
    uint8_t active_voices_;
    SynthMode mode_;
    uint8_t next_voice_;  // For round-robin allocation
};
```

**Architecture:**
- Mode selection via compile-time `#define DRUPITER_MODE`
- Voice array sized at compile-time (`NUM_VOICES`)
- Shared global LFO and modulation per mode
- Per-voice pitch/envelope/filter state

**CPU Impact:** 
- Monophonic: Baseline (~20%)
- Polyphonic (4 voices): 4× oscillators + filters (~60-75%)
- Unison (5 voices): 5× oscillators, shared filter (~50-65%)

**Files:**
- `dsp/voice_allocator.h/cc` (new)
- `drupiter_synth.h/cc` (significant refactoring)
- `dsp/jupiter_dco.h/cc` (unchanged, but used by voice_allocator)

---

#### **1. PWM Sawtooth Waveform** 🔴 PRIORITY 1

**Current State:**
- PWM only works on PULSE waveform
- Sawtooth is static (no modulation)

**Problem:**
- Defining characteristic of Alpha Juno hoover
- Cannot recreate authentic hoover timbre without this

**Implementation Needed:**
```cpp
// In jupiter_dco.cc, modify Process() for SAW_PWM waveform
enum Waveform {
    WAVEFORM_SAW = 0,
    WAVEFORM_SAW_PWM = 1,     // NEW: PWM-able sawtooth
    WAVEFORM_SQUARE = 2,
    WAVEFORM_PULSE = 3,
    WAVEFORM_TRIANGLE = 4,
    // ...
};
```

**Technical Details:**
- Morph sawtooth shape based on pulse_width_ parameter
- Maintain PolyBLEP anti-aliasing for discontinuities
- Efficient implementation (lookup table or analytical)
- Combine with existing PWM modulation routing

**CPU Impact:** Minimal (same as current PULSE PWM)

---

#### 2. **Unison/Supersaw (Multiple Detuned Voices)** 🔴 PRIORITY 1 (Requires Architecture)

**Current State:**
- Single voice per oscillator
- Basic stereo widening (minor chorusing)

**Problem:**
- Hoover requires 3-7+ simultaneous detuned voices
- Single voice lacks the "thick," evolving character

**Implementation Needed:**

```cpp
// New file: dsp/unison_oscillator.h/cc
class UnisonOscillator {
public:
    static constexpr size_t kMaxVoices = 7;
    
    void Init(float sample_rate);
    void SetVoiceCount(uint8_t count);      // 1-7 voices
    void SetDetuneAmount(float amount);     // 0.0-1.0 (0-50 cents)
    void SetSpread(float spread);           // Stereo width 0-1
    void SetPitch(float freq_hz);           // Pitch for all voices
    void Render(float* left, float* right, uint32_t frames);
    
private:
    JupiterDCO voices_[kMaxVoices];
    float detune_amounts_[kMaxVoices];
    float pan_positions_[kMaxVoices];
    uint8_t active_voices_;
};
```

**Note:** In UNISON mode, one `UnisonOscillator` pair (DCO1 + DCO2) renders all voices to single pitch output. In POLYPHONIC mode, each voice has its own oscillators (not unison).

**Features:**
- Configurable voice count (1-7)
- Detune spread algorithms:
  - **Linear**: Equal spacing (e.g., -10, -5, 0, +5, +10 cents)
  - **Exponential**: Natural sounding (Golden ratio spacing)
  - **Random**: Organic evolution (seeded random per voice)
- Pan spread across stereo field
- Phase randomization per voice (natural chorus)
- Shared wavetables (memory efficient)

**Detune Calculation Examples:**
```cpp
// Linear spread: 7 voices, ±10 cents
// Voice 0: -10 cents
// Voice 1: -6.67 cents
// Voice 2: -3.33 cents
// Voice 3: 0 cents (center)
// Voice 4: +3.33 cents
// Voice 5: +6.67 cents
// Voice 6: +10 cents

// Golden ratio spread (more natural):
const float phi = 1.618033988749895f;
detune[i] = (i - center) * amount * pow(phi, abs(i - center) / 3.0f);
```

**CPU Impact:** HIGH (7x oscillator processing)
- Single voice: ~2-3% CPU
- 7 voices: ~15-20% CPU estimated
- Mitigation: NEON SIMD optimization, voice limiting option

---

#### 3. **Phase-Aligned Multi-Waveform Output** 🟡 PRIORITY 2

**Current State:**
- DCO1 and DCO2 are independent oscillators
- Separate phase accumulators
- Cannot guarantee phase alignment

**Problem:**
- Hoover uses SAW PWM + PULSE PWM from same oscillator
- Phase alignment is critical to the sound
- Two independent DCOs drift apart

**Implementation Needed:**

```cpp
// Modify JupiterDCO to support dual-output mode
class JupiterDCO {
public:
    void SetDualWaveformMode(bool enabled);
    void SetPrimaryWaveform(Waveform wf);
    void SetSecondaryWaveform(Waveform wf);
    float ProcessDual(float* primary, float* secondary);
    
private:
    bool dual_output_mode_;
    Waveform primary_waveform_;
    Waveform secondary_waveform_;
};
```

**Usage Example:**
```cpp
// Alpha Juno style: Saw PWM + Pulse PWM from one oscillator
dco1.SetDualWaveformMode(true);
dco1.SetPrimaryWaveform(WAVEFORM_SAW_PWM);
dco1.SetSecondaryWaveform(WAVEFORM_PULSE);
dco1.SetPulseWidth(0.3f);  // Applies to both!

float saw_out, pulse_out;
dco1.ProcessDual(&saw_out, &pulse_out);
float mixed = saw_out * 0.7f + pulse_out * 0.5f;
```

**CPU Impact:** Minimal (process two waveforms per phase advance)

---

### ⚠️ Important (Enhances Hoover)

#### 4. **Pitch Envelope Modulation** 🟡 PRIORITY 2

**Current State:**
- No pitch envelope
- Pitch only controlled by MIDI note + LFO vibrato

**Problem:**
- Many hoover patches use aggressive pitch drop
- Adds expressive "swooping" character
- Common technique in rave/techno

**Implementation Needed:**

```cpp
// Add to MOD HUB destinations
enum ModDestination {
    // ... existing destinations ...
    MOD_ENV_TO_PITCH,    // NEW: Envelope modulates pitch
};

// In DrupiterSynth::Render()
float pitch_mod = GetModulationAmount(MOD_ENV_TO_PITCH);
float env_value = env_vcf_.GetValue();  // Reuse VCF envelope
float pitch_shift_semitones = (env_value - 0.5f) * pitch_mod * 24.0f;  // ±12 semitones
float freq = base_freq * semitones_to_ratio(pitch_shift_semitones);
```

**Typical Hoover Settings:**
- **Attack**: 0ms (instant)
- **Decay**: 50-200ms (fast drop)
- **Sustain**: 0% (pitch settles to note)
- **Release**: Don't care (note is over)
- **Amount**: 50-100% (±6-12 semitones)

**CPU Impact:** Minimal (reuse existing envelope)

---

#### 5. **Enhanced Chorus Effect** 🟡 PRIORITY 3

**Current State:**
- Basic stereo widener with Haas effect
- Single delay line per channel
- No time-varying modulation

**Problem:**
- Hoover requires heavy, evolving chorus
- Current widener is too subtle
- Lacks the "wet" wash of Alpha Juno ensemble

**Implementation Needed:**

```cpp
// New file: dsp/chorus.h/cc
class Chorus {
public:
    static constexpr size_t kNumTaps = 3;
    
    void Init(float sample_rate);
    void SetDepth(float depth);       // 0-1
    void SetRate(float rate_hz);      // 0.1-10 Hz
    void SetFeedback(float feedback); // 0-0.9
    void SetMix(float mix);           // 0-1 (dry/wet)
    void Process(float in_l, float in_r, float* out_l, float* out_r);
    
private:
    float delay_lines_[2][kNumTaps][4800];  // 100ms max @ 48kHz
    float lfo_phases_[kNumTaps];
    float write_pos_;
};
```

**Features:**
- Multi-tap delay (3 taps per channel)
- LFO-modulated delay times (triangle wave typical)
- Feedback control for richness
- Stereo cross-feedback
- Variable wet/dry mix

**Typical Hoover Settings:**
- **Depth**: 70-100%
- **Rate**: 0.5-2 Hz
- **Feedback**: 30-50%
- **Mix**: 60-80% wet

**CPU Impact:** MEDIUM (multi-tap delays + LFO)
- ~5-8% CPU estimated
- Could optimize to 2 taps if needed

---

### 🟢 Desirable (Polish)

#### 6. **Unison Panning Modes**
- Linear spread (equal spacing)
- Random spread (organic)
- Circular spread (surround-like)

#### 7. **Unison Phase Modes**
- Synchronized (all start in phase)
- Random (natural chorus)
- Progressive (staggered start)

#### 8. **Sub-Oscillator Dedicated Control**
- Separate sub-osc level parameter
- Always 2 octaves below
- Square wave only (classic)

#### 9. **Preset: "Classic Hoover"**
Factory preset with authentic settings:
```
DCO1: SAW_PWM, 8' octave
DCO2: PULSE, 8' octave  
Sub: Square, -24 semitones
Unison: 5 voices, 8 cents detune
LFO: Triangle, 1.5Hz → PWM (amount: 60%)
Filter: LP12, cutoff 70%, resonance 40%
Chorus: 80% wet, 1Hz rate
Pitch Env: -12 semitones, 100ms decay
```

---

## Implementation Phases

### Phase 1: Core Hoover Features + Polyphonic Foundation (Essential)

**Goal:** Minimum viable hoover synthesis with voice architecture

**Duration:** 3-4 weeks

**Tasks:**

#### Task 1.0: Voice Allocator & Mode Architecture ✅ COMPLETE (2025-12-25)
- [x] Create `dsp/voice_allocator.h/cc` with Voice struct
- [x] Implement runtime mode switching via MOD HUB
- [x] Implement voice allocation (round-robin for polyphonic)
- [x] Implement MIDI note-on/note-off routing per mode
- [x] Monophonic mode: Preserve existing behavior (backward compat)
- [x] Polyphonic mode: Allocate 7 voices, set pitch, render per-voice
- [x] Unison mode: 7-voice golden ratio detuned unison
- [x] Update `drupiter_synth.h/cc` to use `VoiceAllocator`
- [x] Add runtime mode switching to MOD HUB
- [x] Build test with all three modes functional
- [x] Fix polyphonic voice envelope configuration bug
- [x] Fix UNISON mode DCO2 support

**Files created:**
- `dsp/voice_allocator.h` (199 lines)
- `dsp/voice_allocator.cc` (277 lines)

**Files modified:**
- `drupiter_synth.h` (added VoiceAllocator member, mode enum)
- `drupiter_synth.cc` (integrated voice allocator, mode switching)
- `unit.cc` (route MIDI through allocator)
- `header.c` (added SYNTH MODE parameter, updated MOD HUB max)

**Critical Bugs Fixed:**
1. **Polyphonic Voice Envelopes**: Voice envelopes were never configured with ADSR parameters
   - Added loops in `SetParameter()` to propagate envelope settings to all 7 voices
   - Fixed via `allocator_.GetVoiceMutable(i).env_amp.SetAttack/Decay/Sustain/Release()`
   
2. **UNISON Mode DCO2 Support**: UNISON mode only processed UnisonOscillator, DCO2 was unused
   - Added DCO2 processing with octave multiplier and detune
   - Mixed output: `unison_mono * dco1_level + dco2_out_ * dco2_level`
   - DCO2 OCT parameter now works correctly in UNISON mode

3. **MOD HUB Flickering**: Static buffer race condition in `GetParameterStr()`
   - Separate static buffers for each parameter (`tune_buf`, `modamt_buf`)
   - Eliminated flickering when navigating MOD HUB destinations

**Implementation Notes:**
- Runtime mode switching (not compile-time flags)
- MODE parameter via MOD HUB (#14: SYNTH MODE)
- All three modes fully functional with seamless switching
- 7 voices maximum (configurable at compile-time)
- Backward compatible: Defaults to MONO mode for existing presets

**Success Criteria:**
- ✅ Monophonic mode builds and sounds identical to v1.x
- ✅ Polyphonic mode functional with 7 voices, proper envelope behavior
- ✅ Unison mode functional with 7 detuned voices + DCO2
- ✅ MIDI routing works for all modes
- ✅ No crashes or glitches
- ✅ Mode switching via MOD HUB parameter

#### Task 1.1: PWM Sawtooth Waveform ✅ COMPLETE (2025-12-25)
- [x] Research sawtooth PWM algorithms
- [x] Implement morphing sawtooth generation in `jupiter_dco.cc`
- [x] Apply PolyBLEP anti-aliasing
- [x] Add `WAVEFORM_SAW_PWM` enum (position 4)
- [x] Test with desktop harness (compare spectrograms)
- [x] Validate CPU usage on ARM

**Files modified:**
- `dsp/jupiter_dco.h` (added enum WAVEFORM_SAW_PWM = 4)
- `dsp/jupiter_dco.cc` (implemented PWM sawtooth with PolyBLEP)
- `header.c` (updated DCO1/DCO2 waveform max from 3 to 4)

**Implementation Notes:**
- PWM SAW uses asymmetric triangle morphing (0.0-1.0 pulse width)
- PolyBLEP applied to both rising and falling edges
- CPU impact: Negligible (same as PULSE PWM)
- Backward compatible: Existing presets use waveforms 0-3

#### Task 1.2: UnisonOscillator with Golden Ratio Detune ✅ COMPLETE (2025-12-25)
- [x] Create `dsp/unison_oscillator.h/cc`
- [x] Implement 7-voice unison (full implementation)
- [x] Golden ratio detune spread (±φ^n pattern)
- [x] Stereo panning with golden angle
- [x] Integrate into UNISON mode render path
- [x] Profile CPU usage with QEMU ARM test

**New files:**
- `dsp/unison_oscillator.h` (197 lines)
- `dsp/unison_oscillator.cc` (149 lines)

**Implementation Notes:**
- 7 voices with golden ratio detune: ±φ^n × detune_amount
- Golden angle stereo spread: n × 137.5° → pan position
- Wide stereo imaging with natural distribution
- Integrated with VoiceAllocator
- CPU usage: ~60% in UNISON mode (within target)

#### Task 1.3: MOD HUB Expansion ✅ COMPLETE (2025-12-25)
- [x] Add `MOD_UNISON_DETUNE` to hub destinations (#15)
- [x] Extend `kModDestinations[]` in `drupiter_synth.h`
- [x] Update `GetModDestinationName()` for UI display
- [x] Wire modulation to UnisonOscillator detune
- [x] Fix MOD HUB flickering (static buffer race condition)

**Files modified:**
- `drupiter_synth.h` (added MOD_UNISON_DETUNE = 15)
- `drupiter_synth.cc` (added modulation routing, fixed buffer issue)
- `header.c` (MOD DEST parameter max updated to 15)

**Bug Fixes:**
- Fixed MOD HUB parameter string flickering (separate static buffers)
- Fixed polyphonic mode no sound (voice envelope configuration)
- Fixed UNISON mode DCO2 support (added DCO2 processing and mixing)

**Architecture:**
- UNISON DETUNE controlled via MOD HUB destination #15
- Preserves all 24 direct parameters
- Backward compatible: Defaults to 0% for existing presets

#### Task 1.4: PWM SAW Waveform Modulation Testing ✅ COMPLETE (2025-12-25)
- [x] Test PWM SAW with LFO → PWM modulation
- [x] Test PWM SAW with ENV → PWM modulation
- [x] Validate spectral characteristics (FFT analysis)
- [x] Hardware validation confirmed functional

**Results:**
- PWM SAW responds correctly to LFO modulation
- Envelope modulation creates characteristic sweeps
- Spectral characteristics match design expectations
- CPU impact negligible (same as PULSE PWM)
- Backward compatible with existing presets

#### Task 1.5: Comprehensive Testing & Validation ✅ COMPLETE (2025-12-25)
- [x] **Mode Testing:**
  - ✅ Monophonic mode: All 6 factory presets load correctly
  - ✅ Polyphonic mode: 7-voice chord playing, voice allocation working
  - ✅ Unison mode: 7 detuned voices with golden ratio spread
- [x] Desktop tests: Native presets-editor functional
- [x] CPU profiling per mode:
  - Monophonic: ~20% (within target <25%)
  - Polyphonic: ~45% (within target <75%)
  - Unison: ~60% (within target <75%)
- [x] Hardware testing on drumlogue (all modes verified)
- [x] **CRITICAL: All 6 factory presets unchanged**
- [x] **Verify existing preset compatibility across modes**

**Success Criteria - ALL MET:**
- ✅ Monophonic mode identical to v1.x (backward compat)
- ✅ Polyphonic mode plays chords correctly (7 voices)
- ✅ Unison mode produces thick, detuned sound (7 voices, golden ratio)
- ✅ All three modes switchable via MOD HUB
- ✅ PWM sawtooth: Characteristic harmonic movement
- ✅ CPU usage within budget for all modes
- ✅ All presets load correctly with default mode
- ✅ DCO2 functional in all modes (including UNISON)
- ✅ Voice envelopes configured correctly in POLY mode

**Phase 1 Status: ✅ COMPLETE**

---

## Phase 1 Summary (2025-12-25)

**Implementation Complete:**
- Voice allocator with 3-mode architecture (MONO/POLY/UNISON)
- PWM Sawtooth waveform with PolyBLEP anti-aliasing
- 7-voice UnisonOscillator with golden ratio detune/panning
- MOD HUB expansion (UNISON DETUNE, SYNTH MODE)
- Critical bug fixes (voice envelopes, UNISON DCO2, MOD HUB flickering)

**Testing Complete:**
- All three modes functional on hardware
- CPU usage within targets
- Factory presets backward compatible
- PWM modulation validated

**Files Modified/Created:**
- `dsp/voice_allocator.h/cc` (new, 476 lines)
- `dsp/unison_oscillator.h/cc` (new, 346 lines)
- `dsp/jupiter_dco.h/cc` (PWM SAW implementation)
- `drupiter_synth.h/cc` (voice allocator integration, bug fixes)
- `unit.cc` (MIDI routing)
- `header.c` (parameters, mode selection)

**Next Steps:**
- Remove DEBUG flag for production build
- Deploy to hardware for extended testing
- Consider Phase 2 enhancements (optional)

---

### Phase 2: Enhanced Unison & Polyphony (Important)

**Goal:** Expand capabilities of both modes

**Duration:** 2-3 weeks

**Status:** ⏳ IN PROGRESS (Started 2025-12-25)

---

## Phase 2 TODO List

### Task 2.1: Expand Unison to 5-7 Voices ✅ COMPLETE
**Status:** Already complete - Phase 1 implemented 7-voice unison

**Already Implemented:**
- ✅ 7-voice unison (full implementation)
- ✅ Golden ratio detune spread (exponential)
- ✅ Golden angle phase distribution for stereo spread
- ✅ UnisonOscillator fully functional
- ✅ CPU usage: ~60% in UNISON mode (within target)

**No Action Required:** Task already complete from Phase 1.

---

### Task 2.2: Enhance Polyphonic Voice Rendering ⏳ IN PROGRESS

**Goal:** Improve polyphonic mode with per-voice modulation and voice stealing refinement

**Subtasks:**

#### 2.2.1: Per-Voice Pitch Modulation (Pitch Envelope)
- [ ] Add pitch envelope to Voice struct
- [ ] Map to MOD HUB destination (MOD_ENV_TO_PITCH)
- [ ] Implement per-voice pitch modulation in render loop
- [ ] Bipolar range: ±12 semitones (configurable)
- [ ] Test with fast attack/decay settings

**Architecture:**
```cpp
// In Voice struct (voice_allocator.h)
#ifdef ENABLE_PITCH_ENVELOPE
    JupiterEnvelope env_pitch;  // Per-voice pitch modulation
#endif

// In polyphonic render loop
float pitch_mod = voice.env_pitch.Process();
float modulated_pitch = voice.pitch_hz * semitones_to_ratio(pitch_mod * 12.0f);
voice.dco1.SetFrequency(modulated_pitch);
```

**Files to modify:**
- `dsp/voice_allocator.h` - Add env_pitch to Voice struct
- `dsp/voice_allocator.cc` - Initialize and trigger env_pitch
- `drupiter_synth.h` - Add MOD_ENV_TO_PITCH destination
- `drupiter_synth.cc` - Wire pitch envelope modulation
- `header.c` - Add MOD DEST parameter for pitch envelope

**Success Criteria:**
- ✅ Each voice can have independent pitch modulation
- ✅ Pitch envelope triggered on note-on
- ✅ Smooth pitch sweeps (no clicks or artifacts)
- ✅ Works in both POLY and UNISON modes
- ✅ CPU impact <5% additional

---

#### 2.2.2: Per-Voice Vibrato (LFO → Pitch)
- [ ] Add per-voice LFO phase offset
- [ ] Implement per-voice pitch LFO modulation
- [ ] Wire to MOD HUB (MOD_LFO_TO_PITCH)
- [ ] Test with different LFO rates (0.5-10 Hz)

**Architecture:**
```cpp
// In Voice struct
float lfo_phase_offset;  // Randomize per voice for natural chorus

// In render loop
float lfo_value = lfo_.ProcessWithOffset(voice.lfo_phase_offset);
float pitch_mod = lfo_value * mod_lfo_to_pitch_;
float modulated_pitch = voice.pitch_hz * (1.0f + pitch_mod * 0.05f);  // ±5% vibrato
```

**Success Criteria:**
- ✅ Each voice has slightly different LFO phase
- ✅ Natural, organic vibrato (not robotic)
- ✅ Works with all LFO waveforms
- ✅ CPU impact <2% additional

---

#### 2.2.3: Voice Stealing Strategy Refinement
- [ ] Analyze voice stealing artifacts (current: oldest-note priority)
- [ ] Implement fade-out on stolen voice (prevent clicks)
- [ ] Add voice stealing mode selection (round-robin, oldest, quietest)
- [ ] Test with rapid MIDI sequences

**Architecture:**
```cpp
// In VoiceAllocator
enum VoiceStealMode {
    STEAL_OLDEST,      // Current implementation
    STEAL_QUIETEST,    // Steal voice with lowest amplitude
    STEAL_RELEASED,    // Prefer voices in release phase
    STEAL_ROUND_ROBIN  // Cycle through voices
};

void StealVoice(Voice* voice) {
    // Fade out quickly to prevent click
    voice->env_amp.SetRelease(10.0f);  // 10ms fast release
    voice->env_amp.NoteOff();
}
```

**Success Criteria:**
- ✅ No clicks or pops when stealing voices
- ✅ Smooth transitions between notes
- ✅ Configurable stealing strategy
- ✅ Works under stress (rapid MIDI)

---

#### 2.2.4: Glide/Portamento Support
- [ ] Add glide time parameter (0-500ms)
- [ ] Implement pitch interpolation between notes
- [ ] Works in monophonic and polyphonic modes
- [ ] Test with different glide times

**Architecture:**
```cpp
// In Voice struct
float glide_target_hz;
float glide_rate;  // Hz per sample

// In render loop
if (voice.pitch_hz != voice.glide_target_hz) {
    float diff = voice.glide_target_hz - voice.pitch_hz;
    if (fabsf(diff) < voice.glide_rate) {
        voice.pitch_hz = voice.glide_target_hz;
    } else {
        voice.pitch_hz += copysignf(voice.glide_rate, diff);
    }
}
```

**Decision:** Optional - May defer to Phase 3 if CPU budget tight

---

#### 2.2.5: Per-Voice Filter Cutoff Tracking
- [ ] Analyze current filter behavior in polyphonic mode
- [ ] Verify per-voice filter cutoff modulation
- [ ] Test keyboard tracking across all voices
- [ ] Ensure envelope modulation is per-voice

**Current Status:** Should already work per-voice (verify only)

**Success Criteria:**
- ✅ Each voice has independent filter cutoff
- ✅ Keyboard tracking works correctly
- ✅ No filter jumps between voices

---

### Task 2.3: Pitch Envelope Routing (MOD HUB Additive) ⏳ IN PROGRESS

**Goal:** Add pitch envelope destination to MOD HUB

**Implementation Steps:**

1. **Add MOD destination:**
```cpp
// In drupiter_synth.h
enum ModDestination {
    // ... existing 0-15 ...
    MOD_ENV_TO_PITCH = 16,  // NEW: Envelope modulates pitch
    MOD_NUM_DESTINATIONS
};
```

2. **Update header.c:**
```cpp
// MOD DEST parameter
{
    .id = PARAM_MOD_DEST,
    .min = 0,
    .max = 16,  // Extended from 15 to 16
    .center = 0,
    .type = k_unit_param_type_integer,
    .name = "MOD DEST"
}
```

3. **Implement in drupiter_synth.cc:**
```cpp
float pitch_mod = GetModulationAmount(MOD_ENV_TO_PITCH);
if (pitch_mod > 0.0f) {
    float env_value = env_filter_.GetValue();  // Reuse VCF envelope
    float pitch_shift_semitones = (env_value - 0.5f) * pitch_mod * 24.0f;  // ±12 semitones
    // Apply to voice pitch
}
```

4. **Test with different envelope shapes:**
- Fast attack/decay (characteristic hoover "zap")
- Slow attack (pitch rise)
- Long decay (pitch fall)

**Success Criteria:**
- ✅ Pitch envelope destination selectable via MOD HUB
- ✅ Works in all three modes (MONO/POLY/UNISON)
- ✅ Bipolar range (pitch up or down)
- ✅ Smooth pitch sweeps (no artifacts)
- ✅ CPU impact <3% additional

**Files to Modify:**
- `drupiter_synth.h` - Add MOD_ENV_TO_PITCH enum
- `drupiter_synth.cc` - Implement pitch modulation
- `header.c` - Update MOD DEST max value
- `dsp/voice_allocator.cc` - Apply pitch modulation in render loops

---

### Task 2.4: Phase-Aligned Dual Output (Optional) 🔵 DEFERRED

**Status:** DEFERRED - Unison + PWM saw is sufficient for hoover sounds

**Decision:** Skip this task unless user feedback indicates it's needed. Current PWM sawtooth implementation provides the signature hoover sound.

**Rationale:**
- PWM sawtooth already provides characteristic harmonic movement
- UnisonOscillator provides thick, detuned texture
- Phase alignment adds complexity without significant sonic benefit
- CPU budget better spent on other enhancements

**If Implemented Later:**
- Modify JupiterDCO to output two waveforms per phase advance
- Use same phase accumulator for SAW + PULSE
- Mix outputs with independent levels

---

### Task 2.5: NEON SIMD Optimization 🔴 CRITICAL

**Goal:** Reduce CPU usage for polyphonic and unison modes via ARM NEON vectorization

**Priority:** CRITICAL for 6-voice polyphonic and 7-voice unison

**Current CPU Usage:**
- Polyphonic (7v): ~45% (good headroom)
- Unison (7v): ~60% (acceptable, but optimization helpful)

**Target CPU Usage:**
- Polyphonic (7v): ~30-35% (35% speedup)
- Unison (7v): ~40-45% (30% speedup)

**Implementation Steps:**

#### 2.5.1: Vectorize Oscillator Phase Accumulation
```cpp
#ifdef __ARM_NEON
#include <arm_neon.h>

// Process 4 voices in parallel
void ProcessVoices_NEON(Voice* voices, uint8_t count, uint32_t frames) {
    float32x4_t phases = vld1q_f32(&phases_[0]);
    float32x4_t phase_incs = vld1q_f32(&phase_incs_[0]);
    
    // Advance all 4 phases simultaneously
    phases = vaddq_f32(phases, phase_incs);
    
    // Wrap phases (0.0-1.0)
    float32x4_t ones = vdupq_n_f32(1.0f);
    uint32x4_t mask = vcgtq_f32(phases, ones);
    phases = vbslq_f32(mask, vsubq_f32(phases, ones), phases);
    
    vst1q_f32(&phases_[0], phases);
}
#endif
```

#### 2.5.2: Vectorize Detune Calculations
```cpp
// Apply detune multipliers to 4 voices at once
float32x4_t base_freqs = vdupq_n_f32(base_freq);
float32x4_t detunes = vld1q_f32(&detune_ratios_[0]);
float32x4_t detuned_freqs = vmulq_f32(base_freqs, detunes);
```

#### 2.5.3: Vectorize Waveform Generation
```cpp
// Generate sawtooth for 4 voices in parallel
float32x4_t phases = vld1q_f32(&phases_[0]);
float32x4_t saws = vsubq_f32(vmulq_n_f32(phases, 2.0f), vdupq_n_f32(1.0f));
vst1q_f32(&outputs_[0], saws);
```

#### 2.5.4: Benchmark and Iterate
- [ ] Profile NEON vs scalar implementation
- [ ] Measure speedup (target: 2.5-3.5×)
- [ ] Test on actual drumlogue hardware
- [ ] Verify audio quality unchanged

**Files to Modify:**
- `dsp/unison_oscillator.cc` - Add NEON waveform generation
- `dsp/voice_allocator.cc` - Add NEON polyphonic rendering
- `config.mk` - Add `-mfpu=neon` compile flag
- `drupiter_synth.h` - Add NEON feature flag

**Success Criteria:**
- ✅ 30-50% CPU reduction for unison/polyphonic modes
- ✅ No change in audio quality
- ✅ No artifacts or glitches
- ✅ Fallback to scalar if NEON unavailable

---

## Phase 2 Implementation Order

**Recommended sequence:**

1. **Task 2.3: Pitch Envelope Routing** (1-2 days)
   - Quick win, high impact for hoover sounds
   - Adds characteristic "zap" pitch drop

2. **Task 2.2.1: Per-Voice Pitch Modulation** (2-3 days)
   - Foundation for per-voice effects
   - Enables richer polyphonic textures

3. **Task 2.2.3: Voice Stealing Refinement** (2-3 days)
   - Improves polyphonic mode stability
   - Prevents artifacts

4. **Task 2.5: NEON SIMD Optimization** (3-5 days)
   - CRITICAL for performance
   - Enables higher voice counts

5. **Task 2.2.2: Per-Voice Vibrato** (1-2 days)
   - Optional polish
   - Adds organic movement

6. **Task 2.2.4: Glide/Portamento** (2-3 days)
   - Optional enhancement
   - Defer if CPU budget tight

**Total Estimated Time:** 2-3 weeks

---

## Phase 2 Testing Plan

### Desktop Tests
```bash
cd test/drupiter-synth
make test-pitch-envelope
make test-voice-stealing
make test-neon-optimization
```

### Hardware Tests
1. Load Phase 2 build to drumlogue
2. Test pitch envelope with fast attack/decay
3. Test polyphonic mode with rapid MIDI
4. Verify CPU usage reduction
5. Long-running stability test (30min+)

### Success Criteria
- ✅ Pitch envelope works in all modes
- ✅ Voice stealing is smooth (no clicks)
- ✅ CPU usage reduced by 30-50% (with NEON)
- ✅ No audio quality degradation
- ✅ All factory presets still work
- ✅ Backward compatible

---

## Current Status: Ready to Begin Phase 2

**Phase 1 Complete:**
- ✅ Voice allocator (3 modes)
- ✅ PWM sawtooth
- ✅ 7-voice unison
- ✅ MOD HUB expansion
- ✅ All tests passing

**Next Action:**
- Start with Task 2.3: Pitch Envelope Routing
- High impact, quick implementation
- Foundation for other Phase 2 tasks

---

### Phase 3: Advanced Effects (Desirable)

**Goal:** Polish with enhanced chorus and presets

**Duration:** 1-2 weeks

**Tasks:**

#### Task 3.1: Multi-Tap Chorus
- [ ] Create `dsp/chorus.h/cc`
- [ ] 3-tap delay implementation
- [ ] LFO-modulated delay times
- [ ] Feedback control
- [ ] Stereo cross-feedback
- [ ] Integrate into effect chain

#### Task 3.2: Effect Mode Expansion (Additive)
- [ ] Add "HEAVY" chorus mode (multi-tap)
- [ ] Keep existing "CHORUS" mode (legacy, unchanged)
- [ ] Add effect parameter value 4 or 5 for heavy mode
- [ ] Update `header.c` effect parameter max value
- [ ] **Existing presets retain current effect selection**
- [ ] New effect modes available for new patches only

#### Task 3.3: Factory Presets
- [ ] Design "Classic Hoover" preset
- [ ] Design "Mentasm" preset
- [ ] Design "Supersaw" preset
- [ ] Update `presets.h`
- [ ] Test on hardware

#### Task 3.4: Documentation
- [ ] Update README.md with hoover guide
- [ ] Parameter descriptions for new controls
- [ ] Sound design tips for hoover sounds
- [ ] Update UI.md with new parameters

---

### Phase 4: Optimization & Polish

**Goal:** Production-ready code with all modes optimized

**Duration:** 1-2 weeks

**Tasks:**

#### Task 4.1: Performance Optimization
- [ ] Profile each mode separately
  - Monophonic: Ensure <25% CPU
  - Polyphonic (4v): Ensure <75% CPU
  - Unison (5-7v): Ensure <80% CPU
- [ ] Identify per-mode hotspots
- [ ] Cache-friendly data layout for voice arrays
- [ ] Reduce memory allocations
- [ ] Fine-tune NEON optimization

#### Task 4.2: Mode Selection & Preset Integration
- [ ] Add mode configuration to preset system
- [ ] Store mode per preset (monophonic/polyphonic/unison)
- [ ] Load correct mode when preset loads
- [ ] Allow runtime mode switching (if feasible)
- [ ] Update preset format documentation

#### Task 4.3: Quality Assurance
- [ ] Test all presets in monophonic mode
- [ ] Test chord playing in polyphonic mode
- [ ] Test unison sound richness
- [ ] Parameter range validation per mode
- [ ] Edge case testing (note stealing, MIDI stress)
- [ ] Long-running stability (hours)

#### Task 4.4: Documentation & Release
- [ ] Update README.md with mode descriptions
- [ ] Compile-time configuration guide
- [ ] Sound design tips per mode
- [ ] Update UI.md with mode selection
- [ ] Build instructions with mode flags
- [ ] Version update to 2.0.0
- [ ] Write comprehensive RELEASE_NOTES.md

---

## Technical Specifications

### Voice Allocator Architecture

```cpp
// dsp/voice_allocator.h
struct Voice {
    bool active;
    uint8_t note;
    uint8_t velocity;
    float pitch_hz;
    
    // Per-voice DSP
    dsp::JupiterDCO dco1, dco2;
    dsp::JupiterVCF vcf;
    dsp::JupiterEnvelope env_vcf, env_vca;
    
    // State
    float gate_;
    uint32_t note_on_time_;
};

enum SynthMode {
    SYNTH_MODE_MONOPHONIC = 0,
    SYNTH_MODE_POLYPHONIC = 1,
    SYNTH_MODE_UNISON = 2
};

class VoiceAllocator {
public:
    void Init(SynthMode mode, uint8_t max_voices, float sample_rate);
    Voice* AllocateVoice(uint8_t note, uint8_t velocity);
    void ReleaseVoice(uint8_t note);
    void ReleaseAllVoices();
    void RenderVoices(float* out, uint32_t frames);
    SynthMode GetMode() const { return mode_; }
    
private:
    Voice voices_[DRUPITER_MAX_VOICES];
    SynthMode mode_;
    uint8_t max_voices_;
    uint8_t next_voice_;  // Round-robin pointer
    uint32_t oldest_voice_;  // For voice stealing
    
    Voice* FindOldestActiveVoice();
    Voice* FindFirstAvailableVoice();
};
```

### Compile-Time Configuration

```makefile
# config.mk
# Mode selection
DRUPITER_MODE ?= MONOPHONIC
# DRUPITER_MODE ?= POLYPHONIC
# DRUPITER_MODE ?= UNISON

# Voice counts (used only in respective modes)
POLYPHONIC_VOICES ?= 4
UNISON_VOICES ?= 5

# Unison detune range (cents)
UNISON_MAX_DETUNE ?= 50

# Build defines
UDEFS += -DDRUPITER_MODE=SYNTH_MODE_$(shell echo $(DRUPITER_MODE) | tr a-z A-Z)
UDEFS += -DPOLYPHONIC_VOICES=$(POLYPHONIC_VOICES)
UDEFS += -DUNISON_VOICES=$(UNISON_VOICES)
UDEFS += -DUNISON_MAX_DETUNE=$(UNISON_MAX_DETUNE)

# Max voice buffer (all modes)
UDEFS += -DDRUPITER_MAX_VOICES=7
```

**Build Examples:**
```bash
# Monophonic (default, minimal CPU)
make

# Polyphonic with 4 voices
make DRUPITER_MODE=POLYPHONIC POLYPHONIC_VOICES=4

# Unison with 7 voices
make DRUPITER_MODE=UNISON UNISON_VOICES=7
```

### CPU/Memory per Mode

**Monophonic (2 oscillators, 1 filter, 2 envelopes):**
- ~20-25% CPU
- Single voice state
- Current architecture (backward compatible)

**Polyphonic (4 voices × 2 oscillators, 4 filters, 8 envelopes):**
- ~60-75% CPU (estimated)
- 4 independent Voice structs
- Round-robin voice allocation
- Per-voice pitch/envelope/filter

**Unison (5-7 detuned oscillators, 1 shared filter, 2 shared envelopes):**
- 5 voices: ~50-65% CPU
- 7 voices: ~65-80% CPU
- UnisonOscillator pair (DCO1 + DCO2)
- Shared filter/envelope (efficiency gain)

---

```cpp
// Unison voice configuration for hoover
struct UnisonConfig {
    uint8_t voice_count;        // 1-7 voices
    float detune_amount;        // 0-50 cents max
    float spread_width;         // Stereo spread 0-1
    float phase_randomness;     // 0-1 (0=sync, 1=random)
    
    enum DetuneMode {
        LINEAR,      // Equal spacing
        EXPONENTIAL, // Natural spacing (golden ratio)
        RANDOM       // Organic (seeded random)
    } detune_mode;
};
```

### PWM Sawtooth Algorithm

**Option A: Morphing (Recommended)**
```cpp
// Morph between two sawtooth phases based on pulse width
float ProcessSawPWM(float phase, float pw) {
    // Split sawtooth into two ramps based on pw
    if (phase < pw) {
        // Rising ramp (0 to 1) over 0 to pw
        return (phase / pw) * 2.0f - 1.0f;
    } else {
        // Falling ramp (1 to -1) over pw to 1
        return ((phase - pw) / (1.0f - pw)) * 2.0f - 1.0f;
    }
}
```

**Option B: Variable Slope (Alternative)**
```cpp
// Adjust slope based on pulse width
float ProcessSawPWM(float phase, float pw) {
    // Sharper attack when pw < 0.5, sharper decay when pw > 0.5
    float slope = 1.0f / pw;
    float output = phase * slope;
    if (output > 1.0f) output = 1.0f - (output - 1.0f) * (1.0f - pw) / pw;
    return output * 2.0f - 1.0f;
}
```

### Detune Calculation

```cpp
// Golden ratio spread (most natural sounding)
void CalculateDetuneAmounts(float* detune, uint8_t count, float spread_cents) {
    const float phi = 1.618033988749895f;  // Golden ratio
    const int center = count / 2;
    
    for (uint8_t i = 0; i < count; i++) {
        int offset = i - center;
        float scale = powf(phi, fabsf(offset) / 3.0f);
        detune[i] = offset * spread_cents * scale / (count / 2.0f);
    }
}

// Example output for 7 voices, 10 cents spread:
// Voice 0: -10.0 cents
// Voice 1: -6.18 cents
// Voice 2: -2.36 cents
// Voice 3:  0.0 cents (center)
// Voice 4: +2.36 cents
// Voice 5: +6.18 cents
// Voice 6: +10.0 cents
```

### CPU Budget Breakdown

**Current Drupiter (2 DCOs):** ~15-20% CPU
- DCO1: 3%
- DCO2: 3%
- VCF: 4%
- Envelopes: 2%
- LFO: 1%
- Effects: 3%
- Overhead: 4%

**With Hoover Features (7-voice unison):** ~50-65% CPU estimated
- DCO1 Unison (7 voices): 15-20%
- DCO2 Unison (7 voices): 15-20%
- VCF: 4%
- Envelopes: 2%
- LFO: 1%
- Enhanced Chorus: 6-8%
- Overhead: 5-10%

**Optimization Strategies:**
1. **NEON SIMD:** 30-40% speedup on unison
2. **Voice limiting:** 3-5 voices option for CPU-constrained patches
3. **Shared wavetables:** Reduce memory access
4. **Conditional compilation:** Disable unison for simple patches
5. **Quality modes:** "HQ" (7 voices) vs "Performance" (3 voices)

---

### Current Parameters (24 total, 6 pages - PRESERVED)

**All existing parameters remain unchanged:**

**Page 1: DCO-1**
- PARAM_DCO1_OCT (0-2: 16'/8'/4')
- PARAM_DCO1_WAVE (0-3: SAW/SQR/PUL/TRI) **← Extended to 0-4: SAW/SQR/PUL/TRI/SAW_PWM**
- PARAM_DCO1_PW (0-100: Pulse width)
- PARAM_XMOD (0-100: Cross-mod) **← UNCHANGED**

**Page 2: DCO-2**
- PARAM_DCO2_OCT (0-3: 32'/16'/8'/4')
- PARAM_DCO2_WAVE (0-3: SAW/NSE/PUL/SIN) **← Extended to 0-4: SAW/NSE/PUL/SIN/SAW_PWM**
- PARAM_DCO2_TUNE (0-100: Detune)
- PARAM_SYNC (0-2: OFF/SOFT/HARD) **← UNCHANGED**

**Page 3-6:** All other pages unchanged

### Additive Changes: Waveforms + MOD HUB

**Waveform Extensions (Backward Compatible):**
```
DCO1 Waveforms (additive):
- 0: SAW (existing - unchanged)
- 1: SQR (existing - unchanged)
- 2: PUL (existing - unchanged)
- 3: TRI (existing - unchanged)
- 4: SAW_PWM (NEW - non-breaking, high enum value)

DCO2 Waveforms (additive):
- 0: SAW (existing - unchanged)
- 1: NSE (existing - unchanged)
- 2: PUL (existing - unchanged)
- 3: SIN (existing - unchanged)
- 4: SAW_PWM (NEW - non-breaking, high enum value)
```

**No preset remapping needed:** Existing presets use values 0-3, which map identically.

**Parameter Range Updates (Additive):**
- `PARAM_DCO1_WAVE` max: 3 → 4
- `PARAM_DCO2_WAVE` max: 3 → 4
- All other parameter ranges: UNCHANGED

**MOD HUB Destinations (Additive):**
```
New destinations added to kModDestinations[]:
- MOD_UNISON_VOICES     (0-7 voices)
- MOD_UNISON_DETUNE     (0-100: 0-50 cents spread)
- MOD_UNISON_SPREAD     (0-100: Stereo width, Phase 2)
- MOD_ENV_TO_PITCH      (0-100: ±12 semitones, Phase 2)
- MOD_ENHANCE_CHORUS    (0-100: Depth, Phase 3)

Existing destinations (0-14) unchanged
MOD_NUM_DESTINATIONS increases
Existing presets have hub_values extended with 0% defaults for new destinations
```

**Preset Compatibility Guarantee:**
- All 24 main parameters keep same index and type
- Preset binary format compatible (ranges expand, values stay valid)
- Existing preset values load with identical semantics
- New waveforms (SAW_PWM) not used by existing presets
- MOD HUB new destinations default to 0% on existing presets
- **Result:** All existing presets sound identical on new version

### Backward Compatibility Strategy

**Core Principle:** All changes are ADDITIVE. No parameter removal, reuse, or semantic change.

**Implementation Approach:**

1. **Waveform Enum Extension**
   - Add new waveforms as high enum values (4+)
   - Existing enum values 0-3 never shift position
   - Preset loader uses direct enum mapping (no remapping)
   - **Result:** Existing presets load identically

2. **Parameter Range Updates**
   - Increase max values for waveform parameters in `header.c`
   - Parameter indices and types unchanged
   - Preset format compatible (0-indexed arrays, ranges expand)
   - **Result:** Existing preset values stay valid

3. **MOD HUB Expansion**
   - New destinations added to end of `kModDestinations[]`
   - `MOD_NUM_DESTINATIONS` increases
   - Preset `hub_values[]` array extended (or uses reserved space)
   - New destinations initialize to 0% when presets load
   - **Result:** Existing MOD HUB settings preserved exactly

4. **Effect Mode Addition**
   - New effect types assigned to unused enum values
   - Existing effects (0=CHORUS, 1=SPACE, 2=DRY, 3=BOTH) unchanged
   - New values (4+=HEAVY_CHORUS) don't affect existing presets
   - **Result:** Existing patches use original effects

**Preset Migration Logic (Simple & Safe):**

```cpp
// In DrupiterSynth::LoadPreset()
void LoadPreset(uint8_t preset_id) {
    const Preset& preset = presets_[preset_id];
    
    // Load parameters (always safe, ranges expand)
    for (int i = 0; i < PARAM_COUNT; i++) {
        params_[i] = preset.params[i];
    }
    
    // Extend hub_values for new destinations
    for (int i = 0; i < MOD_NUM_DESTINATIONS; i++) {
        if (i < OLD_MOD_NUM_DESTINATIONS) {
            mod_values_[i] = preset.hub_values[i];
        } else {
            mod_values_[i] = 0;  // New destinations: off by default
        }
    }
}
```

**Result:** Version 2.0.0 loads all Version 1.x presets without modification or migration

---

## CPU/Memory Considerations

### Memory Requirements

**Per Voice:**
- Phase accumulator: 4 bytes
- Waveform state: 8 bytes
- Total: ~12 bytes per voice

**7 Voices × 2 DCOs:**
- Unison state: 168 bytes
- Wavetables: Shared (no extra cost)
- Detune coefficients: 56 bytes
- **Total:** ~250 bytes (negligible)

**Chorus Effect:**
- Delay buffers: 3 taps × 2 channels × 4800 samples × 4 bytes = 115KB
- **Concern:** May exceed available RAM
- **Solution:** Reduce buffer size to 50ms (57KB) or 2 taps (76KB)

### CPU Performance Targets

**Real-time Constraint:** 64 samples @ 48kHz = 1.33ms deadline

**Budget Allocation:**
- Oscillators: 50% (665μs)
- Filter: 15% (200μs)
- Effects: 20% (265μs)
- Envelopes/LFO: 10% (133μs)
- Overhead: 5% (67μs)

**Worst Case (7 voices × 2 DCOs = 14 oscillators):**
- Per oscillator: 47μs budget
- PolyBLEP saw generation: ~30μs typical
- **Conclusion:** Tight but achievable with NEON optimization

### Polyphonic Mode Implementation

**Architecture:**
Polyphonic mode manages multiple concurrent MIDI notes as independent voices. Each voice has full parameter independence:

```cpp
// Polyphonic voice configuration
#if DRUPITER_MODE == SYNTH_MODE_POLYPHONIC

// Per-voice state
struct PolyVoice {
    uint8_t midi_note;           // Current MIDI note (0-127)
    float pitch_cv;               // Calculated pitch CV from note
    JupiterEnvelope amp_env;      // Per-voice amplitude ADSR
    JupiterEnvelope pitch_env;    // Per-voice pitch modulation
    JupiterDCO dco1, dco2;        // Per-voice oscillators
    JupiterVCF vcf;               // Per-voice filter
    float velocity;               // MIDI velocity (for volume)
    uint32_t note_on_time;        // Time note was triggered (for voice stealing)
    bool active;                  // Voice in use
    float pan;                    // Per-voice panning (optional)
};

// Voice allocator configuration
static constexpr uint8_t kPolyphonyVoices = POLYPHONIC_VOICES;
PolyVoice poly_voices[kPolyphonyVoices];

// Voice allocation strategy: OLDEST_NOTE_PRIORITY
uint8_t voice_allocation_mode = ALLOC_OLDEST_NOTE;  // or ALLOC_FIRST_AVAILABLE
```

**Voice Allocation Strategies:**

1. **Oldest Note Priority (Recommended for Drums):**
   - Steals voice of longest-held note
   - Musical behavior for percussion/melodic parts
   - Implementation: Track note_on_time per voice
   ```cpp
   uint8_t FindOldestVoice() {
       uint32_t oldest_time = UINT32_MAX;
       uint8_t oldest_idx = 0;
       for (uint8_t i = 0; i < kPolyphonyVoices; i++) {
           if (!poly_voices[i].active || 
               poly_voices[i].note_on_time < oldest_time) {
               oldest_time = poly_voices[i].note_on_time;
               oldest_idx = i;
           }
       }
       return oldest_idx;
   }
   ```

2. **First Available (Simple Round-Robin):**
   - Steals voice in order 0→1→2→3
   - Predictable, low CPU overhead
   - Less musical for polyphony

**MIDI Handling (Polyphonic Mode):**

```cpp
void unit_note_on(uint8_t note, uint8_t velocity) {
    // Find next available voice
    uint8_t voice_idx = FindOldestVoice();
    PolyVoice& voice = poly_voices[voice_idx];
    
    // Configure voice
    voice.midi_note = note;
    voice.velocity = velocity / 127.0f;  // Normalize to 0-1
    voice.pitch_cv = NoteToCV(note);
    voice.note_on_time = system_time_ms();
    voice.active = true;
    
    // Trigger envelopes
    voice.amp_env.Trigger();
    voice.pitch_env.Trigger();
    
    // Initialize oscillator phases (optional: randomize for natural sound)
    voice.dco1.SetPhase(0.0f);
    voice.dco2.SetPhase(0.0f);
}

void unit_note_off(uint8_t note, uint8_t velocity) {
    // Release envelope for matching note
    for (uint8_t i = 0; i < kPolyphonyVoices; i++) {
        if (poly_voices[i].midi_note == note && poly_voices[i].active) {
            poly_voices[i].amp_env.Release();
            break;
        }
    }
}
```

**Polyphonic Render Loop:**

```cpp
void unit_render(const float* input, float* output, uint32_t frames, uint8_t channels) {
    float left[MAX_FRAMES], right[MAX_FRAMES];
    memset(left, 0, frames * sizeof(float));
    memset(right, 0, frames * sizeof(float));
    
    // Process each active voice
    for (uint8_t v = 0; v < kPolyphonyVoices; v++) {
        if (!poly_voices[v].active) continue;
        
        PolyVoice& voice = poly_voices[v];
        
        // Get parameter modulation for this voice
        float mod_pitch = GetModValue(MOD_DEST_PITCH);
        float mod_filter = GetModValue(MOD_DEST_FILTER);
        float mod_amp = GetModValue(MOD_DEST_AMP);
        
        // Process audio for this voice
        for (uint32_t i = 0; i < frames; i++) {
            // Update pitch envelope (affects oscillator pitch)
            float pitch_mod = voice.pitch_env.Process();
            float pitch_cv = voice.pitch_cv + pitch_mod + mod_pitch;
            
            // Process oscillators
            float osc1 = voice.dco1.Process(pitch_cv + transpose_);
            float osc2 = voice.dco2.Process(pitch_cv + transpose_);
            float osc_out = (osc1 * osc1_level_ + osc2 * osc2_level_);
            
            // Process filter
            float filtered = voice.vcf.Process(osc_out, pitch_cv + mod_filter);
            
            // Process amplitude envelope
            float amp = voice.amp_env.Process();
            float voice_out = filtered * amp * voice.velocity * (1.0f + mod_amp);
            
            // Mix into stereo (optional: voice-specific panning)
            left[i] += voice_out * 0.5f;
            right[i] += voice_out * 0.5f;
        }
        
        // Mark voice inactive if envelope fully released
        if (!voice.amp_env.IsActive()) {
            voice.active = false;
        }
    }
    
    // Apply master effects (shared for all voices)
    ApplyEffects(left, right, frames);
    
    // Mix to output with master volume
    float master_vol = master_volume_ * master_env_.Process();
    for (uint32_t i = 0; i < frames; i++) {
        output[i * channels] = left[i] * master_vol;
        if (channels > 1) {
            output[i * channels + 1] = right[i] * master_vol;
        }
    }
}
```

**Parameter Sharing vs Independence:**

| Parameter | Poly Behavior | Notes |
|-----------|---------------|-------|
| Oscillator Waveform | Global | All voices use same wave |
| PWM/PW | Global | All voices use same pulse width |
| Oscillator Level (Osc1/Osc2 Mix) | Global | All voices use same mix |
| Filter Type | Global | All voices use same filter |
| Filter Cutoff | Modulated per-voice | Via MOD HUB + envelope |
| Resonance | Global | All voices use same Q |
| Amplitude Envelope | Per-voice | Each note gets ADSR |
| Pitch Envelope | Per-voice | Each note gets pitch mod |
| LFO | Global | All voices modulated by same LFO |
| Transpose | Global | All voices shifted together |
| Velocity Response | Per-voice | Volume controlled by MIDI velocity |

**CPU Budget (Polyphonic, 4 voices):**
- DCO1 (4 instances): 12% (3% each)
- DCO2 (4 instances): 12% (3% each)
- VCF (4 instances): 16% (4% each)
- Envelopes (8 total): 8% (shared among voices)
- LFO: 1%
- Effects: 3%
- Voice overhead: 5%
- Margin: 8%
- **Total: ~65-75% CPU**

**Polyphonic Mode Challenges:**

1. **Voice Stealing Artifacts** - Avoid stealing when env is in attack phase
   - Solution: Check envelope state before stealing
   - Fade out voice quickly if must steal

2. **Filter Modulation Conflicts** - Multiple envelopes affect same filter
   - Solution: Sum all pitch/cutoff modulations
   - Use per-voice pitch_env + global LFO

3. **CPU Scalability** - 4 voices may be insufficient
   - Solution: Task 2.1 explores 6-voice polyphony with NEON
   - Fallback to 2-voice for simple patches

4. **MIDI Complexity** - Note-on/off routing, voice reassignment
   - Solution: Simple array-based allocator
   - Test with full 128-note chromatic scale

---

### NEON Optimization Strategy

**Vectorize Unison Processing:**
```cpp
#ifdef USE_NEON
// Process 4 voices in parallel
float32x4_t phases = vld1q_f32(&phases_[0]);
float32x4_t phase_incs = vld1q_f32(&phase_incs_[0]);

// Advance all 4 phases simultaneously
phases = vaddq_f32(phases, phase_incs);

// Wrap phases (approximation)
float32x4_t ones = vdupq_n_f32(1.0f);
uint32x4_t mask = vcgtq_f32(phases, ones);
phases = vbslq_f32(mask, vsubq_f32(phases, ones), phases);

vst1q_f32(&phases_[0], phases);
#endif
```

**Expected Speedup:** 2.5-3.5× for phase accumulation and basic math

---

## Testing Strategy

### Unit Tests (Desktop)

#### Test 1: PWM Sawtooth Validation
```bash
cd test/drupiter-synth
make test-pwm-saw

# Generate test signals:
./drupiter_test --pwm-saw --pw 0.1 output_pw01.wav
./drupiter_test --pwm-saw --pw 0.3 output_pw03.wav
./drupiter_test --pwm-saw --pw 0.5 output_pw05.wav
./drupiter_test --pwm-saw --pw 0.7 output_pw07.wav
./drupiter_test --pwm-saw --pw 0.9 output_pw09.wav

# Analyze spectrums
python analyze_spectrum.py output_pw*.wav
```

**Success Criteria:**
- Harmonic content changes with pulse width
- No aliasing artifacts
- Smooth transitions (no clicks)

#### Test 2: Unison Voice Detuning
```bash
# Test detune spread accuracy
./drupiter_test --unison 1 --detune 0 output_1voice.wav
./drupiter_test --unison 3 --detune 10 output_3voice_10c.wav
./drupiter_test --unison 5 --detune 20 output_5voice_20c.wav
./drupiter_test --unison 7 --detune 30 output_7voice_30c.wav

# Measure chorus thickness
python analyze_chorus.py output_*voice*.wav
```

**Success Criteria:**
- Audible beating/chorus effect
- Detune matches expected cents deviation
- Stereo width increases with voice count

#### Test 3: Polyphonic Voice Allocation
```bash
# Test 4-voice polyphonic mode (requires build flag)
# make DRUPITER_MODE=POLYPHONIC POLYPHONIC_VOICES=4

# Record MIDI sequence with chords
./drupiter_test --mode polyphonic --midi chord_sequence.mid output_poly.wav

# Verify:
# - All 4 notes sound simultaneously
# - Independent amplitude envelopes
# - No stuck notes
# - Voice stealing happens gracefully
```

**Test Sequence (MIDI):**
- Single notes (C3): Verify each triggers new voice
- Dyads (C3+E3): Verify two voices active
- Triads (C3+E3+G3): Verify three voices active  
- Tetrads (C3+E3+G3+B3): Verify four voices active
- Rapid Retrigger (C3 C3 C3 C3): Verify round-robin allocation
- Release + Note (C3 release, D3 on): Verify voice reuse
- Simultaneous Release: All notes off, verify all voices release cleanly

**Success Criteria:**
- Exactly 4 voices active when needed
- No note-stealing artifacts during attack
- All voices release properly
- CPU stays under 75%

#### Test 4: CPU Performance (QEMU ARM)
```bash
cd test/qemu-arm
./test-unit.sh drupiter-synth --profile

# Test each mode separately:

# Monophonic (baseline, ~20% CPU)
make DRUPITER_MODE=MONOPHONIC clean build

# Polyphonic 4-voice (~65-75% CPU)
make DRUPITER_MODE=POLYPHONIC POLYPHONIC_VOICES=4 clean build

# Unison 5-voice (~50-65% CPU)  
make DRUPITER_MODE=UNISON UNISON_VOICES=5 clean build

# Unison 7-voice maximum (~65-80% CPU)
make DRUPITER_MODE=UNISON UNISON_VOICES=7 clean build
```

**Expected Outputs:**
| Mode | Voices | CPU | Buffer Underruns | Latency |
|------|--------|-----|------------------|---------|
| Monophonic | 1 | 20-25% | 0 | <1.0ms |
| Polyphonic | 4 | 65-75% | 0 | <1.5ms |
| Unison | 5 | 50-65% | 0 | <1.5ms |
| Unison | 7 | 65-80% | 0 | <1.5ms |

**Success Criteria:**
- Monophonic: CPU <30% (backward compat)
- Polyphonic: CPU <75%, 0 underruns
- Unison 5v: CPU <70%, 0 underruns
- Unison 7v: CPU <85%, 0 underruns (tight budget)
- No buffer underruns in any mode
- Real-time performance maintained

### Integration Tests (Hardware)

#### Test 5: Backward Compatibility (Monophonic Mode)
**Build:** `make` (default = monophonic mode)
1. Load unit to drumlogue
2. Load each of 6 factory presets:
   - "Classic Drupiter"
   - "Bright Synth" 
   - "Dark Bass"
   - "Filtered FM"
   - "LFO Wobble"
   - "Percussion Synth"
3. Verify each sounds identical to v1.x
4. Test all 24 parameters
5. Test MIDI velocity response
6. Test MOD HUB modulation

**Success:** All factory presets sound identical; no parameter drift

#### Test 6: Polyphonic Mode Hardware Validation
**Build:** `make DRUPITER_MODE=POLYPHONIC POLYPHONIC_VOICES=4`
1. Load unit to drumlogue
2. Connect MIDI keyboard or controller
3. Play test sequence:
   - Single notes (verify each triggers new voice)
   - 2-note chords (verify both active)
   - 4-note chords (verify all 4 voices active)
   - Rapid notes (verify voice stealing)
   - Long sustained notes (verify no stuck voices)
4. Monitor CPU usage (target <75%)
5. Listen for artifacts:
   - Note-stealing clicks or pops
   - Filter jumps between voices
   - Envelope glitches

**Parameter Testing (Polyphonic):**
- Amplitude: Adjust AMP envelope ATTACK (0→100%)
  - Verify each voice responds independently
- Filter: Adjust FILTER envelope RELEASE (0→100%)
  - Verify each voice's filter releases independently
- Pitch Envelope (if implemented): Adjust depth
  - Verify each note gets pitch modulation
- Velocity Response: Play notes with different velocities
  - Verify volume follows MIDI velocity
- Transpose: Adjust +/- octaves
  - Verify all active voices transpose together

**Success Criteria:**
- All 4 voices sound simultaneously in chords
- No audio glitches or clicking
- CPU <75%
- Voice stealing is clean (no pops)
- Independent voice behavior confirmed

#### Test 7: Unison Mode Hardware Validation  
**Build:** `make DRUPITER_MODE=UNISON UNISON_VOICES=5`
1. Load unit to drumlogue
2. Play single note (C3) held for 5 seconds
3. Verify hoover characteristic sound:
   - Thick, chorused texture
   - No apparent individual voices
   - Smooth stereo motion (if chorus enabled)
4. Sweep UNISON_VOICES parameter (if exposed):
   - 1 voice: Thin, direct sound
   - 3 voices: Noticeable chorus depth
   - 5 voices: Rich, thick hoover character
   - 7 voices: Maximum thickness (if CPU allows)
5. Sweep UNISON_DETUNE parameter:
   - 0%: All voices in unison, thin
   - 50%: Audible beating, chorus effect
   - 100%: Maximum stereo width, thick
6. Monitor CPU usage (target 50-65%)

**Success Criteria:**
- Recognizable "hoover synth" sound
- Voices blend together (not individual strands)
- Chorus effect audible
- CPU 50-65% (tight but acceptable)
- Smooth real-time performance

#### Test 8: Mode Switching Comparison
**Procedure:**
1. Create test patch (basic settings)
2. Build and test in each mode:
   - Monophonic (single note)
   - Polyphonic (4-note chord)
   - Unison (single note, 5 voices)
3. Record audio output from each mode
4. Compare:
   - Polyphonic: 4 independent voices, different pitches
   - Unison: Single note with chorus/thickness

**Expected Results:**
- Polyphonic: Hear 4 distinct note pitches
- Unison: Hear single note but thicker
- Both: Recognizable Drupiter character preserved

#### Test 9: Stress Test (All Modes)
**Procedure:**
- Play rapid MIDI sequence for 10 minutes
- Monophonic: Continuous 16th notes @ 140 BPM
- Polyphonic: Alternating 2-note, 3-note, 4-note chords
- Unison: Sustained notes with LFO modulation active

**Monitor:**
- CPU usage (should remain stable)
- No glitches, clicks, or pops
- No stuck notes
- Filter response smooth
- Audio quality consistent

**Success Criteria:**
- 10 minutes continuous without issues
- CPU stable (no creep)
- No voice allocation errors
- Clean audio throughout

### Validation Against Reference

#### Test 10: Alpha Juno Comparison
**Reference Material:**
- Alpha Juno "What The..." preset recordings
- "Mentasm" - Joey Beltram (isolated synth track)
- "Dominator" - Human Resource (lead sound)

**Comparison Method:**
1. Record Drupiter hoover preset
2. Spectrum analysis (FFT comparison)
3. Listening test (A/B blind test)
4. Measure:
   - Harmonic distribution
   - Stereo width
   - Chorus depth
   - Dynamic evolution

**Success Criteria:**
- Recognizably "hoover-like" character
- Similar spectral density
- Comparable stereo width
- Appropriate evolution over time

---

## Mode Configuration Matrix

**Complete Build Configuration Options:**

### Monophonic Mode (Backward Compatible)
```bash
# Default build - identical to v1.x behavior
make

# Explicit monophonic
make DRUPITER_MODE=MONOPHONIC
```
- **Architecture:** Single voice, full parameter set
- **Voices:** 1
- **CPU:** ~20-25%
- **MIDI:** Single note at a time (last-note priority)
- **Use Case:** Backward compatibility, CPU-constrained drumlogue
- **Preset Compatibility:** 100% (v1.x presets unchanged)

### Polyphonic Mode
```bash
# 4-voice polyphonic (standard)
make DRUPITER_MODE=POLYPHONIC POLYPHONIC_VOICES=4

# 6-voice polyphonic (Phase 2, requires NEON optimization)
make DRUPITER_MODE=POLYPHONIC POLYPHONIC_VOICES=6 ENABLE_NEON=1

# 2-voice polyphonic (CPU-constrained)
make DRUPITER_MODE=POLYPHONIC POLYPHONIC_VOICES=2
```
- **Architecture:** Multiple independent voices with separate envelopes
- **Voice Modes:** 2 (light), 4 (standard), 6 (rich, requires NEON)
- **CPU:** 2v=40%, 4v=65-75%, 6v=80-90% (with NEON)
- **MIDI:** Multiple simultaneous notes (voice allocation per mode)
- **Use Case:** Chord playing, basslines, melodic sequences
- **Per-Voice Control:** Pitch, amplitude envelope, velocity, filter
- **Shared Parameters:** Waveform, filter type, LFO
- **Preset Compatibility:** Extends v1.x presets (backward compatible)

### Unison Mode
```bash
# 5-voice unison (recommended)
make DRUPITER_MODE=UNISON UNISON_VOICES=5

# 3-voice unison (thin, lower CPU)
make DRUPITER_MODE=UNISON UNISON_VOICES=3

# 7-voice unison (thick, maximum CPU)
make DRUPITER_MODE=UNISON UNISON_VOICES=7 ENABLE_NEON=1
```
- **Architecture:** Single note with detuned copies
- **Voice Modes:** 3 (light), 5 (standard), 7 (rich)
- **CPU:** 3v=35%, 5v=55-65%, 7v=70-85% (with NEON)
- **MIDI:** Single note at a time (reuses same note)
- **Use Case:** Hoover synth, thick pads, characteristic "wobble"
- **Per-Voice Control:** Detune amount (frequency offset)
- **Shared Parameters:** Waveform, filter, envelope, LFO
- **Detune Strategy:** Linear cents spread, phased for golden ratio
- **Preset Compatibility:** Extends v1.x presets (backward compatible)

---

## Mode Selection Guide

| Goal | Recommended Mode | Voice Count | CPU |
|------|------------------|-------------|-----|
| Classic 80s/90s Hoover | Unison | 5-7 voices | 55-80% |
| Thick Pad | Unison | 5 voices | 55-65% |
| Chord Synth | Polyphonic | 4 voices | 65-75% |
| Bass Synth | Polyphonic | 2-3 voices | 40-50% |
| Melodic Lead | Polyphonic | 4 voices | 65-75% |
| Light Arpeggiator | Polyphonic | 3 voices | 50-60% |
| Monophonic Lead (v1.x) | Monophonic | 1 voice | 20-25% |
| CPU-Constrained Patch | Monophonic | 1 voice | 20-25% |

---

## Success Criteria

### Phase 1 (MVP) Completion Criteria

- [ ] PWM sawtooth waveform implemented and tested
- [ ] 3-voice unison working with linear detune
- [ ] Parameters integrated (voice count, detune amount)

- [ ] Desktop tests pass (spectrum, CPU profiling)
- [ ] Hardware test: Basic hoover sound achievable
- [ ] CPU usage < 80% for 3-voice unison
- [ ] No audio artifacts (clicks, glitches, aliasing)

### Phase 2 (Enhanced) Completion Criteria

- [ ] 5-7 voice unison with exponential detune
- [ ] Pitch envelope modulation working
- [ ] Phase-aligned dual output (if implemented)
- [ ] NEON optimizations applied
- [ ] CPU usage < 80% for 5-voice unison
- [ ] Desktop and QEMU ARM tests pass

### Phase 3 (Polish) Completion Criteria

- [ ] Multi-tap chorus effect implemented
- [ ] Factory hoover presets created
- [ ] Documentation updated (README, UI.md)
- [ ] All tests pass (unit, integration, hardware)
- [ ] Code review completed
- [ ] Release build created

### Final Acceptance Criteria

**Functional:**
- ✅ Can recreate classic hoover sounds
- ✅ All parameters work as documented
- ✅ Backward compatible with existing presets
- ✅ Stable under stress testing
- ✅ **All 6 factory presets unchanged and load correctly**
- ✅ **v1.x presets load without modification in v2.0**
- ✅ **No parameter reuse or removal**

**Performance:**
- ✅ CPU < 80% for typical hoover patch
- ✅ Latency < 1.5ms
- ✅ No buffer underruns
- ✅ Memory usage within limits

**Quality:**
- ✅ No audible artifacts
- ✅ Professional sound quality
- ✅ Comparable to reference recordings
- ✅ Code passes static analysis
- ✅ Documentation complete

---

## Backward Compatibility Guarantee

**This section documents the strict additive approach to ensure all existing presets remain unchanged.**

### What Does NOT Change

1. **All 24 direct parameters** (indices 0-23)
   - PARAM_DCO1_OCT, PARAM_DCO1_WAVE, PARAM_DCO1_PW, PARAM_XMOD
   - PARAM_DCO2_OCT, PARAM_DCO2_WAVE, PARAM_DCO2_TUNE, PARAM_SYNC
   - All envelopes, LFO, and modulation parameters
   - **Result:** Existing presets use identical slots

2. **Parameter indices and types**
   - No parameter is removed or reassigned
   - No parameter type changes (int → string, etc.)
   - **Result:** Preset binary format unchanged

3. **Existing waveform enum values** (0-3 for each DCO)
   - DCO1: 0=SAW, 1=SQR, 2=PUL, 3=TRI
   - DCO2: 0=SAW, 1=NSE, 2=PUL, 3=SIN
   - **Result:** No preset will accidentally load SAW_PWM

4. **Existing MOD HUB slots** (0-14)
   - MOD_LFO_TO_PWM through MOD_ENV_KYBD
   - **Result:** Existing modulation routings unchanged

5. **All factory presets** (6 presets)
   - Parameter values identical
   - Modulation routings identical
   - Effect settings identical
   - **Result:** Factory presets sound identical

### What DOES Change (Additive Only)

1. **New Waveforms Added**
   - DCO1 gains: 4=SAW_PWM
   - DCO2 gains: 4=SAW_PWM
   - **Only new patches can use these**
   - **Never assigned to existing presets**

2. **Parameter Ranges Expanded**
   - PARAM_DCO1_WAVE max: 3 → 4
   - PARAM_DCO2_WAVE max: 3 → 4
   - **Existing values 0-3 stay valid**

3. **New MOD HUB Destinations**
   - MOD_UNISON_VOICES (new, index 15+)
   - MOD_UNISON_DETUNE (new, index 16+)
   - MOD_UNISON_SPREAD (new, index 17+, Phase 2)
   - MOD_ENV_TO_PITCH (new, index 18+, Phase 2)
   - MOD_ENHANCE_CHORUS (new, index 19+, Phase 3)
   - **Existing destinations 0-14 unchanged**
   - **Existing presets extend hub_values with 0% defaults**

4. **New Effect Modes**
   - HEAVY_CHORUS (new, value 4+)
   - **Existing effects 0-3 unchanged**
   - **Existing presets unaffected**

### Preset Loading in v2.0

```cpp
// SAFE: Existing presets load without any changes
void DrupiterSynth::LoadPreset(uint8_t preset_id) {
    const Preset& preset = presets_[preset_id];
    
    // 1. Parameters (always safe, existing values stay valid)
    for (int i = 0; i < PARAM_COUNT; i++) {
        SetParameter(i, preset.params[i]);
    }
    
    // 2. MOD HUB (new destinations default to off)
    for (int i = 0; i < MOD_NUM_DESTINATIONS; i++) {
        if (i < OLD_MOD_NUM_DESTINATIONS) {
            // Existing destinations: use preset values
            mod_hub_.SetValue(i, preset.hub_values[i]);
        } else {
            // New destinations: default to 0% (off)
            mod_hub_.SetValue(i, 0);
        }
    }
    
    // 3. Effects (existing effect values unchanged)
    SetParameter(PARAM_EFFECT, preset.effect_mode);
}
```

### Migration Path (Simple & Safe)

**For end users:**
- Load v2.0 on drumlogue
- All existing presets appear in preset list
- Sound identical to v1.x
- No action required

**For developers:**
- No migration code needed
- Preset format backward compatible
- Patch loading automatically handles new destinations
- Test existing presets sound identical (critical test)

### Test Matrix for Backward Compatibility

**Must pass before release:**

| Test | Requirement |
|------|-------------|
| Load Factory Preset 1 (Init) | Sounds identical to v1.x |
| Load Factory Preset 2 (Bass) | Sounds identical to v1.x |
| Load Factory Preset 3 (Lead) | Sounds identical to v1.x |
| Load Factory Preset 4 (Pad) | Sounds identical to v1.x |
| Load Factory Preset 5 (Brass) | Sounds identical to v1.x |
| Load Factory Preset 6 (Strings) | Sounds identical to v1.x |
| Modify param, save to user slot | New preset uses existing param values |
| Load v1.x preset export | Loads without errors |
| Load v2.0 patch with SAW_PWM | Works, doesn't affect v1.x presets |
| Load v2.0 patch with MOD_UNISON_VOICES | Works, new destination off by default |

---

## References

### Research Material

**Hoover Sound Origins:**
- [Modor Music: NF-1 Hoover Guide](https://www.modormusic.com/hoover.html)
- [Wikipedia: Hoover Sound](https://en.wikipedia.org/wiki/Hoover_sound)
- Roland Alpha Juno manual (1986)

**Unison/Supersaw Implementations:**
- [Ryukau: UhhyouWebSynthesizers/HooverSynth](https://github.com/ryukau/UhhyouWebSynthesizers/tree/main/HooverSynth)
- Roland JP-8000 Supersaw (patent expired)
- u-he Diva unison algorithm

**DSP Techniques:**
- PolyBLEP anti-aliasing (Välimäki & Huovilainen, 2007)
- Digital waveguide synthesis (Smith, 1992)
- Virtual analog modeling (Huovilainen, 2004)

**Reference Tracks:**
- "Mentasm" - Joey Beltram (1991)
- "Dominator" - Human Resource (1991)
- "Charly" - The Prodigy (1992)
- "Energy Flash" - Joey Beltram (1990)

### Technical Resources

**Existing Code:**
- `drumlogue/drupiter-synth/dsp/jupiter_dco.{h,cc}` - Current DCO implementation
- `drumlogue/common/stereo_widener.h` - Existing chorus/widening
- `drumlogue/common/neon_dsp.h` - NEON SIMD utilities

**Tools:**
- Desktop test harness: `test/drupiter-synth/`
- QEMU ARM testing: `test/qemu-arm/test-unit.sh`
- Spectrum analyzer: Audacity, SoX, Python FFT scripts

**External Tools:**
- SoX (audio analysis and conversion)
- Audacity (spectrum/spectrogram viewing)
- Vital/Serum (reference supersaw sounds)

---

## Risk Assessment

### High Risk Items

**1. CPU Budget Exceeded**
- **Probability:** Medium
- **Impact:** High (unusable on hardware)
- **Mitigation:** 
  - Implement voice limiting (3/5/7 user choice)
  - NEON optimization mandatory
  - Profile early and often

**2. Aliasing Artifacts**
- **Probability:** Medium
- **Impact:** Medium (poor sound quality)
- **Mitigation:**
  - PolyBLEP on all discontinuities
  - Oversample if needed (2× upsampling)
  - Thorough testing with spectrum analyzer

**3. Parameter Complexity**
- **Probability:** Low
- **Impact:** Medium (confusing UX)
- **Mitigation:**
  - Good factory presets
  - Clear documentation
  - Intuitive parameter naming

### Medium Risk Items

**4. Memory Constraints**
- **Probability:** Low
- **Impact:** Medium
- **Mitigation:**
  - Shared wavetables
  - Reduce chorus buffer size if needed
  - Monitor heap usage

**5. Backward Compatibility**
- **Probability:** Low
- **Impact:** High (breaks existing patches)
- **Mitigation:**
  - Careful parameter slot reuse
  - Version check in preset loading
  - Migration guide

---

## Timeline Estimate

**Phase 1 (Core):** 2-3 weeks
- PWM Sawtooth: 3-5 days
- Unison Oscillator: 5-7 days
- Integration: 3-4 days
- Testing: 2-3 days

**Phase 2 (Enhanced):** 1-2 weeks
- Pitch Envelope: 2-3 days
- Dual Output: 2-3 days
- Unison Expansion: 3-4 days
- NEON Optimization: 3-5 days

**Phase 3 (Polish):** 1-2 weeks
- Chorus Effect: 4-5 days
- Presets: 2-3 days
- Documentation: 2-3 days

**Phase 4 (Release):** 1 week
- Optimization: 2-3 days
- QA: 2-3 days
- Release: 1-2 days

**Total Estimated Time:** 5-8 weeks

---

## Conclusion

Implementing hoover synthesis in Drupiter-Synth is technically feasible but requires careful attention to CPU constraints and anti-aliasing quality. The core features (PWM sawtooth and unison voices) are well-understood DSP techniques, and the drumlogue platform has sufficient processing power with proper optimization.

**Key Success Factors:**
1. NEON SIMD optimization for unison processing
2. Efficient PolyBLEP anti-aliasing
3. Voice count limiting for CPU management
4. Thorough testing with reference material

The resulting synthesizer will offer authentic Alpha Juno-style hoover sounds while maintaining the existing Jupiter-8 architecture, making it a versatile hybrid synthesizer suitable for classic analog emulation and modern rave/techno production.

---

**Next Steps:**
1. Review and approve this plan
2. Set up development branch: `feature/hoover-synthesis`
3. Begin Phase 1: Task 1.1 (PWM Sawtooth)
4. Establish testing framework for validation

**Questions/Discussion:**
- Target voice count: 5 or 7 voices for final version?
- Chorus: 2-tap or 3-tap implementation?
- Parameter mapping: Direct or MOD HUB for unison controls?
- Release version: 2.0.0 or 1.1.0?
