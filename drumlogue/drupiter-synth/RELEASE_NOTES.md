# Release Notes - Drupiter Synth

## v1.2.0 - 2026-01-18

### 🎛️ MIDI Expression & Bug Fixes

Third release with enhanced MIDI expressiveness and comprehensive stability improvements.

### New Features

**MIDI Expressiveness:**
- **Velocity Sensitivity**: Notes respond to MIDI velocity for dynamic control of filter cutoff (50% modulation depth)
- **Pitch Bend**: ±2 semitone pitch bend with smooth per-buffer smoothing (~200ms response)
- **Channel Pressure**: Real-time pressure modulation of VCF cutoff frequency (+1 octave depth)
- **Aftertouch**: Per-note aftertouch support for expressive, dynamic control
- **Per-Buffer Smoothing**: All MIDI modulations use exponential smoothing to prevent clicks and zipper noise

**Enhanced Common Library:**
- Extended `midi_helper.h` with reusable pressure/aftertouch conversion utilities
- Standardized MIDI parameter scaling (0-127 → 0.0-1.0 normalized range)
- New `catchable_value.h` utility: Generic knob catch mechanism (±3 unit threshold)
- Reusable by all drumlogue units

### Technical Improvements

**DSP Stability:**
- Eliminated dynamic memory allocation in voice allocator (fixes undefined symbols)
- Fixed Q31 fixed-point interpolation with proper PolyBLEP anti-aliasing
- Improved voice allocation in mono/unison modes (last-note priority)
- Fixed mod hub UI page flickering on drumlogue display

**Build System:**
- Automatic symbol verification in `build.sh`
- Preserved build artifacts from container for debugging
- Comprehensive build artifact analysis (object files, memory maps, symbol tables)

**Testing & Debugging:**
- 5 new MIDI tests in desktop test harness (velocity, pitch bend, pressure, aftertouch, smoothing)
- 5 new catchable value tests (basic follow, catch/release, threshold, bipolar, float variant)
- All 13 tests passing (3 voice allocation + 5 MIDI expression + 5 catchable value)
- Complete debug verification checklist per SDK guidelines

### Bug Fixes

- Fixed compilation warnings across all source files
- Fixed VoiceAllocator symbol resolution issues
- Resolved voice allocation edge cases in polyphonic/unison modes
- Eliminated undefined reference errors from static constexpr members

### Knob Catch Mechanism

**New Feature**: Generic knob catch to prevent audio jumps on preset changes
- Applied to 9 continuous parameters (cutoff, mix, resonance, etc.)
- Fixed ±3 unit threshold for intuitive, consistent behavior
- Transparent support for both unipolar and bipolar parameters
- Reusable across all drumlogue units via new `catchable_value.h` utility
- **Behavior**: When knob position differs from preset, DSP holds preset value until knob crosses catch threshold

### Performance

- Velocity modulation: 50% depth on VCF cutoff
- Pitch bend: ±2 semitones with smooth vibrato response
- Pressure modulation: +1 octave VCF swell with expressive feel
- All MIDI modulations: Per-buffer smoothing (exponential convergence)
- CPU usage: Maintained <50% on 48kHz buffer processing

### Compatibility

- **Backward Compatible**: All v1.1.0 presets and functionality preserved
- **MIDI Enhancement**: Optional - MIDI controllers enhance but don't require modification
- **Hardware Requirements**: Standard drumlogue with MIDI input support

---

## v1.1.0 - 2025-12-29

### 🚀 Major Feature Update

Second major release of Drupiter, introducing polyphonic capabilities, expanded waveforms, and advanced modulation features.

### New Features

**Synthesis Modes:**
- **Polyphonic Mode**: True polyphony with voice allocation system
- **Unison Mode**: Rich ensemble effects with golden ratio detuning and stereo spread
- **Mono Mode**: Classic monophonic operation (backward compatible)

**Expanded Waveforms:**
- **5 Waveforms per Oscillator**: SAW/SQR/PULSE/TRI/SAW_PWM (previously 4)
- **SAW PWM**: Pulse-width modulated sawtooth waves for rich, evolving timbral character
- **Enhanced Pulse Width Control**: Improved PWM implementation for all pulse-based waveforms

**Advanced Modulation:**
- **ENV TO PITCH**: Filter envelope can now modulate oscillator pitch (±12 semitones)
- **Extended Modulation Hub**: 18 modulation destinations including:
  - Synth mode switching (Mono/Poly/Unison)
  - Unison detune amount
  - Portamento/glide time
  - Enhanced LFO routing

**Performance & Playability:**
- **Portamento/Glide**: True legato detection with exponential glide curves
- **Voice Allocation**: Intelligent polyphonic voice management
- **Enhanced Presets**: Expanded from 6 to 12 factory presets covering new features

### Technical Improvements

- **DSP Architecture**: Complete rewrite with modular voice allocation system
- **Performance Monitoring**: Integrated cycle counting for CPU usage optimization
- **QEMU ARM Testing**: Enhanced test harness with automatic rebuilding
- **Build System**: Improved container builds with performance monitoring support

### Presets

**New Presets (7-12):**
- 7. PWM Lead - Pulse-width modulated sawtooth lead
- 8. Unison Pad - Rich ensemble pad with golden ratio detuning
- 9. Poly Bass - Polyphonic bass with glide
- 10. ENV FM - Filter envelope modulating pitch
- 11. Stereo Unison - Wide stereo ensemble effects
- 12. Glide Lead - Smooth portamento lead sound

**Existing Presets (1-6):**
- 1. Jupiter Bass - Deep, punchy bass
- 2. Jupiter Lead - Bright, cutting lead
- 3. Sync Lead - Hard-synced aggressive lead
- 4. FM Bell - Cross-modulated bell tones
- 5. Analog Pad - Warm, evolving pad
- 6. Poly Arp - Bright arpeggiator sound

### Compatibility

- **Backward Compatible**: All existing presets and mono mode operation preserved
- **Hardware Requirements**: No additional hardware requirements
- **Firmware**: Compatible with all drumlogue firmware versions

### Bug Fixes

- Improved parameter smoothing for glitch-free modulation
- Enhanced voice stealing algorithm for better polyphonic performance
- Fixed edge cases in unison detuning calculations

---

## v1.0.0 - 2024-12-18

### 🎉 Initial Release

First stable release of Drupiter, a Jupiter-8 inspired monophonic synthesizer for Korg drumlogue.

### Features

**Oscillators:**
- Dual DCOs with 4 waveforms each (Ramp, Square, Pulse, Triangle)
- Hard sync (DCO1 → DCO2) for aggressive timbres
- Cross-modulation (DCO2 → DCO1 FM) for complex harmonic content
- DCO-1: octave selection (16', 8', 4')
- DCO-2: octave selection (32', 16', 8', 4')
- Pulse width control for pulse waveforms
- Detune control for DCO2

**Filter:**
- Multi-mode filter (LP12, LP24, HP12, BP12)
- Self-oscillation capability
- Envelope amount and keyboard tracking
- Resonance control

**Modulation:**
- Dual ADSR envelopes (VCF and VCA)
- LFO with 4 waveforms (Triangle, Sawtooth, Square, S&H)
- LFO delay envelope for gradual modulation
- LFO rate control

**Presets:**
- 6 factory presets covering common synth sounds

**User Interface:**
- 24 parameters across 6 pages
- Hub control system for efficient parameter organization
- Clear parameter naming and formatting</content>
<parameter name="filePath">/Users/mbengtss/code/src/github.com/cldmnky/drumlogue-units/drumlogue/drupiter-synth/RELEASE_NOTES.md