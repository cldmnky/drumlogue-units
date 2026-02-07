# Drupiter - Jupiter-8 Synthesizer for Korg Drumlogue

**Version**: 1.2.0  
**Developer**: CLDM (0x434C444D)  
**Type**: Polyphonic/Monophonic Synthesizer Unit  
**Based on**: Bristol Jupiter-8 Emulation

---

## Overview

Drupiter is a versatile synthesizer unit for the Korg drumlogue that brings the classic Roland Jupiter-8 sound with modern enhancements. Featuring dual oscillators, multi-mode synthesis (mono/polyphonic/unison), expanded waveforms, and comprehensive modulation capabilities, Drupiter delivers rich, warm analog-style tones perfect for bass, leads, pads, and complex polyphonic arrangements.

### Key Features

- **Multi-Mode Synthesis**: Mono, Polyphonic, and Unison modes
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
- **MIDI Expression**: velocity, pitch bend, channel pressure, aftertouch
- **Knob Catch** mechanism to prevent parameter jumps on preset load

---

## Installation

1. Download `drupiter_synth.drmlgunit` from the releases page
2. Connect your drumlogue to your computer via USB
3. The drumlogue will appear as a USB drive
4. Copy the `.drmlgunit` file to the `Units/` folder on the drumlogue
5. Safely eject the drumlogue
6. Power cycle the drumlogue
7. Navigate to the synth selection and choose "Drupiter"

---

## Parameters

Drupiter has 24 parameters organized across 6 pages. Use the drumlogue's page buttons to navigate.

### Page 1: DCO-1 (Oscillator 1)

