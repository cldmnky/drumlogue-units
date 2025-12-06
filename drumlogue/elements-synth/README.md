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
  - STRIKE: Percussive impacts
- **Physical resonator** with 24 modes (optimized for drumlogue)
- **Three resonator models:** Modal, String, Strings
- **Stereo width control** (use drumlogue's chain reverb for spatial effects)

### Moog Ladder Filter
- **24dB/octave lowpass** (Huovilainen model)
- **Self-oscillation** at high resonance
- **Envelope modulation** (positive/negative)
- **Key tracking** for pitch-following filter
- **Non-linear saturation** for analog character

### Modulation
- **LFO** for filter cutoff and pitch vibrato
- **Velocity-sensitive** envelope triggering
- **Legato mode** for smooth pitch transitions

## Parameters (6 Pages)

### Page 1: Exciter Controls
| Parameter | Description |
|-----------|-------------|
| CONTOUR | Envelope shape (AD→ADSR→AR) |
| BOW | Bow exciter level |
| BLOW | Blow/breath exciter level |
| STRIKE | Strike/percussion level |

### Page 2: Exciter Timbres
| Parameter | Description |
|-----------|-------------|
| BOW TIMB | Bow smoothness/granularity |
| FLOW | Air flow wavetable position |
| MALLET | Strike type (samples→mallets→particles) |
| BLW TIMB | Blow pitch/granulation rate |

### Page 3: Resonator
| Parameter | Description |
|-----------|-------------|
| GEOMETRY | Structure type (plate→string→tube→bell) |
| BRIGHT | Brightness/mode damping |
| DAMPING | Decay time |
| POSITION | Excitation position on surface |

### Page 4: Space & Pitch
| Parameter | Description |
|-----------|-------------|
| SPACE | Stereo width |
| COARSE | Coarse pitch (-24 to +24 semitones) |
| FINE | Fine pitch (-50 to +50 cents) |
| STK TIMB | Strike brightness/speed |

### Page 5: Ladder Filter
| Parameter | Description |
|-----------|-------------|
| FLT FREQ | Filter cutoff frequency |
| FLT RES | Filter resonance (0-4, self-oscillation at ~3.8) |
| FLT ENV | Filter envelope amount (bipolar) |
| FLT KEY | Key tracking amount |

### Page 6: Modulation & Mode
| Parameter | Description |
|-----------|-------------|
| MOD FREQ | LFO rate |
| MOD DPTH | LFO depth to filter and pitch |
| MODEL | Resonator model (MODAL/STRING/STRINGS) |

## Presets

| # | Name | Description |
|---|------|-------------|
| 0 | INIT | Clean starting point |
| 1 | PLUCK | Plucked string/harp |
| 2 | BELL | Metallic bell percussion |
| 3 | BOW | Bowed string with vibrato |
| 4 | BREATH | Wind/breath instrument |
| 5 | DRUM | Percussion with filter envelope |
| 6 | PAD | Ambient pad with stereo spread |
| 7 | FILTER | Filter sweep showcase |

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
Elements is CPU-intensive due to the 24-mode resonator. If CPU issues occur on hardware:
- Use STRING or STRINGS model instead of MODAL
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
