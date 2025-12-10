# Elementish Synth - Release Notes

## v1.2.0 (December 2025)

Generative sequencer release with Marbles-inspired note generation.

### New Features

- **Marbles-Inspired Generative Sequencer** (Lightweight mode only)
  - Triggered by drumlogue pattern sequencer or MIDI notes
  - Generates tempo-synced melodic subdivisions
  - 16 presets combining subdivision rate and scale quantization

- **16 Sequencer Presets**
  - OFF: Normal note playback
  - SLOW/MED/FAST: 1/2/4 notes per beat (chromatic)
  - X2/X4: 8/16 notes per beat (extreme subdivisions)
  - MAJ/MIN/PENT/CHROM: Scale-quantized 16th notes
  - OCT/5TH/4TH: Interval-based 8th notes
  - TRI: Triplet feel with triad quantization
  - 7TH: 16th notes quantized to 7th chord tones
  - RAND: Random scale selection per trigger

- **Déjà Vu Loop Buffer**
  - 8-step pattern memory
  - DEJA VU parameter: 0 = fully random, 127 = locked loop
  - Creates repeating melodic patterns from randomness

- **SPREAD Control**
  - Adjusts note range from unison (0) to ±24 semitones (127)
  - Works with scale quantization for musical results

### Technical Improvements

- **Skip-ahead optimization** in Process() - no per-sample iteration
- **Note queue** (8-deep circular buffer) prevents note loss at high subdivisions
- **Gate triggering** - works with drumlogue's built-in pattern sequencer
- **Bounds checking** on preset selection for safety

### Page 6 Parameter Changes (Lightweight Mode)

| Parameter | Description |
|-----------|-------------|
| COARSE | Base note for sequencer (±24 semitones from middle C) |
| SEQ | Sequencer preset (0-15) |
| SPREAD | Note range (0-127) |
| DEJA VU | Pattern looping (0-127) |

### Build Info

- Binary size: ~124KB
- All 16 sequencer presets validated
- Gate and MIDI triggering tested

---

## v1.1.0 (December 2025)

Performance optimization release with ARM NEON SIMD support.

### New Features

- **NEON SIMD Optimizations** - Significant performance improvements on ARM Cortex-A7
  - Modal resonator: ~40% speedup (4-way parallel SVF processing)
  - CosineOscillator batch compute: ~2.4x faster amplitude calculation
  - Stereo soft clamp: 2x faster output processing
  - FastTanh saturation: ~20% speedup (4 values simultaneously)

### Technical Improvements

- **Structure-of-Arrays (SoA) layout** for mode coefficients eliminates scatter-gather overhead in NEON loop
- **NaN protection and stability checks** in NEON mode processing loop
- **Optimized CosineOscillator::Next4()** batch method for modal amplitude modulation
- **Vectorized stereo output processing** using float32x2_t

### Internal Changes

- Moved NEON DSP utilities to shared `common/neon_dsp.h` library
- Added SyncSoAFromModes()/SyncSoAToModes() for efficient coefficient sync
- Coefficient sync only happens when parameters change (not per-sample)

### Build Info

- Binary size: ~124KB (minimal increase for NEON paths)
- All 8 presets validated
- All 3 resonator modes (MODAL/STRING/MSTRING) tested

---

## v1.0.0 (December 2025)

Initial release of Elementish Synth for Korg drumlogue.

### Features

- **Modal Synthesis Engine** - Port of Mutable Instruments Elements
- **Three Excitation Types:**
  - BOW: Continuous friction/bowing with adjustable timbre
  - BLOW: Granular noise/breath with flow control
  - STRIKE: Percussive impacts (5 modes: SAMPLE, GRANULAR, NOISE, PLECTRUM, PARTICLE)
- **Three Resonator Models:**
  - MODAL: Classic 8-partial modal resonator
  - STRING: Karplus-Strong string synthesis
  - MSTRING: 5 sympathetic strings for rich harmonic content
- **12 Mallet Types:** SOFT/MED/HARD/PLEC/STIK/BOW × DK(dark)/BR(bright)
- **8 Factory Presets:** Init, Bowed Str, Bell, Pluck, Blown, Marimba, String, Drone
- **Full MIDI Support:** Velocity-sensitive dynamics, pitch bend (+/- 2 semitones)
- **24 Parameters** across 6 pages
- **Stereo Width Control** via SPACE parameter

### Technical Details

- Sample Rate: 48kHz (native drumlogue)
- Processing: Stereo output
- Optimized for Cortex-A7 with NEON disabled in initial release

### Credits

- DSP Algorithms based on [Mutable Instruments Elements](https://mutable-instruments.net/modules/elements/) by Émilie Gillet
- Original License: MIT License
