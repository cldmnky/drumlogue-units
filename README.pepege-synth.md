# Pepege-Synth for Korg drumlogue

A 2-voice polyphonic PPG wavetable synthesizer for the Korg drumlogue, featuring the authentic PPG Wave 2.2/2.3 wavetables: the 29 tables stored in the EPROM, plus the four classic waves the hardware generated at the end of every table. Inspired by VAST Dynamics Vaporizer2, it delivers the legendary PPG sound with modern polyphony and modulation capabilities.

## Features

- **2-Voice Polyphony** - Play chords with intelligent voice allocation
- **Authentic PPG Wavetables** - 256 waveforms from the legendary PPG Wave synthesizers
- **16 Wavetable Banks** - Upper WT, Resonant, Mellow, Bright, Harsh, Sync, PWM, Vocal, Organ, Bell, and more
- **Three PPG Modes** - HiFi (smooth), LoFi (stepped), Raw (authentic 8-bit crunch)
- **Dual Oscillators** - Two independent oscillators with separate bank/morph/octave controls
- **State Variable Filter** - LP12, LP24, HP12, BP12 modes with resonance
- **Dual ADSR Envelopes** - Independent amp and filter envelopes with exponential curves
- **Stereo Width Control** - SPACE parameter for mono to wide stereo spread
- **LFO Modulation** - Morph position modulation for evolving sounds
- **Modulation Hub** - Centralized modulation routing with 8 destinations

## Wavetable Tables

The A/B BANK parameters select one of the 29 wavetables stored in the EPROM, in the PPG's own order:

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

Each table has 64 slots. The oscillator interpolates the slots between the key waves the ROM stores, exactly as the hardware did when it loaded a table into its workspace RAM, and slots 60-63 hold the classic waves every PPG table ends with: triangle, pulse, rectangular and sawtooth. The PPG's Sync (28) and PWM (29) tables were computed by the synthesizer rather than stored, so they are not reproduced here.

## Installation

1. Download the `pepege_synth.drmlgunit` file
2. Connect your drumlogue to your computer via USB
3. Copy the `.drmlgunit` file to the `Units/Synth/` folder on the drumlogue
4. Safely eject the drumlogue and restart it
5. The synth will appear as "Pepege-Synth" in the Synth slot selection

## Parameters

The unit has 24 parameters organized across 6 pages:

### Page 1: Oscillator A

| Parameter | Range | Description |
|-----------|-------|-------------|
| **BANK** | 0-15 | PPG wavetable bank selection |
| **MORPH** | 0-127 | Position within bank (morphs between 8 waves) |
| **OCTAVE** | -3 to +3 | Octave offset |
| **TUNE** | -64 to +63 | Fine tuning in cents |

### Page 2: Oscillator B

| Parameter | Range | Description |
|-----------|-------|-------------|
| **BANK** | 0-15 | PPG wavetable bank selection |
| **MORPH** | 0-127 | Position within bank (morphs between 8 waves) |
| **OCTAVE** | -3 to +3 | Octave offset |
| **TUNE** | -64 to +63 | Fine tuning in cents |

### Page 3: Filter & Envelopes

| Parameter | Range | Description |
|-----------|-------|-------------|
| **FILTER** | 0-3 | Filter type: LP12, LP24, HP12, BP12 |
| **CUTOFF** | 0-127 | Filter cutoff frequency |
| **RESONANCE** | 0-127 | Filter resonance |
| **ATTACK** | 0-127 | Filter envelope attack time |

### Page 4: Envelopes & Global

| Parameter | Range | Description |
|-----------|-------|-------------|
| **DECAY** | 0-127 | Filter envelope decay time |
| **SUSTAIN** | 0-127 | Filter envelope sustain level |
| **RELEASE** | 0-127 | Filter envelope release time |
| **SPACE** | 0-127 | Stereo width (0 = mono, 127 = wide stereo) |

### Page 5: Amp Envelope

| Parameter | Range | Description |
|-----------|-------|-------------|
| **ATTACK** | 0-127 | Amp envelope attack time |
| **DECAY** | 0-127 | Amp envelope decay time |
| **SUSTAIN** | 0-127 | Amp envelope sustain level |
| **RELEASE** | 0-127 | Amp envelope release time |

### Page 6: Modulation Hub

| Parameter | Range | Description |
|-----------|-------|-------------|
| **LFO SPD** | 0-100 | LFO speed |
| **LFO SHAPE** | 0-5 | LFO waveform: SINE, TRI, SAW↑, SAW↓, SQR, S&H |
| **MORPH MOD** | 0-100 | LFO to wavetable morph amount (50 = center) |
| **FILTER MOD** | 0-100 | LFO to filter cutoff amount (50 = center) |
| **VEL→FILTER** | 0-100 | Velocity to filter amount |
| **KEY TRACK** | 0-100 | Filter keyboard tracking |

## PPG Modes

- **HiFi**: Smooth interpolation between wavetable positions
- **LoFi**: Stepped interpolation for vintage character
- **Raw**: Authentic 8-bit PPG crunch and artifacts

## CPU Usage

- **Low CPU usage** (~20-30%) optimized for drumlogue performance
- **Efficient polyphony** with voice stealing for seamless chord playing

## Credits

- **DSP Algorithms**: Inspired by PPG Wave synthesizers and VAST Dynamics Vaporizer2
- **Wavetable Data**: Based on authentic PPG Wave 2.2/2.3 samples
- **Original License**: MIT License
- **drumlogue Port**: Adapted for Korg logue SDK

## Version History

- **v1.1.0** - MOD HUB update and stability fixes
  - HubControl-based MOD HUB with consistent value formatting
  - MOD HUB values use 0-100 scale with preset migration
  - Updated PPG waveform resource data
  - Smoother wavetable morphing response
  - HPF state reset on voice trigger to avoid state carryover
  - Monophonic last-note priority voice allocation fix
  - Filter type clamping for safety

- **v1.0.0** - Initial Release
  - 2-voice polyphonic PPG wavetable synthesis
  - 16 wavetable banks with 256 waveforms total
  - Three PPG interpolation modes (HiFi/LoFi/Raw)
  - Dual oscillator architecture
  - State variable filter with 4 modes
  - Dual ADSR envelopes
  - Global LFO with 6 shapes
  - Modulation hub with 8 destinations
  - Stereo width control

## License

This unit is based on PPG wavetable synthesis concepts, released under the MIT License. See the LICENSE file for details.