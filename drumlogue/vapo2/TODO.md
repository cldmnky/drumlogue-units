# Vapo2 - Wavetable Synth for Drumlogue

A wavetable synthesizer inspired by VAST Dynamics Vaporizer2, optimized for the Korg drumlogue platform using techniques from Mutable Instruments Plaits.

## Project Overview

### Goals
- Implement a wavetable oscillator with smooth morphing between waveforms
- Band-limited playback to prevent aliasing at all frequencies
- Efficient ARM Cortex-A7 implementation (48kHz, stereo output)
- Pre-computed wavetables (no runtime FFT)
- Parameter expressiveness similar to Vaporizer2 with drumlogue's 24-parameter limit

### Architecture Decision
Following Plaits' approach rather than direct Vaporizer2 port:
- **Integrated wavetable synthesis** (Higher-order integration, Franck & Välimäki DAFX-12)
- **Differentiation at playback** for anti-aliasing
- **Pre-computed int16_t tables** for memory efficiency
- **Linear interpolation** between samples and waveforms

---

## Phase 1: Core Oscillator [CURRENT]

### 1.1 Wavetable Data Structure
- [ ] Define wavetable format (table size, number of waves per bank)
  - Table size: 128 samples (like Plaits) or 256 for higher quality
  - Waves per bank: 8-16 morphable waves
  - Total banks: 4-8 selectable wavetable banks
- [ ] Create Python script to generate wavetables (`resources/wavetables.py`)
  - Classic waveforms (sine, saw, square, triangle)
  - Additive synthesis waves
  - Formant/vocal waves
  - Digital/harsh waves
- [ ] Generate C header with wavetable data (`wavetables.h`)
- [ ] Use int16_t integrated format (like Plaits)

### 1.2 Wavetable Oscillator Core
- [ ] Create `wavetable_osc.h` based on Plaits' `wavetable_oscillator.h`
  - Phase accumulator (normalized 0.0-1.0)
  - Linear sample interpolation
  - Linear waveform crossfade (morphing)
  - Differentiator for anti-aliasing
  - One-pole lowpass for high-frequency attenuation
- [ ] Implement `InterpolateWave()` function
- [ ] Implement `InterpolateWaveHermite()` for higher quality option
- [ ] Add frequency-dependent amplitude scaling

### 1.3 Basic Voice Structure
- [ ] Create `vapo2_voice.h`
  - Single wavetable oscillator
  - Note frequency tracking
  - Velocity sensitivity
  - Basic amplitude envelope (ADSR)

---

## Phase 2: Dual Oscillator & Mixing

### 2.1 Second Oscillator
- [ ] Add Oscillator B (independent wavetable selection)
- [ ] Oscillator B detune (fine and coarse)
- [ ] Oscillator B octave offset
- [ ] Independent morph control per oscillator

### 2.2 Oscillator Mixing
- [ ] Oscillator A/B mix control
- [ ] Ring modulation mode (A × B)
- [ ] Sync mode (hard sync B to A)
- [ ] Unison detune (spread multiple oscillators)

---

## Phase 3: Modulation

### 3.1 Amplitude Envelope
- [ ] ADSR envelope generator
- [ ] Envelope shape/curve options (linear, exponential, analog-style)
- [ ] Velocity to envelope modulation

### 3.2 Filter Envelope
- [ ] Second ADSR for filter
- [ ] Filter envelope amount control
- [ ] Envelope invert option

### 3.3 LFO
- [ ] Single LFO (sine, triangle, saw, square, S&H)
- [ ] LFO rate (free or tempo-synced)
- [ ] LFO depth control
- [ ] LFO destinations: pitch, morph, filter cutoff

### 3.4 Modulation Routing (Simplified)
- [ ] Velocity → Filter cutoff
- [ ] Key tracking → Filter cutoff
- [ ] Aftertouch → Morph position

---

## Phase 4: Filter

### 4.1 Filter Implementation
- [ ] State Variable Filter (SVF) - 12dB/oct
  - Lowpass
  - Highpass
  - Bandpass
- [ ] Filter cutoff control
- [ ] Filter resonance control
- [ ] Consider optional 24dB mode (CPU permitting)

### 4.2 Filter Optimizations
- [ ] Use lookup tables for coefficient calculation
- [ ] Consider using existing `dsp_utils.h` filter implementations

---

## Phase 5: Wavetable Banks & Effects

### 5.1 Multiple Wavetable Banks
- [ ] Bank 1: Classic/Analog (sine, saw, pulse, triangle variants)
- [ ] Bank 2: Digital/Harsh (bit-crushed, FM, sync sounds)
- [ ] Bank 3: Vocal/Formant (formant sweeps, vowels)
- [ ] Bank 4: Texture/Pad (evolving, complex spectra)

### 5.2 Wavetable Effects (Vaporizer2-inspired)
- [ ] Phase distortion option
- [ ] Waveform bend/stretch
- [ ] Harmonic freezing

