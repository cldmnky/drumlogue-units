# Pepege-Synth Release Notes

## Version 1.0.0 (In Development)

### Initial Release

**Features:**
- Dual wavetable oscillators with independent bank/morph controls
- 4 wavetable banks (CLASSIC, DIGITAL, FORMANT, TEXTURE)
- State Variable Filter (LP12, LP24, HP12, BP12)
- Dual ADSR envelopes (amplitude and filter)
- LFO with pitch and morph modulation
- Velocity sensitivity
- Pitch bend support

**Known Limitations:**
- Mono voice (no polyphony beyond drumlogue's voice allocation)
- Wavetable banks use placeholder data (to be expanded)
- No preset storage yet

**Technical Notes:**
- Uses integrated wavetable synthesis for anti-aliasing
- Based on techniques from Mutable Instruments Plaits
- Optimized for ARM Cortex-A7 (drumlogue hardware)
