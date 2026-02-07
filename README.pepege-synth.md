# Pepege-Synth for Korg drumlogue

A 2-voice polyphonic PPG wavetable synthesizer for the Korg drumlogue, featuring authentic PPG Wave 2.2/2.3 wavetables with 256 classic 8-bit waveforms across 16 banks. Inspired by VAST Dynamics Vaporizer2, it delivers the legendary PPG sound with modern polyphony and modulation capabilities.

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

## Wavetable Banks

| Bank | Name | Character |
|------|------|-----------|
| 0 | UPPER_WT | Classic PPG upper wavetable |
| 1 | RESONANT1 | Resonant filter-like tones |
| 2 | RESONANT2 | More resonant variations |
| 3 | MELLOW | Soft, warm sounds |
| 4 | BRIGHT | Cutting, present tones |
| 5 | HARSH | Aggressive, distorted |
| 6 | CLIPPER | Hard-clipped waveforms |
| 7 | SYNC | Oscillator sync timbres |
| 8 | PWM | Pulse width modulation |
| 9 | VOCAL1 | Formant/vocal sounds |
| 10 | VOCAL2 | More vocal timbres |
| 11 | ORGAN | Organ-like harmonics |
| 12 | BELL | Bell/metallic tones |
| 13 | ALIEN | Strange, unusual sounds |
| 14 | NOISE | Noise-based textures |
| 15 | SPECIAL | Special effect waveforms |

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