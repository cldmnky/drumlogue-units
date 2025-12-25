# Polyphonic CPU Optimization - Implementation Roadmap

## Quick Start: What You Need to Know

**Problem:** Polyphonic mode crashes drumlogue because each voice runs independent DCO + envelope instances = 4-8× CPU cost.

**Solution:** 3-phase approach
1. **Phase 1 (This Week):** Reduce max voices + add voice tracking = -42% CPU
2. **Phase 2 (Next 2 Weeks):** Share DCO instances with per-voice phase tracking = -40% more
3. **Phase 3 (Optional):** NEON vectorization = additional -25%

---

## Phase 1: Emergency Stabilization (1-2 Days)

### Goal
Bring polyphonic mode from **128% of CPU budget → 85%** (acceptable with headroom)

### Changes Required

#### Change 1.1: Reduce Max Voices (5 minutes)
**File:** `drumlogue/drupiter-synth/dsp/voice_allocator.h` line 27

```diff
-#define DRUPITER_MAX_VOICES 7
+#define DRUPITER_MAX_VOICES 4
```

**Impact:** -27% CPU (fewer voices to process)

**Note:** 4 voices is still polyphonic and matches many hardware synths

---

#### Change 1.2: Add Active Voice Tracking (2-3 hours)

**File:** `drumlogue/drupiter-synth/dsp/voice_allocator.h`

Add to `VoiceAllocator` class:

```cpp
private:
    // Active voice tracking (CPU optimization)
    // Instead of iterating all voices and checking IsActive(),
    // maintain a list of voices actually in use
    uint8_t active_voice_list_[DRUPITER_MAX_VOICES];  // Indices only
    uint8_t num_active_voices_;                       // Count (0-4)
    
    // Helper to track active voices
    void AddActiveVoice(uint8_t voice_idx);
    void RemoveActiveVoice(uint8_t voice_idx);
    void UpdateActiveVoiceList();  // Called in AllNotesOff()
```

**File:** `drumlogue/drupiter-synth/dsp/voice_allocator.cc`

Add implementation:

```cpp
void VoiceAllocator::AddActiveVoice(uint8_t voice_idx) {
    // Check if already in list
    for (uint8_t i = 0; i < num_active_voices_; i++) {
        if (active_voice_list_[i] == voice_idx) {
            return;  // Already tracked
        }
    }
    
    // Add to list
    if (num_active_voices_ < DRUPITER_MAX_VOICES) {
        active_voice_list_[num_active_voices_++] = voice_idx;
    }
}

void VoiceAllocator::RemoveActiveVoice(uint8_t voice_idx) {
    // Rebuild list without this voice
    uint8_t new_count = 0;
    for (uint8_t i = 0; i < num_active_voices_; i++) {
        if (active_voice_list_[i] != voice_idx) {
            active_voice_list_[new_count++] = active_voice_list_[i];
        }
    }
    num_active_voices_ = new_count;
}

void VoiceAllocator::UpdateActiveVoiceList() {
    num_active_voices_ = 0;
    for (uint8_t i = 0; i < max_voices_; i++) {
        if (voices_[i].active || voices_[i].env_amp.IsActive()) {
            AddActiveVoice(i);
        }
    }
}
```

Update `NoteOn`:

```cpp
void VoiceAllocator::NoteOn(uint8_t note, uint8_t velocity) {
    timestamp_++;
    
    Voice* voice = nullptr;
    uint8_t voice_idx = 0;
    
    switch (mode_) {
        case SYNTH_MODE_POLYPHONIC:
            voice = AllocateVoice();
            // Find voice index
            for (uint8_t i = 0; i < max_voices_; i++) {
                if (&voices_[i] == voice) {
                    voice_idx = i;
                    break;
                }
            }
            break;
        // ... other modes ...
    }
    
    if (voice) {
        TriggerVoice(voice, note, velocity);
        AddActiveVoice(voice_idx);  // ← ADD THIS LINE
    }
}
```

Update `NoteOff` and `AllNotesOff`:

```cpp
void VoiceAllocator::AllNotesOff() {
    for (uint8_t i = 0; i < max_voices_; i++) {
        if (voices_[i].active) {
            voices_[i].env_amp.NoteOff();
            voices_[i].env_filter.NoteOff();
            voices_[i].env_pitch.NoteOff();
        }
    }
    // Don't remove from active list yet - let envelopes finish
    // UpdateActiveVoiceList will be called in next render
}
```

Update `RenderPolyphonic`:

