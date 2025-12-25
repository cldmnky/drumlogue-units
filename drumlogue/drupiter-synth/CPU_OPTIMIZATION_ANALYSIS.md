# CPU Optimization Analysis: Drupiter Polyphonic Mode

## Executive Summary

The polyphonic mode in Drupiter-Synth is exceeding CPU budget and causing drumlogue to crash/break. This document provides a deep analysis of the CPU bottlenecks and concrete optimization strategies.

**Current Status:** ❌ Polyphonic mode uses excessive CPU (breaks drumlogue)
**Target:** ✅ Reduce CPU usage to safe operational level (<70% of budget)

---

## 1. Current Architecture Overview

### 1.1 Rendering Pipeline (Monophonic - Baseline)

```
Per-Buffer (64 frames @ 48kHz = 1.33ms):
├── LFO Processing (1 instance)
│   ├── Phase accumulation
│   ├── Waveform lookup (triangle, ramp, square, S&H)
│   ├── Envelope modulation (optional)
│   └── Output: 1 sample
│
├── Main Envelope Processing (2 instances: VCF, VCA)
│   ├── ADSR state machine × 2
│   ├── Envelope parameter smoothing
│   └── Output: 2 samples
│
├── Per-Frame Loop (64 iterations):
│   ├── DCO1 Processing
│   │   ├── SetWaveform() call
│   │   ├── SetFrequency() + frequency modulation (LFO, pitch ENV)
│   │   ├── Phase accumulation + anti-aliasing
│   │   └── Waveform generation (multiple approaches per waveform type)
│   │
│   ├── DCO2 Processing (if needed, conditional)
│   │   └── Same as DCO1
│   │
│   ├── Oscillator Mixing
│   │   ├── Level scaling
│   │   └── Optional cross-modulation (FM)
│   │
│   ├── HPF Processing (one-pole high-pass)
│   │   └── Simple difference equation
│   │
│   ├── VCF Processing
│   │   ├── Cutoff frequency calculation (non-linear mapping)
│   │   ├── Modulation envelope + LFO + velocity
│   │   ├── Filter coefficient update (optimized: only if cutoff changes >1Hz)
│   │   ├── State-variable filter math (multiple modes: LP12/LP24/HP/BP)
│   │   └── Oversampling (2x) for improved stability
│   │
│   ├── VCA Envelope & Modulation
│   │   ├── Envelope processing
│   │   ├── LFO tremolo
│   │   ├── Keyboard tracking
│   │   └── Level scaling
│   │
│   └── Output scaling + soft clamp
│
└── Effect Processing (Space/Chorus stereo widener)
    ├── Mono-to-stereo conversion
    ├── Delay-based effect processing
    └── Stereo interleave + denormal protection
```

**Estimated CPU per buffer (monophonic, all voices active):** ~25-30% of budget

### 1.2 Rendering Pipeline (Polyphonic - Current)

```
Per-Buffer (64 frames @ 48kHz):
├── LFO Processing (1 shared instance)
│   └── Same as mono
│
├── Main Envelopes (2 instances, NOT per voice)
│   └── Same as mono
│
├── Per-Frame Loop (64 iterations):
│   ├── PER VOICE LOOP (voices 0..7):
│   │   ├── Check if voice active or envelope active (8× checks)
│   │   ├── Set waveform (8× DCO1 SetWaveform)
│   │   ├── Set waveform (8× DCO2 SetWaveform)
│   │   ├── Calculate voice-specific frequencies
│   │   ├── Frequency modulation (LFO + pitch envelope)
│   │   ├── DCO1 Process (8× phase accumulation + waveform)
│   │   ├── DCO2 Process (8× phase accumulation + waveform)
│   │   ├── Voice envelope processing (8× ADSR)
│   │   ├── Per-voice mixing + envelope application
│   │   └── Accumulate to mixed_
│   │
│   ├── Scale by active voice count (sqrt reciprocal)
│   ├── HPF Processing (shared)
│   ├── VCF Processing (shared on mixed signal)
│   ├── VCA + Modulation (shared)
│   └── Output scaling + soft clamp
│
└── Effect Processing (same as mono)
```

