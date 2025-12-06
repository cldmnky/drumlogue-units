# Elements Synth for Drumlogue - Release Notes

## Version 1.0.0 (December 2025)

Initial release of Elements modal synthesis synth for Korg Drumlogue.

### Features

- Full Mutable Instruments Elements DSP ported to 48kHz
- Moog-style 24dB/octave ladder filter with:
  - Cutoff frequency control
  - Self-oscillating resonance
  - Envelope modulation (bipolar)
  - Key tracking
- LFO for filter and pitch modulation
- 8 factory presets
- Professional monophonic voice management with legato support
- All 3 resonator models: MODAL, STRING, STRINGS

### Technical Details

- Binary size: ~407KB
- Memory usage: ~212KB BSS (buffers)
- Sample rate: 48kHz (upscaled from original 32kHz)
- Developer ID: 0x434C444D ("CLDM")
- Unit ID: 0x00000002

### Known Limitations

1. **Monophonic only** - Single voice (hardware limitation of Elements DSP)
2. **CPU intensive** - Modal resonator uses 64 modes; reduce SPACE if CPU issues occur
3. **No Easter egg mode** - Ominous voice not implemented
4. **No external audio input** - Pure synthesizer (no audio passthrough)

### Tested Configurations

- Build environment: logue-sdk Docker container
- Target: Drumlogue firmware (test on actual hardware recommended)

### Parameter Mapping from Original Elements

| Original | Drumlogue | Notes |
|----------|-----------|-------|
| CONTOUR knob | CONTOUR | Envelope shape |
| BOW button + knob | BOW | Level only (always enabled) |
| BLOW button + knob | BLOW | Level only |
| STRIKE button + knob | STRIKE | Level only |
| BOW TIMBRE | BOW TIMB | |
| BLOW META | FLOW | Air flow position |
| STRIKE META | MALLET | Mallet type |
| BLOW TIMBRE | BLW TIMB | |
| COARSE knob | COARSE | ±24 semitones |
| FINE knob | FINE | ±50 cents |
| FM input | (not mapped) | No FM input |
| GEOMETRY | GEOMETRY | |
| BRIGHTNESS | BRIGHT | |
| DAMPING | DAMPING | |
| POSITION | POSITION | |
| SPACE | SPACE | |
| RESONATOR MODEL | MODEL | String selector |

### Additions (not in original)

| Parameter | Description |
|-----------|-------------|
| FLT FREQ | Ladder filter cutoff |
| FLT RES | Ladder filter resonance |
| FLT ENV | Filter envelope amount |
| FLT KEY | Filter key tracking |
| MOD FREQ | LFO rate |
| MOD DPTH | LFO depth |
| STK TIMB | Strike timbre (exposed separately) |

### Build Instructions

```bash
# Requires logue-sdk Docker environment
./build.sh elements-synth

# Clean build
./build.sh elements-synth clean
```

### Credits

- Original Elements DSP: Émilie Gillet / Mutable Instruments
- Ladder filter: Based on Huovilainen improved model
- Drumlogue port: CLDM

### License

MIT License (Elements DSP code)
