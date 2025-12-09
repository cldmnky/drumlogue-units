# Elementish Synth - Release Notes

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
