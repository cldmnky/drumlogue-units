# Drupiter Synthesis Modes: Implementation Guide

**Document**: SYNTHESIS_MODES.md  
**Purpose**: Comprehensive guide to Monophonic, Polyphonic, and Unison synthesis modes  
**Status**: Design Reference (implements PLAN-HOOVER.md Task 1.0)

---

## Quick Start: Build Each Mode

### Monophonic (Default - Backward Compatible)
```bash
cd /Users/mbengtss/code/src/github.com/cldmnky/drumlogue-units
make                                    # Default = monophonic

./build.sh drupiter-synth               # Build unit
# Result: drupiter-synth.drmlgunit (1 voice, v1.x compatible)
```

### Polyphonic (4-voice Multi-Note)
```bash
make DRUPITER_MODE=POLYPHONIC POLYPHONIC_VOICES=4

./build.sh drupiter-synth
# Result: drupiter-synth.drmlgunit (4 simultaneous notes)
```

### Unison (5-voice Thick Sound)
```bash
make DRUPITER_MODE=UNISON UNISON_VOICES=5

./build.sh drupiter-synth
# Result: drupiter-synth.drmlgunit (1 note + 4 detuned copies)
```

---

## Synthesis Mode Overview

### Mode Selection Strategy

| Mode | Architecture | Voices | MIDI | Use Case | CPU |
|------|--------------|--------|------|----------|-----|
| **Monophonic** | 1 voice (v1.x) | 1 | Last note priority | Single melody lines, classic Jupiter | 20-25% |
| **Polyphonic** | N independent voices | 2-6 | All notes (round-robin) | Chords, bass sequences, arpeggios | 40-90% |
| **Unison** | 1 note + N detuned copies | 3-7 | Single note (reuse) | Hoover, thick pads, evolving textures | 35-80% |

### Architecture Differences

**Monophonic (1 voice):**
```
MIDI In → Voice Allocator → Single Voice State
                            ├─ Pitch
                            ├─ Velocity
                            ├─ Gate
                            └─ Note#
```

**Polyphonic (N voices):**
```
MIDI In → Voice Allocator → Voice Array [0..N-1] (each has):
                            ├─ Pitch (independent)
                            ├─ Velocity (independent)
                            ├─ Gate (independent)
                            ├─ Note# (independent)
                            └─ Per-voice envelope/filter
```

**Unison (1 note + detuned copies):**
```
MIDI In → Voice Allocator → Primary Voice State
                            ├─ Pitch
                            └─ UnisonOscillator [0..N-1] (each has):
                                ├─ Detune (relative pitch offset)
                                ├─ Gain (scaling)
                                └─ Pan (stereo positioning)
```

---

## Monophonic Mode (Default)

### Characteristics
- **Single Voice**: Only one note active at a time
- **Last-Note Priority**: When two notes pressed, newest note plays
- **Full Compatibility**: Identical to v1.x behavior
- **Minimal CPU**: ~20-25% (baseline)

### Configuration
```makefile
# In config.mk or build command
DRUPITER_MODE ?= MONOPHONIC
```

### Code Implementation
```cpp
// drupiter_synth.h (monophonic mode)
#if DRUPITER_MODE == SYNTH_MODE_MONOPHONIC

// Single voice state
uint8_t current_note_;
uint8_t current_velocity_;
bool gate_;
float pitch_cv_;

JupiterDCO dco1_, dco2_;
JupiterVCF vcf_;
JupiterEnvelope env_amp_, env_filter_;

void unit_note_on(uint8_t note, uint8_t velocity) {
    current_note_ = note;
    current_velocity_ = velocity / 127.0f;
    pitch_cv_ = NoteToCV(note);
    gate_ = true;
    env_amp_.Trigger();
    env_filter_.Trigger();
}

void unit_note_off(uint8_t note) {
    if (note == current_note_) {
        gate_ = false;
        env_amp_.Release();
        env_filter_.Release();
    }
}

#endif  // SYNTH_MODE_MONOPHONIC
```

### Preset Compatibility
- ✅ All 6 factory presets unchanged
- ✅ v1.x presets load identically
- ✅ All 24 parameters work as before
- ✅ MOD HUB routing preserved

