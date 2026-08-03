# Druteus - Release Notes

## v0.1.0 - 2026-08-03

Initial release of Druteus, an E-mu Proteus/1 SoundFont synthesizer for Korg drumlogue.

### Features

- TinySoundFont SF2 playback with support for files in the drumlogue `Programs/` folder
- 442 Proteus/1 factory patches across six original ROM volumes
- Dual-layer voice architecture with per-layer tuning, volume, pan, and patch sends
- Per-patch AHDSR envelopes and LFO modulation
- Velocity crossfade and keyboard split modes
- User LFO with five waveforms and three destinations
- State-variable filter with LP12, LP24, HP12, and BP12 modes
- Chorus and Griesinger reverb effects
- Tempo-synchronized trance gate with 1-32 step patterns
- 1-16 voice polyphony with oldest-note voice stealing
- MIDI velocity, pitch bend, and channel pressure support

### Improvements

- ARM NEON optimizations for DSP, gain, and stereo interleave processing
- Float-precision TSF lowpass processing for improved drumlogue performance
- Chunked asynchronous SF2 loading in 128 KB blocks to avoid long UI stalls

### Bug Fixes

- Disabled production performance-monitor register access that could hang the unit
- Fixed SF2 loading behavior by moving file reads out of the audio render path

### Known Issues

- The Proteus/1 SoundFont is not bundled with the unit and must be downloaded separately.
- Audio is silent while a SoundFont is loading at startup or after selecting a new file.
