# Pepege-Synth - 2-Voice Polyphonic PPG Wavetable Synthesizer

A **2-voice polyphonic** wavetable synthesizer for the Korg drumlogue, featuring the authentic **PPG Wave 2.2/2.3** wavetables: the 29 tables stored in the EPROM, with the four classic waves the hardware generated at the end of every table.

## Features

- **2-Voice Polyphony** - Play chords with intelligent voice allocation and oldest-note stealing
- **Authentic PPG Wavetables** - the 29 wavetables stored in the PPG Wave 2.2/2.3 EPROM, loaded from the original `(wave index, slot)` key-wave directory
- **64 Slots per Table** - the oscillator interpolates between the stored key waves; slots 60-63 are the classic waves the hardware generated (triangle, pulse, rectangular, sawtooth)
- **Three PPG Modes** - HiFi (smooth), LoFi (stepped), Raw (authentic 8-bit crunch)
- **Dual Oscillators** - Two independent oscillators with separate bank/morph/octave controls
- **State Variable Filter** - LP12, LP24, HP12, BP12 modes with resonance
- **Exponential Envelopes** - True analog-style RC circuit envelope curves
- **Stereo Width** - SPACE parameter for mono to wide stereo spread
- **LFO Modulation** - Morph position modulation for evolving sounds

## Wavetable Tables

The A/B BANK parameters select one of the 29 wavetables stored in the EPROM,
in the PPG's own order:

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

Every table ends with the four classic waves the synthesizer generated
(they are not stored in the ROM):

| Slot | Wave |
|------|------|
| 60 | triangle |
| 61 | pulse |
| 62 | rectangular |
| 63 | sawtooth |

The pulse width is our choice because the hardware's ROM value is not
published. Slots 60-63 replace the interpolation the table definition would
otherwise produce there, exactly as the hardware overrode them.

The PPG's Sync (28) and PWM (29) tables are computed by the hardware rather
than stored in the ROM, so they are not reproduced here.

## Parameters

### Page 1: Oscillator A

| Parameter | Range | Description |
|-----------|-------|-------------|
| A BANK | 0-28 | PPG wavetable selection (see the table above) |
| A MORPH | 0-127 | Position within the table (127 = slot 63, the classic sawtooth) |
| A OCT | -3 to +3 | Octave offset |
| A TUNE | -64 to +63 | Fine tuning in cents |

### Page 2: Oscillator B

| Parameter | Range | Description |
|-----------|-------|-------------|
| B BANK | 0-28 | PPG wavetable selection |
| B MORPH | 0-127 | Position within table |
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

- **2-voice polyphony** with per-voice oscillators, envelopes, and filters
- **Voice allocation**: Prefers free voices, retriggers same note, steals oldest when full
- **PPG wavetables**: the 29 EPROM tables, loaded as 64-slot interpolated wavetables from 256 stored 64-sample 8-bit waves
- **Exponential envelopes**: True RC-circuit modeling with overshoot on attack

### Performance

- Sample rate: 48kHz
- Output: Stereo with configurable width
- Binary size: ~39KB
- CPU: ~89us per 64-frame callback with two notes held (about 7% of the
  1.2M-cycle callback budget at 900MHz), ~27us idle. Measured by building the
  unit with the drumlogue toolchain (GCC 6.5) and running it under qemu-arm,
  i.e. indicative rather than a hardware measurement. Before the libm was
  taken out of the render loop it was ~345us per callback; see
  `test/pepege-synth/perf_test.cc` (`make perf-test`) to measure it again.
- Voice count is `kNumVoices` in `pepege_synth.h` (2, for CPU headroom)

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
