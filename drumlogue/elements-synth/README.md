# Elements Synth for Korg Drumlogue

A port of **Mutable Instruments Elements** modal synthesis voice to the Korg Drumlogue, enhanced with a **Moog-style ladder filter**.

## Overview

Elements is a modal synthesis voice that combines three excitation sources (bow, blow, strike) with a physical resonator to create rich, evolving timbres ranging from plucked strings to bells to breathy wind instruments.

This port adds:
- **48kHz operation** (original runs at 32kHz)
- **Moog ladder filter** with envelope and key tracking
- **LFO modulation** for filter and pitch
- **8 factory presets**
- **Professional voice management** with legato support

## Features

### Modal Synthesis Engine
- **Three excitation types:**
  - BOW: Continuous friction/bowing
  - BLOW: Granular noise/breath
  - STRIKE: Percussive impacts (sample, granular, or noise modes)
- **Physical resonator** with 8 modes
- **Two resonator models:** Modal, String (Karplus-Strong)
- **Stereo width control** (use drumlogue's chain reverb for spatial effects)

### Moog Ladder Filter
- **24dB/octave lowpass**
- **Resonance control** (self-oscillation at high values)
- **Envelope modulation** (fixed amount, adds character to attacks)
- **Non-linear saturation** for analog character

### Modulation
- **LFO** with 8 presets (various shapes targeting cutoff, geometry, position, brightness, or space)
- **Velocity-sensitive** envelope triggering
- **Pitch bend** support (+/- 2 semitones)

## Parameters (6 Pages)

### Page 1: Exciter Mix
| Parameter | Description |
|-----------|-------------|
| BOW | Bow exciter level |
| BLOW | Blow/breath exciter level |
| STRIKE | Strike/percussion level |
| MALLET | Strike sample/timbre (12 variants: SOFT/MED/HARD/PLEC/STIK/BOW × DK/BR) |

### Page 2: Exciter Timbre
| Parameter | Description |
|-----------|-------------|
| BOW TIM | Bow friction/texture (bipolar) |
| BLW TIM | Blow turbulence/noise color (bipolar) |
| STK MOD | Strike mode (SAMPLE/GRANULAR/NOISE) |
| DENSITY | Granular density (bipolar, active in GRANULAR mode) |

### Page 3: Resonator
| Parameter | Description |
|-----------|-------------|
| GEOMETRY | Structure type (string→beam→plate→bell, bipolar) |
| BRIGHT | Brightness/mode damping (bipolar) |
| DAMPING | Decay time (bipolar) |
| POSITION | Excitation position on surface (bipolar) |

### Page 4: Filter & Space
| Parameter | Description |
|-----------|-------------|
| CUTOFF | Filter cutoff frequency |
| RESO | Filter resonance |
| SPACE | Stereo width |
| MODEL | Resonator model (MODAL/STRING) |

### Page 5: Envelope (ADR)
| Parameter | Description |
|-----------|-------------|
| ATTACK | Envelope attack time |
| DECAY | Envelope decay time |
| RELEASE | Envelope release time |
| ENV MOD | Envelope mode (ADR/AD/AR/LOOP) |

### Page 6: LFO & Pitch
| Parameter | Description |
|-----------|-------------|
| LFO RT | LFO rate |
| LFO DEP | LFO modulation depth |
| LFO PRE | LFO preset (OFF/TRI>CUT/SIN>GEO/SQR>POS/TRI>BRI/SIN>SPC/SAW>CUT/RND>SPC) |
| COARSE | Pitch coarse tune (-24 to +24 semitones, bipolar) |

## Presets

| # | Name | Description |
|---|------|-------------|
| 0 | Init | Clean starting point |
| 1 | Bowed Str | Bowed string |
| 2 | Bell | Metallic bell percussion |
| 3 | Wobble | Wobble bass with LFO on cutoff |
| 4 | Blown Tube | Wind/breath instrument |
| 5 | Shimmer | Ambient shimmer with LFO on brightness |
| 6 | Pluck Str | Plucked string |
| 7 | Drone | Evolving drone pad with LFO on space |

## Technical Details

### Sample Rate Adaptation
- Original Elements: 32kHz
- Drumlogue: 48kHz
- All delay lines and filter coefficients scaled accordingly

### Memory Usage
- **Text (code):** ~400KB
- **BSS (buffers):** ~109KB
  - Diffuser buffers: ~8KB
  - Part internal buffers: ~100KB
- **Total binary:** ~403KB

> Note: Reverb was removed to reduce memory footprint. Use the drumlogue's built-in chain reverb for spatial effects.

### CPU Considerations
Elements is CPU-intensive due to the 8-mode resonator. If CPU issues occur on hardware:
- Use STRING model instead of MODAL
- Avoid extreme GEOMETRY values
- Reduce polyphony by playing monophonically

## Installation

1. Connect drumlogue to computer via USB
2. Drumlogue appears as mass storage device
3. Copy `elements_synth.drmlgunit` to `Units/Synth/` folder
4. Eject drumlogue and disconnect
5. Select "Elements" in synth slot

## Building from Source

```bash
# From repository root
./build.sh elements-synth

# Output: drumlogue/elements-synth/elements_synth.drmlgunit
```

Requires the logue-sdk Docker environment.

## Credits

- **Original Elements:** Émilie Gillet / Mutable Instruments (MIT License)
- **Drumlogue Port:** CLDM
- **Ladder Filter:** Based on Huovilainen improved model

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

## Version History

### v1.0.0
- Initial release
- Full Elements DSP integration at 48kHz
- Moog ladder filter with envelope/key tracking
- 8 factory presets
- Legato voice management