### 5.3 Output Effects
- [ ] Stereo width/space
- [ ] Soft clipping/saturation
- [ ] DC offset removal

---

## Phase 6: Parameter Layout (24 params, 6 pages)

### Proposed Parameter Mapping

```
Page 1: Oscillator A
- OSC A BANK  : Wavetable bank selection (0-7)
- OSC A MORPH : Morph position within bank (0-127)
- OSC A OCT   : Octave offset (-3 to +3)
- OSC A TUNE  : Fine tune in cents (-64 to +63)

Page 2: Oscillator B
- OSC B BANK  : Wavetable bank selection (0-7)
- OSC B MORPH : Morph position within bank (0-127)
- OSC B OCT   : Octave offset (-3 to +3)
- OSC B MIX   : Osc A/B mix (0=A only, 127=B only)

Page 3: Filter
- CUTOFF      : Filter cutoff frequency (0-127)
- RESO        : Filter resonance (0-127)
- FLT ENV     : Filter envelope amount (-64 to +63)
- FLT TYPE    : LP12/LP24/HP/BP (0-3)

Page 4: Amp Envelope
- ATTACK      : Amplitude attack time (0-127)
- DECAY       : Amplitude decay time (0-127)
- SUSTAIN     : Amplitude sustain level (0-127)
- RELEASE     : Amplitude release time (0-127)

Page 5: Filter Envelope
- F.ATTACK    : Filter attack time (0-127)
- F.DECAY     : Filter decay time (0-127)
- F.SUSTAIN   : Filter sustain level (0-127)
- F.RELEASE   : Filter release time (0-127)

Page 6: Modulation
- LFO RATE    : LFO speed (0-127)
- LFO>MORPH   : LFO to morph amount (-64 to +63)
- LFO>PITCH   : LFO to pitch amount (-64 to +63)
- VOLUME      : Output level (0-127)
```

---

## Phase 7: Presets

### 7.1 Factory Presets (8 total)
- [ ] INIT: Basic sawtooth
- [ ] PAD: Soft evolving pad
- [ ] BASS: Punchy bass
- [ ] LEAD: Bright lead
- [ ] KEY: Electric piano style
- [ ] PLUCK: Plucked string
- [ ] DIGITAL: Harsh digital
- [ ] VOX: Vocal-ish formant

---

## Implementation Notes

### Memory Budget (rough estimates)
- Wavetable data: ~50KB (8 banks × 8 waves × 128 samples × 2 bytes)
- Voice state: ~1KB per voice
- Filter state: ~200 bytes
- Total: Well under drumlogue limits

### CPU Budget Considerations
- Target: <30% CPU at 48kHz stereo
- Plaits-style differentiation is efficient
- SVF filter is CPU-friendly
- Avoid heavy per-sample calculations

### Files to Create
```
drumlogue/vapo2/
├── TODO.md              # This file
├── README.md            # User documentation
├── RELEASE_NOTES.md     # Version history
├── Makefile             # Build configuration
├── config.mk            # Project settings
├── header.c             # Unit metadata & parameters
├── unit.cc              # SDK callbacks
├── vapo2_synth.h        # Main synth wrapper class
├── wavetable_osc.h      # Wavetable oscillator (Plaits-inspired)
├── envelope.h           # ADSR envelope generator
├── filter.h             # SVF filter
├── lfo.h                # LFO implementation
├── voice.h              # Voice management
└── resources/
    ├── wavetables.py    # Python generator script
    └── wavetables.h     # Generated wavetable data
```

### Reference Materials
- Plaits: `eurorack/plaits/dsp/oscillator/wavetable_oscillator.h`
- Plaits wavetables: `eurorack/plaits/resources/wavetables.py`
- Vaporizer2 oscillator: `VASTWaveTableOscillator.cpp`
- Vaporizer2 wavetable: `VASTWaveTable.cpp`
- Franck & Välimäki, "Higher-order integrated Wavetable Synthesis", DAFX-12

---

## Development Milestones

### Milestone 1: Proof of Concept ✨
- [ ] Basic wavetable playback (single wave, no morphing)
- [ ] Note on/off working
- [ ] Compiles and runs on drumlogue

### Milestone 2: Core Synth
- [ ] Morphing between waves
- [ ] Two oscillators with mixing
- [ ] Basic ADSR envelope

### Milestone 3: Playable Instrument
- [ ] Filter with envelope
- [ ] LFO modulation
- [ ] All 24 parameters functional

### Milestone 4: Polish
- [ ] Factory presets
- [ ] CPU optimization
- [ ] Documentation

---

## Open Questions

1. **Table size**: 128 (Plaits) vs 256 vs 512? Trade-off: quality vs memory
2. **Polyphony**: Mono only? Or support drumlogue's voice allocation?
3. **Wavetable loading**: Built-in only? Or support loading from samples?
4. **Unison**: Include? Would eat CPU but add thickness
5. **Sub oscillator**: Add -1/-2 octave sub?

---

## Changelog

- 2024-12-09: Initial TODO created
