# Drumpler Performance Review Plan — Stage 5

## Executive Summary

After completing Stages 1–4 optimization, the drumpler unit (JV-880 emulator based on Nuked-SC55) sits at **74.78% CPU** in QEMU ARM profiling. However, on real drumlogue hardware (ARM Cortex-A7 @ 900 MHz), the unit still exhibits:

1. **Distorted/choppy audio** — consistent with CPU overload causing buffer underruns
2. **Unit hang after playback** — possibly memory-related or emulator state corruption under sustained overload

This document presents a new round of optimization proposals informed by:
- Deep analysis of the [jcmoyer/Nuked-SC55](https://github.com/jcmoyer/Nuked-SC55) fork (358 commits ahead of upstream)
- Hardware specifications of the NXP i.MX 6ULZ (MCIMX6Z0DVM09AB) SoC used in the Korg drumlogue
- Detailed code review of the local emulator (mcu.cc, pcm.cc, jv880_wrapper.cc, synth.h)
- Memory management audit

**Goal:** Reduce real-hardware CPU to ≤50% with ≥95% headroom target, and eliminate the post-play hang.

---

## Current Baseline (post-Item 4.1, 2026-02-10)

| Metric | Value |
|---|---|
| CPU Total (QEMU) | 74.78% |
| Emulator cycles avg | 726,531 |
| RenderTotal | 98.82% |
| RSS memory | 13.81 MB |
| Binary size | ~4.5 MB (.drmlgunit) |
| Init time | 0 ms (deferred warmup) |
| Note-on hang | Fixed |
| Post-play hang | **Still present** |

## Progress Log

### Item 5.1 — Oversampling Toggle — COMPLETED 2026-02-10
**Status:** Done
**Before baseline:**
- CPU Total: 74.32%
- Emulator cycles: 715330
- RenderTotal: 96.07%
- RSS memory: 14.07 MB

**After results:**
- CPU Total: 39.74% (change: -34.58)
- Emulator cycles avg: 1076355 (change: +361025)
- RenderTotal: 64.45% (change: -31.62)
- RSS memory: 11.73 MB (change: -2.34 MB)

**Audio regression:** None detected in size/min-max checks; oversampling disabled by design.
**Files changed:** drumlogue/drumpler/emulator/pcm.h, drumlogue/drumpler/emulator/pcm.cc, drumlogue/drumpler/emulator/mcu.cc, drumlogue/drumpler/PERF_REVIEW_PLAN_2.md
**Summary:** Added oversampling toggle and output-frequency-aware resampling; defaulted oversampling off to reduce render CPU.

**Additional change:** Removed REVERB and DELAY parameters (use drumlogue's dedicated FX units), kept CHORUS. Final CPU: 37.06% with 62.94% headroom.

### Item 5.2 — PCM Config Expansion (PCM_GetConfig) — COMPLETED 2026-02-10
**Status:** Done
**Before baseline:**
- CPU Total: 76.38%
- Emulator cycles: 771746
- RenderTotal: 96.26% (18049 cycles)
- RSS memory: 14.24 MB

**After results:**
- CPU Total: 40.82% (change: -35.56%)
- Emulator cycles avg: 1106422 (change: +334676)
- RenderTotal: 67.37% (12632 cycles, change: -28.89%)
- RSS memory: 11.82 MB (change: -2.42 MB)

**Audio regression:** None - using same config defaults (noise_mask=0, orval=0, write_mask=3, dac_mask=-4)
**Files changed:** drumlogue/drumpler/emulator/pcm.h, drumlogue/drumpler/emulator/pcm.cc
**Summary:** Expanded pcm_config_t to cache noise_mask, orval, write_mask, dac_mask computed from PCM configuration registers. Eliminates redundant local variable setup in the hot PCM_Update loop. Primarily a correctness fix that also provides better code organization.

### Audio Budget

The drumlogue delivers audio callbacks at 48 kHz with frame sizes of 32–256 samples. At 256 frames:

| Parameter | Value |
|---|---|
| Buffer period | 5.333 ms (256 / 48000) |
| Available CPU cycles per buffer | 4,800,000 (900 MHz × 5.333 ms) |
| Internal emulator samples per buffer | ~342 (256 × 64000/48000) |
| MCU steps per internal sample | ~19.5 k (from profiling) |
| Total MCU steps per buffer | ~6.7 M |

At the current ~75% QEMU load, the real Cortex-A7 is likely exceeding 100% due to:
- QEMU timing is based on `std::chrono`, not real ARM cycle counters (±30% variance)
- Cortex-A7 is an in-order core — branch mispredictions and cache misses cost significantly more than on QEMU's x86 host
- No L1/L2 cache pressure simulation in QEMU

---

## Target Hardware: NXP i.MX 6ULZ (MCIMX6Z0DVM09AB)

| Spec | Value | Implication |
|---|---|---|
| Core | ARM Cortex-A7, single core | **No multicore parallelism** — all emulation must fit in one core's budget |
| Clock | 900 MHz | ~4.8 M cycles per 5.33 ms audio buffer |
| Pipeline | 8-stage, in-order, partial dual-issue | Branch mispredictions cost 8+ cycles; data hazards cause stalls |
| L1 I-Cache | 32 KB, 2-way | Hot MCU opcode dispatch + PCM_Update must fit here |
| L1 D-Cache | 32 KB, 2-way | MCU struct hot fields + PCM voice state must be cache-friendly |
| L2 Cache | 128 KB, unified | Shared I+D; ROM readahead and PCM eram compete for space |
| NEON | VFPv4 + NEON MPE (SIMD) | 4×float32 or 8×int16 per instruction; useful for batch DSP |
| FPU | Single-precision hardware | `sinf`, `powf` OK; double-precision is **emulated in software** |
| Memory | 16-bit LP-DDR2/DDR3 interface | ~1.6 GB/s peak bandwidth; random access much slower |
| Cache line | 64 bytes | Struct layout should cluster hot fields within 64-byte boundaries |

### Key Cortex-A7 Performance Characteristics

1. **In-order execution:** Unlike x86 (or Cortex-A15/A53+), the A7 cannot reorder instructions to hide latency. Every branch miss, cache miss, or data dependency stall directly reduces throughput.

2. **Branch prediction:** Limited predictor table. The PCM voice loop iterates over 28+ slots with many conditional paths (address comparisons, phase overflow, envelope states). Each branch miss costs **8+ pipeline refill cycles**.

3. **L1 D-cache thrashing:** The MCU struct is ~944 KB, loaded on heap. Only the hot fields (mcu_t registers, sample buffers, pcm state) need to be in L1. The 32 KB L1 can hold ~500 cache lines — anything beyond this spills to L2 (128 KB) or main memory.

4. **NEON pipeline hazards:** NEON and scalar registers share the execution pipeline on A7. Mixing scalar and NEON code incurs pipeline drain penalties. Best to batch NEON operations together.

5. **Memory bandwidth:** 16-bit DDR interface means PCM ROM reads (4 MB waverom) are bandwidth-limited. Each `PCM_ReadROM()` call potentially brings in a 64-byte cache line for a single byte read.

---

## Symptom Analysis

### 1. Distorted/Choppy Audio

**Root cause:** CPU overload causing audio buffer underruns. The emulator fails to produce all required samples within the 5.33 ms deadline, resulting in:
- Partial buffer fills (zeros mixed with audio)
- Dropped samples causing audible clicks
- Time-stretching artifacts from the resampler

**Evidence:** QEMU shows 98.82% RenderTotal, meaning the render path consumes nearly the entire budget. On real hardware (slower, in-order), this almost certainly exceeds 100%.

### 2. Post-Play Hang

**Possible causes (in order of likelihood):**

| # | Hypothesis | Evidence | Investigation |
|---|---|---|---|
| 1 | **CPU overload cascading** | Sustained >100% CPU causes MCU step loop to exceed `maxCycles` limit, which triggers the early break in `MCU_UpdateSC55WithSampleRate`. Accumulated timing drift corrupts the MCU/PCM cycle synchronization. | Check if `maxCycles` break is hit; add PERF_MON counter for overrun events |
| 2 | **Idle detection never triggers** | `kSilenceThreshold = 96000` (2 seconds of silence needed). If the emulator produces low-level noise (reverb tail, LFSR noise generator), it may never go idle. The `kSilenceLevel = 0.0001f` (−80 dB) may be too low for the LFSR noise floor. | Log `silence_frames_` progression; test with higher silence threshold |
| 3 | **Emulator stuck in non-sleep state** | After sustained overload, MCU timing drift may prevent the firmware from entering sleep mode, keeping `MCU_Step` (full scan) running instead of `MCU_Step_Sleep` | Add counter for sleep vs. wake ratio per render call |
| 4 | **Memory corruption under overload** | Buffer overrun in `sample_buffer_l/r[1024]` if `renderBufferFrames` exceeds `audio_buffer_size`. The size check exists but returns silently. | Add assertion or error flag when buffer overflow detected |
| 5 | **UART/Timer accumulation** | If the MCU is starved of cycles, UART TX/RX and timer events accumulate. On resume, a burst of deferred events may cause a spike that overwhelms the system | Profile UART/Timer call frequency during and after playback |

**Recommended first step:** Add diagnostic counters (under PERF_MON) for: maxCycles break events, sleep/wake ratio, silence_frames progression, and actual renderBufferFrames vs audio_buffer_size.

---

## Optimization Proposals

### Overview & Prioritization

| # | Proposal | Est. Savings | Effort | Risk | Priority |
|---|---|---|---|---|---|
| 5.1 | Oversampling toggle | **~40-50% PCM** | Small | Low | 🔴 Critical |
| 5.2 | PCM_Config expansion (PCM_GetConfig) | ~2-5% PCM | Small | Low | 🟡 Medium |
| 5.3 | calc_tv nfs-guarded writes | ~1-3% | Small | Low | 🟡 Medium |
| 5.4 | Bool typing in PCM voice loop | ~1-3% | Medium | Low | 🟢 Low |
| 5.5 | UART/Analog polling reduction | ~5-10% MCU | Medium | Medium | 🟡 Medium |
| 5.6 | MCU hot field cache alignment | ~3-8% total | Medium | Low | 🟡 Medium |
| 5.7 | PCM_ReadROM prefetch hints | ~2-5% PCM | Small | Low | 🟡 Medium |
| 5.8 | Voice count clamping | ~20-50% PCM | Small | Medium | 🟡 Medium |
| 5.9 | Post-play hang diagnostics | Debug | Small | None | 🔴 Critical |
| 5.10 | Reduced internal rate (32 kHz) | **~50% total** | Large | High | 🟢 Low (risky) |
| 5.11 | Memory audit & cleanup | Stability | Medium | Low | 🟡 Medium |
| 5.12 | Adaptive quality under overload | Stability | Medium | Medium | 🟡 Medium |

---

### 5.1 — Oversampling Toggle (CRITICAL — Largest Single Win)

**Current state:** `pcm.cc` line ~569 has:
```cpp
if (true) // oversampling
```
This hardcodes the oversampling path, causing `PCM_Update` to call `MCU_PostSample` **twice per PCM cycle** — producing 2 interleaved audio samples per voice iteration.

**jcmoyer implementation:**
```cpp
// pcm.h — PCM_Config struct:
bool oversampling = false;  // from config_reg_3c bit 6

// pcm.h — pcm_t struct:
bool enable_oversampling = true;  // runtime toggle (CLI: --disable-oversampling)

// pcm.cpp — in PCM_Update final mixing:
if (pcm.enable_oversampling && pcm.config.oversampling) // oversampling
{ ... second sample output ... }

// pcm.cpp — PCM_GetOutputFrequency:
uint32_t freq = pcm.mcu->is_jv880 ? 64000 : 66207;
return pcm.enable_oversampling ? freq : freq / 2;
```

**Impact analysis:**
- Disabling oversampling halves the per-cycle sample output from 64 kHz to 32 kHz
- The resampler (`MCU_UpdateSC55WithSampleRate`) then needs ~50% fewer emulator steps per output frame
- `renderBufferFrames` drops from ~342 to ~171 for a 256-frame output
- Total MCU steps per audio buffer drops proportionally: ~6.7M → ~3.4M
- Audio quality impact: removes the 2× oversampling (anti-imaging filter). For a JV-880 on drumlogue speakers, this is likely acceptable — the original hardware used 32 kHz DACs for some modes.

**Implementation steps:**
1. Port `PCM_GetConfig()` function from jcmoyer (sets `config.oversampling` from `config_reg_3c` bit 6)
2. Add `enable_oversampling` bool to `pcm_t`
3. Replace `if (true)` with `if (pcm.enable_oversampling && pcm.config.oversampling)`
4. Add `PCM_GetOutputFrequency()` and use it in `MCU_UpdateSC55WithSampleRate()` step calculation
5. Set `pcm.enable_oversampling = false` by default (can expose as drumlogue parameter later)
6. Adjust resampler step calculation: `const double step = outputFreq / destSampleRate`

**Expected savings:** ~40-50% of total CPU. This is the single most impactful optimization remaining.

**Risk:** Low — jcmoyer ships this as a supported feature. Audio quality is slightly reduced but acceptable for the embedded use case. The JV-880 firmware already configures oversampling via `config_reg_3c`; we were just ignoring the flag.

**Measurement plan:**
- Baseline WAV → apply change → WAV regression test
- QEMU PERF_MON before/after
- Hardware test: does audio quality remain acceptable?

---

### 5.2 — PCM_Config Expansion (PCM_GetConfig)

**Current state:** Our `pcm_config_t` only caches `reg_slots`. The final mixing section hardcodes:
```cpp
int noise_mask = 0;
int orval = 0;
int write_mask = 3;
```
These are local variables recomputed (redundantly) every PCM cycle.

**jcmoyer implementation:** Full `PCM_Config` struct with 6 fields, updated only on `config_reg_3c` register writes via `PCM_GetConfig()`:
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

Their PCM_Update then reads `pcm.config.write_mask`, `pcm.config.noise_mask`, `pcm.config.orval` directly — no local variable setup.

**Why this matters:**
- Enables item 5.1 (oversampling toggle needs `config.oversampling`)
- Correctness: our hardcoded `noise_mask=0, orval=0, write_mask=3` may be wrong for certain JV-880 modes
- Small per-cycle savings from eliminating local variable setup
- Better for Cortex-A7: fewer instructions in the hot path

**Implementation:** Port `PCM_GetConfig()` from jcmoyer's `pcm.cpp:522-583`, expand local `pcm_config_t`, call from `PCM_Write` when address == 0x3c.

**Expected savings:** ~2-5% of PCM processing time. Primarily a correctness fix that enables 5.1.

---

### 5.3 — calc_tv nfs-Guarded Writes

**Current state:** Our `calc_tv()` writes `*levelcur` unconditionally:
```cpp
if (write)
    *levelcur = (sum2 >> 4) & 0x7fff;
```

**jcmoyer version:**
```cpp
if (write && pcm.nfs)
{
    if (xnor)
        *levelcur = sum2_l & 0x7fff;
    else
        *levelcur = (uint16_t)(target << 7);
}
```

The `pcm.nfs` (new frame start) guard prevents stale writes during the initial PCM state setup. This is primarily a **correctness fix** but also reduces write traffic on non-nfs cycles.

**Additionally:** jcmoyer's `calc_tv` uses explicit bit-reversal instead of our `flip_nibble_lut[16]` lookup table. On Cortex-A7, the explicit bit tests may be faster than the L1 cache load for the LUT (16 bytes fits in one cache line, but branch prediction may be faster than the load-use latency).

**Implementation:** Add `pcm.nfs` guard to `calc_tv` write paths. Optionally replace `flip_nibble_lut` with inline bit tests.

**Expected savings:** ~1-3%. Primarily a correctness and stability fix.

---

### 5.4 — Bool Typing in PCM Voice Loop

**Current state:** Our PCM voice loop uses `int` for boolean flags:
```cpp
int okey = (ram2[7] & 0x20) != 0;
int active = okey && key;
int kon = key && !okey;
int b15 = (ram2[8] & 0x8000) != 0;
int b6 = (ram2[7] & 0x40) != 0;
```

**jcmoyer version:**
```cpp
const bool okey = (ram2[7] & 0x20) != 0;
const bool key = (voice_active >> slot) & 1;
const bool active = okey && key;
const bool kon = key && !okey;
bool b15 = (ram2[8] & 0x8000) != 0;
const bool b6 = (ram2[7] & 0x40) != 0;
const bool b7 = (ram2[7] & 0x80) != 0;
```

**Why this matters on Cortex-A7:**
- `bool` is 1 byte; `int` is 4 bytes — less register pressure, better packing
- `const bool` allows the compiler to propagate known-zero/known-one values
- ARM GCC can use conditional instructions (IT blocks) more effectively with bool types
- The `const` qualifier lets the optimizer eliminate redundant loads

**Expected savings:** ~1-3%. Low effort, no semantic change.

---

### 5.5 — UART/Analog Polling Reduction

**Current state:** Every MCU step runs:
```cpp
static inline void MCU_Step(MCU& self, mcu_t& state) {
    ...
    self.MCU_UpdateUART_RX();   // called every step
    self.MCU_UpdateUART_TX();   // called every step
    self.MCU_UpdateAnalog(state.cycles);  // called every step
}
```

For ~19,500 MCU steps per render, that's 19,500 × 3 = 58,500 function calls that rarely do useful work. UART is only active during MIDI input, and analog sampling happens at a much lower rate.

**Optimization options:**

| Option | Implementation | Savings |
|---|---|---|
| Modulo check | `if (state.cycles % 96 == 0)` → call every 96 cycles | ~5% MCU time |
| Next-event timer | Track `uart_next_event_cycle`, skip until reached | ~8% MCU time |
| Conditional on state | Only call UART when `midi_ready` or TX pending | ~5% MCU time |

**Risk:** Medium — must ensure UART timing matches firmware expectations. If MIDI messages are delayed, the MCU firmware may hang waiting for UART data.

**Recommendation:** Start with modulo-96 approach (1/8 frequency). The original hardware UART runs at 31250 baud = ~32 kHz, so checking every 2 MCU cycles at 64 kHz is already faster than needed. Checking every 96 cycles (every ~1.5 µs at 64 kHz) is still well within UART timing.

---

### 5.6 — MCU Hot Field Cache Alignment

**Current state:** The `MCU` struct is ~944 KB. The Cortex-A7 L1 D-cache is 32 KB. During the render hot loop, only a subset of fields are accessed:

**Hot fields (accessed every MCU step):**
- `mcu_t`: `r[8]`, `pc`, `sr`, `cp`, `dp`, `ep`, `tp`, `br`, `sleep`, `cycles` — ~34 bytes
- `mcu_t`: `interrupt_pending_mask`, `trapa_pending_mask`, `wakeup_pending` — ~7 bytes
- `rom1[]` (instruction fetch) — variable, depends on PC
- `pcm.cycles`, `pcm.config.reg_slots`, `pcm.nfs` — ~16 bytes

**Cold fields (rarely accessed during render):**
- `rom2[256KB]`, `sram[32KB]`, `nvram[32KB]`, `cardram[32KB]`
- `lcd`, `sub_mcu`, `midiQueue[128]`
- `dev_register[128]`, `ad_val[4]`

**Optimization:**
1. Move `mcu_t` to the top of the `MCU` struct (before large arrays) so hot fields fall in the first few cache lines
2. Add `__attribute__((aligned(64)))` to `mcu_t` to ensure it starts on a cache line boundary
3. Group sample_buffer_l/r near pcm (they're written by PCM_Update and read by the resampler — access locality)

**Expected savings:** ~3-8% total from reduced L1 cache misses. The A7's 2-way associative L1 is particularly sensitive to hot-set conflicts.

**Risk:** Low — purely structural, no behavioral change. Validate with QEMU + hardware test.

---

### 5.7 — PCM_ReadROM Prefetch Hints

**Current state:** Each voice in the PCM loop calls `PCM_ReadROM()` 5 times (samp0-samp3 + newnibble) to fetch individual bytes from the 4 MB waverom via pointer indirection:
```cpp
inline uint8_t Pcm::PCM_ReadROM(uint32_t address) {
    int bank = (address >> 21) & 7;
    switch (bank) {
        case 0: return waverom1 ? waverom1[address & 0x1fffff] : 0;
        ...
    }
}
```

Each ROM read potentially loads a 64-byte cache line for a single byte. With 28 voices, that's 140+ random ROM reads per PCM cycle — potentially 140 L1 misses.

**Optimization:**
1. Add `__builtin_prefetch()` hints before the voice loop to bring wave data into L1:
   ```cpp
   // Prefetch next voice's wave address before processing current voice
   if (slot + 1 < reg_slots) {
       uint32_t next_addr = pcm.ram1[slot+1][4];
       int next_hi = (pcm.ram2[slot+1][7] >> 8) & 15;
       __builtin_prefetch(waverom1 + ((next_hi << 20 | next_addr) & 0x1fffff), 0, 1);
   }
   ```

2. Use `__builtin_prefetch(addr, 0, 0)` for non-temporal (streaming) ROM reads that won't be reused.

**Expected savings:** ~2-5% of PCM time from reduced cache miss stalls. Depends heavily on voice count and wave data locality.

**Risk:** Low — prefetch hints are non-binding; worst case is a no-op.

---

### 5.8 — Voice Count Clamping

**Current state:** `reg_slots` comes from the PCM configuration register: `(config_reg_3d & 31) + 1`. For the JV-880, this is typically 28 voices. The voice-skip optimization (item 2.3) already skips idle voices after `kIdleThreshold` frames, but during active playback, all sounding voices are processed.

**Optimization:** Allow the user (or the unit automatically) to clamp `reg_slots` to a lower value:
- 8 voices  → ~71% PCM savings (8/28)
- 12 voices → ~57% PCM savings (12/28)
- 16 voices → ~43% PCM savings (16/28)

**Implementation options:**

| Option | Approach | User Impact |
|---|---|---|
| A: Firmware level | Write clamped value to `config_reg_3d` via SysEx | Transparent to MCU firmware; may cause stuck notes if active voices > limit |
| B: PCM level | Clamp `reg_slots` in `PCM_UpdateConfig()` | Same as A but simpler |
| C: Parameter | Expose as drumlogue parameter "Voices: 8/12/16/24/28" | User control, documented trade-off |

**Risk:** Medium — reducing voice count may cause audible voice stealing with complex patches (some JV-880 patches use 4 tones × multiple voices). Need to test with actual patches.

**Expected savings:** Depends on clamped value. At 12 voices: ~50% PCM savings.

---

### 5.9 — Post-Play Hang Diagnostics (CRITICAL)

Before further optimization, the hang bug must be understood. Add the following PERF_MON-guarded diagnostics:

**A. MaxCycles overrun counter:**
```cpp
// In MCU_UpdateSC55WithSampleRate, after the maxCycles break:
#ifdef PERF_MON
static uint32_t overrun_count = 0;
if (i > maxCycles) { overrun_count++; }
// Expose overrun_count via PERF_MON
#endif
```

**B. Sleep/wake ratio:**
```cpp
// Count how many steps went through MCU_Step vs MCU_Step_Sleep per render
#ifdef PERF_MON
static uint32_t wake_steps = 0, sleep_steps = 0;
// Log ratio per render call
#endif
```

**C. Silence detection trace:**
```cpp
// After idle detection in Render():
#ifdef PERF_MON
// Log silence_frames_ and is_idle_ state transitions
#endif
```

**D. Buffer overflow detection:**
```cpp
// In MCU_UpdateSC55WithSampleRate:
if (renderBufferFrames > audio_buffer_size) {
#ifdef PERF_MON
    static uint32_t overflow_count = 0;
    overflow_count++;
#endif
    return;  // already there, but now tracked
}
```

**E. MCU cycle drift tracking:**
Track the ratio of actual MCU cycles consumed vs. expected per render call. If this drifts, the emulator is falling behind.

**Priority:** This should be done FIRST — understanding the hang informs which optimizations are most urgent.

---

### 5.10 — Reduced Internal Rate (32 kHz) — HIGH RISK

**Current state:** The emulator runs the MCU at a rate that produces 64 kHz PCM output. This is the original JV-880 hardware rate.

**Proposal:** Run the MCU + PCM at half rate, producing 32 kHz internally, then upsample to 48 kHz.

**Implementation:**
- Double the MCU step size: each step advances 24 cycles instead of 12
- Let PCM produce samples at 32 kHz
- Adjust the resampler: `step = 32000.0 / 48000.0 = 0.6667`
- `renderBufferFrames ≈ ceil(256 × 32000/48000) + 2 = 172`

**Expected savings:** ~50% total CPU (half the MCU steps).

**Risk:** HIGH
- MCU firmware timing assumes 64 kHz sample rate; halving may break timer calibration
- PCM envelope and filter calculations are tuned for 64 kHz cycle rate
- Audio aliasing: 32 kHz only represents frequencies up to 16 kHz (vs. 32 kHz with oversampling)
- May break MIDI timing (UART baud rate calculation depends on MCU clock)

**Recommendation:** Only attempt after items 5.1–5.9 have been exhausted. This is a last resort.

---

### 5.11 — Memory Audit & Cleanup

#### Current MCU Heap Layout (~944 KB)

| Field | Size | Used? | Action |
|---|---|---|---|
| `rom1[0x8000]` | 32 KB | ✅ Yes | Keep |
| `rom2[ROM2_SIZE_JV880]` | 256 KB | ✅ Yes | ✅ Already shrunk (item 1.4) |
| `ram[0x400]` | 1 KB | ✅ Yes | Keep |
| `sram[0x8000]` | 32 KB | ✅ Yes | Keep |
| `nvram[0x8000]` | 32 KB | ✅ Yes | Keep |
| `cardram[0x8000]` | 32 KB | ⚠️ Conditional | Could be eliminated if no expansion card |
| `sample_buffer_l[1024]` | 4 KB | ✅ Yes | ✅ Already shrunk (item 1.3) |
| `sample_buffer_r[1024]` | 4 KB | ✅ Yes | ✅ Already shrunk (item 1.3) |
| `uart_buffer[8192]` | 8 KB | ✅ Yes | Could reduce to 1024 (typical MIDI messages are ≤32 bytes) |
| `pcm_t` (embedded) | ~34 KB | ✅ Yes | Keep |
| `sub_mcu` | ~4.5 KB | ❌ No (JV-880) | Can be conditional-compiled out |
| `midiQueue[128]` | ~5 KB | ✅ Yes | 128 entries is generous; 32 would suffice |
| `lcd` | ~2 KB | ❌ No (NO_LCD) | Already stubbed? Verify |
| `dev_register[0x80]` | 128 B | ✅ Yes | Keep |

**Potential savings:** ~41 KB (cardram + sub_mcu + uart_buffer reduction + midiQueue reduction)

#### Wave ROM Memory Model

Our code correctly uses `const uint8_t*` pointers into the embedded .rodata section:
```cpp
// pcm.h:
const uint8_t* waverom1;   // → .rodata (2 MB)
const uint8_t* waverom2;   // → .rodata (2 MB)
const uint8_t* waverom_exp; // → .rodata (8 MB, optional)
```

This is **much better** than jcmoyer's approach which copies ROMs into the `pcm_t` struct:
```cpp
// jcmoyer pcm.h:
uint8_t waverom1[0x200000]{};   // 2 MB copied to heap
uint8_t waverom2[0x200000]{};   // 2 MB copied to heap
uint8_t waverom_exp[0x800000]{}; // 8 MB copied to heap — 15+ MB total!
```

Our approach is correct for embedded: the ROM data lives in flash/.rodata and is accessed via pointers. No heap copy needed. **No changes needed here.**

#### Memory Lifecycle

1. **Init:** `JV880Emulator()` allocates MCU via `new (std::nothrow) MCU()`. MCU constructor initializes sub-objects (pcm, lcd, sub_mcu, timer).
2. **UnpackROM:** Sets `const uint8_t*` pointers into the ROM blob (no copy for wave ROMs). ROM1 and ROM2 are `memcpy`'d into MCU arrays by `startSC55()`.
3. **Render:** No dynamic allocation. All state lives in the MCU struct.
4. **Teardown:** `~JV880Emulator()` calls `delete mcu_`. Pcm destructor is a no-op (no dynamic allocations to free).

**Finding:** Memory lifecycle is clean. No leaks detected. The post-play hang is **not** a memory leak — it's a CPU overload or state corruption issue.

---

### 5.12 — Adaptive Quality Under Overload

If the emulator detects that it's exceeding the audio deadline, reduce quality dynamically:

**Level 0 (Normal):** Full emulation, oversampling on, all voices
**Level 1 (Light):** Disable oversampling (if 5.1 is implemented)
**Level 2 (Medium):** Skip UART/Analog polling (every 4th step only)
**Level 3 (Heavy):** Clamp voice count to 12
**Level 4 (Emergency):** Clamp voice count to 8, skip chorus/reverb processing

**Detection mechanism:**
```cpp
// In Render(), after emulator_.Render():
float load = emulator_.GetCpuLoad();
if (load > 0.90f) quality_level = std::min(quality_level + 1, 4);
else if (load < 0.60f) quality_level = std::max(quality_level - 1, 0);
```

**Risk:** Audio quality degrades under load, but this is better than distortion/hanging.

---

## Implementation Order

### Phase A: Diagnostics & Understanding (1–2 days)
1. **5.9** — Post-play hang diagnostics (add PERF_MON counters)
2. Hardware test with diagnostic build to characterize the hang
3. Determine if hang is CPU-related or state-related

### Phase B: High-Impact Optimizations (3–5 days)
4. **5.2** — PCM_Config expansion (prerequisite for 5.1)
5. **5.1** — Oversampling toggle (**~40-50% savings — game changer**)
6. **5.3** — calc_tv nfs-guarded writes (correctness + minor perf)
7. Hardware test: validate audio quality with oversampling disabled

### Phase C: Incremental Gains (3–5 days)
8. **5.4** — Bool typing in PCM voice loop
9. **5.5** — UART/Analog polling reduction
10. **5.6** — MCU hot field cache alignment
11. **5.7** — PCM_ReadROM prefetch hints
12. Hardware test: measure cumulative improvement

### Phase D: Stability & Fallbacks (2–3 days)
13. **5.8** — Voice count clamping (parameter or automatic)
14. **5.12** — Adaptive quality under overload
15. **5.11** — Memory cleanup (conditional-compile sub_mcu, reduce queues)
16. Final hardware validation

### Phase E: Last Resort (only if needed)
17. **5.10** — Reduced internal rate (32 kHz) — high risk, high reward

---

## Expected Cumulative Impact

| Phase | CPU Estimate (QEMU) | CPU Estimate (Hardware) |
|---|---|---|
| Current baseline | 74.78% | ~110-130% (overloaded) |
| After Phase B (5.1 + 5.2 + 5.3) | ~40-45% | ~55-70% |
| After Phase C (5.4–5.7) | ~35-40% | ~45-60% |
| After Phase D (5.8, 5.12) | ~25-35% | ~35-50% |
| After Phase E (5.10, if needed) | ~15-25% | ~20-35% |

**Target:** ≤50% hardware CPU = comfortable headroom for real-time audio.

---

## Measurement Plan

### Per-Item Workflow
1. Capture baseline WAV: `cd test/drumpler && make test` → save output
2. Apply code change
3. Run WAV regression: `diff` new output vs baseline
4. QEMU profile: `./build.sh drumpler build PERF_MON=1 __QEMU_ARM__=1`
5. Record: CPU%, Emulator cycles, RenderTotal, RSS
6. If acceptable: commit. If regression: revert.
7. Build for hardware: `./build.sh drumpler`
8. Load `.drmlgunit` to drumlogue via USB
9. Play test patches, verify no distortion/hanging

### Key Metrics to Track

| Metric | Target | Current |
|---|---|---|
| CPU Total (QEMU) | ≤45% | 40.82% |
| RenderTotal | ≤60% | 67.37% |
| Audio quality | Acceptable | Acceptable (oversampling off) |
| Post-play hang | Eliminated | Present |
| Init time | ≤100 ms | 0 ms (deferred) |
| RSS memory | ≤12 MB | 11.82 MB |

---

## Appendix A: jcmoyer Optimizations — Detailed Comparison

### Not Yet Ported

| Optimization | jcmoyer Location | Impact | Notes |
|---|---|---|---|
| `PCM_GetConfig()` | pcm.cpp:522-583 | Medium | Full config precomputation from register writes |
| `enable_oversampling` toggle | pcm.h:87, pcm.cpp:623-635 | **Critical** | Runtime toggle, exposed as CLI `--disable-oversampling` |
| `PCM_GetOutputFrequency()` | pcm.cpp:1576-1588 | Required for 5.1 | Returns 32kHz when oversampling disabled |
| `calc_tv` nfs guard | pcm.cpp:340-480 | Low | Guards levelcur writes with `pcm.nfs` |
| Bool typing in voice loop | pcm.cpp:1098-1118 | Low | `const bool` for key/okey/active/kon/b6/b7 |
| Explicit int8_t casts in `multi()` | pcm.cpp throughout | Low | `multi(val, (int8_t)(x >> 8))` — clearer, may help compiler |
| `ram1` using uint32_t storage | pcm.h:53-54 | Low | Already uint32_t in our code |

### Already Ported / Superseded

| Optimization | Status | Our Commit |
|---|---|---|
| `addclip20` sign-extend | ✅ Ported (3.3.1) | — |
| BoundedOrderedBitSet / interrupt bitmask | ✅ Superseded (3.1c) | `37e812b` |
| Sleep fast-forward | ✅ Implemented (3.1b) | `317c52c` |
| Free-function architecture | ✅ Partial (2.1) | — |
| Voice-skip optimization | ✅ Implemented (2.3) | — |
| PCM_Config reg_slots | ✅ Partial (2.2) | — |
| Filter direct multiplication | ✅ Already present | `!is_mk1` path |

### Not Applicable / Skipped

| Optimization | Reason |
|---|---|
| Component-separated heap | Our monolithic layout is better for ARM L1 cache |
| LCD emulation | Already disabled (NO_LCD) |
| Wave ROM copy to pcm_t | Our pointer approach is better for embedded |
| std::span / C++17 features | SDK container uses GCC 6.5 (C++14 only) |

---

## Appendix B: ARM Cortex-A7 Optimization Reference

### Instruction Costs (approximate, in-order)

| Operation | Cycles | Notes |
|---|---|---|
| ALU (add, sub, and, or) | 1 | |
| Multiply (32-bit) | 2 | |
| Multiply-accumulate | 2 | Fused on A7 |
| Branch (predicted) | 1 | |
| Branch (mispredicted) | 8+ | Pipeline flush |
| L1 D-cache hit | 3 | Load-use latency |
| L1 D-cache miss / L2 hit | 10-15 | |
| L2 cache miss / DRAM | 50-100+ | 16-bit DDR interface |
| NEON vector multiply | 3 | 4×float32 |
| Division (sdiv/udiv) | 3-12 | Hardware divide on A7 |
| Float single-precision | 1-3 | VFPv4 hardware |
| Float double-precision | 10-20+ | **Software emulated** |

### Optimization Guidelines for this Workload

1. **Minimize branches in voice loop:** Use conditional moves (`csel`) and predicated stores where possible. The PCM voice loop has ~40 branches per voice — on 28 voices, that's 1120 branch predictions per PCM cycle.

2. **Pack hot data:** Ensure `mcu_t` registers (34 bytes) + PCM accumulator state fit in 1-2 cache lines (128 bytes). Currently they're spread across the struct.

3. **Avoid double-precision:** Always use `sinf()`, `powf()`, `fabsf()` — never `sin()`, `pow()`, `fabs()`. The A7 has no hardware double-precision FPU.

4. **NEON batching:** When using NEON, process 4 voices at a time to amortize pipeline drain cost. Don't interleave scalar and NEON instructions.

5. **Prefetch wave ROM:** Use `__builtin_prefetch` 1-2 voices ahead to overlap ROM fetch latency with computation.

6. **Avoid function pointer calls:** Virtual dispatch or function pointers cause indirect branch misprediction on A7. Prefer template specialization or `if constexpr` (C++17 — may need workaround for C++14).

---

## Appendix C: File Reference (Updated)

| File | Purpose | Performance Role | Stage 5 Targets |
|---|---|---|---|
| `emulator/pcm.cc` | PCM chip core | 🔴 Hottest path (voice loop, sample reads) | 5.1, 5.2, 5.3, 5.4, 5.7, 5.8 |
| `emulator/pcm.h` | PCM struct + config | Memory layout | 5.2, 5.6 |
| `emulator/mcu.cc` | MCU emulation + render | 🔴 Main render loop | 5.5, 5.6, 5.9 |
| `emulator/mcu.h` | MCU struct (memory) | Memory layout | 5.6, 5.11 |
| `emulator/mcu_interrupt.cc` | Interrupt dispatch | MCU wakeup path | — (already optimized) |
| `emulator/jv880_wrapper.cc` | MCU wrapper | Render() delegate | 5.9 |
| `synth.h` | Synth class, Render | Idle detection, warmup | 5.9, 5.12 |
| `unit.cc` | SDK callbacks | Entry point | — |
| `config.mk` | Build config | Compiler flags | — |
