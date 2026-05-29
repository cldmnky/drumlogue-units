# DRUTEUS — Implementation Plan

## Purpose

This document describes the current state of the `druteus` drumlogue synth unit — a Proteus/1 SF2 soundfont player built on TinySoundFont (TSF) with DSP effects, ADSR envelope control, and LFO modulation. It serves as a reference for what's been built and what next steps remain.

---

## 1. Identity

- **Unit name (display):** `DRUTEUS` (7 chars, 7-bit ASCII)
- **Developer ID:** `0x434C444DU` ("CLDM")
- **Unit ID:** `0x00000005U`
- **Version:** `0.1.0` → `0x000100U`
- **Module type:** `synth`
- **Target platform:** drumlogue (ARM Cortex-A7, Linux, 48 kHz stereo float)

## 2. SF2 Files

- `Proteus1_Presets.sf2` — 4.2 MB, 129 factory Proteus/1 patches (default)
- `Proteus1_Instruments.sf2` — 4.2 MB, 126 raw instrument building blocks
- Deployed to `/var/lib/drumlogued/userfs/Programs` on device
- Loaded at runtime via chunked async state machine in `unit_render`

## 3. Parameter Set (24 slots, 6 pages)

### Page 1 — Sound Source
| Index | Name   | Type | Range | Default | Notes |
|-------|--------|------|-------|---------|-------|
| 0     | SFONT  | strings | 0–63 | 0 | SF2 filename from Programs/ |
| 1     | PRESET | strings | 0–255 | 0 | Preset name from TSF |
| 2     | VOICES | none  | 1–32 | 16 | Max polyphony |
| 3     | TUNE   | none  | 0–24 (-12..+12) | 12 (0) | Semitone transpose applied as `note + (tune-12)` |

### Page 2 — Pitch & Mix
| Index | Name   | Type | Range | Default | Notes |
|-------|--------|------|-------|---------|-------|
| 4     | FINETN | none  | 0–126 (-63..+63) | 63 (0) | Cent fine-tune via `tsf_channel_set_tuning` |
| 5     | VOLUME | none  | 0–127 | 100 | CC7, via `tsf_channel_midi_control` |
| 6     | PAN    | none  | 0–127 | 64 | CC10, via `tsf_channel_midi_control` |
| 7     | (blank) | none | — | — | — |

### Page 3 — Envelope (ADSR)
| Index | Name   | Type | Range | Default | Notes |
|-------|--------|------|-------|---------|-------|
| 8     | ENV ATK | none | 0–99 | 0 | Attack time (0→1ms, 99→956ms, exponential) |
| 9     | ENV DEC | none | 0–99 | 0 | Decay time |
| 10    | ENV SUS | none | 0–99 | 99 | Sustain level (0=silent..99=full) |
| 11    | ENV REL | none | 0–99 | 99 | Release time |

### Page 4 — Effects & Feel
| Index | Name   | Type | Range | Default | Notes |
|-------|--------|------|-------|---------|-------|
| 12    | CHORUS | none | 0–15 | 0 | DSP chorus mix (0=off); `ChorusStereoWidener` |
| 13    | REVERB | none | 0–127 | 0 | DSP reverb amount (0=off); `rings::Reverb` |
| 14    | V.CURVE | strings | 0–4 | 0 | Velocity curve: LINEAR, EXP, LOG, COMP, STEEP |
| 15    | (blank) | none | — | — | — |

### Page 5 — Play Mode
| Index | Name | Type | Range | Default | Notes |
|-------|------|------|-------|---------|-------|
| 16    | SOLO | none | 0–1 | 0 | 0=poly, 1=mono (monophonic mode) |
| 17    | (blank) | none | — | — | — |
| 18    | (blank) | none | — | — | — |
| 19    | (blank) | none | — | — | — |

### Page 6 — LFO
| Index | Name   | Type | Range | Default | Notes |
|-------|--------|------|-------|---------|-------|
| 20    | LFO RTE | none | 0–127 | 0 | LFO rate |
| 21    | LFO AMT | none | 0–127 | 0 | LFO amount |
| 22    | LFO DST | strings | 0–2 | 0 | LFO destination: PITCH, VOL, BOTH |
| 23    | LFO WAV | strings | 0–4 | 1 | LFO waveform: SINE, TRI, SQUARE, SAW, S&H |

## 4. Architecture

