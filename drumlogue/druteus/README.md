# Druteus — Proteus/1 SF2 Synth for Korg Drumlogue

**Version**: 0.1.0
**Developer**: CLDM (0x434C444D)
**Type**: Polyphonic Synthesizer Unit
**Engine**: TinySoundFont (TSF)

---

## Overview

Druteus is a SoundFont (SF2) sample player that recreates the E-mu Proteus/1 dual-layer voice architecture on the Korg drumlogue. It ships with 442 factory presets extracted from the original Proteus ROM, rendered through TinySoundFont with per-patch AHDSR envelopes, crossfade/keyboard splits, LFO modulation, DSP effects, and a trance gate — all in a polyphonic 16-voice synth.

### Key Features

- **SF2 SoundFont Player** — loads `.sf2` files from `Programs/` (up to 60 browsable)
- **442 Proteus/1 Factory Presets** — all 6 ROM volumes (Fresh Mix, Fusion, Textures, ProPower, Introspection, Full Spectrum)
- **Dual-Layer Architecture** — primary + secondary instrument per patch with independent tuning, volume, pan
- **Per-Patch AHDSR Envelopes** — attack, hold, decay, sustain, release for each layer
- **Keyboard Splits & Velocity Crossfade** — per-patch crossfade modes (VEL/KEY) with configurable switch points
- **Patch Modulation Matrix** — LFO1 + LFO2 per patch with 5 waveforms
- **Live User LFO** — 5 waveforms (TRI, SINE, SQR, SAW, RND), 3 destinations (PITCH, VOL, BOTH)
- **State-Variable Filter** — LP12, LP24, HP12, BP12 with smoothed cutoff and resonance
- **DSP Effects** — Chorus (stereo widener) + Reverb (Griesinger topology)
- **Trance Gate** — 1–32 step tempo-synchronized patterns
- **Workshop Curves** — LINEAR, EXP, LOG, COMP, STEEP
- **Polyphonic** — 1–16 voices with oldest-note voice stealing
- **MIDI Expression** — velocity, pitch bend (±2 octaves), channel pressure (→ expression)
- **NEON-Optimized** — ARM NEON SIMD for DSP, float-precision TSF lowpass filter
- **Async SF2 Loading** — chunked file loading (128 KB per frame), no UI freeze

---

## Installation

1. Copy your `.sf2` files to the `Programs/` folder on the drumlogue (via USB mass storage)
2. Download `druteus.drmlgunit`
3. Copy the `.drmlgunit` file to `Units/Synth/` on the drumlogue
4. Safely eject the drumlogue and restart
5. The synth appears as **"DRUTEUS"** in the Synth slot selection

