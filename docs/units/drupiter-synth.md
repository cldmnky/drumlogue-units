---
layout: unit
title: Drupiter Synth
unit_name: drupiter-synth
unit_type: synth
version: v1.0.0
icon: 🪐
tagline: Jupiter-8 inspired monophonic synthesizer
description: Classic analog-style synthesis with dual DCOs, multi-mode filter, hard sync, cross-modulation, and comprehensive modulation capabilities.
permalink: /units/drupiter-synth/
---

# Drupiter Synth

**Jupiter-8 Inspired Monophonic Synthesizer**

Drupiter brings the classic Roland Jupiter-8 sound to the Korg drumlogue with dual oscillators, a multi-mode filter, and comprehensive modulation capabilities. Built on Bristol Jupiter-8 emulation, it delivers rich, warm analog-style tones perfect for bass, leads, pads, and more.

---

## Features

### 🎹 Dual DCO Architecture
- **4 Waveforms per oscillator:** Ramp, Square, Pulse, Triangle
- **Independent octave selection:** 16', 8', 4' for each DCO
- **Pulse width control** for dynamic timbres
- **DCO2 detune** for thick, chorused sounds
- **Mixer** with independent level controls

### ⚡ Modulation Synthesis
- **Hard Sync:** DCO1 → DCO2 for aggressive timbres
- **Cross-Modulation:** DCO2 → DCO1 FM for complex harmonic content
- Variable amount controls for both effects

### 🔊 Multi-Mode Filter
- **Four filter types:** LP12, LP24, HP12, BP12
- **Self-oscillation** capability
- **Envelope amount** (±100%)
- **Keyboard tracking** (50% default)
- **Resonance** control

### 📊 Dual ADSR Envelopes
- **VCF Envelope:** Controls filter cutoff modulation
- **VCA Envelope:** Controls voice amplitude
- Independent Attack, Decay, Sustain, Release for each
- Smooth exponential curves

### 🌀 Global LFO
- **4 Waveforms:** Triangle, Sawtooth, Square, S&H
- **LFO Delay:** Gradual fade-in for vibrato effects
- **Rate control:** 0.1Hz - 20Hz
- Assignable to filter cutoff and pitch

### 🎛️ Modulation Matrix
- **ENV → Filter:** Bipolar envelope amount
- **LFO → Filter:** Variable modulation depth
- **Key Tracking:** Natural filter response across keyboard
- **Velocity → VCA:** Dynamic expression

---

## Factory Presets

Drupiter includes **6 factory presets** covering common synthesizer sounds:

| # | Name | Description |
|---|------|-------------|
| **1** | **Jupiter Bass** | Deep, punchy bass with filter envelope |
| **2** | **Jupiter Lead** | Bright, cutting lead with sync |
| **3** | **Sync Lead** | Hard-synced aggressive lead sound |
| **4** | **FM Bell** | Cross-modulated bell tones |
| **5** | **Analog Pad** | Warm, evolving pad with slow attack |
| **6** | **Poly Arp** | Bright arpeggiator-style sound |

---

## Parameters

Drupiter has **24 parameters** organized across **6 pages**. Use the drumlogue's page buttons to navigate.

### Page 1: DCO-1 (Oscillator 1)

