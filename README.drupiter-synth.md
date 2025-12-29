# Drupiter Synth for Korg drumlogue

A versatile Jupiter-8 inspired synthesizer for the Korg drumlogue, featuring dual oscillators, multi-mode synthesis (mono/polyphonic/unison), expanded waveforms, and comprehensive modulation capabilities. Based on Bristol Jupiter-8 emulation, it delivers rich, warm analog-style tones perfect for bass, leads, pads, and complex polyphonic arrangements.

## Features

- **Multi-Mode Synthesis**: Mono, Polyphonic, and Unison modes with intelligent voice allocation
- **Dual DCOs** with 5 waveforms each (Ramp, Square, Pulse, Triangle, PWM Sawtooth)
- **Hard Sync** (DCO1 → DCO2) for aggressive timbres
- **Cross-Modulation** (DCO2 → DCO1 FM) for complex harmonic content
- **Unison Oscillator** with golden ratio detuning and stereo spread
- **ENV TO PITCH** modulation (±12 semitones)
- **Portamento/Glide** with true legato detection
- **Multi-Mode Filter** (LP12, LP24, HP12, BP12) with self-oscillation
- **Dual ADSR Envelopes** for VCF and VCA
- **Extended Modulation Hub** (18 destinations)
- **12 Factory Presets** covering all synthesis modes
- **LFO** with 4 waveforms and delay envelope
- **Keyboard Tracking** (50% default) for natural filter response

## Installation

1. Download the `drupiter_synth.drmlgunit` file
2. Connect your drumlogue to your computer via USB
3. Copy the `.drmlgunit` file to the `Units/Synth/` folder on the drumlogue
4. Safely eject the drumlogue and restart it
5. The synth will appear as "Drupiter" in the Synth slot selection

## Parameters

The unit has 24 parameters organized across 6 pages:

### Page 1: DCO-1 (Oscillator 1)

| Parameter | Range | Description |
|-----------|-------|-------------|
| **WAVE** | 0-4 | Waveform: SAW, SQR, PULSE, TRI, SAW_PWM |
| **OCTAVE** | 16', 8', 4' | Oscillator octave |
| **LEVEL** | 0-127 | Oscillator level in mixer |
| **DETUNE** | -64 to +63 | Fine tuning (±1 semitone) |

### Page 2: DCO-2 (Oscillator 2)

| Parameter | Range | Description |
|-----------|-------|-------------|
| **WAVE** | 0-4 | Waveform: SAW, SQR, PULSE, TRI, SAW_PWM |
| **OCTAVE** | 16', 8', 4' | Oscillator octave |
| **LEVEL** | 0-127 | Oscillator level in mixer |
| **DETUNE** | -64 to +63 | Fine tuning (±1 semitone) |

### Page 3: Mixer & Modulation

| Parameter | Range | Description |
|-----------|-------|-------------|
| **DCO1 LVL** | 0-127 | DCO1 level |
| **DCO2 LVL** | 0-127 | DCO2 level |
| **HARD SYNC** | 0-127 | Hard sync amount (DCO1 → DCO2) |
| **CROSS MOD** | 0-127 | Cross-modulation amount (DCO2 → DCO1) |

### Page 4: Filter & Envelopes

| Parameter | Range | Description |
|-----------|-------|-------------|
| **FILTER** | 0-3 | Filter type: LP12, LP24, HP12, BP12 |
| **CUTOFF** | 0-127 | Filter cutoff frequency |
| **RESONANCE** | 0-127 | Filter resonance |
| **ENV AMT** | -64 to +63 | Filter envelope amount |

### Page 5: Envelopes & LFO

| Parameter | Range | Description |
|-----------|-------|-------------|
| **ATTACK** | 0-127 | Envelope attack time |
| **DECAY** | 0-127 | Envelope decay time |
| **SUSTAIN** | 0-127 | Envelope sustain level |
| **RELEASE** | 0-127 | Envelope release time |

### Page 6: Global Controls

| Parameter | Range | Description |
|-----------|-------|-------------|
| **MODE** | 0-2 | Synthesis mode: MONO, POLY, UNISON |
| **GLIDE** | 0-127 | Portamento time |
| **LFO RATE** | 0-127 | LFO speed |
| **LFO SHAPE** | 0-3 | LFO waveform |

## Factory Presets

Drupiter includes 12 factory presets covering all synthesis modes:

| # | Name | Description |
|---|------|-------------|
| **1** | **Jupiter Bass** | Deep, punchy bass with filter envelope |
| **2** | **Jupiter Lead** | Bright, cutting lead with sync |
| **3** | **Sync Lead** | Hard-synced aggressive lead sound |
| **4** | **FM Bell** | Cross-modulated bell tones |
| **5** | **Analog Pad** | Warm, evolving pad with slow attack |
| **6** | **Poly Arp** | Bright arpeggiator-style sound |
| **7** | **PWM Lead** | Pulse-width modulated sawtooth lead |
| **8** | **Unison Pad** | Rich ensemble pad with golden ratio detuning |
| **9** | **Poly Bass** | Polyphonic bass with glide |
| **10** | **ENV FM** | Filter envelope modulating pitch |
| **11** | **Stereo Unison** | Wide stereo ensemble effects |
| **12** | **Glide Lead** | Smooth portamento lead sound |

## CPU Usage

- **Mono Mode**: Low CPU usage (~15-20%)
- **Poly Mode**: Moderate CPU usage (~25-35%)
- **Unison Mode**: Moderate CPU usage (~30-40%)

## Credits

- **DSP Algorithms**: Based on Bristol Jupiter-8 emulation
- **Original License**: GPL/MIT (depending on components)
- **drumlogue Port**: Adapted for Korg logue SDK

## Version History

- **v1.1.0** - Polyphonic Synthesis Update
  - Polyphonic and Unison modes with voice allocation
  - 5 waveforms per oscillator including SAW_PWM
  - ENV TO PITCH modulation (±12 semitones)
  - Portamento/glide with legato detection
  - Extended modulation hub (18 destinations)
  - 12 factory presets covering all modes
  - Complete DSP architecture rewrite

- **v1.0.0** - Initial Release
  - Monophonic Jupiter-8 synthesis
  - Dual DCOs with 4 waveforms each
  - Hard sync and cross-modulation
  - Multi-mode filter with ADSR envelopes
  - 6 factory presets
  - Basic modulation capabilities

## License

This unit is based on code from Bristol, released under the GPL License. See the LICENSE file for details.