### Performance Characteristics
- **CPU Budget**: 20-25% per 48kHz buffer
- **Latency**: <1ms
- **Memory**: Minimal (single voice state)
- **Stress Test**: 10min+ stable, no glitches

---

## Polyphonic Mode

### Characteristics
- **Multiple Independent Voices**: 2-6 simultaneous notes
- **Round-Robin Allocation**: Notes assigned to voices in order
- **Per-Voice Parameters**: Each voice has own pitch, velocity, envelope
- **Chord Capability**: Play full chords (arpeggios, bass+lead)

### Compile-Time Configuration
```makefile
# In config.mk
DRUPITER_MODE ?= POLYPHONIC
POLYPHONIC_VOICES ?= 4          # 2-6 (default 4)
```

### Build Examples
```bash
# 2-voice (light, ~40% CPU)
make DRUPITER_MODE=POLYPHONIC POLYPHONIC_VOICES=2

# 4-voice (standard, ~65-75% CPU)
make DRUPITER_MODE=POLYPHONIC POLYPHONIC_VOICES=4

# 6-voice (rich, ~80-90% CPU, requires NEON optimization)
make DRUPITER_MODE=POLYPHONIC POLYPHONIC_VOICES=6 ENABLE_NEON=1
```

### Code Implementation

**Voice Structure:**
```cpp
struct PolyVoice {
    bool active;                          // Voice in use?
    uint8_t midi_note;                    // MIDI note (0-127)
    float velocity;                       // Normalized 0-1
    float pitch_cv;                       // Calculated CV
    uint32_t note_on_time;               // For voice stealing
    
    // Per-voice oscillators and filter
    JupiterDCO dco1, dco2;
    JupiterVCF vcf;
    JupiterEnvelope env_amp;              // Amplitude ADSR
    JupiterEnvelope env_filter;           // Filter modulation
    
    // Optional: per-voice pitch envelope
    JupiterEnvelope env_pitch;
};
```

**Voice Allocator:**
```cpp
class PolyVoiceAllocator {
    enum AllocationMode {
        ALLOC_ROUND_ROBIN = 0,            // Cycle through voices
        ALLOC_OLDEST_NOTE = 1,            // Steal oldest held note
        ALLOC_FIRST_AVAILABLE = 2,        // Use first free voice
    };
    
    PolyVoice voices_[MAX_POLYPHONY_VOICES];
    uint8_t num_voices_;
    uint8_t next_voice_;                   // For round-robin
    
    void AllocateVoice(uint8_t note, uint8_t velocity);
    void ReleaseVoice(uint8_t note);
    void RenderAllVoices(float* out, uint32_t frames);
};
```

**MIDI Routing:**
```cpp
void unit_note_on(uint8_t note, uint8_t velocity) {
    // Find next available voice
    uint8_t voice_idx = FindNextVoice();
    
    // Configure voice for new note
    voices[voice_idx].midi_note = note;
    voices[voice_idx].velocity = velocity / 127.0f;
    voices[voice_idx].pitch_cv = NoteToCV(note);
    voices[voice_idx].note_on_time = current_time_ms();
    voices[voice_idx].active = true;
    
    // Trigger envelopes
    voices[voice_idx].env_amp.Trigger();
    voices[voice_idx].env_filter.Trigger();
}

void unit_note_off(uint8_t note) {
    // Find voice with matching note and release it
    for (uint8_t i = 0; i < num_voices_; i++) {
        if (voices[i].midi_note == note && voices[i].active) {
            voices[i].env_amp.Release();
            voices[i].env_filter.Release();
            break;  // Only one voice per note in simple allocator
        }
    }
}
```

