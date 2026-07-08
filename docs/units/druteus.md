---
layout: unit
title: Druteus Synth
unit_name: druteus
unit_type: synth
version: v0.1.0
icon: 🎹
tagline: E-mu Proteus/1 SF2 soundfont player with 442 factory presets
description: Polyphonic SoundFont player recreating the E-mu Proteus/1 with dual-layer voice architecture, per-patch AHDSR envelopes, crossfade/keyboard splits, LFO modulation, DSP effects, and trance gate — all in a 16-voice synth.
permalink: /units/druteus/
---

# Druteus Synth

**E-mu Proteus/1 SoundFont Player for Korg Drumlogue**

Druteus brings the classic E-mu Proteus/1 sound library to the drumlogue via TinySoundFont. With 442 authentic factory presets from all six original ROM volumes, dual-layer voice architecture, and a full DSP effects chain, it delivers the distinctive Proteus sound in a polyphonic 16-voice synth.

---

## Features

### 🎹 SF2 SoundFont Engine
- Loads `.sf2` files from `Programs/` folder (up to 60 browsable)
- TinySoundFont v0.9 with **NEON-optimized float lowpass filter** for 40–60% CPU savings
- Chunked async file loading (128 KB per frame, no UI freeze)

### 🎛️ 442 Proteus/1 Factory Presets
- All 6 original ROM volumes: Fresh Mix, Fusion, Textures, ProPower, Introspection, Full Spectrum
- Browsable by original patch name (e.g., "FMstylePiano", "WarmStrings")

### 🔀 Dual-Layer Voice Engine
- Primary + secondary instrument per patch
- Independent tuning, volume, pan, chorus sends per layer
- Velocity crossfade or keyboard split between layers

### 📊 Per-Patch Modulation
- AHDSR envelopes per layer (attack, hold, decay, sustain, release)
- LFO1 + LFO2 per patch with 5 waveforms
- Patch modulation matrix with realtime routing

### 🎚️ Live Performance Controls
- User LFO: 5 waveforms, 3 destinations (PITCH, VOL, BOTH)
- Velocity curves: LINEAR, EXP, LOG, COMP, STEEP
- Tempo-synced trance gate (1–32 steps)

### 🎛️ DSP Effects
- Chorus (stereo widener) with per-patch send levels
- Reverb (Griesinger topology) with per-patch amounts
- State-variable filter: LP12, LP24, HP12, BP12

---

## Parameters

Druteus has **24 parameters** across **6 pages**.

### Page 1: Sound Source

| # | Name | Range | Description |
|---|------|-------|-------------|
| 0 | **SFONT** | 0–63 | SoundFont file from `Programs/` |
| 1 | **PATCH** | 0–441 | Proteus/1 preset (shows name) |
| 2 | **VOICES** | 1–16 | Maximum polyphony |
| 3 | **TUNE** | −12..+12 | Global transpose (semitones) |

### Page 2: Pitch & Mix

| # | Name | Range | Description |
|---|------|-------|-------------|
| 4 | **FINETN** | −63..+63 | Fine tuning (cents) |
| 5 | **VOLUME** | 0–127 | Master volume |
| 6 | **PAN** | 0–127 | Stereo pan (64=center) |

### Page 3: Layer Control

| # | Name | Range | Description |
|---|------|-------|-------------|
| 8 | **XFADE** | OFF, VEL, KEY | Crossfade mode |
| 9 | **LAYERS** | BOTH, PRI, SEC | Layer mode |

### Page 4: Effects & Feel

| # | Name | Range | Description |
|---|------|-------|-------------|
| 12 | **CHORUS** | 0–15 | Chorus mix |
| 13 | **REVERB** | 0–127 | Reverb amount |
| 14 | **V.CURVE** | LINEAR, EXP, LOG, COMP, STEEP | Velocity curve |
| 15 | **TGATE** | OFF, 1–32 | Trance gate steps |

### Page 5: Filter

| # | Name | Range | Description |
|---|------|-------|-------------|
| 16 | **CUTOFF** | 0–127 | Filter cutoff (smoothed) |
| 17 | **RES** | 0–127 | Filter resonance (smoothed) |

### Page 6: LFO

| # | Name | Range | Description |
|---|------|-------|-------------|
| 20 | **LFO RTE** | 0–127 | LFO rate |
| 21 | **LFO AMT** | 0–127 | LFO amount |
| 22 | **LFO DST** | PITCH, VOL, BOTH | Destination |
| 23 | **LFO WAV** | TRI, SINE, SQR, SAW, RND | Waveform |

---

## Installation

1. Download `druteus.drmlgunit` from the [releases page](https://github.com/cldmnky/drumlogue-units/releases)
2. Download `Proteus1_Instruments.sf2` (4.3 MB) from [musical-artifacts.com](https://musical-artifacts.com/artifacts/764)
3. Copy the `.sf2` file to `Programs/` on the drumlogue
4. Copy `druteus.drmlgunit` to `Units/Synth/`
5. Safely eject and restart
6. Select **"DRUTEUS"** in the Synth slot

**Recommended SF2 files:** `Proteus1_Instruments.sf2` (4.3 MB) for the full 442-patch library.

---

## Sound Design Tips

### Dual-Layer Stacks
Set LAYERS to BOTH. Use XFADE=VEL to crossfade layers by velocity — play softly for primary, hard for secondary.

### Velocity Response
Set V.CURVE to EXP for expressive dynamics, STEEP for drum/percussion patches, or LINEAR for consistent levels.

### Pads & Textures
Select a preset from the Textures volume (126–187). Add slow LFO to VOL, increase REVERB and CHORUS for atmosphere.

### Rhythmic Effects
Use TGATE (1–32 steps) synced to drumlogue tempo for gate patterns on sustained sounds.

---

## Factory Presets

| Volume | Presets | Character |
|--------|---------|-----------|
| **Fresh Mix (3100)** | 1–63 | Pop/rock, acoustic, drums |
| **Fusion (3101)** | 64–125 | Synth leads, pads, textures |
| **Textures (3102)** | 126–187 | Ambient, atmospheric pads |
| **ProPower (3114)** | 188–249 | Orchestral, cinematic |
| **Introspection (3115)** | 250–311 | New age, meditative |
| **Full Spectrum (3116)** | 312–441 | Complete range |

---

## Technical Specifications

- **Type**: Polyphonic Synth
- **Engine**: TinySoundFont v0.9 (SF2 playback)
- **Voices**: 1–16 polyphonic (oldest-note stealing)
- **Layers**: Up to 2 per voice (32 TSF channels max)
- **Filter**: State-variable (LP12/24, HP12, BP12)
- **Audio**: 48 kHz stereo float, 64 frame buffer
- **CPU**: ~10–40% depending on voice count
- **MIDI**: Velocity, pitch bend, channel pressure
- **Presets**: 442 Proteus/1 factory presets (embedded)

---

## Credits

- **TinySoundFont**: Bernhard Schelling (MIT)
- **Proteus Patch Data**: Extracted from EMU Proteus/1 ROMs
- **logue FS**: Oleg Burdaev (dukesrg), MIT
- **Reverb**: Mutable Instruments Rings
- **Developer**: CLDM (0x434C444D)

---

## Documentation

- [README.md](https://github.com/cldmnky/drumlogue-units/blob/main/drumlogue/druteus/README.md) — full documentation
- [README.druteus.md](https://github.com/cldmnky/drumlogue-units/blob/main/README.druteus.md) — overview at repo root

---

## Support

Found a bug or have a feature request? Please open an issue on [GitHub](https://github.com/cldmnky/drumlogue-units/issues).
