# Drumpler PCM Optimization Summary

## Status: Ready for Baseline Profiling

**Completed:**
- ✅ Added PERF_MON infrastructure to PCM emulator
- ✅ Identified voice processing hot path PCM_Update()
- ✅ Added high-level profiling counters (avoiding per-voice overhead)

**Current Profiling Design:**
```cpp
PCM_Effects    - Chorus/reverb processing (once per sample)
PCM_VoiceLoop  - All 32 voice processing (aggregate)
```

**Known Hot Paths from Code Analysis:**

| Section | Location | Work |  
|---------|----------|------|
| Sample Reading | pcm.cc:1041-1125 | 4x PCM_ReadROM() per voice |
| Interpol ation | pcm.cc:1217-1250 | 3-stage with lookup tables |
| Filter | pcm.cc:1250-1295 | State-variable filter math |
| Envelopes | pcm.cc:330-445, 1318-1320 | 3x calc_tv() per voice |
| Mixing | pcm.cc:1325-1430 | Volume/pan/effects send |

**Next Steps for Optimization:**

### 1. Voice Activity Detection (High Priority)
**Goal:** Skip inactive voices to save ~50-70% CPU  
**Implementation:**
```cpp
// In pcm_t struct - add activity tracking
bool voice_active_flags[32];
uint16_t voice_silence_frames[32];

// In PCM_Update voice loop
if (!key || volmul1 == 0) {
    voice_silence_frames[slot]++;
    if (voice_silence_frames[slot] > 480) { // 10ms @ 48kHz
        continue; // Skip this voice entirely
    }
}
```

**Expected gain:** 50-70% CPU reduction with typical polyphony (<16 voices active)

### 2. NEON Batch Processing (Medium Priority)
**Goal:** Process 4 voices at once using SIMD  
**Target:** Sample reading and interpolation (most CPU-intensive)

**Current scalar code:**
```cpp
for (int slot = 0; slot < reg_slots; slot++) {
    int samp0 = (int8_t)PCM_ReadROM(addr0);
    int samp1 = (int8_t)PCM_ReadROM(addr1);
    // ... process one voice
}
```

**NEON vectorized (4 voices):**
```cpp
for (int slot = 0; slot < reg_slots; slot += 4) {
    // Load 4 samples at once
    int8x16_t samples = {
        PCM_ReadROM(addr0), PCM_ReadROM(addr1),
        PCM_ReadROM(addr2), PCM_ReadROM(addr3), ...
    };
    //... NEON interpolation
}
```

**Expected gain:** 15-20% on interpolation hotspot

### 3. Optimize calc_tv() Envelopes (Low Priority)
Envelope calculation (`calc_tv`) is called 3x per voice (TVA, TVF, pitch).

**Current:** Scalar bit manipulation with branches  
**Optimized:** Branchless arithmetic, cache envelope state

**Expected gain:** 5-10% CPU reduction

### 4. Sample Cache (Medium Priority)  
Most voices read from same WaveROM regions. Add L1 cache for recently accessed samples.

```cpp
struct SampleCache {
    uint32_t last_addr[4];
    int8_t last_sample[4];
};
```

**Expected gain:** 10-15% on sample read hotspot

## Profiling Plan (Post-Fix)

1. Build with simplified PERF_MON (effects + voice loop aggregate)
2. Run test: `./test-unit.sh drumpler --perf-mon --note 60 --duration 2.0`
3. Analyze cycle breakdown:
   - PCM_Effects: Should be <5% of total
   - PCM_VoiceLoop: Should be 40-50% of render time
   - Remaining: MCU emulation + resampling

4. Implement optimizations in priority order
5. Re-profile after each to validate improvements

## Estimated Cumulative Gains

| Optimization | Est. CPU Reduction | Cumulative |
|--------------|-------------------|------------|
| **Baseline** | - | 100% |
| Voice activity detection | -50% | **50%** |
| NEON interpolation | -8% (15% of 50%) | **42%** |
| Envelope optimization | -2% (5% of 42%) | **40%** |
| Sample cache | -5% (10% of 40%) | **35%** |
| **Target** | | **30-40%** |

Combined with existing idle detection (<1% when silent), drumpler would be:
- **Idle:** <1% CPU
- **Active (optimized):** 30-40% CPU (vs current 100%)
- **Effective improvement:** 60-70% reduction in active CPU load  

## Implementation Timeline

- ✅ **Day 1:** Profiling infrastructure (completed)
-  **Day 2:** Voice activity detection
- **Day 3:** NEON batch processing
- **Day 4:** Envelope + cache optimizations
- **Day 5:** Integration testing & benchmarking

**Total estimated effort:** 4-5 days (vs 6-12 months for full rewrite)