**Render Loop:**
```cpp
void unit_render(const float* input, float* output, uint32_t frames, uint8_t channels) {
    float left[MAX_FRAMES] = {0}, right[MAX_FRAMES] = {0};
    
    // Process each active voice
    for (uint8_t v = 0; v < num_voices_; v++) {
        if (!voices[v].active) continue;
        
        PolyVoice& voice = voices[v];
        
        for (uint32_t i = 0; i < frames; i++) {
            // Get modulation for this frame
            float lfo_out = lfo_.Process();
            float pitch_mod = voice.env_filter.GetPitchOutput();
            float filter_mod = lfo_out * mod_lfo_to_filter_;
            
            // Process oscillators with pitch modulation
            float pitch = voice.pitch_cv + pitch_mod;
            float osc1 = voice.dco1.Process(pitch + transpose_);
            float osc2 = voice.dco2.Process(pitch + transpose_);
            float osc_out = (osc1 * osc1_level_ + osc2 * osc2_level_);
            
            // Process filter
            float filtered = voice.vcf.Process(osc_out, pitch + filter_mod);
            
            // Apply amplitude envelope and velocity
            float amp = voice.env_amp.Process();
            float voice_out = filtered * amp * voice.velocity;
            
            // Mix to stereo
            left[i] += voice_out * 0.5f;
            right[i] += voice_out * 0.5f;
        }
        
        // Mark voice as released if envelope finished
        if (!voice.env_amp.IsActive()) {
            voice.active = false;
        }
    }
    
    // Apply master effects
    ApplyEffects(left, right, frames);
    
    // Mix to output
    float master_volume = master_volume_;
    for (uint32_t i = 0; i < frames; i++) {
        output[i * channels] = left[i] * master_volume;
        if (channels > 1) {
            output[i * channels + 1] = right[i] * master_volume;
        }
    }
}
```

### Parameter Behavior

| Parameter | Behavior | Notes |
|-----------|----------|-------|
| Oscillator Waveform | Global | All voices use same waveform |
| Oscillator Tuning | Global | All voices transpose together |
| Filter Type | Global | All voices use same filter |
| Filter Cutoff | Modulated per-voice | Via envelope + LFO |
| Amplitude Envelope | Per-voice | Each note gets independent ADSR |
| LFO | Global | All voices modulated by same LFO |
| Velocity | Per-voice | Controls volume via MIDI velocity |

### Testing Strategy

**Unit Test:**
```bash
cd test/drupiter-synth
make DRUPITER_MODE=POLYPHONIC test-polyphonic

# MIDI test sequence
./drupiter_test --mode poly --midi polytest.mid output.wav
```

**Hardware Test:**
```bash
# Build and load to drumlogue
make DRUPITER_MODE=POLYPHONIC POLYPHONIC_VOICES=4
./build.sh drupiter-synth

# Connect MIDI keyboard
# Play: C, E, G, B (4-note chord)
# Verify: All 4 notes sound
# Monitor: CPU should stay under 75%
```

### CPU Performance

**Per-Voice Breakdown (48kHz, per buffer):**
- DCO1: 3% (6μs per frame)
- DCO2: 3% (6μs per frame)
- VCF: 4% (8μs per frame)
- Amp Envelope: 0.5% (1μs per frame)
- Filter Envelope: 0.5% (1μs per frame)
- **Per-voice total:** ~11% (22μs per frame)

**Full Synth (4-voice polyphonic):**
- Voice 0-3: 44% (4×11%)
- Shared LFO: 1%
- Master Effects: 3%
- Overhead: 5%
- **Total:** ~53-60% CPU (tight, but functional)

**With NEON Optimization:**
- Vectorize 4-voice oscillator processing
- Expected speedup: 2.5-3.5× on inner loops
- Result: 4-voice → ~35-40% CPU (more headroom)

---

## Unison Mode

### Characteristics
- **Single Note**: Only one MIDI note active at a time
- **Detuned Copies**: 3-7 detuned copies of same note
- **Thick, Evolving Sound**: Characteristic hoover/supersaw texture
- **Shared Filter**: All voices routed through single filter (efficiency)

### Compile-Time Configuration
```makefile
# In config.mk
DRUPITER_MODE ?= UNISON
UNISON_VOICES ?= 5              # 3-7 (default 5)
UNISON_MAX_DETUNE ?= 50         # cents (default 50)
```

