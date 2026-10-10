---
layout: unit
title: Pepege-Synth
unit_name: pepege-synth
unit_type: synth
version: v1.1.0
icon: 🌊
tagline: 2-voice polyphonic PPG wavetable synthesizer
description: Authentic 8-bit wavetable synthesis with dual oscillators, comprehensive modulation, and factory presets inspired by classic PPG Wave sounds.
permalink: /units/pepege-synth/
---

# Pepege-Synth

**2-Voice Polyphonic PPG Wavetable Synthesizer**

Pepege-Synth brings authentic PPG wavetable synthesis to the Korg drumlogue with 2-voice polyphony, dual oscillators, and comprehensive modulation capabilities. Inspired by VAST Dynamics Vaporizer2 and using wavetable techniques from Mutable Instruments Plaits.

---

## Features

### 🎹 2-Voice Polyphony
- Play chords and harmonies
- Round-robin voice allocation
- Oldest-note stealing when all voices are active
- Smooth envelope transitions

### 🌊 Dual PPG Wavetable Oscillators
- **16 wavetable banks** with 256 total waves
- **Independent controls** for each oscillator
- **Three PPG interpolation modes:**
  - **HiFi**: Bilinear interpolation (smoothest)
  - **LoFi**: Sample interpolation only (stepped waves)
  - **Raw**: No interpolation (authentic PPG crunch)
- Morphable wavetable position for evolving timbres

### 🎚️ Wavetable Tables

The A/B BANK parameters select one of the 29 wavetables stored in the PPG Wave 2.2/2.3 EPROM, in the PPG's own order:

| # | Name | # | Name | # | Name |
|---|------|---|------|---|------|
| 0 | Resonant | 10 | Reso 3HP | 20 | Robotic |
| 1 | Resonant2 | 11 | Wind Syn | 21 | StrongHrm |
| 2 | MalletSyn | 12 | HighHarm | 22 | PercOrgan |
| 3 | Sqr-Sweep | 13 | Clipper | 23 | ClipSweep |
| 4 | Bellish | 14 | Organ Syn | 24 | ResoHarms |
| 5 | Pul-Sweep | 15 | SquareSaw | 25 | 2 Echoes |
| 6 | Saw-Sweep | 16 | Formant1 | 26 | Formant 2 |
| 7 | MellowSaw | 17 | Polated | 27 | FmntVocal |
| 8 | Feedback | 18 | Transient | 28 | Upper WT |
| 9 | Add Harm | 19 | ElectricP | | |

Each table has 64 slots: the oscillator interpolates the slots between the key waves the ROM stores, as the hardware did, and slots 60-63 are the classic waves every PPG table ends with (triangle, pulse, rectangular, sawtooth). The PPG's Sync (28) and PWM (29) tables were computed by the synthesizer rather than stored, so they are not reproduced.

### 🔊 State Variable Filter
- Four filter types: **LP12**, **LP24**, **HP12**, **BP12**
- Resonance control
- Dedicated ADSR envelope with bipolar amount
- Key tracking (0-100%)
- LFO modulation (±100%)
- Velocity sensitivity (0-100%)

### 📊 Dual ADSR Envelopes
- **Amp Envelope**: Controls voice amplitude
- **Filter Envelope**: Modulates filter cutoff
- Independent Attack, Decay, Sustain, Release controls
- Smooth exponential curves

### 🔄 Global LFO
- **Six shapes**: Sine, Triangle, Saw Up, Saw Down, Square, Sample & Hold
- Assignable to:
  - Wavetable morph position (±100%)
  - Filter cutoff (±100%)
- Adjustable rate (0.05Hz - 20Hz)

### 🎛️ MOD HUB
Centralized modulation routing with 8 destinations:

1. **LFO Speed**: 0-100 (maps to 0.05-20Hz)
2. **LFO Shape**: Sine, Tri, Saw+, Saw-, Square, S&H
3. **LFO → Morph**: ±100% wavetable position modulation (50 = center)
4. **LFO → Filter**: ±100% cutoff modulation (50 = center)
5. **Velocity → Filter**: 0-100% velocity sensitivity
6. **Key Tracking**: 0-100% (center at middle C)
7. **Osc B Detune**: ±32 cents (50 = center)
8. **Pitch Bend Range**: ±2, ±7, ±12, or ±24 semitones

### 🎵 6 Factory Presets
- **Glass Keys**: Bright bell-like plucks with LFO morph
- **Dust Pad**: Atmospheric evolving pad with resonance
- **Sync Bass**: Aggressive sync lead with detune
- **Noise Sweep**: Sweeping noise FX with high-pass filter
- **Pluck**: Short percussive plucks with fast attack
- **PWM Lead**: Classic pulse lead with modulation

---

## Parameters

### Page 1: Oscillator A
| Parameter | Range | Description |
|-----------|-------|-------------|
| **A BANK** | 0-15 | Wavetable bank selection |
| **A MORPH** | 0-127 | Wave position/morphing |
| **A OCT** | -3 to +3 | Octave transpose |
| **A TUNE** | -64 to +63 | Fine tuning (cents) |