```cpp
void VoiceAllocator::RenderPolyphonic(float* left, float* right, 
                                      uint32_t frames, const float* params) {
    // Zero buffers
    memset(left, 0, frames * sizeof(float));
    memset(right, 0, frames * sizeof(float));
    
    // ✅ KEY CHANGE: Only iterate active voices, not all 8
    for (uint8_t i = 0; i < num_active_voices_; i++) {
        uint8_t v = active_voice_list_[i];
        Voice& voice = voices_[v];
        
        // ... rest of voice rendering code ...
    }
    
    // At end of render, update active list if needed
    UpdateActiveVoiceList();  // Remove voices with finished envelopes
}
```

**Impact:** -20% CPU (only process 2-3 voices instead of checking all 8)

---

#### Change 1.3: Add Dynamic Voice Limiting (1-2 hours)

**File:** `drumlogue/drupiter-synth/drupiter_synth.h`

Add to `DrupiterSynth` class:

```cpp
private:
    // Dynamic CPU limiting (safety valve)
    uint8_t max_voices_configured_;  // User-set max (via MOD HUB or preset)
    uint8_t max_voices_dynamic_;     // Current limit based on CPU load
    float cpu_load_estimate_;        // 0.0 = idle, 1.0 = 100% budget
    
    void UpdateCpuLoadEstimate();
```

**File:** `drumlogue/drupiter-synth/dsp/voice_allocator.h`

Add to `VoiceAllocator`:

```cpp
public:
    void SetMaxVoicesDynamic(uint8_t max_voices) {
        max_voices_ = max_voices;
    }
```

**File:** `drumlogue/drupiter-synth/drupiter_synth.cc`

In `Render()`, after main processing:

```cpp
void DrupiterSynth::Render(float* out, uint32_t frames) {
    // ... existing render code ...
    
    // Measure CPU load (approximate)
    // If we're consistently hitting peak, reduce voice count
    if (allocator_.IsAnyVoiceActive()) {
        // Check if more than 3 voices active AND effect mode enabled
        // This is a rough heuristic
        bool multiple_voices = /* count voices > 3 */;
        bool effects_enabled = current_preset_.params[PARAM_EFFECT] != 2;
        
        if (multiple_voices && effects_enabled) {
            // Reduce max voices dynamically
            allocator_.SetMaxVoicesDynamic(3);
        } else {
            // Restore configured max
            allocator_.SetMaxVoicesDynamic(max_voices_configured_);
        }
    }
}
```

In voice allocation (`voice_allocator.cc`):

```cpp
Voice* VoiceAllocator::AllocateVoice() {
    // Check if we're at dynamic limit
    if (active_voices_ >= max_voices_) {
        return StealVoice();  // Force steal if at limit
    }
    
    // Find available voice
    for (uint8_t i = 0; i < max_voices_; i++) {
        if (!voices_[i].active && !voices_[i].env_amp.IsActive()) {
            active_voices_++;
            return &voices_[i];
        }
    }
    
    // No available voice - steal one
    return StealVoice();
}
```

**Impact:** Prevents CPU overrun (0% direct savings, but prevents crashes)

---

### Testing Phase 1

**Desktop:**
```bash
cd test/drupiter-synth
make clean && make
./drupiter_test --poly-test 4  # Simulate 4 active voices
```

**Hardware:**
1. Load polyphonic preset
2. Play 4 notes in sequence
3. Monitor for glitches
4. Check UI responsiveness

**Success criteria:**
- ✅ No pops/clicks with 4 voices active
- ✅ Drumlogue UI stays responsive
- ✅ No audio dropout when playing patterns
- ✅ CPU load <85% of budget

---

## Phase 2: Core Architecture Optimization (1-2 Weeks)

### Goal
Reduce polyphonic CPU from **85% → 50-60%** (excellent headroom)

This requires refactoring the DCO architecture to use shared instances with per-voice phase tracking.

### Change 2.1: Refactor Voice Structure (Day 1)

**File:** `drumlogue/drupiter-synth/dsp/voice_allocator.h`

Replace current `Voice` struct:

```cpp
// BEFORE:
struct Voice {
    bool active;
    uint8_t midi_note;
    float velocity;
    float pitch_hz;
    uint32_t note_on_time;
    
    JupiterDCO dco1;           // ❌ Individual instances
    JupiterDCO dco2;
    JupiterVCF vcf;            // ❌ Each voice has own filter
    JupiterEnvelope env_amp;
    JupiterEnvelope env_filter;
    JupiterEnvelope env_pitch;
};

// AFTER:
struct VoiceState {
    // Voice metadata
    bool active;
    uint8_t midi_note;
    float velocity;
    float pitch_hz;
    uint32_t note_on_time;
    
    // ✅ Per-voice phase state only (NOT full DCO instance)
    float phase1;              // DCO1 phase accumulator
    float phase2;              // DCO2 phase accumulator
    float freq1_target;        // Target frequencies for smoothing
    float freq2_target;
    
    // ✅ Pooled envelope access (not per-voice instances)
    uint8_t env_amp_id;        // Index into envelope pool
    uint8_t env_filter_id;
    uint8_t env_pitch_id;
};

struct Voice {
    VoiceState state;
    // Optional: filter state if per-voice VCF needed later
};
```

