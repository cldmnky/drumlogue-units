# Release Notes - Drupiter Synth

## v1.0.0 - 2024-12-18

### 🎉 Initial Release

First stable release of Drupiter, a Jupiter-8 inspired monophonic synthesizer for Korg drumlogue.

### Features

**Oscillators:**
- Dual DCOs with 4 waveforms each (Ramp, Square, Pulse, Triangle)
- Hard sync (DCO1 → DCO2) for aggressive timbres
- Cross-modulation (DCO2 → DCO1 FM) for complex harmonic content
- Independent octave selection (16', 8', 4')
- Pulse width control for pulse waveforms
- Detune control for DCO2

**Filter:**
- Multi-mode filter (LP12, LP24, HP12, BP12)
- Self-oscillation capability
- Envelope amount and keyboard tracking
- Resonance control

**Modulation:**
- Dual ADSR envelopes (VCF and VCA)
- LFO with 4 waveforms (Triangle, Sawtooth, Square, S&H)
- LFO delay envelope for gradual modulation
- LFO rate control

**Presets:**
- 6 factory presets covering common synth sounds:
  1. Jupiter Bass - Deep, punchy bass
  2. Jupiter Lead - Bright, cutting lead
  3. Sync Lead - Hard-synced aggressive lead
  4. FM Bell - Cross-modulated bell tones
  5. Analog Pad - Warm, evolving pad
  6. Poly Arp - Bright arpeggiator sound

**User Interface:**
- 24 parameters across 6 pages
- Hub control system for efficient parameter organization
- Clear parameter naming and formatting
- Velocity sensitivity for VCA envelope

### Technical Details

- **Sample Rate:** 48 kHz
- **Monophonic:** Single voice
- **CPU Usage:** Optimized for real-time performance
- **Memory:** Efficient fixed-size buffers
- **MIDI:** Full note on/off support with velocity

### Known Issues

None at this time.

### Installation

1. Download `drupiter_synth.drmlgunit`
2. Copy to `Units/` folder on drumlogue via USB
3. Power cycle the drumlogue
4. Select "Drupiter" from synth menu

### Credits

- Based on Bristol Jupiter-8 emulation
- Developed by CLDM
- Built with Korg logue SDK

---

For detailed parameter documentation, see [README.md](README.md).
For UI reference guide, see [UI.md](UI.md).
