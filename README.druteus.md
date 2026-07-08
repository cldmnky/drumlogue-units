# Druteus for Korg drumlogue

A SoundFont (SF2) sample player for the Korg drumlogue, featuring the complete E-mu Proteus/1 factory patch library (442 presets) rendered through TinySoundFont. Druteus recreates the Proteus/1 dual-layer voice architecture with per-patch AHDSR envelopes, crossfade/keyboard splits, LFO modulation, DSP effects, and a trance gate — all in a polyphonic 16-voice synth.

## Features

- **SF2 SoundFont Player**: Loads `.sf2` files from the drumlogue's `Programs/` folder (up to 60 SF2 files browsable)
- **442 Proteus/1 Factory Presets**: Full preset library from EMU Proteus/1 ROM volumes 1–6
- **Dual-Layer Architecture**: Primary + secondary instrument per patch with independent tuning, volume, pan, chorus sends
- **Per-Patch AHDSR Envelopes**: Attack, hold, decay, sustain, release for each layer from Proteus preset data
- **Patch Modulation Matrix**: LFO1 and LFO2 per patch with 5 waveforms and configurable routing
- **Live LFO**: User-controllable LFO (5 waveforms, 3 destinations) layered on top of patch modulation
- **Crossfade Modes**: Velocity crossfade or keyboard split between layers
- **Layer Modes**: BOTH, PRI (primary only), SEC (secondary only)
- **Velocity Curves**: LINEAR, EXP, LOG, COMP, STEEP
- **Multi-Mode SVF Filter**: LP12, LP24, HP12, BP12 with smoothed cutoff and resonance
- **DSP Effects**: Chorus (stereo widener) + Reverb (Griesinger topology) with per-patch send levels
- **Trance Gate**: Tempo-synchronized rhythmic gating (1–32 step patterns)
- **Polyphonic**: 1–16 voices with intelligent oldest-note voice stealing
- **MIDI Expression**: Velocity, pitch bend, channel pressure (→ expression)
- **NEON-Optimized**: ARM NEON SIMD for DSP, float-precision TSF lowpass filter (40–60% CPU savings vs stock double-precision TSF)
- **Async SF2 Loading**: Chunked file loading (128 KB per frame), no UI freeze or audio thread blocking
- **Release-Ready**: All known critical bugs fixed. Stable for production deployment.

## Installation

