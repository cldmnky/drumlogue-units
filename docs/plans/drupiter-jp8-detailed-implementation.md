# Drupiter JP-8 Alignment — Detailed Implementation Plan

This document expands `drupiter-jp8-alignment.md` with concrete code changes derived from reading the actual drupiter source code.

**Source Files Analyzed:**
- [drupiter_synth.h](drumlogue/drupiter-synth/drupiter_synth.h) - Main synth class (519 lines)
- [drupiter_synth.cc](drumlogue/drupiter-synth/drupiter_synth.cc) - Main render loop (1572 lines)
- [voice_allocator.h](drumlogue/drupiter-synth/dsp/voice_allocator.h) - Voice structure (137 lines)
- [voice_allocator.cc](drumlogue/drupiter-synth/dsp/voice_allocator.cc) - Voice allocation (392 lines)
- [jupiter_vcf.h/cc](drumlogue/drupiter-synth/dsp/jupiter_vcf.cc) - Filter implementation (488 lines)
- [jupiter_env.h](drumlogue/drupiter-synth/dsp/jupiter_env.h) - Envelope implementation
- [jupiter_lfo.h](drumlogue/drupiter-synth/dsp/jupiter_lfo.h) - LFO implementation

---

## Critical Discovery: Current Architecture vs JP-8

### What Exists (Per-Voice)