### Build Examples
```bash
# 3-voice unison (thin, ~35% CPU)
make DRUPITER_MODE=UNISON UNISON_VOICES=3

# 5-voice unison (standard, ~50-65% CPU, RECOMMENDED)
make DRUPITER_MODE=UNISON UNISON_VOICES=5

# 7-voice unison (thick, ~70-85% CPU)
make DRUPITER_MODE=UNISON UNISON_VOICES=7 ENABLE_NEON=1
```

### Code Implementation

**Unison Oscillator:**
```cpp
class UnisonOscillator {
public:
    static constexpr size_t kMaxVoices = 7;
    
    void Init(float sample_rate, uint8_t voice_count);
    void SetPitch(float freq_hz);              // Center frequency
    void SetDetuneAmount(float amount);        // 0.0-1.0 (0-50 cents)
    void SetWaveform(Waveform wave);           // WAVEFORM_*
    void SetPulseWidth(float pw);              // 0.0-1.0
    
    void Render(float* left, float* right, uint32_t frames);
    
private:
    JupiterDCO voices_[kMaxVoices];
    float detune_cents_[kMaxVoices];
    float pan_[kMaxVoices];                    // Stereo positioning
    uint8_t num_voices_;
    
    void CalculateDetuneCents();               // Spread pattern
    void CalculatePanning();                   // Stereo distribution
};
```

**Detune Spread Algorithm:**
```cpp
void UnisonOscillator::CalculateDetuneCents() {
    float spread_cents = detune_amount * UNISON_MAX_DETUNE;
    
    if (num_voices == 1) {
        detune_cents[0] = 0.0f;
    } else {
        // Golden ratio phasing for smooth beating pattern
        const float phi = 1.618034f;
        int center = num_voices / 2;
        
        for (uint8_t i = 0; i < num_voices; i++) {
            int offset = i - center;
            float scale = powf(phi, fabsf(offset) / 3.0f);
            detune_cents[i] = offset * spread_cents * scale / (center);
        }
    }
}
```

**Stereo Distribution:**
```cpp
void UnisonOscillator::CalculatePanning() {
    // Distribute voices across stereo field for natural width
    
    if (num_voices == 1) {
        pan[0] = 0.5f;  // Center
    } else {
        float step = 1.0f / (num_voices - 1);
        for (uint8_t i = 0; i < num_voices; i++) {
            pan[i] = i * step;  // 0.0 (left) to 1.0 (right)
        }
    }
}
```

**Render Loop:**
```cpp
void unit_render(const float* input, float* output, uint32_t frames, uint8_t channels) {
    float left[MAX_FRAMES] = {0}, right[MAX_FRAMES] = {0};
    
    // Only render if gate active (note on)
    if (!gate_) {
        env_amp_.Release();
        if (!env_amp_.IsActive()) {
            memset(output, 0, frames * channels * sizeof(float));
            return;
        }
    }
    
    // Render unison oscillator pair (DCO1 and DCO2 each run unison)
    float unison1_left[MAX_FRAMES], unison1_right[MAX_FRAMES];
    float unison2_left[MAX_FRAMES], unison2_right[MAX_FRAMES];
    
    unison_osc1_.Render(unison1_left, unison1_right, frames);
    unison_osc2_.Render(unison2_left, unison2_right, frames);
    
    // Mix oscillators
    for (uint32_t i = 0; i < frames; i++) {
        float left_mixed = unison1_left[i] * osc1_level_ + 
                          unison2_left[i] * osc2_level_;
        float right_mixed = unison1_right[i] * osc1_level_ + 
                           unison2_right[i] * osc2_level_;
        
        // Process through shared filter
        float filtered_l = vcf_.Process(left_mixed, pitch_cv_);
        float filtered_r = vcf_.Process(right_mixed, pitch_cv_);
        
        // Apply amplitude envelope
        float amp = env_amp_.Process();
        left[i] = filtered_l * amp;
        right[i] = filtered_r * amp;
    }
    
    // Apply master effects
    ApplyEffects(left, right, frames);
    
    // Output
    float master_vol = master_volume_;
    for (uint32_t i = 0; i < frames; i++) {
        output[i * channels] = left[i] * master_vol;
        if (channels > 1) {
            output[i * channels + 1] = right[i] * master_vol;
        }
    }
}
```

### Parameter Behavior

