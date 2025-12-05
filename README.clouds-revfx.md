# Clouds Reverb FX for Korg drumlogue

A lush, atmospheric reverb effect for the Korg drumlogue, inspired by Mutable Instruments Clouds. This unit brings the legendary Clouds reverb algorithm to your drumlogue, enhanced with additional texture processing, micro-granular effects, and pitch shifting capabilities.

## Features

- **High-quality reverb** based on the Griesinger/Dattorro topology from Mutable Instruments Clouds
- **Texture processing** with post-reverb diffusion for additional smearing and depth
- **Micro-granular processing** for ambient textures and freeze effects
- **Pitch shifting** for shimmer and octave effects
- **8 built-in presets** covering a range of reverb styles
- **Smooth parameter changes** with zipper-noise-free transitions

## Installation

1. Download the `clouds_revfx.drmlgunit` file
2. Connect your drumlogue to your computer via USB
3. Copy the `.drmlgunit` file to the `Units/RevFX/` folder on the drumlogue
4. Safely eject the drumlogue and restart it
5. The effect will appear in the Reverb FX slot selection

## Parameters

The unit has 16 parameters organized across 4 pages:

### Page 1 - Main Reverb Controls

| Parameter | Range | Description |
|-----------|-------|-------------|
| **DRY/WET** | 0-100% | Mix between dry input and wet reverb signal |
| **TIME** | 0-127 | Reverb decay time. Higher values = longer decay |
| **DIFFUSION** | 0-127 | Internal reverb diffusion. Higher values = denser, more diffuse sound |
| **LP DAMP** | 0-127 | Lowpass damping filter. Lower values = darker reverb tail |

### Page 2 - Additional Reverb + Texture

| Parameter | Range | Description |
|-----------|-------|-------------|
| **IN GAIN** | 0-127 | Input signal level into the reverb. Adjust to prevent clipping |
| **TEXTURE** | 0-127 | Post-reverb diffusion amount. Adds smearing and depth to the reverb tail |
| **GRAIN AMT** | 0-127 | Granular processor mix amount. Set to 0 to disable granular processing |
| **GRN SIZE** | 0-127 | Size of individual grains in the granular processor |

### Page 3 - Granular Controls

| Parameter | Range | Description |
|-----------|-------|-------------|
| **GRN DENS** | 0-127 | Grain spawn density. Higher values = more overlapping grains |
| **GRN PITCH** | 0-127 | Grain pitch shift. Center (64) = no shift, ±24 semitones range |
| **GRN POS** | 0-127 | Buffer position for grain playback |
| **FREEZE** | 0-1 | Freeze the granular buffer. When enabled, no new audio is recorded |

### Page 4 - Pitch Shifter

| Parameter | Range | Description |
|-----------|-------|-------------|
| **SHIFT AMT** | 0-127 | Pitch shifter mix amount. Set to 0 to disable pitch shifting |
| **SHFT PTCH** | 0-127 | Pitch shift amount. Center (64) = no shift, ±24 semitones range |
| **SHFT SIZE** | 0-127 | Pitch shifter window size. Affects quality and character of shifting |

## Presets

The unit includes 8 carefully crafted presets:

| # | Name | Description |
|---|------|-------------|
| 0 | **INIT** | Clean starting point with moderate reverb settings |
| 1 | **HALL** | Large concert hall with long decay and enhanced texture |
| 2 | **PLATE** | Bright plate reverb with maximum diffusion |
| 3 | **SHIMMER** | Pitched reverb with octave-up shimmer effect |
| 4 | **CLOUD** | Granular texture combined with reverb for ambient clouds |
| 5 | **FREEZE** | Long reverb optimized for frozen/pad sounds with heavy texture |
| 6 | **OCTAVER** | Pitch-shifted reverb with subtle octave-down effect |
| 7 | **AMBIENT** | Lush ambient wash combining reverb, texture, granular, and subtle shimmer |

## Usage Tips

### Creating Ambient Pads

1. Start with the **FREEZE** or **AMBIENT** preset
2. Increase **TIME** and **DIFFUSION** for longer, smoother tails
3. Add **TEXTURE** for additional density
4. Enable **FREEZE** to capture and sustain a pad from your input

### Adding Shimmer

1. Start with the **SHIMMER** preset or the **INIT** preset
2. Set **SHIFT AMT** to taste (60-100 for subtle, higher for obvious shimmer)
3. Set **SHFT PTCH** to 88 (+12 semitones/1 octave up) for classic shimmer
4. Adjust **SHFT SIZE** - smaller values sound more natural

### Granular Textures

1. Increase **GRAIN AMT** to blend in granular processing
2. Experiment with **GRN SIZE** and **GRN DENS** for different textures
3. Use **GRN PITCH** to add pitch variation to grains
4. Enable **FREEZE** to capture audio and manipulate it with the granular controls

### Preventing Clipping

- If the output distorts, reduce **IN GAIN**
- High **TIME** values with high **DRY/WET** can build up level - reduce mix or input gain
- The granular and pitch shifter stages add gain - reduce their amounts if clipping occurs

## Technical Specifications

- **Sample Rate**: 48kHz (native drumlogue rate)
- **Processing**: Stereo in/out
- **Latency**: Minimal (block-based processing)
- **CPU Usage**: Moderate - the granular and pitch shifter stages can be bypassed by setting their amounts to 0

## Credits

- **DSP Algorithms**: Based on [Mutable Instruments Clouds](https://mutable-instruments.net/modules/clouds/) by Émilie Gillet
- **Original License**: MIT License
- **drumlogue Port**: Adapted for Korg logue SDK

## Version History

- **v1.3.0** - Current version
  - 16 parameters across 4 pages
  - 8 built-in presets
  - Parameter smoothing to prevent zipper noise
  - CPU optimization with automatic bypass for disabled effects

## License

This unit is based on code from Mutable Instruments, released under the MIT License. See the LICENSE file for details.
