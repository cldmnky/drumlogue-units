# Vapo2 - Wavetable Synthesizer for Drumlogue

A wavetable synthesizer inspired by [VAST Dynamics Vaporizer2](https://github.com/VASTDynamics/Vaporizer2), optimized for the Korg drumlogue platform using techniques from [Mutable Instruments Plaits](https://github.com/pichenettes/eurorack/tree/master/plaits).

## Features

- **Dual Wavetable Oscillators** - Two independent oscillators with separate bank/morph controls
- **Smooth Morphing** - Seamless interpolation between waveforms within each bank
- **Anti-aliased Playback** - Integrated wavetable synthesis prevents aliasing at all frequencies
- **State Variable Filter** - 12/24dB lowpass, highpass, and bandpass modes
- **Dual ADSR Envelopes** - Independent amplitude and filter envelopes
- **LFO Modulation** - Assignable to morph position and pitch (vibrato)

## Wavetable Banks

1. **CLASSIC** - Traditional analog waveforms (sine, saw, square, triangle, pulse variants)
2. **DIGITAL** - Harsh, metallic sounds
3. **FORMANT** - Vocal-like timbres
4. **TEXTURE** - Complex evolving sounds

## Parameters

### Page 1: Oscillator A
| Parameter | Range | Description |
|-----------|-------|-------------|
| A BANK | CLASSIC/DIGITAL/FORMANT/TEXTURE | Wavetable bank selection |
| A MORPH | 0-127 | Position within bank (morphs between waves) |
| A OCT | -3 to +3 | Octave offset |
| A TUNE | -64 to +63 | Fine tuning in cents |

### Page 2: Oscillator B
| Parameter | Range | Description |
|-----------|-------|-------------|
| B BANK | CLASSIC/DIGITAL/FORMANT/TEXTURE | Wavetable bank selection |
| B MORPH | 0-127 | Position within bank |
| B OCT | -3 to +3 | Octave offset relative to A |
| OSC MIX | 0-127 | Balance between A (0) and B (127) |

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
| ATTACK | 0-127 | Attack time |
| DECAY | 0-127 | Decay time |
| SUSTAIN | 0-127 | Sustain level |
| RELEASE | 0-127 | Release time |

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
| LFO>PIT | -64 to +63 | LFO to pitch (vibrato) depth |
| VOLUME | 0-127 | Output level |

## Technical Details

### Architecture

The synth uses **integrated wavetable synthesis** as described in Franck & Välimäki's "Higher-order integrated Wavetable Synthesis" (DAFX-12). This technique:

1. Stores wavetables as cumulative sums (integrated form)
2. Differentiates during playback
3. Provides natural anti-aliasing without mip-mapping
4. Enables smooth morphing between any waveforms

### Performance

- Sample rate: 48kHz
- Output: Stereo
- CPU usage: ~20-30% (estimated)
- Latency: Minimal (single-sample processing)

## Building

```bash
./build.sh vapo2
```

## Credits

- **Vaporizer2** by VAST Dynamics - Inspiration for wavetable architecture
- **Plaits** by Mutable Instruments - Integrated wavetable technique
- **logue SDK** by KORG - Platform SDK

## License

This project is licensed under the MIT License - see the [LICENSE](../../LICENSE) file for details.