1. Download `Proteus1_Instruments.sf2` (4.3 MB) from [musical-artifacts.com](https://musical-artifacts.com/artifacts/764)
2. Copy the `.sf2` file to the `Programs/` folder on the drumlogue (via USB mass storage)
3. Download `druteus.drmlgunit`
4. Copy the `.drmlgunit` file to the `Units/Synth/` folder on the drumlogue
5. Safely eject the drumlogue and restart it
6. The synth will appear as "DRUTEUS" in the Synth slot selection

**Recommended**: Bundle `Proteus1_Instruments.sf2` and `Proteus1_Presets.sf2` in `Programs/` for the full 442-patch experience.

## Parameters

The unit has 24 parameters organized across 6 pages:

### Page 1: Sound Source

| Parameter | Range | Default | Description |
|-----------|-------|---------|-------------|
| **SFONT** | 0–63 | (first SF2 found) | Select SoundFont file from `Programs/` folder |
| **PATCH** | 0–441 | 0 | Proteus/1 factory preset selector (shows patch names) |
| **VOICES** | 1–16 | 16 | Maximum polyphony (voice stealing when exceeded) |
| **TUNE** | −12..+12 semitones | 0 | Global transpose (applied as note offset) |

### Page 2: Pitch & Mix

| Parameter | Range | Default | Description |
|-----------|-------|---------|-------------|
| **FINETN** | −63..+63 cents | 0 | Fine tuning (deferred to audio thread) |
| **VOLUME** | 0–127 | 100 | Master volume via MIDI CC7 (deferred) |
| **PAN** | 0–127 | 64 (center) | Stereo pan via MIDI CC10 (deferred) |
| *(blank)* | — | — | — |

### Page 3: Layer Control

| Parameter | Range | Default | Description |
|-----------|-------|---------|-------------|
| **XFADE** | OFF, VEL, KEY | OFF | Crossfade override: velocity-based or keyboard split |
| **LAYERS** | BOTH, PRI, SEC | BOTH | Layer mode — both layers, primary only, or secondary only |
| *(blank)* | — | — | — |
| *(blank)* | — | — | — |

### Page 4: Effects & Feel

| Parameter | Range | Default | Description |
|-----------|-------|---------|-------------|
| **CHORUS** | 0–15 | 0 | Chorus mix (blended with per-patch chorus send) |
| **REVERB** | 0–127 | 0 | Reverb amount (blended with per-patch reverb send) |
| **V.CURVE** | LINEAR, EXP, LOG, COMP, STEEP | LINEAR | Velocity response curve |
| **TGATE** | OFF, 1–32 | OFF | Trance gate pattern length (tempo-synced) |

### Page 5: Filter

| Parameter | Range | Default | Description |
|-----------|-------|---------|-------------|
| **CUTOFF** | 0–127 | 127 (fully open) | SVF filter cutoff (smoothed) |
| **RES** | 0–127 | 0 | SVF filter resonance (smoothed) |
| *(blank)* | — | — | — |
| *(blank)* | — | — | — |

### Page 6: LFO

| Parameter | Range | Default | Description |
|-----------|-------|---------|-------------|
| **LFO RTE** | 0–127 | 0 | User LFO rate |
| **LFO AMT** | 0–127 | 0 | User LFO modulation amount |
| **LFO DST** | PITCH, VOL, BOTH | PITCH | User LFO modulation destination |
| **LFO WAV** | TRI, SINE, SQR, SAW, RND | SINE | User LFO waveform |

## Velocity Curves

| Curve | Formula | Character |
|-------|---------|-----------|
| **LINEAR** | `v / 127` | Direct 1:1 mapping |
| **EXP** | `(v / 127)²` | Soft at low velocities, aggressive at high |
| **LOG** | `log(1+v) / log(128)` | Quick response at low velocities |
| **COMP** | `sqrt(v / 127)` | Compressed — reduced dynamic range |
| **STEEP** | `(v / 127)³` | Very quiet at low velocities, full at max |

## Factory Presets

Druteus includes **442 factory presets** extracted from E-mu Proteus/1 ROM volumes. These are authentic patches from the original Proteus hardware covering:

| Volume | Presets | Character |
|--------|---------|-----------|
| **Fresh Mix (3100)** | 1–63 | Pop/rock, acoustic instruments, drums |
| **Fusion (3101)** | 64–125 | Synth leads, pads, fusion textures |
| **Textures (3102)** | 126–187 | Ambient, atmospheric, evolving pads |
| **ProPower (3114)** | 188–249 | Orchestral, cinematic, power brass |
| **Introspection (3115)** | 250–311 | New age, meditative, soft pads |
| **Full Spectrum (3116)** | 312–441 | Complete range, specialty sounds |

The `PATCH` parameter browses all 442 presets by their original Proteus name (e.g., "FMstylePiano", "WarmStrings", "ResoBass").

## CPU Usage

- **Low voice count** (1–4 voices): Moderate (~15–25%)
- **High voice count** (11–16 voices): Higher (~30–45%)
- **DSP effects active**: Additional ~5–10%

NEON float-precision TSF lowpass filters reduce per-voice CPU by 40–60% compared to stock TSF (which uses software-emulated double precision on the Cortex-A7).

## SF2 Files

Place `.sf2` files in `/var/lib/drumlogued/userfs/Programs` on the drumlogue. The unit scans this folder at startup and lists all `.sf2` files in the **SFONT** parameter.

For the full Proteus/1 experience, include:
- `Proteus1_Instruments.sf2` — 126 instrument building blocks (auto-selected at startup)
- `Proteus1_Presets.sf2` — 129 factory preset patches (optional, patches are also embedded)

Download from [musical-artifacts.com](https://musical-artifacts.com/artifacts/764).

## Credits

- **TinySoundFont**: Bernhard Schelling (MIT License)
- **Proteus Patch Data**: Extracted from EMU Proteus/1 SysEx ROM dumps
- **logue SDK Framework**: Oleg Burdaev (dukesrg), MIT License
- **Reverb DSP**: Mutable Instruments Rings (Griesinger reverb)
- **drumlogue Port**: Adapted for Korg logue SDK
- **Developer**: `0x434C444DU` ("CLDM")

## Version History

- **v0.1.0** — Initial Release
  - SF2 SoundFont playback via TinySoundFont
  - 442 Proteus/1 factory presets with dual-layer voice engine
  - Per-patch AHDSR envelopes and LFO modulation matrix
  - Keyboard split / velocity crossfade per patch
  - SVF filter (LP12, LP24, HP12, BP12) with smoothing
  - Chorus + Reverb DSP effects chain
  - Trance gate with tempo sync
  - 5 velocity curves
  - User LFO with 5 waveforms and 3 destinations
  - Polyphonic voice allocation (1–16 voices)
  - Chunked async SF2 loading (128 KB per frame)
  - ARM NEON optimizations (float TSF lowpass, 40–60% CPU savings)
  - MIDI pitch bend and channel pressure support
  - Fixed: unit hang from debug register bus fault (PERF_MON disabled in production)
  - Fixed: dual-layer voice pool sizing for correct polyphony
  - Fixed: SF2 load blocking audio thread (chunked to 128 KB)
  - Removed: `-ffast-math` (broke TSF IEEE float assumptions)

## License

This unit incorporates TinySoundFont (MIT License) and is based on code from various open-source projects. See the LICENSE file for details.