### TSF Integration
- Single-header `tsf.h` (v0.9, unmodified), `TSF_STATIC` + `TSF_NO_STDIO`
- `TSF_STEREO_INTERLEAVED` output mode (48 kHz float)
- Chunked async load: `fread` 131072 bytes per render frame (~33 frames = ~88 ms for 4.2 MB)
- `logue_fs.h` wraps `scandir` for `.sf2` file listing in Programs folder
- File path: `/var/lib/drumlogued/userfs/Programs`

### ADSR Envelope
- Post-TSF per-sample volume scaling
- Triggered on note-on, released when `active_notes` counter reaches 0
- Exponential time curve: `ms = powf(value / 99.0f, 2.0f) * kMaxMs`
- Defaults (ATK=0, DEC=0, SUS=99, REL=99) = transparent pass-through
- Post-release silence hold (10 frames) to suppress TSF release tails

### DSP Effects
- **Chorus**: `dsp::ChorusStereoWidener` from `drumlogue/common/` — modulated short delay lines
- **Reverb**: `rings::Reverb` from `eurorack/rings/` — Griesinger topology, 64 KB buffer
- Both chain through de-interleaved buffers: chorus → reverb → re-interleave

### LFO
- 5 waveforms: sine, triangle, square, saw, sample-and-hold
- 3 destinations: pitch, volume, both
- Phase advances every render frame when active
- Default shape: SINE

### Velocity Curves
- LINEAR: `v / 127.0f`
- EXP: `powf(v / 127.0f, 2.0f)`
- LOG: `logf(1.0f + v) / logf(128.0f)`
- COMP: `sqrtf(v / 127.0f)`
- STEEP: `powf(v / 127.0f, 3.0f)`

### MIDI / Playability
- `unit_note_on`/`unit_note_off` — full polyphonic via TSF
- `unit_pitch_bend` — raw value to `tsf_channel_set_pitchwheel` (bend range set via `tsf_channel_set_pitchrange` at load)
- `unit_channel_pressure` → CC11 expression
- `unit_gate_on`/`unit_gate_off` — drum trigger style using preset NOTE param
- SOLO mode: monophonic retrigger; `active_notes` counter tracks polyphony for ADSR release
- `s_apply_params()` restores volume, pan, tuning, pitchwheel after SF2 reload

## 5. File Layout

```
drumlogue/druteus/
├── Makefile                — SDK Makefile template (DO NOT MODIFY)
├── config.mk               — Project config (includes /repo/eurorack for Rings)
├── header.c                — Unit metadata, all 24 param descriptors
├── unit.cc                 — Full implementation (~924 lines)
├── tsf.h                   — TinySoundFont v0.9 (unmodified)
├── logue_fs.h              — scandir-based file listing, DT_LNK support
├── sfx/
│   ├── Proteus1_Presets.sf2
│   └── Proteus1_Instruments.sf2
├── tools/
│   ├── extract_patches.py  — ROM patch extraction script
│   ├── proteus_patches.h   — Extracted patch data
│   └── proteus_patches.json
├── PLAN.md                 — This file
└── druteus.drmlgunit       — Build artifact (~31 KB)

test/druteus/
├── Makefile                — Desktop test harness
├── main.cc                 — Test entry point
├── druteus_test            — Built binary
└── fixtures/               — WAV test files

test/presets-editor/app/test_druteus.c — Polyphonic + note-off test
```

## 6. Build

```bash
./build.sh druteus           # Hardware build
./build.sh druteus clean     # Clean
```

- ~30,868 bytes final `.drmlgunit`, zero unexpected undefined symbols
- Desktop native build (for `test/presets-editor`): `./build-system/build-native.sh druteus`

## 7. Known Design Decisions

- **Not modifying `tsf.h`** — ADSR, LFO, chorus/reverb are post-TSF DSP shaders
- **TSF_STATIC** — all TSF symbols static, avoids linker conflicts
- **No ROM instrument decoding** — SF2 approach was simpler; Proteus ROM analysis abandoned
- **ADSR defaults = pass-through**: ATK=0, DEC=0, SUS=99, REL=99 (SF2's natural envelope plays)
- **Unit IDs under `0x434C444DU`**: 0x01=clouds-revfx, 0x02=elementish-synth, 0x03=pepege-synth, 0x04=drupiter-synth, 0x05=druteus

## 8. What's Next (Phase 3)

- Custom voice engine with Proteus ROM instrument tables
- Crossfade layers and keyboard splits
- Per-layer effects (distortion, EQ, filter from Proteus architecture)
- Multi-channel MIDI support (drum kit on CH10, melodic on 1–9)
- Preset save/load for user patches
