# Drupiter - Jupiter-8 Synthesizer for Korg Drumlogue

**Version**: 1.0.0  
**Developer**: CLDM (0x434C444D)  
**Type**: Monophonic Synthesizer Unit  
**Based on**: Bristol Jupiter-8 Emulation

---

## Overview

Drupiter is a monophonic synthesizer unit for the Korg drumlogue that brings the classic Roland Jupiter-8 sound to your fingertips. Featuring dual oscillators, a multi-mode filter, and comprehensive modulation capabilities, Drupiter delivers rich, warm analog-style tones perfect for bass, leads, pads, and more.

### Key Features

- **Dual DCOs** with 4 waveforms each (Ramp, Square, Pulse, Triangle)
- **Hard Sync** (DCO1 → DCO2) for aggressive timbres
- **Cross-Modulation** (DCO2 → DCO1 FM) for complex harmonic content
- **Multi-Mode Filter** (LP12, LP24, HP12, BP12) with self-oscillation
- **Dual ADSR Envelopes** for VCF and VCA
- **LFO** with 4 waveforms and delay envelope
- **Keyboard Tracking** (50% default) for natural filter response
- **6 Factory Presets** covering common synth sounds

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
| **D1 OCT** | 0-127 | Oscillator 1 octave (16', 8', 4') |
| **D1 WAVE** | RAMP/SQR/PULSE/TRI | Waveform selection |
| **D1 PW** | 0-127 | Pulse width (only affects PULSE waveform) |
| **D1 LEVEL** | 0-127 | Oscillator 1 volume in mix |

**Tips:**
- Lower octaves (0-42) = 16' (sub-bass)
- Mid octaves (43-84) = 8' (standard pitch)
- High octaves (85-127) = 4' (one octave up)
- PULSE waveform at PW=64 becomes square wave

### Page 2: DCO-2 (Oscillator 2)

| Parameter | Range | Description |
|-----------|-------|-------------|
| **D2 OCT** | 0-127 | Oscillator 2 octave (16', 8', 4') |
| **D2 WAVE** | RAMP/SQR/PULSE/TRI | Waveform selection |
| **D2 TUNE** | 0-127 | Detune (64 = centered, ±10 cents) |
| **D2 LEVEL** | 0-127 | Oscillator 2 volume and cross-mod depth |

**Tips:**
- Detune DCO2 slightly (60-68) for chorus/detune effect
- Higher D2 LEVEL increases both volume AND cross-modulation
- Cross-mod creates FM-style timbres automatically
- Hard sync is always active (DCO1 syncs DCO2)

### Page 3: VCF (Voltage Controlled Filter)

| Parameter | Range | Description |
|-----------|-------|-------------|
| **CUTOFF** | 0-127 | Filter cutoff frequency (20Hz - 20kHz) |
| **RESO** | 0-127 | Filter resonance (self-oscillates at max) |
| **ENV AMT** | 0-127 | Envelope modulation depth (64=none, ±4 octaves) |
| **VCF TYP** | LP12/LP24/HP12/BP12 | Filter mode |

**Tips:**
- LP12 = Classic warm low-pass (12dB/oct)
- LP24 = Steep low-pass (24dB/oct, cascaded)
- HP12 = High-pass for thin/bright sounds
- BP12 = Band-pass for vocal/nasal tones
- RESO > 100 enters self-oscillation (pure sine tone)
- ENV AMT < 64 = negative modulation (filter closes)
- ENV AMT > 64 = positive modulation (filter opens)

### Page 4: VCF Envelope (Filter)

| Parameter | Range | Description |
|-----------|-------|-------------|
| **F.ATK** | 0-127 | Attack time (0 = instant, 127 = ~5 seconds) |
| **F.DCY** | 0-127 | Decay time (0 = instant, 127 = ~5 seconds) |
| **F.SUS** | 0-127 | Sustain level (0 = silent, 127 = full) |
| **F.REL** | 0-127 | Release time (0 = instant, 127 = ~5 seconds) |

**Tips:**
- Fast attack (0-10) + high ENV AMT = plucky sounds
- Slow attack (80-127) = filter sweeps in gradually
- Low sustain (0-30) + long decay = percussive filter movement
- Long release (100-127) = smooth fade-out on note off

### Page 5: VCA Envelope (Amplifier)

| Parameter | Range | Description |
|-----------|-------|-------------|
| **A.ATK** | 0-127 | Attack time (0 = instant, 127 = ~5 seconds) |
| **A.DCY** | 0-127 | Decay time (0 = instant, 127 = ~5 seconds) |
| **A.SUS** | 0-127 | Sustain level (0 = silent, 127 = full) |
| **A.REL** | 0-127 | Release time (0 = instant, 127 = ~5 seconds) |

**Tips:**
- Fast attack (0-5) = punchy sounds (bass, plucks)
- Slow attack (60-127) = pad/string style
- High sustain (100-127) = held notes maintain volume
- Long release (80-127) = smooth, flowing sound

### Page 6: LFO & Modulation

| Parameter | Range | Description |
|-----------|-------|-------------|
| **LFO RATE** | 0-127 | LFO frequency (0.1Hz - 50Hz) |
| **LFO WAV** | TRI/RAMP/SQR/S&H | LFO waveform |
| **LFO>VCO** | 0-127 | LFO to oscillator pitch (vibrato) |
| **LFO>VCF** | 0-127 | LFO to filter cutoff (±2 octaves) |

**Tips:**
- TRI = Smooth vibrato/tremolo
- RAMP = Rising/falling modulation
- SQR = Trill/alternating effect
- S&H = Random stepped modulation
- LFO>VCO at 20-40 = subtle vibrato
- LFO>VCF at 50-80 = filter sweeps/wobbles
- Combine LFO>VCO + LFO>VCF for complex movement

---

## Factory Presets

Drupiter includes 6 carefully crafted factory presets:

### 1. Init (Default)
- Clean starting point
- Square wave, moderate filter
- Good for experimentation

### 2. Bass
- Deep, punchy bass
- Pulse wave with high resonance
- Short decay, low sustain
- Perfect for: basslines, kicks

### 3. Lead
- Bright, cutting lead sound
- Ramp wave with vibrato
- Filter envelope for movement
- Perfect for: solos, melodies

### 4. Pad
- Lush, evolving pad
- Dual DCOs detuned
- Slow attack/release
- Perfect for: atmospheres, backing

### 5. Brass
- Bold brass-like sound
- Ramp wave, filter envelope
- Medium attack
- Perfect for: stabs, hits

### 6. Strings
- String ensemble sound
- Dual DCOs detuned
- Slow attack, high cutoff
- Perfect for: pads, backgrounds

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

- **Polyphony**: Monophonic
- **Sample Rate**: 48kHz
- **Bit Depth**: 32-bit float processing
- **CPU Usage**: ~13% (estimated)
- **Binary Size**: 19,740 bytes
- **Wavetable Size**: 2048 samples per waveform
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

**v1.0.0** (December 10, 2025)
- Initial release
- Dual DCO with 4 waveforms each
- Multi-mode filter with self-oscillation
- Hard sync and cross-modulation
- Dual ADSR envelopes
- LFO with 4 waveforms
- 6 factory presets
- Keyboard tracking

---

## Support

For issues, questions, or feature requests, please visit:
- GitHub: [drumlogue-units repository](https://github.com/cldmnky/drumlogue-units)
- Report bugs via GitHub Issues

**Enjoy creating Jupiter-8 inspired sounds on your drumlogue!** 🎹