| Parameter | Range | Description |
|-----------|-------|-------------|
| **D1 OCT** | 0-127 | Oscillator 1 octave (16', 8', 4') |
| **D1 WAVE** | RAMP/SQR/PULSE/TRI | Waveform selection |
| **D1 PW** | 0-127 | Pulse width (affects PULSE waveform) |
| **D1 LEVEL** | 0-127 | Oscillator 1 volume in mix |

### Page 2: DCO-2 (Oscillator 2)

| Parameter | Range | Description |
|-----------|-------|-------------|
| **D2 OCT** | 0-127 | Oscillator 2 octave (16', 8', 4') |
| **D2 WAVE** | RAMP/SQR/PULSE/TRI | Waveform selection |
| **D2 PW** | 0-127 | Pulse width (affects PULSE waveform) |
| **D2 LEVEL** | 0-127 | Oscillator 2 volume in mix |

### Page 3: Modulation

| Parameter | Range | Description |
|-----------|-------|-------------|
| **DETUNE** | 0-127 | DCO2 fine tuning (±50 cents) |
| **H SYNC** | 0-127 | Hard sync amount (DCO1 → DCO2) |
| **X MOD** | 0-127 | Cross-modulation depth (DCO2 → DCO1) |
| **NOISE** | 0-127 | White noise level |

### Page 4: VCF (Filter)

| Parameter | Range | Description |
|-----------|-------|-------------|
| **VCF MODE** | LP12/LP24/HP12/BP12 | Filter type |
| **CUTOFF** | 0-127 | Filter cutoff frequency |
| **RES** | 0-127 | Filter resonance |
| **ENV AMT** | -64/+63 | Filter envelope amount (bipolar) |

### Page 5: VCF Envelope

| Parameter | Range | Description |
|-----------|-------|-------------|
| **VCF ATK** | 0-127 | Filter envelope attack time |
| **VCF DEC** | 0-127 | Filter envelope decay time |
| **VCF SUS** | 0-127 | Filter envelope sustain level |
| **VCF REL** | 0-127 | Filter envelope release time |

### Page 6: VCA Envelope & LFO

| Parameter | Range | Description |
|-----------|-------|-------------|
| **VCA ATK** | 0-127 | Amp envelope attack time |
| **VCA DEC** | 0-127 | Amp envelope decay time |
| **VCA SUS** | 0-127 | Amp envelope sustain level |
| **VCA REL** | 0-127 | Amp envelope release time |

*Note: LFO parameters are accessed via dedicated drumlogue LFO controls.*

---

## Sound Design Tips

### Rich Bass Sounds
1. Set DCO1 to **Ramp** at **16'** (sub-octave)
2. Set DCO2 to **Square** at **8'** with slight **DETUNE**
3. Use **LP24** filter with moderate resonance
4. Short **VCF envelope** with positive amount
5. Try the **Jupiter Bass** preset as starting point

### Cutting Leads
1. Use **Pulse** or **Square** waveforms
2. Enable **Hard Sync** for brightness
3. **LP12** or **BP12** filter for character
4. Moderate **VCF envelope** amount
5. Consider **X MOD** for harmonic complexity

### Warm Pads
1. Mix **Ramp** (DCO1) and **Triangle** (DCO2)
2. Detune DCO2 slightly
3. **LP24** filter with low resonance
4. Long **VCA attack** and **VCF attack**
5. Use **Analog Pad** preset

### Bell Tones
1. Enable **Cross-Modulation** (X MOD)
2. Use **Ramp** or **Triangle** waveforms
3. **BP12** or **HP12** filter
4. Fast **VCF decay** to zero sustain
5. Check **FM Bell** preset

### Sync Leads
1. Set DCO1 to **Ramp** or **Pulse**
2. Set DCO2 to **Ramp** or **Saw**
3. Enable **Hard Sync** (50-100%)
4. **LP12** filter with moderate cutoff
5. Modulate filter with envelope

---

## Installation

1. Download `drupiter_synth.drmlgunit` from the [releases page](https://github.com/cldmnky/drumlogue-units/releases)
2. Connect your drumlogue to your computer via USB
3. The drumlogue will appear as a USB drive
4. Copy the `.drmlgunit` file to the `Units/` folder
5. Safely eject the drumlogue
6. Power cycle the device
7. Navigate to the synth selection menu
8. Select **"Drupiter"**

---

## Technical Specifications

- **Type:** Monophonic Synthesizer
- **Voices:** 1 (monophonic)
- **Oscillators:** 2 DCOs with 4 waveforms each
- **Filter:** Multi-mode state-variable (12/24dB LP, 12dB HP/BP)
- **Envelopes:** 2x ADSR (VCF, VCA)
- **LFO:** 1 global with 4 waveforms
- **Parameters:** 24 (6 pages × 4 parameters)
- **Presets:** 6 factory presets
- **Sample Rate:** 48 kHz
- **MIDI:** Full note on/off with velocity

---

## Credits

- **Based on:** Bristol Jupiter-8 Emulation
- **Developer:** CLDM (0x434C444D)
- **Built with:** Korg logue SDK
- **Inspired by:** Roland Jupiter-8

---

## Download

<div class="download-section">
  <a href="https://github.com/cldmnky/drumlogue-units/releases/download/drupiter-synth/v1.0.0/drupiter_synth.drmlgunit" class="download-button">
    ⬇️ Download Drupiter v1.0.0
  </a>
  <p class="download-info">File: drupiter_synth.drmlgunit • Size: 31 KB</p>
</div>

---

## Documentation

- [README.md](https://github.com/cldmnky/drumlogue-units/blob/main/drumlogue/drupiter-synth/README.md) - Detailed documentation
- [UI.md](https://github.com/cldmnky/drumlogue-units/blob/main/drumlogue/drupiter-synth/UI.md) - User interface guide
- [RELEASE_NOTES.md](https://github.com/cldmnky/drumlogue-units/blob/main/drumlogue/drupiter-synth/RELEASE_NOTES.md) - Version history

---

## Support

Found a bug or have a feature request? Please open an issue on the [GitHub repository](https://github.com/cldmnky/drumlogue-units/issues).
