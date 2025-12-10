# Pepege-Synth - 4-Voice Polyphonic PPG Wavetable Synthesizer

A **4-voice polyphonic** wavetable synthesizer for the Korg drumlogue, featuring authentic **PPG Wave 2.2/2.3** wavetables with 256 classic 8-bit waveforms across 16 banks.

## Features

- **4-Voice Polyphony** - Play chords with intelligent voice allocation and oldest-note stealing
- **Authentic PPG Wavetables** - 256 waveforms from the legendary PPG Wave synthesizers
- **16 Wavetable Banks** - Upper WT, Resonant, Mellow, Bright, Harsh, Sync, PWM, Vocal, Organ, Bell, and more
- **Three PPG Modes** - HiFi (smooth), LoFi (stepped), Raw (authentic 8-bit crunch)
- **Dual Oscillators** - Two independent oscillators with separate bank/morph/octave controls
- **State Variable Filter** - LP12, LP24, HP12, BP12 modes with resonance
- **Exponential Envelopes** - True analog-style RC circuit envelope curves
- **Stereo Width** - SPACE parameter for mono to wide stereo spread
- **LFO Modulation** - Morph position modulation for evolving sounds

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

## Parameters

### Page 1: Oscillator A

| Parameter | Range | Description |
|-----------|-------|-------------|
| A BANK | 0-15 | PPG wavetable bank selection |
| A MORPH | 0-127 | Position within bank (morphs between 8 waves) |
| A OCT | -3 to +3 | Octave offset |
| A TUNE | -64 to +63 | Fine tuning in cents |

### Page 2: Oscillator B

| Parameter | Range | Description |
|-----------|-------|-------------|
| B BANK | 0-15 | PPG wavetable bank selection |
| B MORPH | 0-127 | Position within bank |
| B OCT | -3 to +3 | Octave offset relative to A |
| OSC MOD | HiFi/LoFi/Raw | PPG interpolation mode |

### Page 3: Filter

| Parameter | Range | Description |
|-----------|-------|-------------|
| CUTOFF | 0-127 | Filter cutoff frequency |
| RESO | 0-127 | Filter resonance |
| FLT ENV | -64 to +63 | Filter envelope amount (bipolar) |
| FLT TYP | LP12/LP24/HP12/BP12 | Filter type |

### Page 4: Amplitude Envelope

| Parameter | Range | Description |
|-----------|-------|-------------|
| ATTACK | 0-127 | Attack time (1ms - 8s, exponential) |
| DECAY | 0-127 | Decay time (5ms - 12s, exponential) |
| SUSTAIN | 0-127 | Sustain level |
| RELEASE | 0-127 | Release time (5ms - 12s, exponential) |

### Page 5: Filter Envelope

| Parameter | Range | Description |
|-----------|-------|-------------|
| F.ATK | 0-127 | Filter attack time |
| F.DCY | 0-127 | Filter decay time |
| F.SUS | 0-127 | Filter sustain level |
| F.REL | 0-127 | Filter release time |

### Page 6: Modulation & Output

| Parameter | Range | Description |
|-----------|-------|-------------|
| LFO RT | 0-127 | LFO speed |
| LFO>MRP | -64 to +63 | LFO to morph modulation depth |
| OSC MIX | 0-127 | Balance between Osc A (0) and B (127) |
| SPACE | 0-127 | Stereo width (0=mono, 64=normal, 127=wide) |

## PPG Oscillator Modes

| Mode | Description |
|------|-------------|
| **HiFi** | Bilinear interpolation - smooth morphing between waves and samples |
| **LoFi** | Sample interpolation only - stepped wave transitions, smoother samples |
| **Raw** | No interpolation - authentic PPG 8-bit crunch with aliasing artifacts |

## Technical Details

### Architecture

- **4-voice polyphony** with per-voice oscillators, envelopes, and filters
- **Voice allocation**: Prefers free voices, retriggers same note, steals oldest when full
- **PPG wavetables**: 256 waves × 256 samples × 8-bit = 64KB wave data
- **Exponential envelopes**: True RC-circuit modeling with overshoot on attack

### Performance

- Sample rate: 48kHz
- Output: Stereo with configurable width
- Binary size: ~29KB
- CPU usage: ~6% per voice (~24% max with 4 voices)

### NEON Optimizations

- Stereo widening uses ARM NEON SIMD
- Buffer sanitization and gain control vectorized
- Efficient voice mixing

## Building

```bash
./build.sh pepege-synth
```

The build produces `pepege-synth.drmlgunit` (~30KB) which can be copied to the drumlogue via USB.

## Installation

1. Connect drumlogue to computer via USB
2. Copy `pepege-synth.drmlgunit` to `Units/Synth/` folder
3. Eject and disconnect
4. Select Pepege-Synth from the synth menu

## Credits

- **PPG Wave** by Wolfgang Palm - Original wavetable synthesis pioneer
- **Mutable Instruments** - DSP techniques and inspiration
- **logue SDK** by KORG - Platform SDK

## License

This project is licensed under the MIT License - see the [LICENSE](../../LICENSE) file for details.