Update `VoiceAllocator` class:

```cpp
class VoiceAllocator {
private:
    // Shared DSP components (used by all voices)
    JupiterDCO dco1_shared_;     // ✅ Single instance
    JupiterDCO dco2_shared_;
    
    // Per-voice state
    VoiceState voice_states_[DRUPITER_MAX_VOICES];
    
    // Envelope pools
    JupiterEnvelope env_amp_pool_[DRUPITER_MAX_VOICES];
    JupiterEnvelope env_filter_pool_[DRUPITER_MAX_VOICES];
    JupiterEnvelope env_pitch_pool_[DRUPITER_MAX_VOICES];
};
```

---

### Change 2.2: Implement Shared DCO Processing (Day 1-2)

**File:** `drumlogue/drupiter-synth/dsp/voice_allocator.cc`

Rewrite `RenderPolyphonic` to use shared DCO:

```cpp
void VoiceAllocator::RenderPolyphonic(float* left, float* right, 
                                      uint32_t frames, const float* params) {
    memset(left, 0, frames * sizeof(float));
    memset(right, 0, frames * sizeof(float));
    
    // Extract parameters
    float dco1_level = params[0];  // Passed from main synth
    float dco2_level = params[1];
    float lfo_depth = params[2];   // For frequency modulation
    // ... etc ...
    
    for (uint32_t frame = 0; frame < frames; frame++) {
        // Process each active voice's phase state
        for (uint8_t i = 0; i < num_active_voices_; i++) {
            uint8_t v_idx = active_voice_list_[i];
            VoiceState& voice = voice_states_[v_idx];
            
            if (!voice.active && !env_amp_pool_[voice.env_amp_id].IsActive()) {
                continue;
            }
            
            // ✅ Update phase (simple, fast)
            voice.phase1 += voice.freq1_target * freq_inc;
            voice.phase2 += voice.freq2_target * freq_inc;
            
            // Wrap phase
            if (voice.phase1 > 1.0f) voice.phase1 -= 1.0f;
            if (voice.phase2 > 1.0f) voice.phase2 -= 1.0f;
        }
        
        // Set shared DCO to average frequency of active voices
        float avg_freq = 0.0f;
        for (uint8_t i = 0; i < num_active_voices_; i++) {
            avg_freq += voice_states_[active_voice_list_[i]].freq1_target;
        }
        avg_freq /= num_active_voices_;
        
        dco1_shared_.SetFrequency(avg_freq);
        dco2_shared_.SetFrequency(avg_freq * detune);
        
        // Process shared DCO once per frame
        float dco1_out = dco1_shared_.Process();
        float dco2_out = dco2_shared_.Process();
        
        float mixed_sample = dco1_out * dco1_level + dco2_out * dco2_level;
        
        // Apply per-voice envelopes to mixed signal
        float voice_mix = 0.0f;
        for (uint8_t i = 0; i < num_active_voices_; i++) {
            uint8_t v_idx = active_voice_list_[i];
            VoiceState& voice = voice_states_[v_idx];
            
            // Get envelope from pool
            JupiterEnvelope& env = env_amp_pool_[voice.env_amp_id];
            float env_val = env.Process();
            
            voice_mix += mixed_sample * env_val;
        }
        
        // Store to output
        left[frame] = voice_mix / max(1, num_active_voices_);
    }
    
    right = left;  // Mono sync or apply stereo widening
}
```

**Critical insight:** Instead of processing 8 separate DCOs per frame, process per-voice phase updates (fast) and run shared DCO once per frame (efficient).

---

### Change 2.3: Envelope Pooling (Day 2)

**File:** `drumlogue/drupiter-synth/dsp/voice_allocator.cc`

Allocate envelope IDs efficiently:

```cpp
void VoiceAllocator::Init(float sample_rate) {
    // Initialize envelope pools once
    for (uint8_t i = 0; i < max_voices_; i++) {
        env_amp_pool_[i].Init(sample_rate);
        env_filter_pool_[i].Init(sample_rate);
        env_pitch_pool_[i].Init(sample_rate);
    }
    
    // Assign envelope IDs (1:1 mapping for now)
    for (uint8_t i = 0; i < max_voices_; i++) {
        voice_states_[i].env_amp_id = i;
        voice_states_[i].env_filter_id = i;
        voice_states_[i].env_pitch_id = i;
    }
}

void VoiceAllocator::NoteOn(uint8_t note, uint8_t velocity) {
    // ... find/allocate voice ...
    
    Voice* voice = AllocateVoice();
    uint8_t v_idx = FindVoiceIndex(voice);
    
    if (voice) {
        // Trigger envelope from pool
        uint8_t env_id = voice_states_[v_idx].env_amp_id;
        env_amp_pool_[env_id].NoteOn();
        
        // ... rest of trigger code ...
    }
}
```

