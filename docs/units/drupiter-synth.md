---
layout: unit
title: Drupiter Synth
unit_name: drupiter-synth
unit_type: synth
version: v1.1.0
icon: 🪐
tagline: Jupiter-8 inspired polyphonic/monophonic synthesizer
description: Classic analog-style synthesis with dual DCOs, multi-mode filter, hard sync, cross-modulation, and comprehensive modulation capabilities. Now with polyphonic and unison modes.
permalink: /units/drupiter-synth/
---

# Drupiter Synth

**Jupiter-8 Inspired Polyphonic/Monophonic Synthesizer**

Drupiter brings the classic Roland Jupiter-8 sound to the Korg drumlogue with dual oscillators, a multi-mode filter, and comprehensive modulation capabilities. Now featuring polyphonic and unison modes alongside the original monophonic operation. Built on Bristol Jupiter-8 emulation, it delivers rich, warm analog-style tones perfect for bass, leads, pads, and more.

---

## Features

### 🎹 Synthesis Modes
- **Polyphonic Mode:** 4-voice polyphony with intelligent voice allocation
- **Unison Mode:** Rich ensemble effects with golden ratio detuning and stereo spread
- **Mono Mode:** Classic monophonic operation with portamento/glide

### 🎹 Dual DCO Architecture
- **5 Waveforms per oscillator:** SAW, SQR, PULSE, TRI, SAW_PWM (DCO1), SAW, NOISE, PULSE, SINE, SAW_PWM (DCO2)
- **DCO-1 octave selection:** 16', 8', 4'
- **DCO-2 octave selection:** 32', 16', 8', 4'
- **Pulse width control** for dynamic timbres
- **SAW_PWM:** Pulse-width modulated sawtooth for evolving timbral character
- **DCO2 fine tuning** for thick, chorused sounds
- **Oscillator mix** with independent level controls

### ⚡ Modulation Synthesis
- **Hard Sync:** DCO2 syncs to DCO1 (OFF/SOFT/HARD modes)
- **Cross-Modulation:** DCO2 → DCO1 FM for complex harmonic content
- Variable amount controls for both effects

### 🔊 Multi-Mode Filter
- **State-variable filter** with resonance
- **Keyboard tracking** (0-120% adjustable)
- **Envelope amount** (±100%)
- **Self-oscillation** capability

### 📊 Dual ADSR Envelopes
- **Filter Envelope:** Controls filter cutoff modulation
- **Amp Envelope:** Controls voice amplitude
- Independent Attack, Decay, Sustain, Release for each
- Smooth exponential curves

### 🌀 Global LFO & Modulation Hub
- **LFO Rate:** 0.1Hz - 50Hz adjustable
- **Modulation Hub:** 18 modulation destinations including:
  - Synth mode switching (poly/mono/unison)
  - Unison detuning and stereo spread
  - Envelope to pitch modulation
  - Portamento/glide control
  - Filter and amplitude modulation

### 🎛️ Effects
- **Chorus/Space effects** with dry/wet/both modes
- **Stereo widening** capabilities

---

## Factory Presets

Drupiter includes **12 factory presets** covering common synthesizer sounds:

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

---

## Parameters

Drupiter has **24 parameters** organized across **6 pages**. Use the drumlogue's page buttons to navigate.

### Page 1: DCO-1 (Oscillator 1)

