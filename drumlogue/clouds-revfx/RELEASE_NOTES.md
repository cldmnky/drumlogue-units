# Clouds Reverb FX - Release Notes

## v1.2.0 (December 2025)

Feature and performance release with NEON optimizations and refined presets.

### New Features

- **Dual Assignable LFOs** - Two independent LFOs with 15 modulation targets each
  - Targets: DRY/WET, TIME, DIFFUSION, LP, INPUT GAIN, TEXTURE, all GRAIN params, all SHIFT params
  - 3 waveforms: Sine, Saw, Random (S&H)
  - Cross-modulation: LFO1 can modulate LFO2 speed
  - 8 new parameters on pages 5-6

- **NEON SIMD Optimizations** - Significant performance improvements on ARM Cortex-A7
  - SanitizeAndClamp: Vectorized NaN removal and output clamping
  - InterleaveStereo: Fast stereo buffer interleaving  
  - FastTanh4: 4-way parallel soft saturation

- **Refined Presets** - All 8 presets redesigned for distinct sonic character
  - INIT: Neutral starting point with balanced parameters
  - HALL: Large concert hall with long decay, slightly dark
  - PLATE: Classic bright plate with max diffusion
  - SHIMMER: Ethereal octave-up with grain sparkle and LFO modulation
  - CLOUD: Dense granular with dual LFO (position + density)
  - FREEZE: Infinite sustain pad machine
  - OCTAVER: Sub-octave thickener (-12 semitones)
  - AMBIENT: Evolving wash with dual LFO on texture/density

### Bug Fixes

- **Fixed critical preset selector bug** (#24): `getPresetName()` now returns "Invalid" instead of `nullptr` for out-of-range indices. This prevents the drumlogue firmware from treating the unit as having no presets, which was causing the preset selector to lock at index 0.
- Fixed preset loading: moved preset data to file scope for stable memory addresses
- Fixed preset loading: use `setParameter()` to ensure proper DSP state updates
- Fixed missing symbol: added `stmlib/utils/random.cc` to build

### Technical Improvements

- Parameter smoothing prevents zipper noise during preset changes
- Moved shared NEON utilities to `common/neon_dsp.h`
- Stack allocation optimized (temp buffers moved to class members)

### Parameters (6 Pages)

| Page | Parameters |
|------|------------|
| 1 - Main | DRY/WET, TIME, DIFFUSION, LP DAMP |
| 2 - Texture | IN GAIN, TEXTURE, GRAIN AMT, GRN SIZE |
| 3 - Granular | GRN DENS, GRN PITCH, GRN POS, FREEZE |
| 4 - Pitch | SHIFT AMT, SHFT PTCH, SHFT SIZE, — |
| 5 - LFO1 | LFO1 ASGN, LFO1 SPD, LFO1 DPTH, LFO1 WAVE |
| 6 - LFO2 | LFO2 ASGN, LFO2 SPD, LFO2 DPTH, LFO2 WAVE |

---

## v1.1.0 (December 2025)

Added dual assignable LFOs for dynamic modulation.

### New Features

- **Dual LFOs** with 15 assignable modulation targets
- **3 Waveforms:** Sine, Saw, Random
- **Cross-modulation:** LFO1 → LFO2 speed

---

## v1.0.0 (December 2025)

First stable release.

### Changes from v1.0.0-pre

- Fixed undefined symbol (`stmlib/utils/random.cc`)
- Validated all presets

---

## v1.0.0-pre (Initial Pre-release)

This is the initial pre-release of the Clouds Reverb FX unit for Korg drumlogue.

### Features

- **Lush Reverb Engine** - Based on the acclaimed Griesinger/Dattorro topology from Mutable Instruments Clouds
- **Texture Processing** - Post-reverb diffusion for additional smearing and atmospheric depth
- **Micro-Granular Processing** - Ambient textures with freeze capability for drone and pad sounds
- **Pitch Shifter** - Shimmer and octave effects with adjustable window size
- **16 Parameters** across 4 pages for deep sound design
- **8 Built-in Presets** - INIT, HALL, PLATE, SHIMMER, CLOUD, FREEZE, OCTAVER, AMBIENT
- **Parameter Smoothing** - Zipper-noise-free transitions for live performance
- **CPU Optimization** - Automatic bypass for disabled effect stages

### Parameters

| Page | Parameters |
|------|------------|
| 1 - Main | DRY/WET, TIME, DIFFUSION, LP DAMP |
| 2 - Texture | IN GAIN, TEXTURE, GRAIN AMT, GRN SIZE |
| 3 - Granular | GRN DENS, GRN PITCH, GRN POS, FREEZE |
| 4 - Pitch | SHIFT AMT, SHFT PTCH, SHFT SIZE |

### Technical Details

- Sample Rate: 48kHz (native drumlogue)
- Processing: Stereo in/out
- Latency: Minimal (block-based processing)

### Credits

- DSP Algorithms based on [Mutable Instruments Clouds](https://mutable-instruments.net/modules/clouds/) by Émilie Gillet
- Original License: MIT License
- drumlogue Port: Adapted for Korg logue SDK

### Known Issues

- This is a pre-release version for testing purposes
- Please report any issues on the GitHub repository

### What's Next

- Full release after community testing
- Potential parameter refinements based on feedback
