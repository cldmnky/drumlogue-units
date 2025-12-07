# Elementish Synth for Korg Drumlogue

A port of **Mutable Instruments Elements** modal synthesis voice to the Korg Drumlogue, optimized for real-time performance.

## Overview

Elements is a modal synthesis voice that combines three excitation sources (bow, blow, strike) with a physical resonator to create rich, evolving timbres ranging from plucked strings to bells to breathy wind instruments.

This port features:

- **48kHz operation** (original runs at 32kHz)
- **Three resonator models:** Modal, String, and Multi-String
- **NEON SIMD optimization** for ARM performance
- **8 factory presets**
- **Velocity-sensitive** envelope and dynamics
- **Pitch bend** support (+/- 2 semitones)

## Features

### Modal Synthesis Engine

- **Three excitation types:**
  - BOW: Continuous friction/bowing with adjustable timbre
  - BLOW: Granular noise/breath with flow control
  - STRIKE: Percussive impacts (sample, granular, noise, plectrum, or particle modes)
- **Physical resonator** with 8 spectral modes
- **Three resonator models:**
  - MODAL: Classic Elements 8-partial modal resonator
  - STRING: Karplus-Strong string synthesis
  - MSTRING: 5 sympathetic strings for rich harmonic content
- **Stereo width control** via SPACE parameter

### Exciter Options

- **12 mallet types:** SOFT/MED/HARD/PLEC/STIK/BOW × DK(dark)/BR(bright)
- **5 strike modes:** SAMPLE, GRANULAR, NOISE, PLECTRUM, PARTICLE
- **Granular density control** for textured attacks

## Parameters (6 Pages)

### Page 1: Exciter Mix

| Parameter | Description |
|-----------|-------------|
| BOW | Bow exciter level (continuous friction) |
| BLOW | Blow/breath exciter level (granular noise) |
| STRIKE | Strike/percussion level |
| MALLET | Mallet type - 12 variants (see Exciter Options above) |

### Page 2: Exciter Timbre

| Parameter | Description |
|-----------|-------------|
| BOW TIM | Bow friction/texture (bipolar: smooth↔rough) |
| FLOW | Air turbulence for blow exciter (bipolar) |
| STK MOD | Strike mode (SAMPLE/GRANULAR/NOISE/PLECTRUM/PARTICLE) |
| DENSITY | Granular density (bipolar, active in GRANULAR/PARTICLE modes) |

### Page 3: Resonator

| Parameter | Description |
|-----------|-------------|
| GEOMETRY | Structure shape (bipolar: string→bar→membrane→plate→bell) |
| BRIGHT | High-frequency mode damping (bipolar: wood↔glass) |
| DAMPING | Energy dissipation/decay time (bipolar) |
| POSITION | Excitation point on surface (bipolar, affects harmonics) |

### Page 4: Model & Space

| Parameter | Description |
|-----------|-------------|
| MODEL | Resonator model (MODAL/STRING/MSTRING) |
| SPACE | Stereo width (0=mono, 127=wide) |
| VOLUME | Output level |
| — | (unused) |

### Page 5: Envelope

| Parameter | Description |
|-----------|-------------|
| ATTACK | Envelope attack time |
| DECAY | Envelope decay time |
| RELEASE | Envelope release time |
| CONTOUR | Envelope mode (ADR/AD/AR/LOOP) |

### Page 6: Tuning

| Parameter | Description |
|-----------|-------------|
| COARSE | Pitch coarse tune (bipolar: -24 to +24 semitones) |
| FINE | Pitch fine tune (bipolar: -100 to +100 cents) |
| — | (unused) |
| — | (unused) |

## Presets

| # | Name | Description |
|---|------|-------------|
| 0 | Init | Clean starting point (strike only) |
| 1 | Bowed Str | Bowed string with sustain |
| 2 | Bell | Metallic bell percussion |
| 3 | Wobble | Wobble bass |
| 4 | Blown Tube | Wind/breath instrument |
| 5 | Shimmer | Ambient shimmer |
| 6 | Pluck Str | Plucked string (STRING model) |
| 7 | Drone | Evolving drone pad |

## Technical Details

### Sample Rate Adaptation

- Original Elements: 32kHz
- Drumlogue: 48kHz
- All delay lines and filter coefficients scaled accordingly

### Build Modes

The synth supports two build configurations:

**Lightweight Mode (default):** `ELEMENTS_LIGHTWEIGHT`

- Removes LFO and Moog filter for maximum performance
- Page 4: MODEL, SPACE, VOLUME
- Page 6: COARSE, FINE tuning
- Recommended for drumlogue hardware

**Full Mode:** (disable `ELEMENTS_LIGHTWEIGHT` in config.mk)

- Includes 24dB/oct Moog ladder filter with envelope modulation
- Includes LFO with 8 preset shape/destination combinations
- Higher CPU usage

### Binary Size

- **Lightweight mode:** ~123KB
- Uses NEON SIMD optimizations for ARM

### CPU Usage

Measured on test harness (representative):

- MODAL model: ~0.5% CPU per voice
- STRING model: ~0.3% CPU per voice
- MSTRING model: ~0.3% CPU per voice

All models run well within real-time budget.

## Installation

1. Connect drumlogue to computer via USB
2. Drumlogue appears as mass storage device
3. Copy `elements_synth.drmlgunit` to `Units/Synth/` folder
4. Eject drumlogue and disconnect
5. Select "Elementish" in synth slot

## Building from Source

```bash
# From repository root
./build.sh elements-synth

# Output: drumlogue/elements-synth/elements_synth.drmlgunit
```

To build full mode (with filter and LFO), edit `config.mk` and comment out:

```makefile
# UDEFS = -DELEMENTS_LIGHTWEIGHT
```

Requires the logue-sdk Docker environment.

## Credits

- **Original Elements:** Émilie Gillet / Mutable Instruments (MIT License)
- **Drumlogue Port:** CLDM

## License

MIT License (same as original Elements DSP code)

See `eurorack/README.md` for Mutable Instruments licensing details.

## Known Limitations

- Monophonic (single voice)
- No external audio input processing (pure synthesizer)
- Some original Elements features not exposed:
  - Easter egg mode (Ominous voice)
  - CV/Gate inputs (handled by drumlogue)
  - LED metering
- Lightweight mode removes filter and LFO for performance

## Version History

### v1.0.0

- Initial release
- Full Elements DSP integration at 48kHz
- Three resonator models: MODAL, STRING, MSTRING
- Level-balanced output across all models
- NEON SIMD optimizations
- 8 factory presets
- Lightweight mode for optimal drumlogue performance
