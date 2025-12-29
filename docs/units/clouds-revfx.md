---
layout: unit
title: Clouds Reverb FX
tagline: Lush, atmospheric reverb with texture processing, granular effects, pitch shifting, and dual LFOs
unit_type: Reverb Effect
version: v1.2.1
filename: clouds_revfx.drmlgunit
download_url: https://github.com/cldmnky/drumlogue-units/releases
permalink: /units/clouds-revfx/
---

## Overview

A lush, atmospheric reverb effect for the Korg drumlogue, inspired by **Mutable Instruments Clouds**. This unit brings the legendary Clouds reverb algorithm to your drumlogue, enhanced with additional texture processing, micro-granular effects, and pitch shifting capabilities.

## Features

- **High-quality reverb** based on the Griesinger/Dattorro topology from Mutable Instruments Clouds
- **Texture processing** with post-reverb diffusion for additional smearing and depth
- **Micro-granular processing** for ambient textures and freeze effects
- **Pitch shifting** for shimmer and octave effects
- **Dual assignable LFOs** with 16 modulation targets and 3 waveforms
- **Smooth parameter changes** with zipper-noise-free transitions

> **Note:** Presets are not supported for reverb/delay effects on drumlogue hardware due to a known firmware limitation.

---

## Installation

1. Download the `clouds_revfx.drmlgunit` file from [GitHub Releases](https://github.com/cldmnky/drumlogue-units/releases)
2. Connect your drumlogue to your computer via USB
3. Copy the `.drmlgunit` file to the `Units/RevFX/` folder on the drumlogue
4. Safely eject the drumlogue and restart it
5. The effect will appear in the Reverb FX slot selection

---

## Parameters

The unit has 24 parameters organized across 6 pages:

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
| *(reserved)* | - | Reserved for future use |

### Page 5 - LFO 1

| Parameter | Range | Description |
|-----------|-------|-------------|
| **LFO1 ASSIGN** | 0-15 | Modulation target (see LFO Targets table below) |
| **LFO1 SPD** | 0-127 | LFO speed (~0.05Hz to ~10Hz, exponential curve) |
| **LFO1 DPTH** | 0-127 | Modulation depth (0 = no modulation) |
| **LFO1 WAVE** | 0-2 | Waveform: SINE, SAW, or RANDOM |

### Page 6 - LFO 2

| Parameter | Range | Description |
|-----------|-------|-------------|
| **LFO2 ASSIGN** | 0-15 | Modulation target (see LFO Targets table below) |
| **LFO2 SPD** | 0-127 | LFO speed (~0.05Hz to ~10Hz, exponential curve) |
| **LFO2 DPTH** | 0-127 | Modulation depth (0 = no modulation) |
| **LFO2 WAVE** | 0-2 | Waveform: SINE, SAW, or RANDOM |

---

## LFO Targets

| Value | Target | Description |
|-------|--------|-------------|
| 0 | OFF | LFO disabled |
| 1 | DRY/WET | Modulate wet/dry mix |
| 2 | TIME | Modulate reverb decay time |
| 3 | DIFFUSN | Modulate diffusion amount |
| 4 | LP DAMP | Modulate lowpass damping |
| 5 | IN GAIN | Modulate input gain |
| 6 | TEXTURE | Modulate texture amount |
| 7 | GRN AMT | Modulate granular mix |
| 8 | GRN SZ | Modulate grain size |
| 9 | GRN DNS | Modulate grain density |
| 10 | GRN PTCH | Modulate grain pitch |
| 11 | GRN POS | Modulate grain position |
| 12 | SFT AMT | Modulate pitch shifter mix |
| 13 | SFT PTCH | Modulate pitch shift amount |
| 14 | SFT SZ | Modulate shifter window size |
| 15 | LFO2 SPD | Cross-modulation: LFO1 modulates LFO2 speed |

---

## LFO Waveforms

| Value | Waveform | Description |
|-------|----------|-------------|
| 0 | SINE | Smooth sinusoidal oscillation |
| 1 | SAW | Ramp down from +1 to -1 |
| 2 | RANDOM | Smoothly interpolated random values (sample & hold with glide) |

---

## Usage Tips

### Using the LFOs

The dual LFOs add dynamic movement to your reverb sounds:

1. **Basic Modulation**: Set **LFO1 ASSIGN** to your target parameter (e.g., TEXTURE), adjust **LFO1 SPD** for rate and **LFO1 DPTH** for intensity
2. **Slow Evolution**: Use low **SPD** values (0-30) with SINE waveform for subtle, evolving textures
3. **Rhythmic Effects**: Higher **SPD** values with SAW waveform create rhythmic pumping effects
4. **Random Variation**: RANDOM waveform adds organic, unpredictable movement
5. **Cross-Modulation**: Set LFO1's target to "LFO2 SPD" to have LFO1 modulate LFO2's speed for complex, evolving modulation

### Creating Ambient Pads

1. Increase **TIME** and **DIFFUSION** for longer, smoother tails
2. Add **TEXTURE** for additional density
3. Enable **FREEZE** to capture and sustain a pad from your input
4. Adjust **DRY/WET** to blend the effect with your source

### Adding Shimmer

1. Set **SHIFT AMT** to taste (60-100 for subtle, higher for obvious shimmer)
2. Set **SHFT PTCH** to 88 (+12 semitones/1 octave up) for classic shimmer
3. Adjust **SHFT SIZE** - smaller values sound more natural
4. Increase **TIME** and **DIFFUSION** for longer shimmer tails

### Granular Textures

1. Increase **GRAIN AMT** to blend in granular processing
2. Experiment with **GRN SIZE** and **GRN DENS** for different textures
3. Use **GRN PITCH** to add pitch variation to grains
4. Combine with reverb by increasing **TIME** for atmospheric results

## Technical Specifications

| Specification | Value |
|---------------|-------|
| **Sample Rate** | 48kHz (native drumlogue rate) |
| **Processing** | Stereo in/out |
| **Latency** | Minimal (block-based processing) |
| **CPU Usage** | Moderate - granular and pitch shifter can be bypassed by setting amounts to 0 |

---

## Version History

### v1.1.0 - LFO Update

- Added dual assignable LFOs (LFO1 and LFO2)
- 16 modulation targets including cross-modulation
- 3 waveforms: Sine, Saw, Random
- Exponential speed mapping for musical control
- 24 parameters across 6 pages
- Enhanced modulation capabilities

### v1.0.0-pre - Initial Pre-release

- 16 parameters across 4 pages
- Parameter smoothing to prevent zipper noise
- CPU optimization with automatic bypass for disabled effects

---

## Credits

- **DSP Algorithms**: Based on [Mutable Instruments Clouds](https://mutable-instruments.net/modules/clouds/) by Émilie Gillet
- **Original License**: MIT License
- **drumlogue Port**: CLDM
