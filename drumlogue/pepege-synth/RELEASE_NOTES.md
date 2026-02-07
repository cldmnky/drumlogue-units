# Pepege-Synth - Release Notes

## v1.1.0 - 2026-02-07

### Improvements

- MOD HUB now uses shared HubControl for consistent destination names and value formatting
- MOD HUB values use a 0-100 UI scale with preset migration from the previous 0-127 range
- Updated PPG waveform resource data
- Smoother wavetable morphing response
- Stereo width smoothing uses the current smoothed value per render for stability
- Per-voice mix headroom scales by voice count

### Bug Fixes

- Reset HPF state on voice trigger to prevent filter state carryover
- Fix monophonic last-note priority voice allocation bug
- Clamp filter type selection to valid range

## v1.0.0 (December 2025)

Initial release of Pepege-Synth for Korg drumlogue.

### Features

- **2-Voice Polyphony** - Play chords and harmonies
- **Dual PPG Wavetable Oscillators** - Authentic 8-bit character
  - 16 wavetable banks (256 waves total)
  - Independent octave and tuning controls
  - Three PPG interpolation modes: HiFi, LoFi, Raw
  - Morphing wavetable position

- **Wavetable Banks**
  - UPPER: Classic PPG upper harmonics
  - RESNT1/RESNT2: Resonant waveforms
  - MELLOW/BRIGHT/HARSH: Timbral variations
  - CLIPPR: Clipped/distorted waves
  - SYNC: Sync-style waves
  - PWM: Pulse width modulation
  - VOCAL1/VOCAL2: Formant-like vowels
  - ORGAN: Harmonic organ tones
  - BELL: Metallic/bell sounds
  - ALIEN: Strange/experimental
  - NOISE: Noise-based textures
  - SPECAL: Special effects

- **State Variable Filter** - Four filter types
  - LP12: 12dB/octave lowpass
  - LP24: 24dB/octave lowpass
  - HP12: 12dB/octave highpass
  - BP12: 12dB/octave bandpass

- **Dual ADSR Envelopes**
  - Independent amp and filter envelopes
  - Full ADSR control for each

- **Global LFO**
  - Six shapes: Sine, Triangle, Saw Up, Saw Down, Square, Sample & Hold
  - Assignable to oscillator morph and filter cutoff

- **MOD HUB** - Centralized modulation routing
  - 8 destinations with dedicated parameter page
  - LFO speed and shape selection
  - LFO to wavetable morph (±100%)
  - LFO to filter cutoff (±100%)
  - Velocity to filter (0-100%)
  - Filter key tracking (0-100%)
  - Oscillator B detune (±32 cents)
  - Pitch bend range (±2/7/12/24 semitones)

- **Stereo Output**
  - Adjustable stereo width (SPACE parameter)
  - NEON-optimized stereo processing

- **6 Factory Presets**
  - Glass Keys: Bright bell-like plucks
  - Dust Pad: Atmospheric evolving pad
  - Sync Bass: Aggressive sync lead
  - Noise Sweep: Sweeping noise FX
  - Pluck: Short percussive plucks
  - PWM Lead: Classic pulse lead

### Voice Allocation

- Round-robin with oldest-note stealing
- Smooth envelope transitions on voice stealing
- Polyphonic pitch bend and aftertouch

### Technical Details

- Built with Korg logue SDK
- Based on PPG wavetable synthesis
- ARM Cortex-A7 optimized
- 48kHz sample rate
- NEON SIMD optimizations for output stage

### Controls

**Page 1 - Oscillator A**
- A BANK: Wavetable bank selection
- A MORPH: Wave position/morphing
- A OCT: Octave transpose
- A TUNE: Fine tuning

**Page 2 - Oscillator B**
- B BANK: Wavetable bank selection
- B MORPH: Wave position/morphing
- B OCT: Octave transpose
- OSC MOD: PPG mode (HiFi/LoFi/Raw)

**Page 3 - Filter**
- CUTOFF: Filter frequency
- RESO: Filter resonance
- FLT ENV: Filter envelope amount
- FLT TYPE: Filter type (LP12/LP24/HP12/BP12)

**Page 4 - Amp Envelope**
- ATTACK: Envelope attack time
- DECAY: Envelope decay time
- SUSTAIN: Envelope sustain level
- RELEASE: Envelope release time

**Page 5 - Filter Envelope**
- F.ATTACK: Filter attack time
- F.DECAY: Filter decay time
- F.SUSTAIN: Filter sustain level
- F.RELEASE: Filter release time

**Page 6 - MOD HUB & Output**
- MOD SEL: Select modulation destination
- MOD VAL: Set value for selected destination
- OSC MIX: A/B oscillator balance
- SPACE: Stereo width

### Build Info

- Binary size: ~30KB
- RAM usage: ~1KB
- 2 polyphonic voices
- All 16 wavetable banks included
- 6 factory presets

### Credits

- Inspired by VAST Dynamics Vaporizer2
- Wavetable techniques from Mutable Instruments Plaits
- PPG wavetable data from Mutable Instruments codebase
