# JV-880 ROM Format Analysis

## Executive Summary

**Question:** Can drumpler be ported away from emulation to native implementation?

**Answer:** **Theoretically yes, but EXTREMELY difficult.** This would require:
1. Reverse engineering the proprietary Roland PCM chip DSP core
2. Reverse engineering JV-880 patch format from NVRAM
3. Understanding sample addressing, ADPCM decode, envelope/filter algorithms
4. Rewriting ~50,000 lines of hardware-accurate emulator code

**Estimated effort:** 6-12+ months of full-time reverse engineering work  
**Recommended:** Keep current emulation approach with idle detection optimization

---

## ROM Files Overview

### Base JV-880 ROMs (4.32 MB total)

| File | Size | Purpose | Contents |
|------|------|---------|----------|
| `jv880_rom1.bin` | 32 KB | CPU Firmware | H8/300H executable code (low half) |
| `jv880_rom2.bin` | 256 KB | CPU Firmware Extended | H8/300H executable + system tables |
| `jv880_waverom1.bin` | 2 MB | PCM Samples | Roland proprietary sample format |
| `jv880_waverom2.bin` | 2 MB | PCM Samples | Additional PCM samples |
| `jv880_nvram.bin` | 32 KB | Patches/Presets | 128 user/factory patches |

**WaveROM Header:**
```
Offset  Content
0x00    "Roland JV80     " (ASCII identifier)
0x10    "Internal Ver1.00" (version string)
0x20+   PCM sample data (likely 8-bit or ADPCM compressed)
```

### Expansion ROMs (8 MB each)

SR-JV80 series expansion cards add additional samples:
- SR-JV80-01 (Pop)
- SR-JV80-02 (Orchestral)
- SR-JV80-05 (World)
- SR-JV80-08 (Keyboards)
- SR-JV80-10 (Bass & Drums)
- RD-500 (Piano expansion)

**Format:** Scrambled/encrypted, requires `UnscrambleAndLoadExpansionROM()` before use.

---

## NVRAM Structure (Patch Format)

### Observations from Hexdump

**Sample patches found at:**
- Offset `0x0B0`: "Syn Lead    " (12-char ASCII name, padded)
- Offset `0x170`-`0x180`: "Encounter X E" (split across boundary)
- Offset `0x240`: "Analog Pad  "

**Structure hypothesis:**
- **128 patches total** (user + factory)
- **Fixed-size records** (likely 256 bytes each = 32KB / 128)
- **Patch name:** 12 characters, 7-bit ASCII, space-padded
- **Parameter data:** Following name, likely includes:
  - Tone structure (multi-timbral parts)
  - Voice parameters (pitch, filter, envelope)
  - Effects settings (reverb, chorus, delay)
  - Sample/waveform assignments

**Challenge:** No documentation of field offsets, need to reverse engineer from CPU firmware behavior.

---

## PCM Chip Architecture

### Hardware Overview

The JV-880 uses a proprietary Roland PCM synthesis chip with:
- **32 simultaneous voices** (polyphony)
- **Sample playback** from WaveROMs with interpolation
- **Hardware envelopes** (TVF, TVA - time-variant filter/amp)
- **Effects processing** (reverb, chorus, filtering)

### Register Structure

From `pcm.h`:
```cpp
struct pcm_t {
    uint32_t ram1[32][8];      // Voice parameter RAM (32 channels × 8 registers)
    uint16_t ram2[32][16];     // Additional voice RAM (32 channels × 16 registers)
    uint8_t eram[0x4000];      // Effect RAM (16KB)
    // ... control registers ...
};
```

**Each voice has:**
- 8 × 32-bit registers (ram1) - sample address, pitch, envelope state
- 16 × 16-bit registers (ram2) - volume, pan, filter cutoff, resonance

### Sample Access

From `pcm.cc` line 1021-1125:
```cpp
int newnibble = PCM_ReadROM((hiaddr << 20) | wave_address);
int samp0 = (int8_t)PCM_ReadROM((hiaddr << 20) | address_cnt);
```

**Sample format:**
- Samples stored as **signed 8-bit values** (`int8_t` cast)
- 24-bit addressing: `(hiaddr << 20) | address`
- Supports multiple WaveROMs via high address bits
- **Interpolation:** Uses lookup tables (`interp_lut[3][128]`) for sample smoothing

