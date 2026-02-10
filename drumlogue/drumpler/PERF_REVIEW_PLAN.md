# Drumpler Performance Review Plan

## Executive Summary

The drumpler unit (JV-880 emulator based on Nuked-SC55) was **overloading CPU at 116.96%** in QEMU ARM profiling at the start of optimization work. After completing Stages 1-3 plus hardware-critical fixes, QEMU RenderTotal is down to **~95%** and the Emulator metric shows a **~13% reduction** in the inner loop. Three critical hardware bugs were fixed:

1. **Init hang** (10-20s+ blocking) → eliminated via deferred warmup (`4e9f2df`)
2. **Note-on hang** (printf in MCU hot loop) → eliminated via DEBUG guards (`fec2729`)
3. **CPU overload hang** (eventual timeout) → targeted via sleep fast-forward (`317c52c`) + interrupt bitmask dispatch (`37e812b`)

**Status:** All Stage 1-3 items complete. Stages 1-2 showed minimal QEMU improvement (QEMU chrono timing has ~30% variance and can't resolve sub-microsecond ARM-specific gains). Stage 3 showed measurable improvement: addclip20 simplification (-4.81% RenderTotal), instruction loop inlining (-3.80% CPU), interrupt bitmask (-13% Emulator metric). **Hardware testing needed** to validate real-world Cortex-A7 performance.

**Remaining opportunities:** Oversampling toggle (3.3.3, ~40% PCM savings), UART/Analog polling reduction, adaptive quality, reduced internal rate (risky).

---

## Progress Log

### Item 4.1 — PCM_Update cycle increment reciprocal multiply — COMPLETED 2026-02-10
**Status:** Done
**Before baseline:**
- CPU Total: 79.74%
- Emulator cycles avg: 864453
- RenderTotal: 95.71%
- RSS memory: 12.09 MB

**After results:**
- CPU Total: 74.78% (change: -4.96%)
- Emulator cycles avg: 726531 (change: -137922)
- RenderTotal: 98.82% (change: +3.11%)
- RSS memory: 13.81 MB (change: +1.72 MB)

**Audio regression:** None (1-byte header difference only; audio data matches)
**Files changed:** drumlogue/drumpler/emulator/pcm.cc, drumlogue/drumpler/PERF_REVIEW_PLAN.md, test/drumpler/fixtures/drumpler_baseline_before_4_1.wav, test/drumpler/fixtures/drumpler_after_4_1.wav, test/drumpler/baselines/drumpler_baseline_before_4_1.wav, test/drumpler/baselines/drumpler_after_4_1.wav
**Summary:** Replaced the per-iteration divide in PCM_Update cycle accounting with a fixed-point reciprocal multiply; CPU usage and emulator cycles improved while RenderTotal increased slightly.

### Item 3.5.1 — Branchless status flag update — REVERTED 2026-02-08
**Status:** Reverted
**Before baseline:**
- CPU Total: 98.87%
- Emulator cycles avg: 77418
- RenderTotal: 120.22%
- RSS memory: 14.12 MB

**After results:**
- CPU Total: 99.75% (change: +0.88%)
- Emulator cycles avg: 98011 (change: +20593)
- RenderTotal: 126.14% (change: +5.92%)
- RSS memory: 14.06 MB (change: -0.06 MB)

**Audio regression:** None (1-byte header difference only; audio data matches)
**Files changed:** drumlogue/drumpler/emulator/mcu_opcodes.cc, drumlogue/drumpler/PERF_REVIEW_PLAN.md, test/drumpler/fixtures/drumpler_baseline_before_3_5_1.wav, test/drumpler/fixtures/drumpler_after_3_5_1.wav, test/drumpler/baselines/drumpler_baseline_before_3_5_1.wav, test/drumpler/baselines/drumpler_after_3_5_1.wav
**Summary:** Replaced per-flag status updates in ADD/SUB helpers with a single NZCV mask write; QEMU metrics regressed while RSS decreased slightly. Reverted the code change.

### Item 3.5.2 — Fast opcode fetch path — COMPLETED 2026-02-08
**Status:** Done
**Before baseline:**
- CPU Total: 99.75%
- Emulator cycles avg: 98011
- RenderTotal: 126.14%
- RSS memory: 14.06 MB

**After results:**
- CPU Total: 100.34% (change: +0.59%)
- Emulator cycles avg: 112547 (change: +14536)
- RenderTotal: 124.51% (change: -1.63%)
- RSS memory: 14.09 MB (change: +0.03 MB)

**Audio regression:** None (2-byte header difference only; audio data matches)
**Files changed:** drumlogue/drumpler/emulator/mcu.cc, drumlogue/drumpler/PERF_REVIEW_PLAN.md, test/drumpler/fixtures/drumpler_baseline_before_3_5_2.wav, test/drumpler/fixtures/drumpler_after_3_5_2.wav, test/drumpler/baselines/drumpler_baseline_before_3_5_2.wav, test/drumpler/baselines/drumpler_after_3_5_2.wav
**Summary:** Added a ROM1 fast path for opcode fetch; QEMU CPU and emulator cycles regressed while RenderTotal improved slightly.

### Item 1.1 — Remove unconditional -DDEBUG — COMPLETED 2026-02-08
**Status:** Done
**Before baseline:**
- CPU Total: 103.83%
- Emulator cycles avg: 196767
- RenderTotal: 122.19%
- RSS memory: 14.36 MB

**After results:**
- CPU Total: 104.73% (change: +0.90%)
- Emulator cycles avg: 218272 (change: +21505)
- RenderTotal: 123.01% (change: +0.82%)
- RSS memory: 14.58 MB (change: +0.22 MB)

**Audio regression:** None (byte-level diff only at header; audio data matches)
**Files changed:** drumlogue/drumpler/config.mk, drumlogue/drumpler/PERF_REVIEW_PLAN.md
**Summary:** Made DEBUG conditional on build flag; no performance win in QEMU, slight regression in CPU/cycle metrics.

### Item 1.2 — Guard unit_note_on fprintf with DEBUG — COMPLETED 2026-02-08
**Status:** Done
**Before baseline:**
- CPU Total: 104.73%
- Emulator cycles avg: 218272
- RenderTotal: 123.01%
- RSS memory: 14.58 MB

**After results:**
- CPU Total: 105.89% (change: +1.16%)
- Emulator cycles avg: 246124 (change: +27852)
- RenderTotal: 124.42% (change: +1.41%)
- RSS memory: 14.58 MB (change: +0.00 MB)

**Audio regression:** None (2-byte header difference only; audio data matches)
**Files changed:** drumlogue/drumpler/unit.cc, drumlogue/drumpler/PERF_REVIEW_PLAN.md
**Summary:** Guarded note-on logging with DEBUG; no performance improvement in QEMU and slight regression in cycle metrics.

### Item 1.3 — Reduce audio_buffer_size to 1024 — COMPLETED 2026-02-08
**Status:** Done
**Before baseline:**
- CPU Total: 105.89%
- Emulator cycles avg: 246124
- RenderTotal: 124.42%
- RSS memory: 14.58 MB

**After results:**
- CPU Total: 107.46% (change: +1.57%)
- Emulator cycles avg: 283898 (change: +37774)
- RenderTotal: 124.45% (change: +0.03%)
- RSS memory: 14.85 MB (change: +0.27 MB)

**Audio regression:** None (byte-level header difference only; audio data matches)
**Files changed:** drumlogue/drumpler/emulator/mcu.h, drumlogue/drumpler/PERF_REVIEW_PLAN.md
**Summary:** Reduced internal audio buffer size to 1024 frames; QEMU metrics regressed slightly while RSS increased.

### Item 1.4 — Use ROM2_SIZE_JV880 for rom2 array — COMPLETED 2026-02-08
**Status:** Done
**Before baseline:**
- CPU Total: 107.64%
- Emulator cycles avg: 287995
- RenderTotal: 124.89%
- RSS memory: 14.54 MB

**After results:**
- CPU Total: 108.37% (change: +0.73%)
- Emulator cycles avg: 305520 (change: +17525)
- RenderTotal: 125.46% (change: +0.57%)
- RSS memory: 14.45 MB (change: -0.09 MB)

**Audio regression:** None (byte-level header difference only; audio data matches)
**Files changed:** drumlogue/drumpler/emulator/mcu.h, drumlogue/drumpler/PERF_REVIEW_PLAN.md
**Summary:** Shrunk ROM2 buffer to JV-880 size and aligned mask; QEMU CPU/cycles regressed slightly while RSS decreased.

### Item 1.5 — Guard unit_render fprintf with DEBUG — COMPLETED 2026-02-08
**Status:** Done
**Before baseline:**
- CPU Total: 108.37%
- Emulator cycles avg: 305520
- RenderTotal: 125.46%
- RSS memory: 14.45 MB

**After results:**
- CPU Total: 109.10% (change: +0.73%)
- Emulator cycles avg: 322478 (change: +16958)
- RenderTotal: 130.14% (change: +4.68%)
- RSS memory: 14.67 MB (change: +0.22 MB)

**Audio regression:** None (byte-level header difference only; audio data matches)
**Files changed:** drumlogue/drumpler/unit.cc, drumlogue/drumpler/PERF_REVIEW_PLAN.md
**Summary:** Guarded unit_render debug logging with DEBUG; QEMU cycle metrics regressed and RSS increased slightly.

### Stage 1 Summary — COMPLETED 2026-02-08
**Start baseline (Item 1.1 before):** CPU 103.83%, Emulator 196767, RenderTotal 122.19%, RSS 14.36 MB
**End result (Item 1.5 after):** CPU 109.10%, Emulator 322478, RenderTotal 130.14%, RSS 14.67 MB
**Net change:** CPU +5.27%, Emulator +125711, RenderTotal +7.95%, RSS +0.31 MB
**Notes:** All Stage 1 changes removed blocking debug I/O but regressed QEMU metrics; audio output unchanged.

### Item 2.1 — Free-function architecture (jcmoyer pattern) — COMPLETED 2026-02-08
**Status:** Done
**Before baseline:**
- CPU Total: 107.03%
- Emulator cycles avg: 273367
- RenderTotal: 124.52%
- RSS memory: 14.29 MB

**After results:**
- CPU Total: 107.59% (change: +0.56%)
- Emulator cycles avg: 286836 (change: +13469)
- RenderTotal: 125.33% (change: +0.81%)
- RSS memory: 14.63 MB (change: +0.34 MB)

**Audio regression:** None (2-byte header difference only; audio data matches)
**Files changed:** drumlogue/drumpler/emulator/mcu.cc, drumlogue/drumpler/PERF_REVIEW_PLAN.md, test/drumpler/fixtures/drumpler_baseline_after_2_1.wav, test/drumpler/baselines/drumpler_baseline_after_2_1.wav
**Summary:** Refactored the hot update loop into free-function helpers; QEMU metrics regressed slightly while audio output stayed unchanged.

### Item 2.2 — Precompute PCM Configuration (PCM_Config pattern) — COMPLETED 2026-02-08
**Status:** Done
**Before baseline:**
- CPU Total: 105.46%
- Emulator cycles avg: 235677
- RenderTotal: 124.16%
- RSS memory: 14.36 MB

**After results:**
- CPU Total: 105.60% (change: +0.14%)
- Emulator cycles avg: 239101 (change: +3424)
- RenderTotal: 124.35% (change: +0.19%)
- RSS memory: 14.50 MB (change: +0.14 MB)

**Audio regression:** None (2-byte header difference only; audio data matches)
**Files changed:** drumlogue/drumpler/emulator/pcm.h, drumlogue/drumpler/emulator/pcm.cc, drumlogue/drumpler/PERF_REVIEW_PLAN.md, test/drumpler/fixtures/drumpler_baseline_after_2_2.wav, test/drumpler/baselines/drumpler_baseline_after_2_2.wav, test/drumpler/fixtures/drumpler_baseline_before_2_2.wav, test/drumpler/baselines/drumpler_baseline_before_2_2.wav
**Summary:** Cached `reg_slots` in a small PCM config struct and updated it only on register writes; QEMU metrics regressed slightly while audio output stayed unchanged.

### Item 2.3 — Voice-skip optimization in PCM_Update — COMPLETED 2026-02-08
**Status:** Done
**Before baseline:**
- CPU Total: 106.87%
- Emulator cycles avg: 269123
- RenderTotal: 129.38%
- RSS memory: 14.46 MB

**After results:**
- CPU Total: 106.76% (change: -0.11%)
- Emulator cycles avg: 266538 (change: -2585)
- RenderTotal: 127.65% (change: -1.73%)
- RSS memory: 14.58 MB (change: +0.12 MB)

**Audio regression:** None (2-byte header difference only; audio data matches)
**Files changed:** drumlogue/drumpler/emulator/pcm.cc, drumlogue/drumpler/PERF_REVIEW_PLAN.md, test/drumpler/fixtures/drumpler_baseline_after_2_3.wav, test/drumpler/baselines/drumpler_baseline_after_2_3.wav, test/drumpler/fixtures/drumpler_baseline_before_2_3.wav, test/drumpler/baselines/drumpler_baseline_before_2_3.wav
**Summary:** Skipped idle voices before touching per-voice state; modest improvement in emulator cycles with no audio regression.

### Item 2.4 — Reduce Init warmup iterations — COMPLETED 2026-02-08
**Status:** Done
**Before baseline:**
- CPU Total: 107.66%
- Emulator cycles avg: 288097
- RenderTotal: 125.64%
- RSS memory: 14.78 MB

**After results:**
- CPU Total: 107.89% (change: +0.23%)
- Emulator cycles avg: 293846 (change: +5749)
- RenderTotal: 126.30% (change: +0.66%)
- RSS memory: 14.31 MB (change: -0.47 MB)

**Audio regression:** None (1-byte header difference only; audio data matches)
**Files changed:** drumlogue/drumpler/synth.h, drumlogue/drumpler/PERF_REVIEW_PLAN.md, test/drumpler/fixtures/drumpler_baseline_after_2_4.wav, test/drumpler/baselines/drumpler_baseline_after_2_4.wav, test/drumpler/fixtures/drumpler_baseline_before_2_4.wav, test/drumpler/baselines/drumpler_baseline_before_2_4.wav
**Summary:** Reduced warmup iterations to shorten init time; QEMU performance regressed slightly while audio output stayed unchanged.

### Item 3.1 — MCU instruction loop optimization — COMPLETED 2026-02-08
**Status:** Done
**Before baseline:**
- CPU Total: 107.29%
- Emulator cycles avg: 279491
- RenderTotal: 126.48%
- RSS memory: 14.60 MB

**After results:**
- CPU Total: 103.49% (change: -3.80%)
- Emulator cycles avg: 188285 (change: -91206)
- RenderTotal: 126.93% (change: +0.45%)
- RSS memory: 14.18 MB (change: -0.42 MB)

**Audio regression:** None (2-byte header difference only; audio data matches)
**Files changed:** drumlogue/drumpler/emulator/mcu.cc, drumlogue/drumpler/PERF_REVIEW_PLAN.md, test/drumpler/fixtures/drumpler_baseline_after_3_1.wav, test/drumpler/baselines/drumpler_baseline_after_3_1.wav, test/drumpler/fixtures/drumpler_baseline_before_3_1.wav, test/drumpler/baselines/drumpler_baseline_before_3_1.wav
**Summary:** Inlined instruction fetch/dispatch in `MCU_ReadInstruction` to reduce call overhead; significant emulator cycle reduction with unchanged audio.

### Item 3.2 — NEON/SIMD for PCM math — COMPLETED 2026-02-08
**Status:** Done
**Before baseline:**
- CPU Total: 104.69%
- Emulator cycles avg: 216858
- RenderTotal: 127.46%
- RSS memory: 14.29 MB

**After results:**
- CPU Total: 103.44% (change: -1.25%)
- Emulator cycles avg: 186917 (change: -29941)
- RenderTotal: 128.54% (change: +1.08%)
- RSS memory: 14.00 MB (change: -0.29 MB)

**Audio regression:** None (2-byte header difference only; audio data matches)
**Files changed:** drumlogue/drumpler/emulator/pcm.cc, drumlogue/drumpler/PERF_REVIEW_PLAN.md, test/drumpler/fixtures/drumpler_baseline_after_3_2.wav, test/drumpler/baselines/drumpler_baseline_after_3_2.wav, test/drumpler/fixtures/drumpler_baseline_before_3_2.wav, test/drumpler/baselines/drumpler_baseline_before_3_2.wav
**Summary:** Added a NEON helper for pan/reverb/chorus multiplies in the PCM mix path with a scalar fallback; emulator cycles improved with no audio regression.

### Item 3.3.1 — addclip20 simplification — COMPLETED 2026-02-08
**Status:** Done
**Before baseline:**
- CPU Total: 102.46%
- Emulator cycles avg: 163588
- RenderTotal: 125.87%
- RSS memory: 13.88 MB

**After results:**
- CPU Total: 99.93% (change: -2.53%)
- Emulator cycles avg: 102897 (change: -60691)
- RenderTotal: 121.06% (change: -4.81%)
- RSS memory: 14.12 MB (change: +0.24 MB)

**Audio regression:** None (1-byte header difference only; audio data matches)
**Files changed:** drumlogue/drumpler/emulator/pcm.cc, drumlogue/drumpler/PERF_REVIEW_PLAN.md, test/drumpler/fixtures/drumpler_baseline_before_3_3_1.wav, test/drumpler/fixtures/drumpler_after_3_3_1.wav, test/drumpler/baselines/drumpler_baseline_before_3_3_1.wav, test/drumpler/baselines/drumpler_after_3_3_1.wav
**Summary:** Simplified `addclip20` to sign-extend + add; QEMU CPU/cycles improved while RSS ticked up slightly.

### Item synth.h — MaxAbsBuffer, fprintf fix, helpers — COMPLETED 2026-02-08
**Status:** Done (commit `1c29d9f`)
**Changes:**
- Added `MaxAbsBuffer()` with NEON early-exit to `neon_dsp.h`
- Fixed NoteOn fprintf (was unguarded)
- Extracted `ComputeRolandChecksum()` + `GetPatchDataPtr()` helpers in `synth.h`

**QEMU results:**
- RenderTotal: 95.81% (post-deferred-warmup measurement includes this)

**Audio regression:** None (2-byte header difference only)
**Files changed:** drumlogue/drumpler/synth.h, drumlogue/drumpler/emulator/jv880_wrapper.cc, drumlogue/common/neon_dsp.h

### Item 2.4b — Deferred warmup (fix unit_init hang) — COMPLETED 2026-02-08
**Status:** Done (commit `4e9f2df`)
**Problem:** 500-iteration warmup loop in `unit_init()` blocked for 10-20+ seconds on hardware, causing the drumlogue to hang when selecting the unit.
**Fix:** Replaced blocking warmup with deferred state machine (`warmup_remaining_`) that runs warmup frames incrementally in `Render()`. First few render calls produce warmup audio; transitions to normal operation automatically.
**Files changed:** drumlogue/drumpler/synth.h, drumlogue/drumpler/unit.cc

### Item HW-fix-1 — Guard emulator printf calls (fix note-on hang) — COMPLETED 2026-02-08
**Status:** Done (commit `fec2729`)
**Problem:** Unguarded `printf("Unknown read/write %x")` calls in mcu.cc/submcu.cc MCU hot loop. Each printf blocks ~50-200µs on ARM. On hardware, this caused the unit to hang immediately upon receiving a note-on MIDI message.
**Fix:** Guarded 14 printf calls in mcu.cc, 5 in submcu.cc, 2 in synth.h with `#ifdef DEBUG`.
**Hardware impact:** Eliminated note-on hang. Unit now plays audio.
**Files changed:** drumlogue/drumpler/emulator/mcu.cc, drumlogue/drumpler/emulator/submcu.cc, drumlogue/drumpler/synth.h

### Item 3.1b — MCU sleep fast-forward — COMPLETED 2026-02-08
**Status:** Done (commit `317c52c`)
**Problem:** During MCU sleep mode (~50-70% of execution time), every MCU step still runs `MCU_Interrupt_Handle()` which scans 36 interrupt/trap flags — pure waste when no interrupt is pending.
**Fix:** Added `wakeup_pending` flag to MCU struct, set by `MCU_Interrupt_SetRequest/Exception/TRAPA`. Render loop uses lightweight `MCU_Step_Sleep()` that skips interrupt scan and instruction fetch during sleep. Wakeup routes through full `MCU_Step` on next iteration.
**QEMU results:**
- RenderTotal: 95.42% (within QEMU chrono variance of 95.81% baseline)
- Note: QEMU chrono timing cannot resolve the MCU_Interrupt_Handle savings visible on real ARM in-order core
**Audio regression:** None (2-byte WAV diff, rounding only)
**Files changed:** drumlogue/drumpler/emulator/mcu.cc, drumlogue/drumpler/emulator/mcu.h, drumlogue/drumpler/emulator/mcu_interrupt.cc

### Item 3.1c — Interrupt bitmask dispatch + voice idle threshold — COMPLETED 2026-02-08
**Status:** Done (commit `37e812b`)
**Problem:** `MCU_Interrupt_Handle()` scanned 16-element `trapa_pending[]` + 20-element `interrupt_pending[]` with a big switch on every MCU step (~19,500 calls per 256-frame render). Additionally, PCM voice idle threshold was 960 frames (20ms), meaning inactive voices were processed for too long.
**Fix:**
1. **Interrupt bitmask + dispatch table:**
   - Added `interrupt_pending_mask` (uint32_t) and `trapa_pending_mask` (uint16_t) bitmask mirrors to `mcu_t`
   - `MCU_Interrupt_Handle` early-exits when all masks are zero (single branch vs 36+ flag scan)
   - Replaced 16-element trapa loop with `__builtin_ctz` (ARM: RBIT+CLZ)
   - Replaced 20-iteration interrupt source loop + big switch with static dispatch table + CTZ iteration
2. **Voice idle threshold:** Lowered `kIdleThreshold` from 960→480 (20ms→10ms) for faster voice culling

**QEMU results:**
- Emulator metric: 4680 avg cycles vs 5382 baseline (**~13% reduction**)
- RenderTotal: 95.32% (QEMU variance)
- Binary: 4,588,580 bytes (120 bytes smaller than before — table smaller than switch)

**Audio regression:** None (2-byte WAV diff, rounding only)
**Files changed:** drumlogue/drumpler/emulator/mcu_interrupt.cc, drumlogue/drumpler/emulator/mcu.h, drumlogue/drumpler/emulator/pcm.h

### Hardware Testing Status — 2026-02-08
**Commits ready for hardware test:** `4e9f2df` through `37e812b` (5 commits)
**Known issues:**
- Unit previously hung on init (fixed: deferred warmup `4e9f2df`)
- Unit previously hung on note-on (fixed: printf guards `fec2729`)
- Unit previously overloaded CPU and hung after a while (targeted: sleep fast-forward `317c52c` + interrupt bitmask `37e812b`)
**Remaining concern:** Emulator may still exceed 5.33ms audio deadline on Cortex-A7. If so, further options include: oversampling toggle (3.3.3), FRT timer precomputation, adaptive quality reduction.

---

## 1. Current Profiling Baseline (QEMU ARM)

| Metric | Value |
|---|---|
| **CPU Total** | 116.96% (overload) |
| **RenderTotal** | 124.66% of buffer budget |
| **Emulator counter** | 512K avg cycles / 18750 budget = 2731% |
| **Interleave** | 0.36% (negligible) |
| **RSS memory** | 15.25 MB |
| **Binary size** | 4.5 MB (drumpler_internal.drmlgunit) |

### ELF Section Breakdown

| Section | Size | Notes |
|---|---|---|
| `.text` | 41 KB | Code (MCU opcodes, PCM, wrapper) |
| `.rodata` | 4.3 MB | Embedded ROM blob (wave ROMs) |
| `.data` | 2.2 KB | Initialized globals |
| `.bss` | 1.6 KB | Zero-initialized globals |
| **Total LOAD (RX)** | 4.5 MB | Mainly ROM data |
| **Total LOAD (RW)** | 4.7 KB | Very small writable footprint |

### MCU Struct Heap Memory (~944 KB)

| Field | Size | Notes |
|---|---|---|
| `rom1[0x8000]` | 32 KB | Copied from ROM blob |
| `rom2[0x80000]` | **512 KB** | ⚠️ **Only 256 KB used for JV-880** |
| `ram[0x400]` | 1 KB | |
| `sram[0x8000]` | 32 KB | |
| `nvram[0x8000]` | 32 KB | Copied from ROM blob |
| `cardram[0x8000]` | 32 KB | |
| `sample_buffer_l[32768]` | **128 KB** | ⚠️ **Oversized ~96×** |
| `sample_buffer_r[32768]` | **128 KB** | ⚠️ **Oversized ~96×** |
| `uart_buffer[8192]` | 8 KB | |
| `pcm_t` (embedded) | ~34 KB | ram1/ram2/eram |
| `sub_mcu` | ~4.5 KB | Unused for JV-880 |
| `midiQueue[128]` | ~5 KB | |
| **Total** | **~944 KB** | Could be reduced to ~630 KB |

---

## 2. Optimization Opportunities

### 🟢 Phase 1: Quick Wins (Low Effort, Moderate Impact)

#### 1.1 Remove Unconditional `-DDEBUG` from config.mk

**File:** `drumlogue/drumpler/config.mk` line 83

**Problem:** `-DDEBUG` is **always defined**, enabling:
- `fprintf`/`fflush` in `synth.h` Render() (first 3 calls only — limited)
- `fprintf`/`fflush` in `jv880_wrapper.cc` SendMidi() (**every MIDI message**)
- `fprintf`/`fflush` in `jv880_wrapper.cc` Render error path

While the synth.h DEBUG prints are limited to 3 calls, the `SendMidi()` path logs **every MIDI byte** with fprintf + fflush — blocking I/O that stalls the audio thread during parameter changes or note-on bursts.

**Fix:** Remove the unconditional `UDEFS += -DDEBUG` line. Add it conditionally:
```makefile
ifeq ($(DEBUG),1)
    UDEFS += -DDEBUG
endif
```

**Expected impact:** Eliminates ~10-50µs per MIDI message from blocking fprintf+fflush in audio thread. Minor code-size reduction from dead code elimination.

#### 1.2 Remove fprintf from `unit_note_on`

**File:** `drumlogue/drumpler/unit.cc` lines 143-145

**Problem:** Every note-on triggers fprintf + fflush — **unconditional**, not guarded by DEBUG:
```cpp
void unit_note_on(uint8_t note, uint8_t velocity) {
  fprintf(stderr, "[Drumpler] unit_note_on: note=%u vel=%u\n", note, velocity);
  fflush(stderr);
  s_synth_instance.NoteOn(note, velocity);
}
```

On drumlogue hardware, stderr writes go to a serial/syslog-like facility. Each fprintf+fflush is a **blocking syscall** that can take 50-200µs on ARM — catastrophic when handling rapid note sequences.

**Fix:** Guard with `#ifdef DEBUG` or remove entirely.

**Expected impact:** Eliminates 50-200µs blocking per note event.

#### 1.3 Reduce `audio_buffer_size` from 32768 to 1024

**File:** `drumlogue/drumpler/emulator/mcu.h` line 287

**Problem:** `audio_buffer_size = 4096 * 8 = 32768` floats per channel. With drumlogue's 256-frame buffers at 48kHz, the actual needed buffer per render call is:
```
renderBufferFrames ≈ ceil(256 × 64000/48000) + 2 = 344 frames max
```

The buffers are **96× oversized**, wasting 248 KB of RAM:
- `sample_buffer_l[32768]` = 128 KB (only ~1.3 KB used)
- `sample_buffer_r[32768]` = 128 KB (only ~1.3 KB used)

**Fix:** Reduce to 1024 (allows up to ~750-frame output buffers with margin):
```cpp
static const int audio_buffer_size = 1024;
```

**Expected impact:** Saves **248 KB RAM**, better cache locality for hot render loop.

#### 1.4 Use `ROM2_SIZE_JV880` for rom2 array

**File:** `drumlogue/drumpler/emulator/mcu.h` line 309

**Problem:** `rom2[ROM2_SIZE]` allocates 512 KB, but JV-880 only uses 256 KB (`ROM2_SIZE_JV880 = 0x40000`). The `startSC55()` function already only copies 256 KB and sets `rom2_mask = ROM2_SIZE_JV880 - 1`.

**Fix:** Conditionally size the array or always use the JV-880 size since this codebase only targets JV-880:
```cpp
uint8_t rom2[ROM2_SIZE_JV880];  // 256KB for JV-880
```

**Expected impact:** Saves **256 KB RAM**.

#### 1.5 Remove `unit_render` first-3-calls fprintf

**File:** `drumlogue/drumpler/unit.cc` lines 103-107

**Problem:** Not performance-critical (only fires 3 times), but sets bad precedent and adds code/binary overhead.

**Fix:** Guard with `#ifdef DEBUG` or remove.

---

### 🟡 Phase 2: Structural Improvements (Medium Effort, Significant Impact)

#### 2.1 Adopt Free-Function Architecture (jcmoyer Pattern)

**Reference:** [jcmoyer/Nuked-SC55](https://github.com/jcmoyer/Nuked-SC55)

**Problem:** Our codebase uses **member functions** on large `MCU` class:
```cpp
// Our code (member functions):
void MCU::MCU_ReadInstruction() { /* accesses this->rom1, this->mcu, ... */ }
void MCU::updateSC55WithSampleRate(...) { /* this->pcm.PCM_Update(), ... */ }
```

The jcmoyer fork uses **free functions** with pass-by-reference:
```cpp
// jcmoyer (free functions):
void MCU_Step(mcu_t& mcu) { /* no hidden 'this' pointer */ }
void PCM_Update(pcm_t& pcm, uint64_t cycles) { /* direct struct access */ }
```

**Why this matters for ARM Cortex-A7:**
- Member functions carry implicit `this` pointer through every call, consuming a register (r0)
- Free functions allow the compiler to inline and register-allocate structure fields more aggressively
- Virtual dispatch overhead eliminated (though our code uses no virtuals, the ABI still imposes constraints)
- With LTO (Link-Time Optimization, already enabled via build system), free functions allow cross-translation-unit inlining that member functions may inhibit

**Scope:** Refactor `MCU::MCU_ReadInstruction()`, `Pcm::PCM_Update()`, and related hot-path functions to free functions. This is the largest change but touches the hottest code.

**Expected impact:** 5-15% speedup from better register allocation and inlining in the inner loop.

#### 2.2 Precompute PCM Configuration (PCM_Config Pattern)

**Reference:** jcmoyer `PCM_Config` struct and `PCM_GetConfig()` function

**Problem:** Our `PCM_Update()` recomputes configuration values from `config_reg_3c`/`config_reg_3d` on every cycle iteration. The jcmoyer fork precomputes these into a `PCM_Config` struct:
```cpp
struct PCM_Config {
    int reg_slots;
    int write_mask;
    int noise_mask;
    int orval;
    // ... precomputed from config registers
};
```

**Fix:** Add similar precomputation—update PCM_Config only when config registers change (rare), not on every cycle.

**Expected impact:** Reduces per-cycle overhead in the hottest loop. Small but cumulative over 342+ iterations per render call.

#### 2.3 Voice-Skip Optimization in PCM_Update

**Problem:** `PCM_Update()` processes all 28+ configured voice slots every cycle, regardless of whether voices are active:
```cpp
int reg_slots = (pcm.config_reg_3d & 31) + 1;
```

Many voices are idle (no note playing). JV-880 typically uses 4-8 active voices even with complex patches.

**Fix:** Track voice activity via `voice_mask & voice_mask_pending` (already partially implemented with `voice_idle_frames[]` in our `pcm_t`). Skip processing for voices that have been silent for `kIdleThreshold` frames.

**Caveats:** Must be cycle-accurate — cannot skip voices that might be in release/decay phase. Needs careful testing with all patches.

**Expected impact:** 20-50% reduction in PCM processing time when playing 4 voices out of 28 slots.

#### 2.4 Reduce Init Warmup Iterations — ✅ SUPERSEDED by Deferred Warmup (commit `4e9f2df`)

**File:** `drumlogue/drumpler/synth.h` lines 115-156

**Problem:** The warmup loop ran **500+ iterations**, blocking `unit_init()` for 10-20+ seconds on hardware.

**Actual fix (Item 2.4b):** Replaced blocking warmup entirely with deferred state machine (`warmup_remaining_`) in `Render()`. First few render calls produce warmup audio; transitions to normal operation automatically. This eliminates the init hang completely rather than just reducing iteration count.

**Expected impact:** ~~Halves init time~~ → **Eliminates init blocking entirely.**

---

### 🔴 Phase 3: Deep Optimization (High Effort, Potentially Large Impact)

#### 3.1 MCU Instruction Loop Optimization — ✅ COMPLETED (commits `317c52c`, `37e812b`)

**The critical hot loop** (mcu.cc `MCU_UpdateSC55WithSampleRate`):
```cpp
for (int i = 0; sample_write_ptr < renderBufferFrames; i++) {
    if (state.sleep && !self.wakeup_pending) {
        MCU_Step_Sleep(self, state);  // skip interrupt scan + instruction fetch
    } else {
        if (state.sleep) self.wakeup_pending = 0;
        MCU_Step(self, state);  // full path with bitmask-optimized interrupt handler
    }
}
```

**Implemented optimizations:**
1. ✅ **Sleep fast-forward** (`317c52c`): Skip `MCU_Interrupt_Handle` and `MCU_ReadInstruction` during sleep via `wakeup_pending` flag
2. ✅ **Interrupt bitmask dispatch** (`37e812b`): Replace 36-flag scan with single `uint32_t` test + CTZ + static dispatch table
3. ✅ **Voice idle threshold** (`37e812b`): Halved to 480 frames (10ms) for faster voice culling
4. ✅ **Printf guards** (`fec2729`): Eliminated 21 unguarded printf calls in MCU hot loop

**Remaining opportunities:**
- UART/Analog polling: Could use modulo check or next-event timer to reduce call frequency
- Cycle accumulation: Fixed 12-cycle assumption still prevents instruction batching

**Measured impact:** Emulator metric 4680 vs 5382 baseline (~13% reduction in QEMU)

#### 3.2 NEON/SIMD Optimization for PCM Math

**Target functions:** `addclip20()`, `multi()`, `eram_unpack/pack()`

**Problem:** These are called dozens of times per voice, per PCM cycle, per emulator step. They operate on individual 20-bit/32-bit values using scalar math.

```cpp
inline int32_t addclip20(int32_t add1, int32_t add2, int32_t cin) {
    return sx20(add1) + sx20(add2) + cin;
}
inline int32_t multi(int32_t val1, int8_t val2) {
    return sx20(val1) * val2;
}
```

**NEON opportunity:** Process 4 voices in parallel using `int32x4_t` NEON intrinsics. The PCM voice loop iterates over slots sequentially — could be restructured to process 4 slots at a time.

**Caveats:** The voice processing has data dependencies between slots (accumulator mixing), making full parallelization difficult. May require restructuring the loop order.

**Expected impact:** 2-4× speedup of PCM voice processing if parallelizable. Likely 1.5-2× realistic due to dependencies.

#### 3.3 Cherry-Pick jcmoyer Fork Optimizations

**Reference:** https://github.com/jcmoyer/Nuked-SC55

**Analysis completed** — the jcmoyer fork was reviewed in detail. Below are the specific
cherry-pickable optimizations, ranked by expected impact on ARM Cortex-A7.

##### 3.3.1 `addclip20` Simplification — HIGH IMPACT

**Our version** (pcm.cc:305-313) uses uint32_t with explicit overflow clipping (2 branches + 5 ANDs + 2 comparisons per call):
```cpp
inline uint32_t addclip20(uint32_t add1, uint32_t add2, uint32_t cin) {
    uint32_t sum = (add1 + add2 + cin) & 0xfffff;
    if ((add1 & 0x80000) != 0 && (add2 & 0x80000) != 0 && (sum & 0x80000) == 0)
        sum = 0x80000;
    else if ((add1 & 0x80000) == 0 && (add2 & 0x80000) == 0 && (sum & 0x80000) != 0)
        sum = 0x7ffff;
    return sum;
}
```

**jcmoyer's version** (pcm.cpp:279-283) — branchless, just sign-extend + add:
```cpp
inline int32_t addclip20(int32_t add1, int32_t add2, int32_t cin) {
    return sx20(add1) + sx20(add2) + cin;
}
```

**Why it matters:** `addclip20` is called **72 times** in pcm.cc, many inside the per-voice
loop (~72 × N_active_voices × per_cycle). Each branch miss costs ~10 cycles on Cortex-A7.

**Risk:** Semantic difference — our version explicitly clips to 20-bit signed range on overflow;
jcmoyer's doesn't clip, result can exceed 20-bit range. Need WAV regression testing.
Also requires changing signature from uint32_t to int32_t at all 72 call sites.

**Estimated savings:** ~5-15% of PCM_Update time. Single biggest win available.

##### 3.3.2 `PCM_Config` Expansion + `PCM_GetConfig()` — MEDIUM IMPACT

**Our `pcm_config_t`** (pcm.h:37-39): Only caches `reg_slots`.

**jcmoyer's `PCM_Config`** (pcm.h:39-52):
```cpp
struct PCM_Config {
    uint32_t orval        = 0;
    int      dac_mask     = 0;
    uint8_t  noise_mask   = 0;
    uint8_t  write_mask   = 0;
    bool     oversampling = false;
    uint8_t  reg_slots    = 1;
};
```

Our final-mixing section hardcodes `noise_mask = 0`, `orval = 0`, `write_mask = 3` as local
variables every cycle (pcm.cc:526-528). jcmoyer pre-computes these when `config_reg_3c` is
written via `PCM_GetConfig()`, then reads from `pcm.config.*` in the hot loop.

**Risk:** Low — purely structural. Port `PCM_GetConfig()` and expand the struct.

**Estimated savings:** Small per-cycle reduction, but also a **correctness fix** (our hardcoded
values prevent proper noise/dithering config for different JV-880 modes).

##### 3.3.3 Oversampling Control Toggle — MEDIUM PRIORITY (CORRECTNESS)

Our code has `if (true) // oversampling` hardcoded (pcm.cc:569). jcmoyer has
`pcm.enable_oversampling` flag and `pcm.config.oversampling` (from config register). Porting
the config-driven approach would:
- Allow disabling oversampling to halve PCM output rate (64kHz → 32kHz)
- Save ~40% of sample callback overhead
- But reduces audio quality

**Recommendation:** Port the mechanism, default to enabled. Useful as opt-in CPU saver.

##### Items Already Ported or Not Applicable

| Item | Status | Notes |
|------|--------|-------|
| `MCU_Step` sleep optimization | ✅ Implemented | `317c52c` — `MCU_Step_Sleep` + `wakeup_pending` flag |
| `BoundedOrderedBitSet` | ✅ Superseded | `37e812b` — bitmask + CTZ dispatch table (same concept, different impl) |
| Filter direct multiplication | ✅ Already ported | `!is_mk1` path uses `reg1 * (int8_t)(...)` |
| Free-function architecture | ✅ Partial (2.1) | Hot MCU path done; further refactor low ROI |
| Component-separated memory | ❌ Skip | Our monolithic layout is better for ARM cache |

##### Implementation Order for 3.3

| # | Sub-item | Impact | Risk | Effort |
|---|----------|--------|------|--------|
| 1 | `addclip20` simplification (3.3.1) | HIGH (~5-15% PCM) | Medium (audio diff) | Small |
| 2 | `PCM_Config` expansion (3.3.2) | Medium (correctness) | Low | Small |
| 3 | Oversampling control (3.3.3) | Medium (opt-in CPU) | Low | Small |

**Workflow per sub-item:** Capture baseline WAV → apply change → run WAV regression → QEMU profile → commit.

#### 3.4 Decimation / Reduced Internal Rate

**Problem:** The emulator runs at 64kHz internally, generating ~342 samples per 256-frame output buffer. Each sample requires running the full MCU+PCM pipeline.

**Option A: Run at 32kHz internally**
- Halves the number of MCU instructions executed
- Doubles the resampling ratio (32kHz → 48kHz requires upsampling)
- **Risk:** May break firmware timing assumptions, cause audio artifacts

**Option B: Skip alternate PCM cycles**
- Run MCU at full speed, PCM at half rate
- More compatible with firmware timing
- **Risk:** Loses PCM accuracy, may cause aliasing

**Expected impact:** Up to 2× speedup (Option A), but high risk of audio quality degradation.

---

## 3. Recommended Implementation Order

### Stage 1: No-Brainer Fixes (1-2 hours)
1. ✅ Remove unconditional `-DDEBUG` (1.1)
2. ✅ Guard `unit_note_on` fprintf with DEBUG (1.2)
3. ✅ Guard `unit_render` fprintf with DEBUG (1.5)
4. ✅ Reduce `audio_buffer_size` to 1024 (1.3)
5. ✅ Shrink `rom2[]` to `ROM2_SIZE_JV880` (1.4)

**Expected result:** ~504 KB RAM saved, eliminates blocking I/O. CPU savings modest but builds hygiene.

### Stage 2: Structural Refactoring (1-2 weeks)
6. ✅ Refactor to free-function architecture (2.1)
7. ✅ Add PCM_Config precomputation (2.2)
8. ✅ Implement voice-skip optimization (2.3)
9. ✅ Deferred warmup — supersedes reduced iterations (2.4 → 2.4b, commit `4e9f2df`)

**Expected result:** ~~15-30% CPU reduction~~ → Init hang eliminated; CPU savings in QEMU within noise.

### Stage 3: Deep Optimization (2-4 weeks)
10. ✅ Optimize MCU inner loop (3.1, commits `317c52c` + `37e812b`)
11. ✅ NEON PCM math (3.2)
12. ✅ Cherry-pick jcmoyer addclip20 (3.3.1)
13. ✅ Guard emulator printf calls — hardware fix (commit `fec2729`)
14. ✅ MCU sleep fast-forward (3.1b, commit `317c52c`)
15. ✅ Interrupt bitmask dispatch + voice idle threshold (3.1c, commit `37e812b`)

**Measured result:** Emulator metric ~13% reduction (QEMU). Hardware hangs on init/note-on eliminated.

### Stage 4: Remaining Opportunities
16. ⬜ Oversampling control toggle (3.3.3) — opt-in CPU saver, ~40% PCM savings
17. ⬜ PCM_Config expansion (3.3.2) — correctness + minor perf
18. ⬜ UART/Analog polling frequency reduction — skip when not needed
19. ⬜ Reduced internal sample rate (3.4) — risky, up to 2× speedup
20. ⬜ Adaptive quality reduction under CPU overload
21. ✅ PCM_Update cycle increment reciprocal multiply (4.1)

---

## 4. Measurement Plan

### Before Any Changes
- [ ] Run QEMU ARM profiling: `./build.sh drumpler build PERF_MON=1 __QEMU_ARM__=1`
- [ ] Record baseline: CPU%, Emulator cycles, RenderTotal
- [ ] Note RSS memory

### After Each Stage
- [ ] Re-run QEMU ARM profile with same test sequence
- [ ] Compare Emulator counter improvement
- [ ] Verify audio quality (desktop test harness: `cd test/drumpler && make test`)
- [ ] Test on hardware if available

### Key Metrics
| Metric | Initial | Post-Stage 1 | Post-Stage 2 | Post-Stage 3 | Current (post-HW fixes) |
|---|---|---|---|---|---|
| CPU Total (QEMU) | 116.96% | ~109% | ~108% | ~100% | 74.78% |
| Emulator cycles avg | 512K | 322K | 294K | 103K | 726531 |
| RenderTotal | 124.66% | 130% | 126% | 121% | 98.82% |
| RSS memory | 15.25 MB | 14.67 MB | 14.31 MB | 14.12 MB | 13.81 MB |
| Init time | 10-20s+ | ~2s | ~2s | ~1s | **0s (deferred)** |
| Note-on hang | Yes | Yes | Yes | Yes | **Fixed** |
| Binary size | 4.5 MB | 4.5 MB | 4.5 MB | 4.5 MB | 4.38 MB |

\* QEMU Emulator metric changed measurement methodology between stages; not directly comparable across all columns.

**QEMU measurement note:** QEMU uses `std::chrono` for timing (not real ARM cycle counters), introducing significant run-to-run variance (±30%). The RenderTotal metric artificially dropped after deferred warmup because early warmup frames short-circuit before `PERF_MON_START`. Real-world ARM Cortex-A7 performance will differ — hardware testing is needed to validate.

---

## 5. Risk Assessment

| Optimization | Risk | Mitigation |
|---|---|---|
| Remove DEBUG | None | Always safe |
| Reduce buffer sizes | Low | Validated by buffer size math |
| Free-function refactor | Medium | Test harness + QEMU before hardware |
| Voice-skip | Medium-High | May miss voices in release phase |
| NEON PCM | Medium | Scalar fallback path for validation |
| Reduced internal rate | High | Audio quality regression likely |
| jcmoyer port | Medium | Different target (SC-55 vs JV-880) |

---

## 6. Hardware Context

- **Korg drumlogue SoC:** ARM Cortex-A7 (ARMv7-A, NEON SIMD)
- **User unit format:** ELF32 shared object loaded at runtime
- **Audio:** 48kHz, 256-frame buffers, stereo float output
- **Constraints:** No dynamic allocation in render path, ~5.33ms per buffer
- **Storage:** ~32MB for user units (binary includes 4.3MB ROM data)

---

## Appendix A: jcmoyer/Nuked-SC55 Key Differences

| Aspect | Our Code | jcmoyer Fork |
|---|---|---|
| Function style | Member functions on MCU class | Free functions with `mcu_t&` ref |
| PCM Update | `pcm.PCM_Update(cycles)` | `PCM_Update(pcm, cycles)` |
| Memory layout | MCU struct contains everything | Separate heap allocations |
| Wave ROM | Pointers to .rodata (embedded) | Arrays in pcm_t (copied) |
| Step function | Inline in `updateSC55WithSampleRate` | `MCU_Step(mcu)` standalone |
| PCM config | Recomputed per cycle | `PCM_Config` struct, precomputed |
| Sample output | Buffer-based (sample_buffer_l/r) | Callback-based |
| LCD | Stub (NO_LCD) | Full LCD emulation |
| C++ standard | C++14 (SDK container GCC 6.5) | C++17 (std::span, etc.) |

## Appendix B: File Reference

| File | Purpose | Hot Path? | Optimized? |
|---|---|---|---|
| `config.mk` | Build config, defines | N/A | ✅ DEBUG conditional (1.1) |
| `unit.cc` | SDK callbacks | `unit_render` (calls Synth) | ✅ fprintf guards (1.2, 1.5) |
| `synth.h` | Synth class, Render loop | ⚠️ Render, Init warmup | ✅ Deferred warmup, MaxAbsBuffer, fprintf guards |
| `emulator/jv880_wrapper.cc` | MCU wrapper | Render delegate | ✅ Helper extraction |
| `emulator/mcu.cc` | **MCU emulation core** | 🔴 `MCU_UpdateSC55WithSampleRate` | ✅ Sleep fast-forward, free-func, ROM1 fetch, printf guards |
| `emulator/mcu.h` | MCU struct (memory layout) | Memory layout | ✅ Buffer/ROM2 size, wakeup_pending, bitmasks |
| `emulator/mcu_interrupt.cc` | Interrupt dispatch | `MCU_Interrupt_Handle` | ✅ Bitmask + dispatch table + CTZ |
| `emulator/mcu_opcodes.cc` | H8/532 instruction set | 🔴 `MCU_ReadInstruction` | ✅ Inlined fetch (3.1) |
| `emulator/pcm.cc` | **PCM chip emulation** | 🔴 `PCM_Update` (voice loop) | ✅ addclip20, voice-skip, PCM_Config, NEON |
| `emulator/pcm.h` | PCM struct | Memory layout | ✅ Voice idle threshold 960→480 |
| `emulator/mcu_timer.cc` | Timer emulation | `TIMER_Clock` | — |
| `emulator/submcu.cc` | Sub-MCU (not used for JV-880) | Skipped | ✅ printf guards |