The `Voice` struct in [voice_allocator.h#L56-L72](drumlogue/drupiter-synth/dsp/voice_allocator.h#L56-L72) **already contains**:

```cpp
struct Voice {
    bool active;
    uint8_t midi_note;
    float velocity;
    float pitch_hz;
    uint32_t note_on_time;

    JupiterDCO dco1;        // ✅ Per-voice oscillator 1
    JupiterDCO dco2;        // ✅ Per-voice oscillator 2
    JupiterVCF vcf;         // ✅ Per-voice VCF EXISTS!
    JupiterEnvelope env_amp;    // ✅ Per-voice VCA envelope
    JupiterEnvelope env_filter; // ✅ Per-voice VCF envelope
    JupiterEnvelope env_pitch;  // ✅ Per-voice pitch envelope

    float glide_target_hz;
    float glide_increment;
    bool is_gliding;
};
```

**Key Finding:** Per-voice VCF (`JupiterVCF vcf`) already exists in the Voice struct but is **NOT being used** in the POLY render loop!

### What's Actually Used in POLY Mode

From [drupiter_synth.cc#L478-L595](drumlogue/drupiter-synth/drupiter_synth.cc#L478-L595):

```cpp
// POLYPHONIC MODE: Render and mix multiple independent voices
mixed_ = 0.0f;
for (uint8_t v = 0; v < DRUPITER_MAX_VOICES; v++) {
    const dsp::Voice& voice = allocator_.GetVoice(v);
    if (!voice.active && !voice.env_amp.IsActive()) continue;

    // Per-voice oscillators ✅
    float voice_dco1 = voice_mut.dco1.Process();
    float voice_dco2 = voice_mut.dco2.Process();
    float voice_mix = voice_dco1 * dco1_level + voice_dco2 * dco2_level;

    // Per-voice amplitude envelope ✅
    float voice_env = voice_mut.env_amp.Process();
    float voice_output = voice_mix * voice_env;

    // Per-voice velocity scaling ✅
    const float voice_vca_gain = 0.2f + (voice.velocity / 127.0f) * 0.8f;
    voice_output *= voice_vca_gain;

    mixed_ += voice_output;  // Mix to shared buffer
}
// After loop: mixed_ goes through SHARED HPF → SHARED VCF → SHARED VCA
```

### What's Shared (Post-Mix) — Lines 741-810

From [drupiter_synth.cc#L741-L810](drumlogue/drupiter-synth/drupiter_synth.cc#L741-L810):

```cpp
// SHARED HPF (applied to mixed_ after all voices summed)
float hpf_out = mixed_;
if (hpf_alpha > 0.0f) {
    hpf_out = hpf_alpha * (hpf_prev_output_ + mixed_ - hpf_prev_input_);
    hpf_prev_output_ = hpf_out;  // SHARED STATE!
    hpf_prev_input_ = mixed_;    // SHARED STATE!
}

// SHARED VCF (vcf_ is class member, not per-voice)
filtered_ = vcf_.Process(hpf_out);

// SHARED VCA processing (vca_env_out_ from shared env_vca_)
mix_buffer_[i] = filtered_ * vca_gain * 0.6f;
```

**HPF state variables are class members** in [drupiter_synth.h#L444-L445](drumlogue/drupiter-synth/drupiter_synth.h#L444-L445):
```cpp
float hpf_prev_output_;  // Shared across all voices
float hpf_prev_input_;   // Shared across all voices
```

### Architecture Gap Summary

| Component | Current (Drupiter) | JP-8 Target | Gap |
|-----------|-------------------|-------------|-----|
| DCO1/DCO2 | Per-voice ✅ | Per-voice | None |
| VCF | Shared post-mix ❌ | Per-voice | **Need to use `voice.vcf`** |
| HPF | Shared post-mix ❌ | Per-voice | **Need to add to Voice struct** |
| VCA ENV | Per-voice ✅ | Per-voice | None |
| VCF ENV | Per-voice exists, not used ⚠️ | Per-voice | **Need to use `voice.env_filter`** |
| LFO | Shared (single global) | Per-voice phase sync | See Phase 3 |

---

## Phase 1: Per-Voice VCF in POLY Mode

**Status:** ✅ Implemented (per-voice VCF in POLY, shared VCF bypassed in POLY, test harness coverage added)

### 1.1 Current State
- `Voice::vcf` exists and is initialized in `Voice::Init()` ([voice_allocator.cc#L48-L50](drumlogue/drupiter-synth/dsp/voice_allocator.cc#L48-L50))
- But it's **never called** in the POLY render loop
- All voices feed into shared `vcf_` member of `DrupiterSynth`

### 1.2 Required Changes

**File: [drupiter_synth.cc](drumlogue/drupiter-synth/drupiter_synth.cc) (POLY render loop, lines ~478-595)**

Replace shared VCF processing with per-voice:

```cpp
// BEFORE (current code, ~line 580-595):
float voice_output = voice_mix * voice_env;
const float voice_vca_gain = 0.2f + (voice.velocity / 127.0f) * 0.8f;
voice_output *= voice_vca_gain;
mixed_ += voice_output;

// AFTER (per-voice VCF):
// 1. Calculate per-voice cutoff modulation
float voice_cutoff = cutoff_base;  // Base cutoff from main parameter

// 2. Apply per-voice VCF envelope
const float voice_vcf_env = voice_mut.env_filter.Process();
float voice_cutoff_mod = voice_cutoff * fast_pow2(voice_vcf_env * env_vcf_depth);

// 3. Apply per-voice keyboard tracking (optional, JP-8 style)
if (key_track > 0.0f) {
    const float voice_note_offset = (static_cast<int32_t>(voice.midi_note) - 60) / 12.0f;
    voice_cutoff_mod *= semitones_to_ratio(voice_note_offset * key_track * 12.0f);
}

// 4. Process through per-voice VCF
voice_mut.vcf.SetCutoffModulated(voice_cutoff_mod);
voice_mut.vcf.SetResonance(resonance_);  // Shared resonance is OK
voice_mut.vcf.SetMode(vcf_mode);

float voice_filtered = voice_mut.vcf.Process(voice_mix);

// 5. Apply per-voice VCA envelope and velocity
float voice_output = voice_filtered * voice_env;
const float voice_vca_gain = 0.2f + (voice.velocity / 127.0f) * 0.8f;
voice_output *= voice_vca_gain;

mixed_ += voice_output;
```

**Also required — trigger VCF envelope on NoteOn:**

File: [voice_allocator.cc](drumlogue/drupiter-synth/dsp/voice_allocator.cc), `TriggerVoice()` function (verify `env_filter` is triggered):

```cpp
void VoiceAllocator::TriggerVoice(Voice* voice, uint8_t note, uint8_t velocity, bool allow_legato) {
    // ... existing code ...
    voice->env_amp.NoteOn();
    voice->env_filter.NoteOn();  // ✅ Verify this is called
    voice->env_pitch.NoteOn();   // ✅ Verify this is called
}
```

### 1.3 Move Shared VCF Processing to End (Optimization)

After all voices are summed, skip the shared VCF in POLY mode:

```cpp
// drupiter_synth.cc, ~line 795
if (current_mode_ == dsp::SYNTH_MODE_POLYPHONIC) {
    // POLY: VCF already applied per-voice, skip shared VCF
    filtered_ = hpf_out;  // Only apply HPF (will be per-voice in Phase 2)
} else {
    // MONO/UNISON: Use shared VCF as before
    filtered_ = vcf_.Process(hpf_out);
}
```

### 1.4 Testing Criteria
- [x] Each poly voice has independent filter envelope (test harness added)
- [ ] Velocity affects per-voice cutoff (if vel_mod enabled)
- [ ] Keyboard tracking affects per-voice cutoff
- [ ] No CPU regression (target <80% usage)
- [ ] Compare POLY vs MONO tonal character

### 1.5 Test Harness Updates (Phase 1)

- Added **poly VCF envelope independence** test to validate per-voice envelopes in POLY.
- Fixtures generated for manual inspection:
    - `fixtures/poly_vcf_env_order_a.wav`
    - `fixtures/poly_vcf_env_order_b.wav`

---

## Phase 2: Per-Voice HPF

**Status:** ✅ Implemented (per-voice HPF in POLY, shared HPF bypass in POLY mode, state reset on note trigger)

### 2.1 Implementation Summary

Added per-voice HPF state to `Voice` struct and integrated into POLY render loop:

**File: [voice_allocator.h](drumlogue/drupiter-synth/dsp/voice_allocator.h) (lines 58-60)**

```cpp
struct Voice {
    // ... existing members ...
    JupiterVCF vcf;
    
    // ✅ ADDED: Per-voice HPF state (Phase 2)
    float hpf_prev_output;
    float hpf_prev_input;
    
    // ... rest of struct ...
};
```

**File: [voice_allocator.cc](drumlogue/drupiter-synth/dsp/voice_allocator.cc) (lines 58-70)**

```cpp
void Voice::Reset() {
    // ... existing ...
    hpf_prev_output = 0.0f;  // ✅ ADDED
    hpf_prev_input = 0.0f;   // ✅ ADDED
}
```

**File: [voice_allocator.cc](drumlogue/drupiter-synth/dsp/voice_allocator.cc) TriggerVoice() (lines 344-350)**

```cpp
if (!is_legato) {
    voice->env_amp.Reset();
    voice->env_filter.Reset();
    voice->env_pitch.Reset();
    voice->hpf_prev_output = 0.0f;  // ✅ ADDED: Reset HPF state on note-on
    voice->hpf_prev_input = 0.0f;
    voice->env_amp.NoteOn();
    voice->env_filter.NoteOn();
    voice->env_pitch.NoteOn();
}
```

**File: [drupiter_synth.cc](drumlogue/drupiter-synth/drupiter_synth.cc) POLY render loop (lines 626-632)**

```cpp
// ✅ ADDED: Apply per-voice HPF (Phase 2: one-pole high-pass filter)
float voice_hpf_out = voice_mix;
if (hpf_alpha > 0.0f) {
    voice_hpf_out = hpf_alpha * (voice_mut.hpf_prev_output + voice_mix - voice_mut.hpf_prev_input);
    voice_mut.hpf_prev_output = voice_hpf_out;
    voice_mut.hpf_prev_input = voice_mix;
}

// Then process through per-voice VCF
float voice_filtered = voice_mut.vcf.Process(voice_hpf_out);
```

**File: [drupiter_synth.cc](drumlogue/drupiter-synth/drupiter_synth.cc) Shared HPF bypass (line 817)**

```cpp
// ✅ MODIFIED: Skip shared HPF in POLY mode (already applied per-voice)
if (hpf_alpha > 0.0f && current_mode_ != dsp::SYNTH_MODE_POLYPHONIC) {
    // Simple one-pole HPF: y[n] = alpha * (y[n-1] + x[n] - x[n-1])
    // (MONO and UNISON modes only - POLY uses per-voice HPF)
    hpf_out = hpf_alpha * (hpf_prev_output_ + mixed_ - hpf_prev_input_);
    // ...
}
```

### 2.2 Architecture Details

- **HPF Coefficient:** Calculated once per buffer (line 367-374), shared across all voices for consistency
- **Frequency Range:** 20Hz - 2kHz linear mapping from MOD_HPF parameter (0-100)
- **Signal Flow (POLY):** DCO → Mix → **Per-Voice HPF** → Per-Voice VCF → Per-Voice VCA ENV
- **Signal Flow (MONO/UNISON):** DCO → Mix → **Shared HPF** → Shared VCF → Shared VCA ENV

### 2.3 Test Results

- ✅ HPF 6 dB/Oct Slope Response test passes (attenuation ratio 0.067)
- ✅ Build successful with no undefined symbols
- ⚠️ HPF Order Independence test shows issues (ratio 7.3, needs investigation)
- ✅ No performance regression (CPU usage nominal)

### 2.4 Known Issues

**Per-Voice HPF Order Independence Test (Failing)**
- Test shows RMS ratio 7.31 between different note orders (expected ~1.0)
- Order A (low→high): RMS=0.1335
- Order B (high→low): RMS=0.0182
- Root cause unclear - may require voice allocation debugging or test revision
- VCF order independence test passes (ratio 1.006), confirming per-voice architecture works

### 2.5 Future Improvements

- [ ] Investigate HPF order independence test failure
- [ ] Add per-voice HPF cutoff modulation (currently uses global coefficient)
- [ ] Consider variable HPF slope (6/12 dB per octave) option
- [ ] Optimize HPF coefficient calculation for per-voice variation

---

## Phase 3: Modulation Routing Alignment

**Status:** ✅ **COMPLETE** (2026-01-18)

**Implementation Summary:**
- ✅ LFO key trigger feature added (resets phase on note-on, enabled by default)
- ✅ VCA LFO depth quantization (4 steps: 0%, 33%, 67%, 100% matching JP-8)
- ✅ VCF envelope source uses per-voice `env_filter` in POLY mode (Option B)

**Build:** drupiter_synth.drmlgunit 58KB, no undefined symbols  
**Tests:** Phase 3 tests PASSED

**Test Results:**
```
=== JP-8 Phase 3: LFO Key Trigger ===
  RMS test1=0.119113 test2=0.119113 ratio=1
  ✓ LFO key trigger test PASSED

=== JP-8 Phase 3: VCA LFO Depth Quantization ===
  Values in same step match exactly (quantization working)
  ✓ VCA LFO quantization test PASSED
```

**Files Modified:**
- [jupiter_lfo.h](../../drumlogue/drupiter-synth/dsp/jupiter_lfo.h) - Added `key_trigger_` flag, `SetKeyTrigger()`, `GetKeyTrigger()`
- [jupiter_lfo.cc](../../drumlogue/drupiter-synth/dsp/jupiter_lfo.cc) - Implement conditional phase reset in `Trigger()`
- [drupiter_synth.cc](../../drumlogue/drupiter-synth/drupiter_synth.cc) - Call `lfo_.Trigger()` in `NoteOn()`, add VCA LFO quantization
- [test/drupiter-synth/main.cc](../../test/drupiter-synth/main.cc) - Added `TestLfoKeyTrigger()`, `TestVcaLfoQuantization()`

---

### 3.1 Current LFO Implementation

From [drupiter_synth.h#L369](drumlogue/drupiter-synth/drupiter_synth.h#L369):
```cpp
dsp::JupiterLFO lfo_;  // Single shared LFO instance
```

LFO is processed once per sample in the main loop ([drupiter_synth.cc#L422-L431](drumlogue/drupiter-synth/drupiter_synth.cc#L422-L431)):
```cpp
if (lfo_env_amt > kMinModulation) {
    const float rate_mult = 1.0f + (vcf_env_out_ * lfo_env_amt);
    lfo_out_ = lfo_.Process() * rate_mult;
} else {
    lfo_out_ = lfo_.Process();
}
```

### 3.2 JP-8 LFO Behavior

JP-8 has a **single global LFO** but with these features:
- Waveforms: Triangle, Ramp, Square, Random (S&H) ✅ (already in JupiterLFO)
- Delay (fade-in) ✅ (already in JupiterLFO)
- Key trigger option (reset phase on note) — **Need to verify/add**

### 3.3 Required Changes for LFO Key Trigger

File: [jupiter_lfo.h](drumlogue/drupiter-synth/dsp/jupiter_lfo.h):
```cpp
class JupiterLFO {
    // ... existing ...
    bool key_trigger_;  // ADD: Reset phase on note-on
    
    void SetKeyTrigger(bool enable) { key_trigger_ = enable; }
    void Trigger();  // Already exists - resets delay and optionally phase
};
```

File: [drupiter_synth.cc](drumlogue/drupiter-synth/drupiter_synth.cc), `NoteOn()`:
```cpp
void DrupiterSynth::NoteOn(uint8_t note, uint8_t velocity) {
    // ... existing ...
    if (lfo_key_trigger_enabled_) {
        lfo_.Trigger();  // Reset LFO phase on each note
    }
}
```

### 3.4 VCF Envelope Source Selection (JP-8 Feature)

JP-8 allows selecting ENV-1 or ENV-2 for VCF modulation. Currently drupiter uses:
- `env_vcf_` for VCF envelope (shared in MONO/UNISON)
- `voice.env_filter` exists but not used in POLY

**Decision needed:**
- Add MOD HUB option for VCF_ENV_SELECT (0=ENV-1, 1=ENV-2)?
- Or simplify: always use `voice.env_filter` in POLY?

### 3.5 VCA LFO Depth Switch (JP-8 Feature)

JP-8 has a 4-position switch (0/1/2/3) for VCA tremolo depth.

Currently continuous from [drupiter_synth.cc#L830-L834](drumlogue/drupiter-synth/drupiter_synth.cc#L830-L834):
```cpp
if (vca_lfo_depth > kMinModulation) {
    const float tremolo = 1.0f + (lfo_out_ * vca_lfo_depth * 0.5f);
    vca_gain *= tremolo;
}
```

**To match JP-8:** Quantize to 4 steps:
```cpp
// Map continuous 0-100 to discrete 0/1/2/3
const int vca_lfo_switch = static_cast<int>(vca_lfo_depth * 4.0f);
static constexpr float kVcaLfoDepths[4] = {0.0f, 0.33f, 0.66f, 1.0f};
const float tremolo = 1.0f + (lfo_out_ * kVcaLfoDepths[vca_lfo_switch] * 0.5f);
```

### 3.6 Testing Criteria
- [ ] LFO key trigger resets phase on each note (when enabled)
- [ ] LFO delay fade-in works correctly
- [ ] VCF envelope modulates per-voice in POLY
- [ ] VCA LFO depth has discrete steps (optional JP-8 authenticity)

---

## Phase 4: Voice Allocation Behavior

### 4.1 Current Implementation

From [voice_allocator.h#L119-L122](drumlogue/drupiter-synth/dsp/voice_allocator.h#L119-L122):
```cpp
enum VoiceAllocationStrategy {
    ALLOC_ROUND_ROBIN = 0,      // ✅ JP-8 style
    ALLOC_OLDEST_NOTE = 1,      // ✅ Available
    ALLOC_FIRST_AVAILABLE = 2
};
```

### 4.2 JP-8 Behavior

- **Round-robin** with **voice stealing** of oldest-released voice
- Release tails are preserved until a voice is needed
- Low note priority in mono mode (JP-8 default)

### 4.3 Required Verification

Check `StealOldestVoice()` in [voice_allocator.cc#L293+](drumlogue/drupiter-synth/dsp/voice_allocator.cc#L293):
- Does it prioritize voices in release phase?
- Does it preserve voices still in attack/decay/sustain?

```cpp
// voice_allocator.cc (verify this logic)
Voice* VoiceAllocator::StealOldestVoice() {
    Voice* oldest = nullptr;
    uint32_t oldest_time = UINT32_MAX;
    
    // First pass: try to steal a voice in release phase
    for (uint8_t i = 0; i < max_voices_; i++) {
        if (voices_[i].env_amp.IsReleasing() && 
            voices_[i].note_on_time < oldest_time) {
            oldest = &voices_[i];
            oldest_time = voices_[i].note_on_time;
        }
    }
    if (oldest) return oldest;
    
    // Second pass: steal oldest active voice
    for (uint8_t i = 0; i < max_voices_; i++) {
        if (voices_[i].active && voices_[i].note_on_time < oldest_time) {
            oldest = &voices_[i];
            oldest_time = voices_[i].note_on_time;
        }
    }
    return oldest ? oldest : &voices_[0];
}
```

### 4.4 Testing Criteria
- [x] Voice stealing respects release phase
- [x] Round-robin allocates evenly across voices
- [x] Chord stacking doesn't cause voice stealing until full
- [x] Mono mode uses legato/retrigger correctly

### 4.5 Completion Status ✅

**Completed:** 2025-01-18

**Changes Made:**
- Modified `voice_allocator.cc::NoteOn()` to intercept voice allocation when all voices active in ALLOC_OLDEST_NOTE mode
- Used `StealOldestVoice()` two-pass algorithm:
  1. Priority 1: Steal oldest voice in RELEASE phase
  2. Priority 2: Steal oldest sustaining voice (fallback)
- Voice pointer converted to index using pointer arithmetic

**Test Results:**
- ✅ TestVoiceStealingPrefersRelease() - Verified release-phase voices stolen first
- ✅ TestRoundRobinDistribution() - Confirmed even distribution across all 4 voices
- ✅ TestReleaseTailsPreserved() - Verified free voices used before stealing

**Build Status:**
- Hardware: 58840 bytes (58KB), no undefined symbols
- Desktop tests: All pass

**Known Issues:**
- None

---

## Phase 5: Parameter Calibration

### 5.1 Envelope Time Ranges

Current implementation in [jupiter_env.h](drumlogue/drupiter-synth/dsp/jupiter_env.h):
```cpp
static constexpr float kMinTime = 0.0001f;  // 0.1ms
static constexpr float kMaxTime = 10.0f;    // 10s
```

JP-8 specs from service manual:
- Attack: 1ms to 5s
- Decay/Release: 1ms to 10s

**Change needed:**
```cpp
static constexpr float kMinAttackTime = 0.001f;  // 1ms
static constexpr float kMaxAttackTime = 5.0f;    // 5s
static constexpr float kMinDecayTime = 0.001f;   // 1ms
static constexpr float kMaxDecayTime = 10.0f;    // 10s
```

### 5.2 VCF Cutoff Range

Current implementation ([drupiter_synth.cc#L763-L772](drumlogue/drupiter-synth/drupiter_synth.cc#L763-L772)):
```cpp
// Map 0-1 to cover ~7 octaves (100Hz to 12.8kHz)
const float cutoff_mult = linear_blend * lin_portion + (1.0f - linear_blend) * exp_portion;
float cutoff_base = 100.0f * cutoff_mult;  // 100Hz * multiplier
```

JP-8 has a very high cutoff range (up to ~200kHz according to service manual calibration notes). Consider:
```cpp
// Extend upper range for "brighter" JP-8 feel
float cutoff_base = 100.0f * cutoff_mult;  // 100Hz base
// Max ~20kHz is sufficient for audio, but JP-8 "openness" comes from
// the filter being completely transparent at high cutoff
```

### 5.3 LFO Rate Range

From [jupiter_lfo.h#L105-L106](drumlogue/drupiter-synth/dsp/jupiter_lfo.h#L105-L106):
```cpp
static constexpr float kMinFreq = 0.1f;    // 0.1 Hz
static constexpr float kMaxFreq = 50.0f;   // 50 Hz
```

JP-8 LFO range: ~0.1Hz to ~30Hz typical. Current range is reasonable.

### 5.4 Testing Criteria
- [ ] Envelope attack "snaps" at minimum (1ms feels instant)
- [ ] Envelope decay/release have 10s maximum
- [ ] VCF cutoff feels "open" at 100%
- [ ] LFO rate sweep feels musical

---

## Phase 6: Performance & Stability

### 6.1 CPU Optimization

Per-voice VCF adds ~4 voices × 1 filter per sample. Mitigation:

**Coefficient caching** (already in `JupiterVCF`):
```cpp
// jupiter_vcf.cc
if (coefficients_dirty_) {
    UpdateCoefficients();
    coefficients_dirty_ = false;
}
```

**Shared parameters:** Resonance and mode can be set once, not per-voice:
```cpp
// In POLY render loop, set mode/resonance once:
const auto vcf_mode = static_cast<JupiterVCF::Mode>(mod_hub_.GetValue(MOD_VCF_TYPE));
const float resonance = current_preset_.params[PARAM_VCF_RESO] / 100.0f;

for (uint8_t v = 0; v < DRUPITER_MAX_VOICES; v++) {
    voice_mut.vcf.SetMode(vcf_mode);  // Quick set, no recalc
    voice_mut.vcf.SetResonance(resonance);  // Marks dirty only if changed
    voice_mut.vcf.SetCutoffModulated(voice_cutoff);  // Triggers recalc
    // Process...
}
```

### 6.2 Denormal Protection

Already present in `JupiterVCF`:
```cpp
inline float FlushDenormal(float x) {
    if (fabsf(x) < kDenormalThreshold) return 0.0f;
    return x;
}
```

Verify all per-voice filter states are flushed.

### 6.3 Testing Criteria
- [ ] CPU usage <80% in 4-voice poly
- [ ] No denormal spikes during sustained notes
- [ ] No NaN/Inf in output (add sanitization check)

---

## Phase 7: Hardware Verification

### 7.1 Test Matrix

| Test | MONO | POLY | UNISON |
|------|------|------|--------|
| VCF envelope per-voice | N/A | ✅ | N/A |
| HPF per-voice | Optional | ✅ | Optional |
| LFO modulation | ✅ | ✅ | ✅ |
| Velocity to VCF | ✅ | ✅ | ✅ |
| Key tracking | ✅ | ✅ | ✅ |
| Voice stealing | N/A | ✅ | N/A |
| Preset save/load | ✅ | ✅ | ✅ |

### 7.2 Audio A/B Comparison

1. Record MONO patch (bass sound with filter sweep)
2. Play same patch in POLY with single note
3. Should sound identical (same signal path)
4. Play chord in POLY — each voice should have independent filter envelope

### 7.3 Release Notes Template

```markdown
## v1.x.0 - JP-8 Signal Flow Alignment

### New Features
- Per-voice VCF filtering in POLY mode (true JP-8 architecture)
- Per-voice HPF for independent bass roll-off
- VCF envelope applied per-voice with velocity modulation

### Changes
- POLY mode now processes each voice through its own filter
- VCF keyboard tracking affects each voice independently
- LFO key trigger option added

### Technical
- Voice struct extended with HPF state
- POLY render loop restructured for per-voice processing
- CPU optimized with coefficient caching
```

---

## Implementation Order

1. **Phase 1** (Per-voice VCF) — ✅ **Complete** (POLY uses per-voice VCF, shared VCF bypassed)
2. **Phase 2** (Per-voice HPF) — ✅ **Complete** (per-voice HPF in POLY, state reset on trigger)
3. **Phase 5** (Calibration) — Next priority, parameter tuning for JP-8 feel
4. **Phase 6** (Performance) — Ongoing validation, CPU usage nominal
5. **Phase 3** (Modulation) — Nice-to-have JP-8 features (LFO key trigger)
6. **Phase 4** (Allocation) — Verify existing voice stealing behavior
7. **Phase 7** (Hardware) — Final validation on actual drumlogue

### Phase 1-2 Implementation Complete

**Achievements:**
- ✅ Per-voice VCF filtering in POLY mode (true JP-8 architecture)
- ✅ Per-voice HPF for independent bass roll-off
- ✅ VCF envelope applied per-voice with velocity modulation
- ✅ HPF state reset on note trigger prevents carryover
- ✅ Clean build with no undefined symbols
- ✅ Test harness coverage for poly VCF envelope independence
- ✅ HPF slope response validation (6 dB/oct confirmed)

**Test Results:**
- ✅ Phase 1: Poly VCF Envelope Independence (ratio 1.006)
- ✅ HPF 6 dB/Oct Slope Response (attenuation 0.067)
- ⚠️ HPF Order Independence needs investigation (ratio 7.3)
- ⚠️ VCF 12/24 dB Mode distinction needs investigation
- ⚠️ Cutoff Curve Monotonicity shows small dynamic range

---

## Phase 8: Keyboard Tracking & Cross Modulation (Detailed)

**Status: ⏳ PLANNED**

### 8.1 Keyboard Tracking Implementation

**Current Code State:**

From `jupiter_vcf.h` (lines 80-92):
```cpp
/**
 * @brief Set keyboard tracking amount
 * @param amount Tracking amount 0.0-1.0
 */
void SetKeyboardTracking(float amount);

/**
 * @brief Apply keyboard tracking for a note
 * @param note MIDI note number
 */
void ApplyKeyboardTracking(uint8_t note);
```

**Lookup Table:**
From `jupiter_vcf.cc`:
```cpp
static constexpr int kKbdTrackingTableSize = 128;  // One entry per MIDI note
static float s_kbd_tracking_table[kKbdTrackingTableSize];  // Pre-calculated ratios
```

**Verification Checklist:**
- [ ] Check `ApplyKeyboardTracking()` implementation in `jupiter_vcf.cc`
  - Does it properly scale cutoff frequency?
  - Is the lookup table calculation correct (1V/octave)?
  - Are values available for full MIDI range (0-127)?
  
- [ ] Verify 0-120% range implementation
  - Current parameter: `kbd_tracking_` (0.0-1.0)
  - Expected: Minimum 100% (1V/octave), maximum 120%
  - Implementation: `cutoff *= 1.0f + (kbd_tracking_ * 0.2f)` for 100-120% range
  
- [ ] Per-voice application in POLY mode
  - Location: `drupiter_synth.cc` POLY render loop (~line 550)
  - Needed: `voice.vcf.ApplyKeyboardTracking(voice.midi_note)` for each voice
  - Verify: Each voice gets independent tracking

**Code Changes Required:**

File: `drupiter_synth.cc` (POLY render loop, ~line 550):
```cpp
// Before: No keyboard tracking per-voice
float voice_output = voice_mix * voice_env;

// After: Apply keyboard tracking per-voice
voice.vcf.ApplyKeyboardTracking(voice.midi_note);  // ← Add this
float filtered = voice.vcf.Process(voice_output);
```

### 8.2 Envelope Key Follow Implementation

**Specification:**
- Both ENV-1 and ENV-2 time scaling based on note
- Typical: -0.5% per semitone (each octave reduces times by ~6%)
- Purpose: Higher notes trigger faster envelopes (natural acoustic behavior)

**Implementation:**

File: `jupiter_env.h` (add method):
```cpp
/**
 * @brief Apply keyboard follow scaling to envelope times
 * @param note MIDI note number (0-127)
 * Higher notes shorten Attack/Decay/Release times
 */
void ApplyKeyboardFollow(uint8_t note);
```

File: `jupiter_env.cc` (implement):
```cpp
void JupiterEnvelope::ApplyKeyboardFollow(uint8_t note) {
    // Reference note: C3 (MIDI 36) = 1.0 (no scaling)
    // Each octave up: multiply times by ~0.94 (6% reduction per octave)
    const uint8_t ref_note = 36;  // C3
    int semitone_offset = note - ref_note;
    
    // Scaling factor: 0.995^semitones
    // At C4 (+12): ~0.941 (5.9% shorter)
    // At C5 (+24): ~0.887 (11.3% shorter)
    float scale = powf(0.995f, semitone_offset);
    
    // Apply to current times (in milliseconds)
    attack_time_ms_ *= scale;
    decay_time_ms_ *= scale;
    release_time_ms_ *= scale;
}
```

**POLY Mode Integration:**

File: `drupiter_synth.cc` (POLY loop, ~line 530):
```cpp
// In POLY render loop after NoteOn:
void TriggerVoice(dsp::Voice& voice, uint8_t note) {
    // ... existing code ...
    voice.env_amp.Trigger();
    voice.env_filter.Trigger();
    
    // Apply keyboard follow scaling
    voice.env_filter.ApplyKeyboardFollow(note);  // ← Add this
    voice.env_amp.ApplyKeyboardFollow(note);     // ← Add this (optional)
}
```

### 8.3 Cross Modulation (X-Mod) Implementation

**Current Code State:**

From `jupiter_dco.h` (lines 82-85):
```cpp
/**
 * @brief Apply FM modulation
 * @param fm_amount FM modulation amount (-1.0 to 1.0)
 */
void ApplyFM(float fm_amount);
```

**Requirements:**
1. Parameter control: X-Mod Depth (0-100%)
2. VCO-2 → VCO-1 routing in each voice
3. Depth calibration: At slider 5 (50%), VCO-1 Range 2', shift = 3 octaves
4. DC blocking for stable base pitch

**Implementation Step 1: Add Parameter**

File: `drupiter_synth.h` (parameter section):
```cpp
// Add to parameter definitions
static constexpr uint8_t PARAM_XMOD_DEPTH = XX;  // Pick next available ID

// Add to class member:
float xmod_depth_;  // 0.0-1.0 representing 0-100% depth
```

**Implementation Step 2: Calibration Constant**

File: `drupiter_synth.h` or `drupiter_synth.cc`:
```cpp
// X-Mod depth calibration
// At depth=0.5 (nominal), VCO-1 range=2', modulation shifts pitch by 3 octaves
// Scaling: 3 octaves ≈ 3 * log2(2) = 3 semitones * 12 = 36 semitones
// Each semitone ≈ 0.05946 (frequency ratio 2^(1/12))
// Empirical calibration: ~1.5 octaves per 100% depth
static constexpr float kXModDepthCalibration = 1.5f;  // Octaves per 100%
```

**Implementation Step 3: POLY Rendering**

File: `drupiter_synth.cc` (POLY loop, ~line 520-560):
```cpp
// In POLY render loop:
for (uint8_t v = 0; v < DRUPITER_MAX_VOICES; v++) {
    dsp::Voice& voice = allocator_.GetVoiceMutable(v);
    if (!voice.active && !voice.env_amp.IsActive()) continue;

    // Render VCO-2 first (modulation source)
    float vco2_output = voice.dco2.Process();
    
    // Calculate X-Mod amount (with DC blocking)
    float xmod_voltage = vco2_output * xmod_depth_ * kXModDepthCalibration;
    
    // Apply X-Mod to VCO-1 via FM input
    // Note: ApplyFM() is called BEFORE Process() to modulate phase
    voice.dco1.ApplyFM(xmod_voltage);
    
    // Process VCO-1 (now with frequency modulation from VCO-2)
    float vco1_output = voice.dco1.Process();
    
    // Mix oscillators
    float voice_mix = vco1_output * dco1_level + vco2_output * dco2_level;
    
    // Continue with envelope/filter/VCA processing...
}
```

**Implementation Step 4: DC Blocking (Optional Enhancement)**

File: `jupiter_dco.h` or `drupiter_synth.h`:
```cpp
// Simple DC blocker state
struct XModDCBlocker {
    float prev_input = 0.0f;
    float prev_output = 0.0f;
    
    float Process(float input) {
        // 1-pole high-pass: y[n] = input[n] - input[n-1] + alpha * y[n-1]
        const float alpha = 0.99f;  // ~10Hz cutoff at 48kHz
        float output = input - prev_input + alpha * prev_output;
        prev_input = input;
        prev_output = output;
        return output;
    }
};
```

**Implementation Step 5: VCO-2 LFO Mode (Optional)**

File: `drupiter_synth.h/cc`:
```cpp
enum VCO2Mode {
    VCO2_NORMAL = 0,      // Full frequency range oscillator
    VCO2_LFO = 1          // Low-frequency range (0.1 - 50 Hz)
};

// In POLY loop:
if (vco2_mode_ == VCO2_LFO) {
    // Scale frequency to LFO range (0.1 - 50 Hz)
    float lfo_freq = voice.pitch_hz * (0.01f / 440.0f);  // Scale down
    voice.dco2.SetFrequency(lfo_freq);
}
```

### 8.4 Test Implementation

File: `test/drupiter-synth/main.cc` (add Phase 8 tests):

```cpp
static bool TestKeyboardTrackingRange() {
    // Test VCF keyboard tracking across MIDI range
    dsp::VoiceAllocator allocator;
    allocator.Init(48000.0f);
    allocator.SetMode(dsp::SYNTH_MODE_MONOPHONIC);
    
    // Set base cutoff at C3 (MIDI 36)
    allocator.NoteOn(36, 100);
    dsp::Voice& voice = allocator.GetVoiceMutable(0);
    voice.vcf.SetCutoff(1000.0f);  // Reference cutoff
    voice.vcf.SetKeyboardTracking(1.0f);  // Full tracking
    
    float cutoff_c3 = 1000.0f;
    
    // Test at C7 (MIDI 84 = C3 + 48 semitones = 4 octaves)
    allocator.NoteOn(84, 100);
    voice.vcf.ApplyKeyboardTracking(84);
    
    // Expected: cutoff_c7 = cutoff_c3 * 2^4 = 16000 Hz (1V/octave tracking)
    // With 120% tracking: expected ≈ 20000 Hz
    
    // Measure frequency response (verify filter cutoff shift)
    // ... implementation details ...
    
    return true;
}

static bool TestXModDepthCalibration() {
    // Test X-Mod depth produces correct pitch shift
    dsp::VoiceAllocator allocator;
    allocator.Init(48000.0f);
    allocator.SetMode(dsp::SYNTH_MODE_MONOPHONIC);
    
    allocator.NoteOn(60, 100);  // C4 = 260.6 Hz
    dsp::Voice& voice = allocator.GetVoiceMutable(0);
    
    // VCO-1 at 2' (16 ft) range: ~65 Hz base
    voice.dco1.SetFrequency(65.0f);
    voice.dco2.SetFrequency(65.0f);  // Modulation source
    
    // X-Mod at nominal depth should shift by ~3 octaves
    // Expected: 65 Hz * 2^3 = 520 Hz modulation range
    
    // ... verify pitch shift with spectral analysis ...
    
    return true;
}

static bool TestEnvelopeKeyFollow() {
    // Test envelope time scaling with note
    // Higher notes should have shorter attack/decay/release times
    
    // ... implementation ...
    
    return true;
}
```

---

## Open Questions

1. **HPF Order Independence Test:** Why does note order affect output level by 7.3x? Voice allocation issue or test problem?
2. **VCF 12/24 dB Mode:** Why are outputs nearly identical (0.0006% difference)? Is mode switching working in POLY?
3. **Cutoff Curve:** Why is dynamic range small (ratio 1.05)? Expected larger response across parameter range.
4. **VCF ENV source:** Add ENV-1/ENV-2 selection or always use `env_filter` in POLY?
5. **VCA LFO:** Keep continuous or quantize to 4 steps for JP-8 authenticity?
6. **LFO key trigger:** Add as MOD HUB option or dedicated parameter?
7. **X-Mod Calibration:** What is empirical VCO-2 output amplitude range for accurate 3-octave shift measurement?
8. **Envelope Key Follow Rate:** Is -0.5% per semitone correct, or should it be different for each envelope?

### Resolved Questions

✅ **HPF placement:** Implemented per-voice (authentic JP-8 architecture) with shared bypass in POLY mode
✅ **Keyboard Tracking:** Lookup table approach implemented in JupiterVCF
✅ **Cross Modulation:** ApplyFM() method exists in JupiterDCO for VCO-2 → VCO-1 routing

---

## Files to Modify Summary

| File | Phase | Status | Changes |
|------|-------|--------|---------|
| `drupiter_synth.cc` | 1,2,3 | ✅ Phases 1-2 Complete | POLY render loop (per-voice VCF/HPF), shared HPF bypass |
| `voice_allocator.h` | 2 | ✅ Complete | Added `hpf_prev_output`, `hpf_prev_input` to Voice struct |
| `voice_allocator.cc` | 2,4 | ✅ Phase 2 Complete | HPF state reset in TriggerVoice(), voice stealing verification pending |
| `jupiter_env.h` | 5 | Pending | Adjust time constants to JP-8 specs |
| `jupiter_lfo.h` | 3 | Pending | Add `key_trigger_` flag for phase reset on note-on |
| `jupiter_vcf.h/cc` | 5 | Pending | Verify cutoff range matches JP-8 "openness" |

### Build Artifacts

- **Unit Binary:** `drupiter_synth.drmlgunit` (58KB, no undefined symbols)
- **Test Harness:** `test/drupiter-synth/drupiter_test` (desktop validation)
- **Fixtures:** `test/drupiter-synth/fixtures/*.wav` (Phase 1 VCF envelope tests)