| Parameter | Behavior | Notes |
|-----------|----------|-------|
| Oscillator Waveform | Global | All unison voices use same waveform |
| Oscillator Tuning | Global | All voices detune relative to center pitch |
| Pulse Width | Global | All voices use same PW (if SAW_PWM used) |
| Filter Type | Shared | Single filter for all unison voices |
| Filter Cutoff | Modulated | Via envelope + LFO |
| Amplitude Envelope | Shared | All voices amplitude controlled together |
| Unison Voices (MOD HUB) | Per-patch | Configure voice count via modulation |
| Unison Detune (MOD HUB) | Per-patch | Spread width via modulation |
| LFO | Global | Modulates all voices + filter |

### Testing Strategy

**Unit Test:**
```bash
cd test/drupiter-synth
make DRUPITER_MODE=UNISON test-unison

# Test voice counts
./drupiter_test --mode unison --voices 3 output_3v.wav
./drupiter_test --mode unison --voices 5 output_5v.wav
./drupiter_test --mode unison --voices 7 output_7v.wav
```

**Hardware Test:**
```bash
# Build 5-voice unison
make DRUPITER_MODE=UNISON UNISON_VOICES=5
./build.sh drupiter-synth

# Connect to drumlogue
# Play single note (C3) and hold for 3 seconds
# Listen for:
# - Thick, chorused texture
# - Slow beating pattern (chorus effect)
# - No individual voice artifacts
# - Smooth LFO modulation
```

### CPU Performance

**Single Unison Oscillator (5 voices):**
- 5× phase accumulation: 2.5% (5μs per frame)
- 5× waveform generation: 7.5% (15μs per frame)
- 5× phase wrapping: 2.5% (5μs per frame)
- **Unison osc total:** ~12.5% (25μs per frame)

**Full Synth (2 unison oscillators + shared filter):**
- DCO1 Unison (5 voices): 12.5%
- DCO2 Unison (5 voices): 12.5%
- Shared VCF: 4%
- Envelopes (shared): 1%
- LFO: 1%
- Effects: 3%
- Overhead: 5%
- **Total:** ~39-45% CPU (comfortable headroom)

**With 7-voice unison:**
- DCO1 Unison (7 voices): 17.5% (7×2.5%)
- DCO2 Unison (7 voices): 17.5%
- Shared VCF: 4%
- Envelopes: 1%
- LFO: 1%
- Effects: 3%
- Overhead: 5%
- **Total:** ~49-55% CPU (tight, but acceptable)

**With NEON Optimization:**
- Vectorize 4-voice phase accumulation per cycle
- Process 4 phases simultaneously
- Expected speedup: 3-4× on oscillator cores
- Result: 7-voice → ~25-30% CPU (significant headroom)

---

## Building Multiple Mode Variants

### Create Compile-Time Configuration

**config.mk:**
```makefile
# Drupiter-Synth Build Configuration

PROJECT := drupiter_synth
PROJECT_TYPE := synth

# Synthesis mode: MONOPHONIC, POLYPHONIC, UNISON
DRUPITER_MODE ?= MONOPHONIC

# Voice counts (compile-time)
POLYPHONIC_VOICES ?= 4
UNISON_VOICES ?= 5
UNISON_MAX_DETUNE ?= 50  # cents

# Feature flags
ENABLE_NEON ?= 0
ENABLE_PITCH_ENVELOPE ?= 1
ENABLE_DUAL_OUTPUT ?= 0

# Build defines
UDEFS += -DDRUPITER_MODE_$(DRUPITER_MODE)
UDEFS += -DPOLYPHONIC_VOICES=$(POLYPHONIC_VOICES)
UDEFS += -DUNISON_VOICES=$(UNISON_VOICES)
UDEFS += -DUNISON_MAX_DETUNE=$(UNISON_MAX_DETUNE)

ifeq ($(ENABLE_NEON),1)
UDEFS += -DENABLE_NEON
UDEFS += -mfpu=neon
endif
```

### Build Examples