| Parameter | Range | Description |
|-----------|-------|-------------|
| **D1 OCT** | 0-2 | Oscillator 1 octave (16', 8', 4') |
| **D1 WAVE** | 0-4 | Waveform: SAW, SQR, PULSE, TRI, SAW_PWM |
| **D1 PW** | 0-100% | Pulse width (affects PULSE waveform) |
| **XMOD** | 0-100% | Cross-modulation depth (DCO2 → DCO1 FM) |

**Tips:**
- 16' = sub-bass octave (lower)
- 8' = standard pitch range
- 4' = one octave higher
- SAW_PWM creates evolving timbres with pulse width modulation

### Page 2: DCO-2 (Oscillator 2)

| Parameter | Range | Description |
|-----------|-------|-------------|
| **D2 OCT** | 0-3 | Oscillator 2 octave (32', 16', 8', 4') |
| **D2 WAVE** | 0-4 | Waveform: SAW, NOISE, PULSE, SINE, SAW_PWM |
| **D2 TUNE** | 0-100 | Fine tuning (-50 to +50 cents, 50=center) |
| **SYNC** | 0-2 | Oscillator sync: OFF, SOFT, HARD |

**Tips:**
- 32' = ultra-sub-bass octave (DCO-2 only)
- Fine tune with D2 TUNE for chorus effects
- SOFT sync = gentle locking, HARD sync = aggressive timbres
- NOISE waveform for percussive sounds

### Page 3: Mix & Filter

| Parameter | Range | Description |
|-----------|-------|-------------|
| **MIX** | 0-100% | Oscillator mix (0=DCO1 only, 100=DCO2 only) |
| **CUTOFF** | 0-100% | Filter cutoff frequency |
| **RESO** | 0-100% | Filter resonance |
| **KEYFLW** | 0-100% | Keyboard tracking (0-120%, 50=half tracking) |

**Tips:**
- MIX at 50% = equal balance between oscillators
- Higher KEYFLW = more natural filter response across keyboard
- RESO creates emphasis at cutoff frequency

### Page 4: Filter Envelope

| Parameter | Range | Description |
|-----------|-------|-------------|
| **F.ATK** | 0-100% | Filter envelope attack time |
| **F.DCY** | 0-100% | Filter envelope decay time |
| **F.SUS** | 0-100% | Filter envelope sustain level |
| **F.REL** | 0-100% | Filter envelope release time |

**Tips:**
- Fast attack = plucky, percussive filter movement
- Slow attack = gradual filter opening
- Low sustain + long decay = sweeping filter effects

### Page 5: Amp Envelope

| Parameter | Range | Description |
|-----------|-------|-------------|
| **A.ATK** | 0-100% | Amp envelope attack time |
| **A.DCY** | 0-100% | Amp envelope decay time |
| **A.SUS** | 0-100% | Amp envelope sustain level |
| **A.REL** | 0-100% | Amp envelope release time |

**Tips:**
- Fast attack = punchy, percussive sounds
- Slow attack = pad/string style sounds
- High sustain = sustained notes maintain volume

### Page 6: Modulation Hub

| Parameter | Range | Description |
|-----------|-------|-------------|
| **LFO RT** | 0-100% | LFO rate (0.1Hz - 50Hz) |
| **MOD HUB** | 0-17 | Modulation destination selector |
| **MOD AMT** | 0-100 | Amount for selected modulation destination |
| **EFFECT** | 0-3 | Effect: CHORUS, SPACE, DRY, BOTH |

**Tips:**
- MOD HUB selects from 18 modulation destinations
- Includes synth mode switching, unison controls, envelope modulation
- EFFECT adds chorus/space processing

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

## Sound Design Tips

### Creating Classic Jupiter-8 Sounds

**Fat Bass:**
1. DCO1: 16' octave, Square or Pulse wave
2. DCO2: 16' octave, slightly detuned (60-62)
3. Filter: Low cutoff (30-50), high resonance (60-80)
4. VCF Env: Fast attack, medium decay, low sustain
5. VCA Env: Fast attack, medium decay, medium sustain

**Sync Lead:**
1. DCO1: 8' octave, Ramp wave
2. DCO2: 8' octave, Square wave, detuned (70-90)
3. High D2 LEVEL for strong sync effect
4. Filter: Medium cutoff (60-80), low resonance
5. Add LFO>VCO (20-30) for vibrato

**Pad with Movement:**
1. DCO1: 8' octave, Triangle wave
2. DCO2: 8' octave, Pulse wave, slight detune (62-66)
3. Filter: High cutoff (80-100), medium resonance (40)
4. Slow VCF envelope (ATK 60, DCY 80, SUS 70, REL 80)
5. LFO>VCF (40-60) with Triangle wave

**FM-Style Bells:**
1. DCO1: 8' octave, Sine (use Triangle)
2. DCO2: 8' octave, Sine, max level for strong FM
3. Filter: HP12 mode, high cutoff
4. VCA Env: Fast attack, long decay, zero sustain
5. VCF Env: Fast attack, medium decay, low sustain

---

## Technical Specifications

- **Polyphony**: 4-voice polyphonic + unison mode
- **Sample Rate**: 48kHz
- **Bit Depth**: 32-bit float processing
- **CPU Usage**: ~15-40% (mode dependent)
- **Latency**: Zero added latency

### DSP Architecture

- **Oscillators**: Wavetable synthesis with linear interpolation
- **Filter**: Chamberlin state-variable filter
- **Envelopes**: Linear ADSR
- **LFO**: Mathematical waveform generation
- **Modulation**: Sample-rate routing

---

## Known Characteristics

### Filter Behavior
- Self-oscillation at maximum resonance (RESO > 110)
- Slight volume increase with resonance (gain compensated)
- Keyboard tracking active at 50% (opens with higher notes)

### Oscillator Behavior
- Hard sync always active (DCO1 → DCO2)
- Cross-modulation scales with DCO2 level
- Detune range: ±10 cents around center (64)

### Envelope Behavior
- Linear slopes (not exponential like analog)
- Retriggerable (new note doesn't reset level)
- Velocity sensitive (affects initial level)

---

## Troubleshooting

### Unit Doesn't Load
- Ensure file is named correctly: `drupiter_synth.drmlgunit`
- Check file is in `Units/` folder, not a subdirectory
- Try power cycling the drumlogue
- Verify drumlogue firmware is up to date

### No Sound Output
- Check VCA envelope sustain level (should be > 0)
- Verify oscillator levels (D1 LEVEL, D2 LEVEL > 0)
- Check filter cutoff isn't too low
- Ensure filter type is appropriate for sound

### Unexpected Sounds
- Check if filter is self-oscillating (RESO > 110)
- Verify LFO depths aren't too high
- Check cross-modulation (D2 LEVEL affects FM)
- Reset to factory preset and rebuild sound

---

## Credits

**Development**: Based on the Bristol Jupiter-8 emulation  
**Original Bristol**: Nick Copeland  
**Drumlogue Port**: CLDM  
**Platform**: Korg logue SDK  

### References
- [Bristol Synthesizer](https://github.com/nomadbyte/bristol-fixes)
- [Korg logue SDK](https://github.com/korginc/logue-sdk)
- [Roland Jupiter-8 Service Manual](https://www.synthfool.com/docs/Roland/Jupiter_8/)

---

## License

This unit is released under MIT License. See LICENSE file for details.

**Note**: "Jupiter-8" and "Roland" are trademarks of Roland Corporation. This is an independent implementation inspired by the Jupiter-8 architecture and is not affiliated with or endorsed by Roland Corporation.

---

## Version History

**v1.2.0** (2026-02-07)
- MIDI expression (velocity, pitch bend, pressure, aftertouch)
- Knob catch mechanism for smoother preset transitions
- Renderer refactor for mono/poly/unison consistency
- Waveform fixes and HPF state reset on voice trigger

**v1.1.0** (2025-12-29)
- Polyphonic and unison modes with 12 presets
- Extended modulation hub (18 destinations)
- ENV to pitch modulation and portamento/glide

**v1.0.0** (December 10, 2025)
- Initial release
- Dual DCO with 4 waveforms each
- Multi-mode filter with self-oscillation
- Hard sync and cross-modulation
- Dual ADSR envelopes
- LFO with 4 waveforms
- 12 factory presets
- Keyboard tracking

---

## Support

For issues, questions, or feature requests, please visit:
- GitHub: [drumlogue-units repository](https://github.com/cldmnky/drumlogue-units)
- Report bugs via GitHub Issues

**Enjoy creating Jupiter-8 inspired sounds on your drumlogue!** 🎹