**Recommended**: Bundle `Proteus1_Instruments.sf2` (4.3 MB) in `Programs/` for the full 442-patch experience.
Download from [musical-artifacts.com](https://musical-artifacts.com/artifacts/764).

---

## Parameters

Druteus has 24 parameters across 6 pages.

### Page 1: Sound Source

| # | Name | Range | Default | Description |
|---|------|-------|---------|-------------|
| 0 | **SFONT** | 0–63 | first SF2 | SoundFont file selector from `Programs/` |
| 1 | **PATCH** | 0–441 | 0 | Proteus/1 preset (shows patch name) |
| 2 | **VOICES** | 1–16 | 16 | Maximum polyphony |
| 3 | **TUNE** | −12..+12 | 0 | Global transpose (semitones) |

### Page 2: Pitch & Mix

| # | Name | Range | Default | Description |
|---|------|-------|---------|-------------|
| 4 | **FINETN** | −63..+63 | 0 | Fine tuning (cents) |
| 5 | **VOLUME** | 0–127 | 100 | Master volume (CC7) |
| 6 | **PAN** | 0–127 | 64 | Stereo pan (CC10, center=64) |
| 7 | *(blank)* | — | — | — |

### Page 3: Layer Control

| # | Name | Range | Default | Description |
|---|------|-------|---------|-------------|
| 8 | **XFADE** | OFF, VEL, KEY | OFF | Crossfade mode between layers |
| 9 | **LAYERS** | BOTH, PRI, SEC | BOTH | Layer mode |
| 10 | *(blank)* | — | — | — |
| 11 | *(blank)* | — | — | — |

### Page 4: Effects & Feel

| # | Name | Range | Default | Description |
|---|------|-------|---------|-------------|
| 12 | **CHORUS** | 0–15 | 0 | Chorus mix (blended with patch sends) |
| 13 | **REVERB** | 0–127 | 0 | Reverb amount |
| 14 | **V.CURVE** | LINEAR, EXP, LOG, COMP, STEEP | LINEAR | Velocity response curve |
| 15 | **TGATE** | OFF, 1–32 | OFF | Trance gate pattern length |

### Page 5: Filter

| # | Name | Range | Default | Description |
|---|------|-------|---------|-------------|
| 16 | **CUTOFF** | 0–127 | 127 | SVF filter cutoff (smoothed) |
| 17 | **RES** | 0–127 | 0 | SVF filter resonance (smoothed) |
| 18 | *(blank)* | — | — | — |
| 19 | *(blank)* | — | — | — |

### Page 6: LFO

| # | Name | Range | Default | Description |
|---|------|-------|---------|-------------|
| 20 | **LFO RTE** | 0–127 | 0 | User LFO rate |
| 21 | **LFO AMT** | 0–127 | 0 | User LFO modulation amount |
| 22 | **LFO DST** | PITCH, VOL, BOTH | PITCH | Destination |
| 23 | **LFO WAV** | TRI, SINE, SQR, SAW, RND | SINE | Waveform |

---

## Factory Presets

Druteus ships with **442 presets** extracted from the original E-mu Proteus/1 ROM:

| Volume | Presets | Character |
|--------|---------|-----------|
| **Fresh Mix (3100)** | 1–63 | Pop/rock, acoustic instruments, drums |
| **Fusion (3101)** | 64–125 | Synth leads, pads, fusion textures |
| **Textures (3102)** | 126–187 | Ambient, atmospheric, evolving pads |
| **ProPower (3114)** | 188–249 | Orchestral, cinematic, power brass |
| **Introspection (3115)** | 250–311 | New age, meditative, soft pads |
| **Full Spectrum (3116)** | 312–441 | Complete range, specialty sounds |

Presets are browsable by their original Proteus name (e.g., "FMstylePiano", "WarmStrings", "ResoBass").

---

## Sound Design Tips

### Dual-Layer Stacks

1. Pick a preset with two layers (most have them)
2. Set **LAYERS** to BOTH for full stack
3. Use **XFADE** = VEL to crossfade layers by playing velocity
4. Mix layer volumes with patch's built-in per-layer volume

### Velocity Crossfade

- Set **XFADE** to VEL
- Play softly → primary layer dominates
- Play hard → secondary layer fades in
- Switch point determined by the patch's `switchpoint` value

### Keyboard Splits

- Set **XFADE** to KEY
- Left hand → primary layer (bass/comp)
- Right hand → secondary layer (lead/pad)
- Split point at patch's `switchpoint` key

### Rich Pads

1. Select a pad preset (Textures volume, presets 126–187)
2. Increase **REVERB** to 60–80
3. Add slow **LFO RTE** (20–30) with **LFO AMT** (30–50) to **VOL**
4. Increase **CHORUS** to 8–12 for width

### Percussive Sounds

1. Select a drum/percussion preset (Fresh Mix volume)
2. Set **V.CURVE** to EXP or STEEP for velocity sensitivity
3. Use **TGATE** for rhythmic patterns (syncs to drumlogue tempo)

---

## Velocity Curves

| Curve | Formula | Character |
|-------|---------|-----------|
| **LINEAR** | v / 127 | Direct 1:1 |
| **EXP** | (v / 127)² | Soft low, aggressive high |
| **LOG** | log(1+v) / log(128) | Quick low-end response |
| **COMP** | sqrt(v / 127) | Reduced dynamic range |
| **STEEP** | (v / 127)³ | Quiet low, full at max |

---

## SF2 Files

Place `.sf2` files in `/var/lib/drumlogued/userfs/Programs`. The unit scans this folder at startup. **SFONT** parameter lists all `.sf2` files found.

For the full Proteus/1 experience:
- `Proteus1_Instruments.sf2` (4.3 MB) — 126 instrument building blocks (auto-selected at startup)
- `Proteus1_Presets.sf2` (4.2 MB) — 129 factory preset patches (optional, patches embedded)

Download the SF2 files from [musical-artifacts.com](https://musical-artifacts.com/artifacts/764).

---

## CPU Usage

- **1–4 voices**: ~10–15%
- **5–10 voices**: ~15–25%
- **11–16 voices**: ~25–40%
- **DSP effects active**: additional ~5–10%

NEON SIMD and float-precision TSF lowpass (vs double) reduce per-voice CPU by 40–60% compared to stock TSF.

---

## Troubleshooting

### Unit doesn't load

- Ensure file is named `druteus.drmlgunit`
- Check file is in `Units/Synth/`, not a subdirectory
- Power cycle the drumlogue
- Verify drumlogue firmware is up to date

### Corrupt / distorted audio

- The unit works correctly. Distortion on the hardware was traced to a PERF_MON debug register access (fixed in current build)
- If the unit hangs on load, ensure `Proteus1_Instruments.sf2` is present in `Programs/` and is not corrupted

### No sound

- Check VOLUME parameter (default 100)
- Check patch selection — some patches have very low default volume
- Check VOICES is > 0
- Ensure an SF2 file is selected in SFONT

### Wrong patch names

- Patch names are loaded from the embedded Proteus/1 patch table, not the SF2 file. These are fixed at build time

---

## Known Characteristics

- **Dual-layer polyphony**: Each voice uses 2 TSF channels (primary + secondary). VOICES=16 means up to 32 TSF voices, but polyphony is limited to 16 simultaneous notes
- **SF2 loading**: 4.3 MB takes ~34 frames (181 ms) to load. Audio is silent during loading
- **Per-voice envelopes**: CPU cost scales with number of TSF voices playing, not the polyphony setting
- **Filter quality**: The SVF filter uses `float` coefficients. At very high cutoff (>20 kHz), it approaches but does not exceed stability limits

---

## Credits

- **TinySoundFont**: Bernhard Schelling (MIT License)
- **Proteus Patch Data**: Extracted from EMU Proteus/1 SysEx ROM dumps
- **logue FS Framework**: Oleg Burdaev (dukesrg), MIT License
- **Reverb DSP**: Mutable Instruments Rings (Griesinger reverb)
- **Chorus DSP**: common/ChorusStereoWidener
- **Voice Allocator**: common/VoiceAllocatorCore

---

## Version History

**v0.1.0** (2026-07-08)
- SF2 SoundFont playback via TinySoundFont
- 442 Proteus/1 factory presets with dual-layer voice engine
- Per-patch AHDSR envelopes and LFO modulation matrix
- Velocity crossfade and keyboard splits per patch
- State-variable filter (LP12, LP24, HP12, BP12) with smoothing
- Chorus + Reverb DSP effects chain
- Trance gate with tempo sync
- 5 velocity curves
- User LFO with 5 waveforms and 3 destinations
- Polyphonic voice allocation (1–16 voices, oldest-note stealing)
- Chunked async SF2 loading (128 KB per frame)
- ARM NEON optimizations (float-precision TSF lowpass, SIMD gain/interleave)
- MIDI pitch bend (±2 octaves) and channel pressure support
- Fixed: unit hang from DWT register access (PERF_MON disabled in production)

---

## License

This unit incorporates TinySoundFont (MIT License) and is based on code from various open-source projects. See the repository LICENSE file for details.