```bash
# Build all three modes
make clean build DRUPITER_MODE=MONOPHONIC
cp drumlogue/drupiter-synth/drupiter_synth.drmlgunit \
   ./builds/drupiter_mono.drmlgunit

make clean build DRUPITER_MODE=POLYPHONIC POLYPHONIC_VOICES=4
cp drumlogue/drupiter-synth/drupiter_synth.drmlgunit \
   ./builds/drupiter_poly4.drmlgunit

make clean build DRUPITER_MODE=UNISON UNISON_VOICES=5
cp drumlogue/drupiter-synth/drupiter_synth.drmlgunit \
   ./builds/drupiter_unison5.drmlgunit

make clean build DRUPITER_MODE=UNISON UNISON_VOICES=7 ENABLE_NEON=1
cp drumlogue/drupiter-synth/drupiter_synth.drmlgunit \
   ./builds/drupiter_unison7_neon.drmlgunit
```

---

## Switching Between Modes (Runtime vs Compile-Time)

### Current Design: Compile-Time Only

All three modes are **mutually exclusive** and selected at **compile-time**:

```cpp
#if DRUPITER_MODE == SYNTH_MODE_MONOPHONIC
    // Monophonic voice allocation code
#elif DRUPITER_MODE == SYNTH_MODE_POLYPHONIC
    // Polyphonic voice allocation code
#elif DRUPITER_MODE == SYNTH_MODE_UNISON
    // Unison oscillator code
#endif
```

**Rationale:**
- Reduces binary size (only one mode compiled in)
- Allows aggressive mode-specific optimization
- Simplifies parameter mapping
- Avoids runtime overhead of mode switching

**Limitation:**
- Can't switch modes without rebuilding
- Each mode is separate `.drmlgunit` file

### Future Enhancement: Runtime Switching (Phase 3)

Could add runtime mode selection via:
- MOD HUB parameter (select mode via dedicated control)
- Preset field (specify default mode per preset)
- Requires runtime branch overhead (acceptable since ~5% of CPU)

---

## Preset Compatibility

### Backward Compatibility Guarantee

**All existing presets work unchanged in all modes:**

```
v1.x Monophonic Preset (24 parameters)
         ↓
v2.0 Monophonic Mode → Sounds identical ✅
v2.0 Polyphonic Mode → Sounds similar (single voice context) ✅
v2.0 Unison Mode     → Sounds similar (single voice context) ✅
```

**Why?**
- All 24 direct parameters preserved
- Parameter indices and types unchanged
- Preset loading is parameter-value based (not mode-aware)
- Mode is orthogonal to preset data

### Preset Structure

```cpp
struct Preset {
    char name[16];
    
    // 24 direct parameters (UNCHANGED)
    int16_t param_values[24];
    
    // MOD HUB routing (extended, with backward compat)
    struct {
        uint8_t source;       // LFO, ENV, etc
        uint8_t destination;  // PITCH, FILTER, etc
        uint8_t amount;       // 0-100
    } modulation_slots[MOD_NUM_SLOTS];
    
    // New in v2.0 (optional)
    uint8_t mode;             // MONO, POLY, UNISON (not used for loading)
    uint8_t voice_count;      // Suggested count (informational)
};

// Backward compatibility:
// v1.x presets load as: {24 params} + {8 mod slots} + {mode=MONO, count=1}
// New presets can override mode for per-preset defaults (Phase 2+)
```

---

## Summary

| Feature | Monophonic | Polyphonic | Unison |
|---------|-----------|-----------|--------|
| **Voices** | 1 | 2-6 | 3-7 |
| **CPU** | 20-25% | 40-90% | 35-80% |
| **MIDI Notes** | 1 | All | 1 |
| **Use Case** | Classic leads | Chords, bass | Hoover, pads |
| **Backward Compat** | 100% | Yes | Yes |
| **Build Flag** | Default | `DRUPITER_MODE=POLYPHONIC` | `DRUPITER_MODE=UNISON` |

---

## References

- **PLAN-HOOVER.md** - Master implementation specification
- **drupiter-synth/dsp/voice_allocator.h** - Voice management (Task 1.0)
- **drupiter-synth/dsp/unison_oscillator.h** - Unison implementation (Task 1.2)
- **drupiter-synth/config.mk** - Build configuration
- **test/drupiter-synth/** - Unit and integration tests