| Parameter | Range | Description |
|-----------|-------|-------------|
| **D1 OCT** | 0-2 | Oscillator 1 octave (16', 8', 4') |
| **D1 WAVE** | 0-4 | Waveform: SAW, SQR, PULSE, TRI, SAW_PWM |
| **D1 PW** | 0-100% | Pulse width (affects PULSE waveform) |
| **XMOD** | 0-100% | Cross-modulation depth (DCO2 → DCO1 FM) |

### Page 2: DCO-2 (Oscillator 2)

| Parameter | Range | Description |
|-----------|-------|-------------|
| **D2 OCT** | 0-3 | Oscillator 2 octave (32', 16', 8', 4') |
| **D2 WAVE** | 0-4 | Waveform: SAW, NOISE, PULSE, SINE, SAW_PWM |
| **D2 TUNE** | 0-100 | Fine tuning (-50 to +50 cents, 50=center) |
| **SYNC** | 0-2 | Oscillator sync: OFF, SOFT, HARD |

### Page 3: Mix & Filter

| Parameter | Range | Description |
|-----------|-------|-------------|
| **MIX** | 0-100% | Oscillator mix (0=DCO1 only, 100=DCO2 only) |
| **CUTOFF** | 0-100% | Filter cutoff frequency |
| **RESO** | 0-100% | Filter resonance |
| **KEYFLW** | 0-100% | Keyboard tracking (0-120%, 50=half tracking) |

### Page 4: Filter Envelope

| Parameter | Range | Description |
|-----------|-------|-------------|
| **F.ATK** | 0-100% | Filter envelope attack time |
| **F.DCY** | 0-100% | Filter envelope decay time |
| **F.SUS** | 0-100% | Filter envelope sustain level |
| **F.REL** | 0-100% | Filter envelope release time |

### Page 5: Amp Envelope

| Parameter | Range | Description |
|-----------|-------|-------------|
| **A.ATK** | 0-100% | Amp envelope attack time |
| **A.DCY** | 0-100% | Amp envelope decay time |
| **A.SUS** | 0-100% | Amp envelope sustain level |
| **A.REL** | 0-100% | Amp envelope release time |

### Page 6: Modulation Hub

| Parameter | Range | Description |
|-----------|-------|-------------|
| **LFO RT** | 0-100% | LFO rate (0.1Hz - 50Hz) |
| **MOD HUB** | 0-17 | Modulation destination selector |
| **MOD AMT** | 0-100 | Amount for selected modulation destination |
| **EFFECT** | 0-3 | Effect: CHORUS, SPACE, DRY, BOTH |

---

## Sound Design Tips

### Rich Bass Sounds
1. Set DCO1 to **SAW** at **16'** (sub-octave)
2. Set DCO2 to **SQR** at **8'** with slight **D2 TUNE**
3. Adjust **MIX** for balance between oscillators
4. Use filter with moderate **RESO** and **KEYFLW**
5. Short filter envelope with positive amount
6. Try the **Jupiter Bass** preset as starting point

### Cutting Leads
1. Use **PULSE** or **SQR** waveforms
2. Enable **SYNC** (SOFT or HARD) for brightness
3. Adjust **MIX** for oscillator balance
4. Moderate filter envelope amount
5. Use **XMOD** for harmonic complexity
6. Check **Sync Lead** preset

### Warm Pads
1. Mix **SAW** (DCO1) and **TRI** (DCO2)
2. Fine-tune DCO2 with **D2 TUNE**
3. Adjust **MIX** for blend
4. Long amp envelope **A.ATK** and filter **F.ATK**
5. Use **Analog Pad** preset

### Bell Tones
1. Enable **XMOD** (cross-modulation)
2. Use **SAW** or **TRI** waveforms
3. Fast filter **F.DCY** to zero sustain
4. Check **FM Bell** preset

### Sync Leads
1. Set DCO1 to **SAW** or **PULSE**
2. Set DCO2 to **SAW** or **SAW_PWM**
3. Enable **SYNC** (50-100%)
4. Adjust **MIX** for oscillator balance
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

- **Type:** Polyphonic/Monophonic Synthesizer
- **Voices:** 4-voice polyphony + unison mode
- **Oscillators:** 2 DCOs with 5 waveforms each
- **Filter:** Multi-mode state-variable filter
- **Envelopes:** 2x ADSR (VCF, VCA)
- **LFO:** 1 global LFO with modulation hub
- **Parameters:** 24 (6 pages × 4 parameters)
- **Presets:** 12 factory presets
- **Sample Rate:** 48 kHz
- **MIDI:** Full note on/off with velocity
- **CPU Usage:** High (approaches drumlogue limits)

---

## Credits

- **Based on:** Bristol Jupiter-8 Emulation
- **Developer:** CLDM (0x434C444D)
- **Built with:** Korg logue SDK
- **Inspired by:** Roland Jupiter-8

---

## Download

<div class="download-section">
  <a href="https://github.com/cldmnky/drumlogue-units/releases/download/drupiter-synth/v1.1.0/drupiter_synth.drmlgunit" class="download-button">
    ⬇️ Download Drupiter v1.1.0
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