### Page 2: Oscillator B
| Parameter | Range | Description |
|-----------|-------|-------------|
| **B BANK** | 0-15 | Wavetable bank selection |
| **B MORPH** | 0-127 | Wave position/morphing |
| **B OCT** | -3 to +3 | Octave transpose |
| **OSC MOD** | 0-2 | PPG mode (HiFi/LoFi/Raw) |

### Page 3: Filter
| Parameter | Range | Description |
|-----------|-------|-------------|
| **CUTOFF** | 0-127 | Filter cutoff frequency |
| **RESO** | 0-127 | Filter resonance/Q |
| **FLT ENV** | 0-127 | Filter envelope amount (bipolar) |
| **FLT TYPE** | 0-3 | LP12/LP24/HP12/BP12 |

### Page 4: Amp Envelope
| Parameter | Range | Description |
|-----------|-------|-------------|
| **ATTACK** | 0-127 | Envelope attack time |
| **DECAY** | 0-127 | Envelope decay time |
| **SUSTAIN** | 0-127 | Envelope sustain level |
| **RELEASE** | 0-127 | Envelope release time |

### Page 5: Filter Envelope
| Parameter | Range | Description |
|-----------|-------|-------------|
| **F.ATTACK** | 0-127 | Filter attack time |
| **F.DECAY** | 0-127 | Filter decay time |
| **F.SUSTAIN** | 0-127 | Filter sustain level |
| **F.RELEASE** | 0-127 | Filter release time |

### Page 6: MOD HUB & Output
| Parameter | Range | Description |
|-----------|-------|-------------|
| **MOD SEL** | 0-7 | Select modulation destination |
| **MOD VAL** | 0-100 | Value for selected destination |
| **OSC MIX** | -64 to +63 | A/B oscillator balance |
| **SPACE** | -64 to +63 | Stereo width |

---

## Sound Design Tips

### 🎨 Creating Classic PPG Sounds

**Glass Pads:**
1. Set both oscillators to **BRIGHT** or **BELL** banks
2. Detune Osc B by +10 cents
3. Set OSC MODE to **HiFi** for smooth sound
4. Add LFO to morph for movement
5. Use long attack/release times

**Digital Basses:**
1. Choose **SYNC** or **CLIPPR** banks
2. Set OSC MODE to **Raw** for grit
3. Short envelope (attack=5, decay=40, sustain=70, release=25)
4. Add key tracking for brightness
5. Use LP24 filter with moderate resonance

**Evolving Textures:**
1. Use different banks for Osc A and B (e.g., VOCAL1 + ALIEN)
2. Set slow LFO to morph (rate=30, depth=±80%)
3. Add LFO to filter (±40%)
4. Long envelopes with high sustain
5. Experiment with **LoFi** mode for character

**Percussive Plucks:**
1. **BELL** or **ORGAN** banks
2. Fast attack (0-5), short decay (20-40), low sustain (20-40)
3. Filter envelope with negative amount for brightness decay
4. High key tracking (80-100%)
5. OSC MODE: **HiFi**

### 🎛️ MOD HUB Workflow

The MOD HUB provides a centralized way to manage modulation:

1. **Turn MOD SEL** to choose a destination (e.g., "LFO>MRP")
2. **Adjust MOD VAL** to set the amount
3. Display shows the current value (rate, %, cents, etc.)
4. All 8 destinations remain active simultaneously

**Common Routings:**
- **LFO>MRP + LFO>FLT**: Organic movement on both timbre and tone
- **VEL>FLT + KEY TRK**: Dynamic brightness that responds to playing
- **B DETUNE + OSC MIX**: Subtle chorus/ensemble effect

---

## Installation

1. Download the latest `.drmlgunit` file from the [Releases](https://github.com/cldmnky/drumlogue-units/releases) page
2. Connect your Korg drumlogue to your computer via USB
3. Copy the file to `Units/Synth/` on the drumlogue
4. Safely eject and restart the drumlogue

---

## Technical Details

- **Voices**: 2-voice polyphony
- **Sample Rate**: 48kHz
- **Binary Size**: ~30KB
- **RAM Usage**: ~1KB
- **Optimizations**: NEON SIMD for stereo output stage
- **Voice Allocation**: Round-robin with oldest-note stealing

---

## Credits

- **Inspired by**: VAST Dynamics Vaporizer2
- **Wavetable techniques**: Mutable Instruments Plaits
- **PPG wavetable data**: Mutable Instruments codebase
- **Developer**: CLDM (0x434C444D)

---

## Version History

- **v1.1.0** (2026-02-07): MOD HUB update, waveform refresh, and smoother morphing
- **v1.0.0** (December 2025): Initial release

---

## Support

- [GitHub Repository](https://github.com/cldmnky/drumlogue-units)
- [Issue Tracker](https://github.com/cldmnky/drumlogue-units/issues)
- [Documentation](https://cldmnky.github.io/drumlogue-units/)
