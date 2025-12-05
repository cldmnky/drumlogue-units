# Clouds Reverb FX - Release Notes

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