**Key Issue: DCO processing scales with voice count (8×), but VCF is shared**

---

## 2. CPU Cost Analysis (ARM Cortex-A7)

### 2.1 Component Costs (Per-Sample or Per-Buffer)

| Component | Mono Cost | Notes |
|-----------|-----------|-------|
| **LFO Phase Accum** | ~5 cycles | Per-buffer; simple float multiply + add |
| **LFO Waveform** | ~15 cycles | Phase lookup + interpolation |
| **Envelope (ADSR)** | ~20 cycles | Per-sample state machine + exponential calc |
| **DCO Phase Accum** | ~8 cycles | Per-sample; float multiply |
| **DCO Waveform** | ~40-60 cycles | Depends on waveform: SAW/SQR/PUL/TRI |
| **DCO Anti-alias** | ~15 cycles | Optional, if enabled |
| **VCF Coefficient Calc** | ~100 cycles | Expensive: log2, pow2, trig approximations (every 1-2ms) |
| **VCF Filter Step** | ~60 cycles | Per-sample; state-variable math (4 state variables) |
| **VCF Oversampling 2x** | ×2 multiplier | Two filter steps per output sample |
| **Envelope Release Detection** | ~3 cycles | IsActive() check |
| **Soft Clamp** | ~5 cycles | Comparisons + conditionals |

### 2.2 Polyphonic Cost Scaling

For **4 active voices in polyphonic mode**, per 64-frame buffer (1.33ms @ 48kHz):

```
LFO:                     1 × 20 = 20 cycles
Envelopes (2×):          2 × 64 × 20 = 2,560 cycles

Voice Loop (64 frames × 8 voices max):
  ├── Voice checks:      64 × 8 × 3 = 1,536 cycles
  ├── DCO1 process:      64 × 8 × 50 = 25,600 cycles
  ├── DCO2 process:      64 × 8 × 50 = 25,600 cycles (if 50% active)
  ├── Voice env process: 64 × 8 × 20 = 10,240 cycles
  ├── Mixing/envelope:   64 × 8 × 10 = 5,120 cycles
  └── Subtotal:          ~68,000 cycles

HPF:                     64 × 10 = 640 cycles
VCF Coefficient:         ~100 cycles (amortized)
VCF Filter (2x):         64 × 120 = 7,680 cycles
VCA + modulation:        64 × 15 = 960 cycles
Soft clamps:             64 × 5 = 320 cycles
Effect processing:       64 × 30 = 1,920 cycles
Output interleave:       64 × 10 = 640 cycles

TOTAL (4 voices):        ~82,000 cycles ≈ 1.7ms @ 48MHz
BUDGET:                  1.33ms (full buffer)
OVERHEAD:                +28% over budget ❌
```

**With 8 voices (max):** ~140,000+ cycles ≈ 2.9ms = +118% over budget 💥

### 2.3 Why Polyphonic Breaks Drumlogue

The drumlogue CPU budget is approximately **~1.33ms per 64-frame buffer @ 48kHz**, totaling the time available:
- **Mono/UNISON mode:** Uses ~0.9-1.0ms → Safe
- **4-voice polyphonic:** Uses ~1.7ms → +28% over budget
- **8-voice polyphonic:** Uses ~2.9ms → +118% over budget → **BREAKS DRUMLOGUE**

The issue: **Multiple independent DCO instances per voice multiply the CPU cost per active voice.**

---

## 3. Bottleneck Identification

### 3.1 Primary Bottleneck: Per-Voice DCO Processing (60% of poly overhead)

**Location:** `drupiter_synth.cc:387-473` (polyphonic voice loop)