### Envelope Generator

`calc_tv()` function (lines 330-445):
- **3 envelope generators per voice:**
  - e=0: TVA (amplitude envelope)
  - e=1: TVF (filter envelope)  
  - e=2: Pitch envelope
- **Parameters:**
  - `target`: Target level (0-255)
  - `speed`: Envelope rate/speed (with mode bits)
  - `levelcur`: Current level (15-bit)
- **Complex logic:** Attack/decay/sustain/release with saturation handling

### Filter Implementation

Details not fully analyzed yet, but evidence suggests:
- State-variable filters (lowpass, highpass, bandpass)
- Resonance control
- Cutoff frequency modulation by envelope

### Voice Processing Loop

From `PCM_Update()` (line 448+):
- Called once per sample at 64kHz internal rate
- Iterates through active voices (32 max)
- For each voice:
  1. Read sample from WaveROM
  2. Apply pitch shift (fractional addressing)
  3. Calculate envelopes (TVA, TVF, pitch)
  4. Apply filter
  5. Mix to stereo output (`accum_l`, `accum_r`)

---

## What Direct Implementation Would Require

### 1. Sample Player (Moderate Difficulty)

**Requirements:**
- Parse WaveROM sample bank structure
- Implement 24-bit sample addressing
- Handle loop points (start, end, loop mode)
- Fractional pitch shifting with interpolation
- Multi-sample playback (32 voices)

**Feasibility:** **Moderate** - standard DSP techniques, but need ROM structure docs

### 2. Patch Decoder (High Difficulty)

**Requirements:**
- Reverse engineer NVRAM patch format (256 bytes × 128 patches)
- Parse tone structures (layer assignments, key zones)
- Extract voice parameters (pitch, filter, envelope settings)
- Map to synthesizer parameters

**Feasibility:** **Difficult** - requires extensive reverse engineering and validation

### 3. Envelope Generators (Moderate Difficulty)

**Requirements:**
- Implement `calc_tv()` envelope logic (TVA, TVF, pitch)
- Handle attack, decay, sustain, release, key-follow
- Match hardware timing behavior

**Feasibility:** **Moderate** - algorithm is understood from pcm.cc, but complex

### 4. Filter Engine (High Difficulty)

**Requirements:**
- State-variable filter implementation
- Cutoff and resonance control
- Envelope modulation
- Match Roland's filter character

**Feasibility:** **Difficult** - proprietary filter design, no public specs

### 5. Effects Processing (Very High Difficulty)

**Requirements:**
- Reverb algorithm (likely algorithmic reverb)
- Chorus algorithm (multi-tap delay with modulation)
- Delay effects
- Effects RAM buffer management

**Feasibility:** **Very Difficult** - proprietary effects, reverse engineer from `eram` access patterns

### 6. Voice Allocation & MIDI (Low Difficulty)

**Requirements:**
- Note on/off handling
- Voice stealing (polyphony limit)
- MIDI CC mapping to parameters
- Program change (preset loading)

**Feasibility:** **Easy** - standard MIDI implementation

---

## Performance Analysis

### Current Emulation (With Idle Detection)

From PERF_MON tests:
- **Active (playing notes):** ~104% CPU (52% emulator core, 50% resampling/other)
- **Idle (silent):** <1% CPU
- **Emulator breakdown:**
  - H8/300H CPU emulation: ~40% of active time
  - PCM chip DSP: ~10-15% of active time
  - Resampling 64kHz→48kHz: ~50% of active time (NEON optimized)

### Estimated Direct Implementation

**Best case scenario:**
- No CPU emulation overhead: **-40%**
- Native 48kHz (no resampling): **-50%**
- Optimized voice processing: **+20%** (needs careful NEON optimization)
- **Total:** **~30% CPU** when playing notes

**Realistic scenario:**
- Initial implementation: **~50-60% CPU**
- After optimization: **~35-40% CPU**

**Benefit:** 50-60% CPU reduction vs current emulation  
**But:** Idle detection already achieves <1% when silent, so real-world improvement is less significant

---

## Reverse Engineering Challenges

### 1. No Public Documentation

- Roland never published JV-880 patch format specs
- PCM chip is proprietary, no datasheets
- WaveROM structure is undocumented
- Effects algorithms are trade secrets

### 2. Validation Difficulty