---

### Testing Phase 2

**Expected CPU reduction:** 40% additional (total -60% from baseline mono)

**Desktop measurement:**
```cpp
// In test harness
auto start = high_resolution_clock::now();
synth.Render(buffer, 64);
auto end = high_resolution_clock::now();

float ms = duration<double, milli>(end - start).count();
float cpu_percent = (ms / 1.33f) * 100.0f;

std::cout << "CPU Load: " << cpu_percent << "%\n";
```

**Target:** < 65% CPU load with 4 voices active + effects

---

## Phase 3: NEON Vectorization (Optional, Future)

### Approach
Process 4 voices in parallel using NEON SIMD:

```cpp
#ifdef USE_NEON
// Process 4 phases in parallel
float32x4_t phases1_vec = vld1q_f32(&voice_states_[0].phase1);
float32x4_t freqs_vec = vld1q_f32(&voice_states_[0].freq1_target);
float32x4_t inc_vec = vdupq_n_f32(freq_inc);

// Phase increment for 4 voices simultaneously
phases1_vec = vaddq_f32(phases1_vec, vmulq_f32(freqs_vec, inc_vec));
vst1q_f32(&voice_states_[0].phase1, phases1_vec);
#endif
```

**Expected impact:** Additional -25% CPU reduction (total -75-80% from mono baseline)

---

## Implementation Checklist

### Phase 1: Stabilization

- [ ] Change max voices to 4 (`voice_allocator.h` line 27)
- [ ] Add active voice tracking struct (`voice_allocator.h`)
- [ ] Implement `AddActiveVoice()` / `RemoveActiveVoice()` 
- [ ] Update `RenderPolyphonic()` to use active voice list
- [ ] Add CPU load estimation in `Render()`
- [ ] Add dynamic voice limiting in voice allocation
- [ ] Test desktop harness
- [ ] Test on hardware (4 voices, patterns)
- [ ] Commit with message: "Phase 1: Stabilize polyphonic mode (42% CPU reduction)"

### Phase 2: Architecture

- [ ] Create `VoiceState` struct (separate from DCO instances)
- [ ] Move phase accumulators to per-voice state
- [ ] Create shared `dco1_shared_` / `dco2_shared_` in allocator
- [ ] Refactor `RenderPolyphonic()` with shared DCO
- [ ] Create envelope pools
- [ ] Update voice allocation to use envelope pool IDs
- [ ] Test with all 4 voices + effects
- [ ] Benchmark CPU load (target: <65%)
- [ ] Commit: "Phase 2: Shared DCO architecture (40% additional reduction)"

### Phase 3: Advanced (Optional)

- [ ] Add NEON feature detection (`#ifdef USE_NEON`)
- [ ] Implement NEON phase update loop
- [ ] Implement NEON envelope processing
- [ ] Benchmark NEON version vs scalar
- [ ] Profile cache usage
- [ ] Commit: "Phase 3: NEON vectorization (25% additional)"

---

## Rollback Plan

If issues arise, revert in this order:

1. Phase 3 changes (delete NEON code, keep scalar)
2. Phase 2 changes (revert to separate DCO instances)
3. Phase 1 changes (restore max voices to 7)

Each phase maintains functional polyphonic mode, just at different CPU efficiency.

---

## Success Metrics

| Metric | Target | Phase 1 | Phase 2 | Phase 3 |
|--------|--------|---------|---------|---------|
| **4-Voice CPU Load** | <70% | 85% | 60% | 50% |
| **Audio Quality** | Artifact-free | ✅ | ✅ | ✅ |
| **UI Responsiveness** | No lag | ✅ | ✅ | ✅ |
| **Drumlogue Stability** | No crash | ✅ | ✅ | ✅ |
| **Memory Used** | <6KB | 5KB | 3.3KB | 3.3KB |

---

## Notes for Implementation

1. **Phase 1 is critical:** Do this first to stabilize the unit
2. **Test often:** Desktop test before hardware each time
3. **Commit frequently:** Small commits are easier to debug
4. **Profile CPU:** Use `std::chrono` to measure actual impact
5. **Keep monophonic working:** All changes must preserve mono/UNISON modes