**Current approach:**
```cpp
for (uint8_t v = 0; v < DRUPITER_MAX_VOICES; v++) {
    dsp::Voice& voice_mut = const_cast<dsp::Voice&>(voice);
    
    // ❌ EXPENSIVE: These happen per voice, per sample
    voice_mut.dco1.SetWaveform(...);    // Polyphonic instances
    voice_mut.dco2.SetWaveform(...);
    voice_mut.dco1.SetFrequency(...);   // 8 frequency calcs + modulation
    voice_mut.dco2.SetFrequency(...);
    
    float voice_dco1 = voice_mut.dco1.Process();  // 8 DCO processes
    float voice_dco2 = voice_mut.dco2.Process();
    float voice_env = voice_mut.env_amp.Process();  // 8 envelope processes
    
    // Mix and accumulate
    float voice_mix = voice_dco1 * dco1_level + voice_dco2 * dco2_level;
    mixed_ += voice_mix * voice_env;
}
```

**Cost factors:**
- **8 DCO1 waveform generators** running in parallel (vs. 1 in mono)
- **8 DCO2 waveform generators** running in parallel
- **8 envelope processors** (each is ADSR with exponential curves)
- **8 frequency modulation calculations** (LFO + pitch envelope per voice)

---

### 3.2 Secondary Bottleneck: Per-Voice Parameter Setup

**Cost:** SetWaveform/SetFrequency call overhead with dynamic state updates

Each voice has separate state:
- DCO phase accumulator
- Waveform state machines
- Filter coefficients (if per-voice VCF existed)
- Envelope state

**Per-voice in loop = cache misses + repeated work**

---

### 3.3 Tertiary Bottleneck: Voice Allocation & Checking

**Cost:** ~3-5 cycles per voice check, 8 checks per frame

```cpp
if (!voice.active && !voice.env_amp.IsActive()) {
    continue;  // Skip inactive voice
}
```

Minor but adds up over 64 frames × 8 voices.

---

## 4. Optimization Strategies

### 4.1 Strategy A: Limit Maximum Polyphonic Voices (Immediate, -30% CPU)

**Implementation:** Reduce `DRUPITER_MAX_VOICES` from 7 to 4

**Impact:**
```
Current:  8 voices × 50 cycles/sample = 400 cycles/sample = 25,600 cycles/frame
Reduced:  4 voices × 50 cycles/sample = 200 cycles/sample = 12,800 cycles/frame
Savings:  12,800 cycles ≈ 27% reduction
```

**Pros:**
- Trivial to implement (1 line change: `#define DRUPITER_MAX_VOICES 4`)
- Immediate CPU relief
- No algorithmic changes

**Cons:**
- Reduces polyphonic capability (4 voices is still useful, common in hardware)
- Not a real optimization, just a limitation

**Verdict:** **IMPLEMENT IMMEDIATELY as band-aid while pursuing deeper optimizations**

---

### 4.2 Strategy B: Voice Pool Recycling & Intelligent Skipping (Immediate, -20% CPU)

**Problem:** Currently checks all 8 voices even if only 2-3 are active

**Implementation:**
```cpp
// Track active voice indices instead of checking all
static uint8_t active_voice_indices[DRUPITER_MAX_VOICES];
static uint8_t active_voice_count = 0;

// NoteOn: Maintain list of active voices
void VoiceAllocator::NoteOn(...) {
    // ... allocate voice ...
    if (voice->active) {
        active_voice_indices[active_voice_count++] = voice_idx;
    }
}

// In render loop: Only iterate active voices
for (uint8_t i = 0; i < active_voice_count; i++) {
    uint8_t v = active_voice_indices[i];
    Voice& voice = voices_[v];
    // Process only this voice
    // ... DCO, envelope, etc ...
}
```

**Cost Reduction:**
- **Before:** 8 IsActive() checks per frame = 64 checks
- **After:** 2-4 voice iterations (typical) per frame

**Impact:** ~20-25% reduction in per-frame voice overhead

**Pros:**
- Simple to implement
- Works with existing code
- Scales with actual voice count

**Cons:**
- Adds bookkeeping overhead in NoteOn/NoteOff
- Needs careful synchronization with voice stealing

**Verdict:** **IMPLEMENT (Priority: High)**

---

### 4.3 Strategy C: Voice-Specific DCO Consolidation (Advanced, -40% CPU)