- How to verify correct behavior without hardware?
- Patch parameters: trial-and-error vs. emulator comparison
- Audio quality: subjective vs. cycle-accurate emulation

### 3. Legal/Licensing Issues

- Nuked-SC55 emulator: Non-commercial use only
- ROM files: Copyrighted by Roland
- Direct implementation still requires ROM data (samples, patches)
- May violate reverse engineering clauses in Roland licensing

### 4. Maintenance Burden

- Emulator is actively maintained by nukeykt
- Direct implementation: all bugs/issues are YOUR responsibility
- No upstream fixes or improvements

---

## Recommendations

### Option 1: Keep Emulation ✅ **RECOMMENDED**

**Pros:**
- Already working and tested
- Cycle-accurate audio quality
- Idle detection eliminates CPU waste when silent
- Upstream maintenance and bug fixes
- Legal: uses existing open-source emulator

**Cons:**
- ~100% CPU when actively playing (still under ARM limit)
- Larger binary size (~4-13 MB per ROM pack)

**Verdict:** Best choice for reliability and maintainability

### Option 2: Optimize Emulation

**Possible improvements:**
- Profile hot loops in `PCM_Update()` for NEON optimization
- Reduce 64kHz internal rate to 48kHz (may affect accuracy)
- Implement dynamic voice allocation (only process active voices)
- Cache-friendly data structures

**Estimated gain:** 10-20% CPU reduction  
**Effort:** 1-2 weeks  
**Risk:** Low

### Option 3: Hybrid Approach

**Concept:**
- Keep CPU emulation for MIDI/parameter processing
- Rewrite PCM chip voice processing (sample playback, envelopes, filters)
- Bypass effects processing (use drumlogue's master effects)

**Estimated gain:** 30-40% CPU reduction  
**Effort:** 2-3 months  
**Risk:** Medium-high

### Option 4: Full Native Implementation ⚠️ **NOT RECOMMENDED**

**Effort:** 6-12+ months full-time  
**Risk:** Very high (incomplete reverse engineering, bugs, audio quality issues)  
**Benefit:** 50-60% CPU reduction  
**Verdict:** Cost/benefit doesn't justify effort for hobby project

---

## Conclusion

**Answer to original question:** "Can we port away from emulation?"

**Yes, technically possible, but STRONGLY NOT RECOMMENDED.**

The current emulation approach with idle detection is:
- ✅ Working reliably
- ✅ Cycle-accurate audio quality
- ✅ <1% CPU when idle
- ✅ ~100% CPU when playing (acceptable on ARM Cortex-M7 @ 216MHz)
- ✅ Maintainable and legally clear

**Direct implementation would require:**
- ❌ 6-12 months reverse engineering effort
- ❌ Extensive validation and debugging
- ❌ Ongoing maintenance burden
- ❌ Potential legal/licensing issues
- ✅ 50-60% CPU reduction (but only when actively playing)

**Recommendation:** Focus optimization efforts on:
1. **Voice allocation optimization** - only process active voices
2. **Dynamic quality scaling** - reduce polyphony when CPU load high
3. **Additional idle detection** - sleep individual voices when envelope finished
4. **Profile-guided optimization** - NEON-optimize hot loops in PCM_Update()

These incremental improvements can achieve 20-30% CPU reduction with **1-2 weeks effort** vs. 6-12 months for full rewrite.

---

## Next Steps (If Pursuing Optimization)

1. **Profile PCM_Update()** with PERF_MON at voice-level granularity
2. **Identify hottest code paths** in sample interpolation and envelope calculation
3. **NEON-optimize voice processing loop** (batch 4 voices at once)
4. **Implement voice activity detection** (skip voices with zero amplitude)
5. **Benchmark and iterate**

Expected outcome: **70-80% CPU** when playing vs current 100%, with **<1 week effort**.

---

## References

- Emulator source: `drumlogue/drumpler/emulator/`
- PCM chip implementation: `pcm.cc` lines 448+ (`PCM_Update()`)
- Envelope generator: `pcm.cc` lines 330-445 (`calc_tv()`)
- Sample interpolation: `pcm.cc` lines 1021-1125
- Porting documentation: `PORTING_PLAN.md`, `PORT_STATUS.md`
- ROM files: `drumlogue/drumpler/resources/*.bin`

