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

> **Note (Phase 3 / review #22):** the original Phase-1 user ADSR page
> was removed in favour of per-patch AHDSR data carried by each
> Proteus preset.  The Page 3 / Page 5 layout below matches
> `header.c` as of the current build.

### Page 1 — Sound Source
| Index | Name   | Type | Range | Default | Notes |
|-------|--------|------|-------|---------|-------|
| 0     | SFONT  | strings | 0–63 | 0 | SF2 filename from Programs/ |
| 1     | PATCH  | strings | 0–441 | 0 | Proteus patch name from `proteus_patches.h` |
| 2     | VOICES | none  | 1–16 | 16 | Max polyphony (voice stealing via VoiceAllocatorCore) |
| 3     | TUNE   | none  | −12..+12 | 0 | Semitone transpose applied as `note + tune` |

### Page 2 — Pitch & Mix
| Index | Name   | Type | Range | Default | Notes |
|-------|--------|------|-------|---------|-------|
| 4     | FINETN | none  | −63..+63 | 0 | Cent fine-tune via `tsf_channel_set_tuning` (deferred to render thread) |
| 5     | VOLUME | none  | 0–127 | 100 | CC7, via `tsf_channel_midi_control` (deferred to render thread) |
| 6     | PAN    | none  | 0–127 | 64 | CC10, via `tsf_channel_set_pan` (deferred to render thread) |
| 7     | (blank) | none | — | — | — |

### Page 3 — Layer Control
| Index | Name   | Type | Range | Default | Notes |
|-------|--------|------|-------|---------|-------|
| 8     | XFADE  | strings | 0–2 | 0 | Crossfade override: OFF, VEL, KEY |
| 9     | LAYERS | strings | 0–2 | 0 | Layer mode: BOTH, PRI, SEC |
| 10    | (blank) | none | — | — | — |
| 11    | (blank) | none | — | — | — |

### Page 4 — Effects & Feel
| Index | Name   | Type | Range | Default | Notes |
|-------|--------|------|-------|---------|-------|
| 12    | CHORUS | none | 0–15 | 0 | DSP chorus mix (0=off); `ChorusStereoWidener` (smoothed) |
| 13    | REVERB | none | 0–127 | 0 | DSP reverb amount (0=off); `rings::Reverb` (smoothed) |
| 14    | V.CURVE | strings | 0–4 | 0 | Velocity curve: LINEAR, EXP, LOG, COMP, STEEP |
| 15    | (blank) | none | — | — | — |

### Page 5 — Filter
| Index | Name | Type | Range | Default | Notes |
|-------|------|------|-------|---------|-------|
| 16    | CUTOFF | none | 0–127 | 127 | SVF cutoff (smoothed) |
| 17    | RES    | none | 0–127 | 0 | SVF resonance (smoothed) |
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

### Voice Allocator
- `common::VoiceAllocatorCore` from `drumlogue/common/`
- Manages polyphony limiting and voice stealing
- Oldest-note stealing strategy (steals the voice that was triggered earliest)
- Polyphonic mode with configurable max voices (1–16)
- Tracks active notes for ADSR envelope release triggering
- VOICES param dynamically reinitializes allocator via `voice_allocator.Init(value)`

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

## 8. Phase 3 — Proteus Preset Voice Engine

Phase 3 replaces the current flat SF2 playback with a **preset-aware voice engine** that uses the extracted Proteus/1 patch data (`proteus_patches.h`, 768 presets) to recreate the original dual-layer architecture on top of TinySoundFont. The SF2 files remain the sound source — no raw ROM access needed.

### 8.1 What We Have

| Resource | Content | Status |
|----------|---------|--------|
| `proteus_patches.h` | 768 presets: `proteus_patch_t` with layer indices, tuning, volume, pan, chorus, crossfade, LFO shapes | Ready, not yet integrated |
| `proteus_patches.json` | Full 116-parameter extraction for all 768 presets (envelopes, modulation matrix, key ranges) | Available for tooling |
| SF2 files | 4.2 MB PCM samples in SoundFont format (samples, loop points, root keys, instrument defs) | In use via TSF |
| TinySoundFont | Multi-channel SF2 playback (16 MIDI channels, per-channel preset/volume/pan/tuning) | In use |

### 8.2 What TSF Provides

TSF already handles sample playback, looping, root-key pitch shifting, and per-channel control. What's missing is the **preset-level voice routing** that the Proteus/1 did on top of that:

- Dual-layer voices (primary + secondary instrument per preset)
- Crossfade between layers (velocity or keyboard)
- Keyboard splits (up to 4 zones via link presets)
- Per-preset LFO modulation with 5 waveforms and routing matrix
- Per-preset AHDSR envelopes (currently we use a single global ADSR)
- Per-layer chorus sends and reverb

### 8.3 Architecture

```
┌─────────────────────────────────────────────────────────┐
│                    Preset Selector                        │
│  (768 patches from proteus_patches.h, selectable via     │
│   PRESET param — replaces current flat SF2 preset list)  │
└──────────────────────┬──────────────────────────────────┘
                       │ proteus_patch_t
                       ▼
┌─────────────────────────────────────────────────────────┐
│                  Voice Router                             │
│  Reads patch data, configures TSF channels:              │
│  - CH0: Primary instrument (i1instrument)                │
│  - CH1: Secondary instrument (i2instrument)              │
│  - Per-channel: volume, pan, tuning, chorus send         │
│  - Key range filtering (i1lowkey/highkey, i2lowkey/hk)   │
│  - Crossfade: velocity or keyboard split                 │
└──────────────────────┬──────────────────────────────────┘
                       │ tsf_channel_note_on/off
                       ▼
┌─────────────────────────────────────────────────────────┐
│              TinySoundFont (2 channels)                   │
│  CH0 + CH1 render interleaved float → mixed to output    │
└──────────────────────┬──────────────────────────────────┘
                       │ stereo float
                       ▼
┌─────────────────────────────────────────────────────────┐
│              Post-TSF DSP (unchanged)                     │
│  ADSR envelope → Chorus → Reverb → output                │
└─────────────────────────────────────────────────────────┘
```

### 8.4 Implementation Steps

#### Step 3A: Integrate Patch Data

**Goal:** Make all 768 Proteus presets selectable. Replace the current PRESET param (0–255 flat SF2 presets) with a patch-indexed selector.

**Changes:**
1. `#include "tools/proteus_patches.h"` in `unit.cc`
2. Add `param_patch` enum value (replaces or extends current `param_preset`)
3. PRESET param range: 0–767, string display from `kProteusPatchTable[value].name`
4. On preset change, load the `proteus_patch_t` and configure voice routing (Step 3B)
5. Keep SF2 file loaded — patches reference instrument indices within the same SF2

**Instrument index mapping:** The `i1instrument` / `i2instrument` values (0–16383) are Proteus ROM instrument numbers. These need to be mapped to SF2 preset indices. The SF2 files were built from the same ROM, so the mapping is: `sf2_preset = proteus_instrument_index % tsf_get_presetcount(soundfont)`. Verify this mapping with a test preset.

#### Step 3B: Dual-Layer Voice Engine

**Goal:** Play primary and secondary instruments simultaneously using TSF channels 0 and 1.

**Changes:**
1. On note-on, trigger both channels:
   ```cpp
   tsf_channel_note_on(soundfont, 0, note, vel * vol1);  // primary
   tsf_channel_note_on(soundfont, 1, note, vel * vol2);  // secondary
   ```
2. Configure per-channel from patch data:
   ```cpp
   tsf_channel_set_presetindex(soundfont, 0, patch.i1instrument % preset_count);
   tsf_channel_set_presetindex(soundfont, 1, patch.i2instrument % preset_count);
   tsf_channel_set_volume(soundfont, 0, patch.i1volume / 127.0f);
   tsf_channel_set_volume(soundfont, 1, patch.i2volume / 127.0f);
   tsf_channel_set_pan(soundfont, 0, (patch.i1pan + 7) * 9);  // -7..+7 → 0..127
   tsf_channel_set_pan(soundfont, 1, (patch.i2pan + 7) * 9);
   ```
3. Apply per-layer tuning:
   ```cpp
   int cents1 = patch.i1tuningcoarse * 100 + patch.i1tuningfine;
   tsf_channel_set_tuning(soundfont, 0, 440.0f * powf(2.0f, cents1 / 1200.0f));
   ```
4. On note-off, release both channels
5. Skip secondary if `i2instrument == -1` (single-layer patch)

#### Step 3C: Crossfade and Key Range Filtering

**Goal:** Implement velocity crossfade and keyboard splits from patch data.

**Crossfade modes** (from `crossfademode`):
- `0` = Off: both layers always play
- `1` = Velocity crossfade: layer volumes scale with velocity relative to `switchpoint`
- `2` = Keyboard crossfade: layers split at `switchpoint` key

**Implementation:**
```cpp
float vel_scale_0 = 1.0f, vel_scale_1 = 1.0f;
if (patch.crossfademode == 1) {
    float sp = patch.switchpoint / 127.0f;
    float v = velocity / 127.0f;
    vel_scale_0 = (v < sp) ? 1.0f : 1.0f - (v - sp) / (1.0f - sp);
    vel_scale_1 = (v > sp) ? 1.0f : v / sp;
}
```

**Key range filtering:**
```cpp
bool play_layer_0 = (note >= patch.i1lowkey && note <= patch.i1highkey);
bool play_layer_1 = (note >= patch.i2lowkey && note <= patch.i2highkey);
```

**Keyboard splits (link presets):** If `link1/2/3 != -1`, the preset references other presets for different key zones (`lowkey0-3` / `highkey0-3`). On note-on, check which zone the note falls in and load the linked preset's instrument for that channel. This requires caching up to 4 linked presets.

#### Step 3D: Per-Preset LFO Modulation

**Goal:** Apply the Proteus LFO shapes and modulation routing from patch data instead of the current global LFO.

**From patch data (JSON, not yet in `.h`):**
- `lfo1shape` (0–4), `lfo1frequency`, `lfo1delay`, `lfo1variation`, `lfo1amount`
- `lfo2shape` (same), same fields
- Modulation destinations: pitch, volume, pan, filter cutoff, etc.

**Implementation:**
1. Extend `proteus_patch_t` in a new header to include LFO frequency/delay/amount fields (regenerate from JSON)
2. Replace the current global LFO with per-preset LFO state
3. Apply LFO to TSF channels via `tsf_channel_set_pitchwheel` (pitch) and `tsf_channel_midi_control` CC7 (volume)
4. LFO delay: ramp amount from 0 to target over `lfo_delay` frames after note-on

**Modulation matrix:** The JSON contains 8 real-time modulation slots (source → destination → amount). Implement the most useful ones:
- Mod wheel (CC1) → filter cutoff or LFO amount
- Aftertouch → volume or pitch
- Pitch bend → pitch (already implemented)

#### Step 3E: Per-Preset Envelopes

**Goal:** Use the Proteus AHDSR envelope data from patches instead of the current global ADSR.

**From patch data (JSON):**
- `i1attack`, `i1hold`, `i1decay`, `i1sustain`, `i1release` (0–99 each)
- `i1envelopeon` (enable/disable)
- Same for layer 2 (`i2*`)

**Implementation:**
1. Extend `proteus_patch_t` to include envelope fields (or read from JSON at build time into a separate table)
2. On note-on, initialize per-voice AHDSR state from patch data
3. The current ADSR code becomes the per-voice envelope engine — parameterized by patch values instead of global params
4. If `i1envelopeon == 0`, skip envelope for that layer (TSF's SF2 envelope plays naturally)

**Envelope time curve:** The Proteus uses a proprietary curve for 0–99 → milliseconds. Approximation: `ms = powf(value / 99.0f, 1.5f) * 5000.0f` (0→0ms, 99→5000ms). Fine-tune by ear against hardware reference.

#### Step 3F: Per-Layer Chorus Sends

**Goal:** Use per-layer chorus send levels from patch data instead of the current global chorus mix.

**From patch data:** `i1chorus` (0–15), `i2chorus` (0–15)

**Implementation:**
1. Render each TSF channel to separate buffers (TSF supports per-channel rendering)
2. Apply chorus to each channel's buffer scaled by its chorus send level
3. Mix channels to output
4. This replaces the current single global chorus mix

**Alternative (simpler):** Use the per-layer chorus values to set the global chorus mix as a weighted average: `chorus_mix = (i1chorus * vol1 + i2chorus * vol2) / (vol1 + vol2) / 15.0f`

### 8.5 Parameter Changes

The current 24-param layout needs adjustment for Phase 3:

| Change | Reason |
|--------|--------|
| PATCH range: 0–441 | 442 Proteus patches (header.c `max=441`) instead of flat SF2 presets |
| Remove ENV ATK/DEC/SUS/REL (params 8–11) | Replaced by per-patch AHDSR from patch data |
| Add PATCH param (new) | Select from 442 named Proteus presets |
| Keep LFO RTE/AMT/DST/WAV (params 20–23) | Override per-patch LFO with user values |
| Keep CHORUS/REVERB (params 12–13) | Override per-patch effects with user values |

**Revised param layout (proposed):**

| Page | Slot | Name | Notes |
|------|------|------|-------|
| 1 | 0 | SFONT | SF2 file selector (unchanged) |
| 1 | 1 | PATCH | 0–441 Proteus preset selector (replaces PRESET) |
| 1 | 2 | VOICES | Max polyphony (unchanged) |
| 1 | 3 | TUNE | Global transpose (unchanged) |
| 2 | 4 | FINETN | Global fine tune (deferred to audio thread) |
| 2 | 5 | VOLUME | Global volume (deferred to audio thread) |
| 2 | 6 | PAN | Global pan (deferred to audio thread) |
| 2 | 7 | (blank) | |
| 3 | 8 | XFADE | Crossfade override: OFF, VEL, KEY (replaces ENV ATK) |
| 3 | 9 | LAYERS | Layer mode: BOTH, PRI, SEC (replaces ENV DEC) |
| 3 | 10 | (blank) | (replaces ENV SUS) |
| 3 | 11 | (blank) | (replaces ENV REL) |
| 4 | 12 | CHORUS | Chorus override (smoothed) |
| 4 | 13 | REVERB | Reverb override (smoothed) |
| 4 | 14 | V.CURVE | Velocity curve (unchanged) |
| 4 | 15 | (blank) | |
| 5 | 16 | CUTOFF | SVF cutoff (smoothed) |
| 5 | 17 | RES | SVF resonance (smoothed) |
| 5 | 18 | (blank) | |
| 5 | 19 | (blank) | |
| 6 | 20 | LFO RTE | LFO rate override (unchanged) |
| 6 | 21 | LFO AMT | LFO amount override (unchanged) |
| 6 | 22 | LFO DST | LFO destination (unchanged) |
| 6 | 23 | LFO WAV | LFO waveform (unchanged) |

### 8.6 Data Pipeline

To get the full patch data into firmware:

1. **Extend `proteus_patches.h`:** Regenerate from `proteus_patches.json` with additional fields:
   - Envelope: `i1attack`, `i1hold`, `i1decay`, `i1sustain`, `i1release`, `i1envelopeon`
   - LFO: `lfo1frequency`, `lfo1delay`, `lfo1amount`, same for LFO2
   - Key ranges: `i1lowkey`, `i1highkey`, `i2lowkey`, `i2highkey`
   - Crossfade: `crossfadedirection`, `crossfadebalance`, `crossfadeamount`, `switchpoint`
   - Links: `link1`, `link2`, `link3`, `lowkey0-3`, `highkey0-3`

2. **Estimate size:** Current `proteus_patch_t` is ~32 bytes × 768 = 24 KB. Extended struct (~64 bytes) × 768 = ~48 KB. Fits comfortably in the binary.

3. **Instrument index mapping:** Build a lookup table mapping Proteus instrument numbers to SF2 preset indices. Generate from the SF2 file's preset names matched against known Proteus instrument names.

### 8.7 Testing Strategy

1. **Desktop test:** Extend `test/druteus/` to load a patch by index, trigger notes, verify dual-channel output
2. **Preset verification:** Write a script that iterates all 768 patches, triggers a note, and checks for silence/crashes
3. **A/B comparison:** Compare output of Phase 3 engine against current TSF-only playback for known presets (e.g., "FMstylePiano" = patch 0)
4. **Hardware test:** Load unit, scroll through patches, verify dual-layer behavior, crossfade, and keyboard splits

### 8.8 Risks and Mitigations

| Risk | Mitigation |
|------|------------|
| Instrument index mapping incorrect | Verify with known presets; build mapping table from SF2 preset names |
| 48 KB patch table too large for binary | Current binary is 31 KB; 48 KB data + code should stay under 128 KB |
| Per-channel TSF rendering doubles CPU | Profile on hardware; fall back to single-channel if needed |
| Envelope curves don't match Proteus | Tune by ear; the curves are approximations anyway |
| Crossfade sounds unnatural | Start with simple linear crossfade; add curved crossfade if needed |

### 8.9 Future (Phase 4)

- **Multi-channel MIDI:** Drum kit on CH10, melodic patches on CH1–9 (TSF supports 16 channels)
- **Preset save/load:** Store user-modified patches (override values) in drumlogue preset slots
- **Per-layer DSP:** Distortion, EQ, filter per layer (from Mutable Instruments eurorack modules)
- **Sample ROM extraction:** If original Proteus sample ROMs become available, extract raw PCM for a fully custom voice engine without TSF dependency

---

## 9. Design Decisions (post-Phase-3 review)

### 9.1 Keyboard crossfade split uses the transposed (audible) note

`compute_crossfade_weights` is called with the transposed `note` value (after `Params[param_transpose]` is applied). This means the keyboard split point at `switchpoint` moves with the TUNE param — when the user transposes, the split point follows the music. The previous reference code used a hard-coded `64`, which was a placeholder; using the transposed note is more ergonomic for performance.

### 9.2 `param_soundfont` is user-selectable, defaulting to Proteus

`unit_init` defaults the SFONT param to the index of `Proteus1_Instruments.sf2` if that file is present in `Programs/`. The user is free to pick any other SF2 from the listing; the value is *not* re-asserted on reload or in `unit_set_param_value`. Previously the param was force-overwritten on every load and every user write, which silently undid user selection.

### 9.3 Stacked same-note voice-off releases the youngest voice

When the same MIDI note is held by multiple voice slots (e.g. fast repeated gate triggers, or drumlogue's held-note re-trigger behavior), a single note-off releases only the *most recent* voice. The older voice continues to ring. Implemented via `find_youngest_active_voice_for_note` in `unit.cc` (returns the slot with the highest `note_on_time` from the allocator).

### 9.4 Per-voice L1 release tail frees the allocator slot

When the L1 envelope release tail completes, both `voice_env[vi].active` and `voice_allocator.GetVoice(vi).active` are cleared in lockstep. This keeps the allocator and the envelope state in sync and frees the slot for new `NoteOn` calls.

### 9.5 Out-of-range Proteus instrument IDs fall back to preset 1, not 0

`resolve_proteus_instrument_to_sf2_preset` returns `-1` for unmapped Proteus IDs. The caller in `s_load_patch` clamps the result to `1` (not `0`) because preset index 0 in many SF2 files is an EOA/dummy entry. Falling back to `1` selects the first audible preset, which is closer to the previous behavior and avoids shipping a silent fallback.