**Problem:** Each voice has separate DCO1 + DCO2 instances = expensive

**Current:**
```
Voice 0: dco1[0] + dco2[0]
Voice 1: dco1[1] + dco2[1]
Voice 2: dco1[2] + dco2[2]
...
Voice 7: dco1[7] + dco2[7]
Total: 16 DCO instances in memory (worst-case 8 active)
```

**Optimization: Shared DCO with per-voice parameters**

Instead of separate DCO instances, use **single DCO pair** with cached per-voice state:

```cpp
struct VoiceOscillatorState {
    float phase1, phase2;           // Per-voice phase (critical!)
    float frequency1, frequency2;   // Per-voice frequencies
    // Waveform, sync, FM shared across voices
};

class VoiceAllocator {
    JupiterDCO dco1_shared;  // Single instance
    JupiterDCO dco2_shared;
    VoiceOscillatorState voice_osc_state[DRUPITER_MAX_VOICES];
};
```

**Implementation approach:**
1. Move phase accumulation to per-voice state
2. Set DCO frequency from voice state
3. Process shared DCO, but use per-voice phase
4. Update per-voice phase after processing

**Cost analysis:**
- **Current:** 8 DCO Process() = 8 × 50 cycles = 400 cycles/sample
- **Optimized:** 1 phase update/sample per voice + 1 shared DCO = 8×5 + 50 = 90 cycles/sample
- **Savings:** 310 cycles/sample = 19,840 cycles/frame ≈ -77% DCO cost

**Pros:**
- Massive CPU reduction (40%+ overall)
- Maintains per-voice polyphony
- Better cache locality

**Cons:**
- Requires refactoring DCO architecture
- Complex implementation
- Need to preserve per-voice phase state between updates

**Verdict:** **IMPLEMENT (Priority: Very High, significant effort)**

---

### 4.4 Strategy D: Shared Envelope Instances with Per-Voice Tracking (Medium, -15% CPU)

**Problem:** Each voice has separate `env_amp`, `env_filter`, `env_pitch` envelopes

**Current architecture:**
```cpp
struct Voice {
    JupiterEnvelope env_amp;        // Separate ADSR instance per voice
    JupiterEnvelope env_filter;
    JupiterEnvelope env_pitch;
};
```

**Optimization: Envelope pool with lookup**

```cpp
class VoiceAllocator {
    JupiterEnvelope env_pool[DRUPITER_MAX_VOICES];  // Reuse efficiently
    
    // Instead of per-voice envelopes, use pool indices
    struct VoiceState {
        uint8_t env_amp_idx;     // Index into pool
        uint8_t env_filter_idx;
        uint8_t env_pitch_idx;
    };
};
```

**Cost reduction:**
- Consolidates envelope storage (minor RAM savings)
- Allows envelope processing to be pooled/vectorized
- Reduces cache misses from scattered voice instances

**Impact:** ~10-15% CPU reduction in envelope processing

**Verdict:** **IMPLEMENT (Priority: Medium, with Strategy C)**

---

### 4.5 Strategy E: Reduce Active Voice Count at High CPU Load (Dynamic, -20-30% CPU)

**Problem:** With 4+ voices active AND effects + modulation, CPU exceeds budget

**Implementation: Adaptive voice limiting**

```cpp
class VoiceAllocator {
    // Monitor CPU budget utilization
    float cpu_load_estimate;
    
    void Render(...) {
        // Estimate CPU usage (measure frame time)
        if (cpu_load_estimate > 0.9f) {  // 90% budget used
            // Reduce max voices dynamically
            max_voices_dynamic = 3;  // Limit to 3 active
        } else if (cpu_load_estimate < 0.7f) {
            // Restore to configured limit
            max_voices_dynamic = max_voices_config;
        }
        
        // In voice allocation, respect dynamic limit
        if (active_voices_ >= max_voices_dynamic) {
            StealVoice();  // Force steal if exceeding limit
        }
    }
};
```

