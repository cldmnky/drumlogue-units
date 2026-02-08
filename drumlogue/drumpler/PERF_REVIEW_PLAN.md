# Drumpler Performance Review Plan

## Executive Summary

The drumpler unit (JV-880 emulator based on Nuked-SC55) is **overloading CPU at 116.96%** in QEMU ARM profiling, meaning it needs to become **~1.17× faster** to fit in budget. The emulator counter alone consumes 2731% of a per-sample budget, indicating the MCU instruction loop + PCM voice processing dominates runtime. This document outlines actionable optimizations ranked by effort-vs-impact.

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

#### 2.4 Reduce Init Warmup Iterations

**File:** `drumlogue/drumpler/synth.h` lines 115-156

**Problem:** The warmup loop runs **750 iterations × 128 frames = 96,000 frames ≈ 2 seconds**. Since the emulator already overloads CPU, this warmup takes even longer in real time. On hardware, this blocks `unit_init()` for several seconds.

The warmup sends a test note at iteration 375 to verify audio, but the MCU firmware actually initializes within ~500ms (375 iterations × 128/48000 ≈ 1 second).

**Fix:** 
1. Reduce to 375 iterations (or detect ready state from MCU internal state)
2. Remove test note (verification is good for debugging but not needed in production)
3. Consider lazy init: skip warmup entirely, let first few render calls naturally warm up (may cause ~1 second of silence on first play)

**Expected impact:** Halves init time (or eliminates it with lazy init).

---

### 🔴 Phase 3: Deep Optimization (High Effort, Potentially Large Impact)

#### 3.1 MCU Instruction Loop Optimization

**The critical hot loop** (mcu.cc `updateSC55WithSampleRate`, lines 1392-1419):
```cpp
for (int i = 0; sample_write_ptr < renderBufferFrames; i++) {
    MCU_Interrupt_Handle(this);
    MCU_ReadInstruction();     // H8/532 opcode decode + execute
    mcu.cycles += 12;          // FIXME: assume 12 cycles per instruction
    pcm.PCM_Update(mcu.cycles);  // PCM chip update
    mcu_timer.TIMER_Clock(mcu.cycles);
    MCU_UpdateUART_RX();
    MCU_UpdateUART_TX();
    MCU_UpdateAnalog(mcu.cycles);
}
```

This loop runs **thousands of times per 256-frame buffer**. Every function call here is critical.

**Optimization opportunities:**
1. **Function call overhead:** Each iteration calls 6-7 functions. With free-function refactoring (Phase 2.1), the compiler can inline most of these.
2. **Cycle accumulation:** `mcu.cycles += 12` is a fixed assumption. The real H8/532 has variable instruction timing. Not a performance issue per se, but means we can't batch instructions.
3. **UART/Analog polling:** `MCU_UpdateUART_RX/TX` and `MCU_UpdateAnalog` only need to run at specific intervals, not every cycle. Could use a modulo check or next-event timer to reduce call frequency.
4. **Interrupt handling:** `MCU_Interrupt_Handle` could be skipped when no interrupt is pending (check flag first).

**Expected impact:** 10-30% reduction in inner loop overhead.

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

#### 3.3 Port to jcmoyer Fork Codebase

**Reference:** https://github.com/jcmoyer/Nuked-SC55

The jcmoyer fork has several months of optimization work beyond nukeykt's original code:
- Free-function architecture (already discussed)
- Separated heap allocations for MCU, PCM, SubMcu, Timer, LCD
- `PCM_Config` struct for precomputed values
- Cleaner `MCU_Step()` function combining all per-cycle operations
- Sample callback architecture (vs. buffer-based)

**Trade-offs:**
- **Pro:** Most optimizations already implemented and tested
- **Con:** jcmoyer targets desktop (SC-55/SC-55mkII), not JV-880 specifically. Would need to merge JV-880 adaptations from our fork.
- **Con:** Uses C++17 features (std::span, std::unique_ptr) that may need adaptation for the GCC 6.5 toolchain in the logue SDK container
- **Con:** Different memory model (waverom arrays embedded in pcm_t vs. our pointer approach — our approach is better for embedded)

**Expected impact:** Combined 15-30% speedup from all jcmoyer optimizations together.

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
6. Refactor to free-function architecture (2.1)
7. Add PCM_Config precomputation (2.2)
8. Implement voice-skip optimization (2.3)
9. Reduce warmup iterations (2.4)

**Expected result:** 15-30% CPU reduction. Combined with Stage 1, may bring total below 100%.

### Stage 3: Deep Optimization (2-4 weeks)
10. Optimize MCU inner loop (3.1)
11. NEON PCM math (3.2)
12. Cherry-pick jcmoyer optimizations (3.3)

**Expected result:** Additional 15-30% CPU reduction. Target: 70-80% CPU budget.

### Stage 4: Research / Risky (Experimental)
13. Reduced internal sample rate (3.4)
14. Cycle-accurate instruction timing
15. PCM partial evaluation / JIT compilation (extreme)

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
| Metric | Current | Stage 1 Target | Stage 2 Target | Stage 3 Target |
|---|---|---|---|---|
| CPU Total | 116.96% | ~112% | ~85% | ~70% |
| Emulator cycles | 512K avg | ~500K | ~400K | ~300K |
| RSS memory | 15.25 MB | ~14.7 MB | ~14.7 MB | ~14.7 MB |
| Init time | ~2s+ | ~2s | ~1s | ~1s |

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

| File | Purpose | Hot Path? |
|---|---|---|
| `config.mk` | Build config, defines | N/A |
| `unit.cc` | SDK callbacks | `unit_render` (calls Synth) |
| `synth.h` | Synth class, Render loop | ⚠️ Render, Init warmup |
| `emulator/jv880_wrapper.cc` | MCU wrapper | Render delegate |
| `emulator/mcu.cc` | **MCU emulation core** | 🔴 `updateSC55WithSampleRate` |
| `emulator/mcu.h` | MCU struct (memory hog) | Memory layout |
| `emulator/mcu_opcodes.cc` | H8/532 instruction set | 🔴 `MCU_ReadInstruction` |
| `emulator/pcm.cc` | **PCM chip emulation** | 🔴 `PCM_Update` (voice loop) |
| `emulator/pcm.h` | PCM struct | Memory layout |
| `emulator/mcu_timer.cc` | Timer emulation | `TIMER_Clock` |
| `emulator/submcu.cc` | Sub-MCU (not used for JV-880) | Skipped |