**Pros:**
- Prevents CPU spikes and drumlogue crashes
- Graceful degradation
- Can be user-configurable (e.g., "4 voice max")

**Cons:**
- Can be noticeable (notes cut off suddenly)
- Requires CPU measurement/estimation

**Verdict:** **IMPLEMENT (Priority: High, as safety net)**

---

### 4.6 Strategy F: NEON SIMD Vectorization (Advanced, -25% CPU)

**Problem:** ARM Cortex-A7 has NEON unit but current code is scalar

**Approach: Vectorize voice processing**

```cpp
#ifdef USE_NEON
// Process 4 voices in parallel using NEON
void VoiceAllocator::RenderPolyphonic_NEON(float* out, uint32_t frames) {
    for (uint32_t i = 0; i < frames; i++) {
        // Load 4 voice frequencies into NEON register
        float32x4_t freq_vec = vld1q_f32(&voice_freqs[0]);
        
        // Update 4 phase accumulators in parallel
        float32x4_t phase_vec = vld1q_f32(&phases[0]);
        phase_vec = vaddq_f32(phase_vec, freq_vec);
        vst1q_f32(&phases[0], phase_vec);
        
        // Convert phases to samples (more complex with waveforms)
        // ...
    }
}
#endif
```

**Impact:** ~25-35% overall reduction (depends on workload)

**Verdict:** **IMPLEMENT (Priority: Medium, deferred optimization)**

---

## 5. Recommended Implementation Plan

### Phase 1: Immediate (This Week)
1. **Strategy A:** Reduce `DRUPITER_MAX_VOICES` from 7 → 4
   - File: `drumlogue/drupiter-synth/dsp/voice_allocator.h` line 27
   - Change: `#define DRUPITER_MAX_VOICES 4`
   - CPU Savings: ~27%
   
2. **Strategy B:** Active voice index tracking
   - File: `drumlogue/drupiter-synth/dsp/voice_allocator.h` + `.cc`
   - CPU Savings: ~20%

3. **Strategy E:** Dynamic voice limiting (safety net)
   - File: `drumlogue/drupiter-synth/drupiter_synth.cc`
   - CPU Savings: ~Prevents crashes

**Cumulative:** ~47% reduction → brings 4-voice poly to safe territory

### Phase 2: Core Optimization (Next 2 Weeks)
4. **Strategy C:** Voice-specific DCO consolidation
   - File: `drumlogue/drupiter-synth/dsp/voice_allocator.*`
   - Refactor: Move phase accumulation to per-voice state
   - CPU Savings: ~40%
   
5. **Strategy D:** Envelope pooling
   - File: `drumlogue/drupiter-synth/dsp/voice_allocator.*`
   - CPU Savings: ~15%

**Cumulative with Phase 1:** ~75% reduction → full polyphonic headroom

### Phase 3: Advanced (Future)
6. **Strategy F:** NEON vectorization
7. **Further optimizations:** Per-voice VCF sharing, parallel rendering

---

## 6. Testing & Validation Plan

### 6.1 Measurement Strategy

**Desktop test harness:**
```cpp
// In test/drupiter-synth/main.cc
#include <chrono>

auto start = std::chrono::high_resolution_clock::now();
synth.Render(out_buffer, 64);  // 1.33ms @ 48kHz
auto end = std::chrono::high_resolution_clock::now();

auto duration_ms = std::chrono::duration<double, std::milli>(end - start).count();
float cpu_load = (duration_ms / 1.33f) * 100.0f;  // % of budget

std::cout << "CPU Load: " << cpu_load << "% (budget: 100%)" << std::endl;
```

### 6.2 Hardware Testing

**On drumlogue:**
1. Load polyphonic preset
2. Play 4 consecutive notes (increasing voice count: 1, 2, 3, 4)
3. Monitor for:
   - Audio glitches/pops
   - Dropped notes
   - Envelope cutoff
   - Slow UI response

### 6.3 Acceptance Criteria

- ✅ Monophonic mode: <50% CPU load
- ✅ 4-voice polyphonic: <70% CPU load
- ✅ 8-voice unison: <70% CPU load
- ✅ No audio glitches when playing with 4 voices + effects
- ✅ No drumlogue freezing/crashes

---

## 7. Detailed Code Changes (Phase 1)

### 7.1 Change 1: Reduce Max Voices

**File:** `drumlogue/drupiter-synth/dsp/voice_allocator.h`

```cpp
// Line 27: CHANGE FROM:
#ifndef DRUPITER_MAX_VOICES
#define DRUPITER_MAX_VOICES 7
#endif

// TO:
#ifndef DRUPITER_MAX_VOICES
#define DRUPITER_MAX_VOICES 4  // Reduced from 7 to ensure CPU budget
#endif
```

**Rationale:**
- 4 voices is still "poly" (useful for patterns)
- Reduces CPU footprint by ~27%
- Common in hardware synthesizers (Juno-60, Microbrute)

---

### 7.2 Change 2: Active Voice Tracking

**File:** `drumlogue/drupiter-synth/dsp/voice_allocator.h`

Add to `VoiceAllocator` class:
```cpp
private:
    // Active voice tracking (optimization)
    uint8_t active_voice_list_[DRUPITER_MAX_VOICES];  // Indices of active voices
    uint8_t num_active_voices_;                       // Count
    
    void UpdateActiveVoiceList();  // Rebuild list on NoteOn/NoteOff
    void RemoveFromActiveList(uint8_t voice_idx);
    void AddToActiveList(uint8_t voice_idx);
```

**File:** `drumlogue/drupiter-synth/dsp/voice_allocator.cc`

Implement updates in NoteOn/NoteOff:
```cpp
void VoiceAllocator::NoteOn(uint8_t note, uint8_t velocity) {
    // ... existing code ...
    
    if (voice) {
        TriggerVoice(voice, note, velocity);
        // Add to active list
        if (num_active_voices_ < DRUPITER_MAX_VOICES) {
            active_voice_list_[num_active_voices_++] = voice_idx;
        }
    }
}

void VoiceAllocator::NoteOff(uint8_t note) {
    // ... mark voice for release ...
    // Keep in active_voice_list_ until envelope fully finished
}
```

Update render loop:
```cpp
void VoiceAllocator::RenderPolyphonic(float* left, float* right, 
                                      uint32_t frames, const float* params) {
    // Only iterate active voices
    for (uint8_t i = 0; i < num_active_voices_; i++) {
        uint8_t v = active_voice_list_[i];
        Voice& voice = voices_[v];
        
        // ... render voice ...
    }
}
```

---

## 8. Summary: Expected CPU Reduction

| Strategy | Implementation | CPU Savings | Cumulative | Difficulty |
|----------|-----------------|------------|-----------|------------|
| **A. Max Voices 7→4** | 1 line change | 27% | 27% | ⭐ Trivial |
| **B. Active Voice List** | Voice tracking | 20% | 42% | ⭐⭐ Easy |
| **E. Dynamic Limiting** | Safety valve | 0% (prevents crash) | 42% | ⭐⭐ Easy |
| **C. Shared DCO** | Phase refactor | 40% | 77% | ⭐⭐⭐⭐ Hard |
| **D. Envelope Pool** | Storage remap | 15% | 85% | ⭐⭐⭐ Medium |
| **F. NEON SIMD** | Vectorization | 25% | >90% | ⭐⭐⭐⭐⭐ Very Hard |

---

## Conclusion

**The polyphonic mode breaks drumlogue because each voice processes independent DCO + envelope instances, multiplying CPU cost by 4-8×.**

**Immediate solution:** Implement Strategies A + B + E (3 days of work)
- **Reduces CPU by 42%** → brings 4-voice poly to safe ~85-90% of budget
- **Prevents crashes** via dynamic limiting

**Long-term solution:** Implement Strategies C + D (2 weeks)
- **Reduces CPU by 75-85% overall** → enables full 8-voice unison + effects

**Priority:** Implement Phase 1 immediately to stabilize polyphonic mode, then pursue Phase 2 for headroom.

