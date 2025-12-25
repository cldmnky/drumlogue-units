# Hoover Synthesis Implementation Roadmap

**Document**: IMPLEMENTATION_ROADMAP.md  
**Purpose**: Quick reference for Phase 1 implementation priorities  
**Target Audience**: Developers implementing Tasks 1.0-1.5

---

## Phase 1: Foundation + Core Hoover (3-4 weeks)

### Task 1.0: Voice Allocator (CRITICAL - Week 1-2)

**Purpose**: Foundational architecture for all three synthesis modes  
**Status**: ✅ IN PROGRESS - Core files created  
**Priority**: 🔴 CRITICAL (prerequisite for Tasks 1.1-1.5)

#### Deliverables

1. ✅ **COMPLETED: New File: `dsp/voice_allocator.h`** (200 lines)
   - SynthMode enum (MONOPHONIC, POLYPHONIC, UNISON)
   - Voice struct with per-voice DSP components
   - VoiceAllocator class with mode-specific rendering
   - MIDI to frequency conversion utilities
   - Performance optimizations: fixed arrays, compile-time mode selection

2. ✅ **COMPLETED: New File: `dsp/voice_allocator.cc`** (350 lines)
   - Voice initialization and reset
   - Mode-specific voice allocation (monophonic, polyphonic, unison)
   - Voice stealing strategies (oldest note, round-robin, first available)
   - Optimized render loops for each mode
   - Performance: O(1) allocation, cache-friendly layout

3. ✅ **COMPLETED: Modify: `config.mk`**
   - Added voice_allocator.cc to CXXSRC
   - Compile-time mode selection: DRUPITER_MODE (MONOPHONIC/POLYPHONIC/UNISON)
   - Voice count configuration: POLYPHONIC_VOICES, UNISON_VOICES, MAX_VOICES
   - Feature flags: ENABLE_NEON, ENABLE_PITCH_ENVELOPE
   - Performance flags: -O2, -ffast-math

4. ⏳ **TODO: Modify: `drupiter_synth.h`**
   ```cpp
   enum SynthMode { SYNTH_MODE_MONOPHONIC, SYNTH_MODE_POLYPHONIC, SYNTH_MODE_UNISON };
   
   struct Voice {
       bool active;
       uint8_t midi_note;
       float velocity;
       float pitch_hz;
       JupiterDCO dco1, dco2;
       JupiterVCF vcf;
       JupiterEnvelope env_amp, env_filter;
       // Mode-specific fields added by derived classes
   };
   
   class VoiceAllocator {
       void Init(SynthMode mode, uint8_t max_voices);
       Voice* AllocateVoice(uint8_t note, uint8_t velocity);
       void ReleaseVoice(uint8_t note);
       void RenderVoices(float* out, uint32_t frames);
   };
   ```

2. **New File: `dsp/voice_allocator.cc`** (300-400 lines)
   - Mode detection and voice array initialization
   - Voice allocation logic (round-robin)
   - Voice release and cleanup
   - Render dispatch by mode

3. **Modify: `drupiter_synth.h`**
   - Replace `current_note_`, `gate_`, `pitch_cv_` with `VoiceAllocator allocator_`
   - Add mode selection compile-time constant
   - Keep all 24 parameters unchanged

4. **Modify: `unit.cc`**
   - Route MIDI `note_on()` → `allocator_.AllocateVoice()`
   - Route MIDI `note_off()` → `allocator_.ReleaseVoice()`
   - Call `allocator_.RenderVoices()` in `unit_render()`

5. **Modify: `config.mk`**
   ```makefile
   DRUPITER_MODE ?= MONOPHONIC
   POLYPHONIC_VOICES ?= 4
   UNISON_VOICES ?= 5
   
   UDEFS += -DDRUPITER_MODE=SYNTH_MODE_$(shell echo $(DRUPITER_MODE) | tr a-z A-Z)
   UDEFS += -DPOLYPHONIC_VOICES=$(POLYPHONIC_VOICES)
   UDEFS += -DUNISON_VOICES=$(UNISON_VOICES)
   UDEFS += -DDRUPITER_MAX_VOICES=7
   ```

#### Testing (Task 1.0)
```bash
# Compile monophonic mode (baseline)
make DRUPITER_MODE=MONOPHONIC clean build
./build.sh drupiter-synth
# Load to drumlogue, verify identical to v1.x (6 factory presets)

# Compile polyphonic mode (basic)
make DRUPITER_MODE=POLYPHONIC POLYPHONIC_VOICES=4 clean build
./build.sh drupiter-synth
# Test: Play single note (verify voice allocated correctly)

# Compile unison mode (basic)
make DRUPITER_MODE=UNISON UNISON_VOICES=5 clean build
./build.sh drupiter-synth
# Test: Play single note (verify no crash)
```

#### Success Criteria
- ✅ Code compiles in all three modes without errors
- ✅ Monophonic mode sounds identical to v1.x
- ✅ Polyphonic mode allocates voices correctly (test with MIDI)
- ✅ Unison mode initializes without crash
- ✅ All 24 parameters still accessible
- ✅ CPU usage stable in all modes

---

### Task 1.1: PWM Sawtooth Waveform (Week 2)

**Purpose**: Add PWM-able sawtooth waveform  
**Depends On**: Task 1.0 (voice allocator)  
**Status**: Not Started

#### Deliverables

1. **Modify: `dsp/jupiter_dco.h`**
   ```cpp
   enum Waveform {
       WAVEFORM_SAW = 0,
       WAVEFORM_SQUARE = 1,
       WAVEFORM_PULSE = 2,
       WAVEFORM_TRIANGLE = 3,
       WAVEFORM_SAW_PWM = 4,     // NEW: PWM-able sawtooth
   };
   ```

2. **Modify: `dsp/jupiter_dco.cc` - `Process()` method**
   - Add case for `WAVEFORM_SAW_PWM`
   - Morph sawtooth shape based on `pulse_width_` parameter
   - Maintain PolyBLEP anti-aliasing (existing code)
   - Smooth transitions between SAW and PWM states

3. **Modify: `header.c`** (parameter descriptor for waveform)
   ```cpp
   // DCO1 waveform parameter
   {
       .id = PARAM_DCO1_WAVE,
       .min = 0,
       .max = 4,        // Increased from 3 to 4
       .center = 0,
       .type = k_unit_param_type_integer,
       .name = "DCO1 WAVE"
   }
   ```

#### Testing (Task 1.1)
```bash
# Desktop test
cd test/drupiter-synth
./drupiter_test --pwm-saw --pw 0.1 out_pw01.wav
./drupiter_test --pwm-saw --pw 0.3 out_pw03.wav
./drupiter_test --pwm-saw --pw 0.5 out_pw05.wav
./drupiter_test --pwm-saw --pw 0.7 out_pw07.wav
./drupiter_test --pwm-saw --pw 0.9 out_pw09.wav

# Spectrum analysis (check harmonic content changes)
python analyze_spectrum.py out_pw*.wav

# Hardware test
# - Set DCO1 waveform to 4 (SAW_PWM)
# - Play note and modulate PW with LFO
# - Listen for characteristic sawtooth/square morphing
```

#### Success Criteria
- ✅ Waveform enum extends to 4 (v1.x unaffected at 0-3)
- ✅ PWM modulation affects SAW_PWM in real-time
- ✅ No aliasing artifacts (desktop spectrum clean)
- ✅ Smooth transitions (no clicks when sweeping PW)
- ✅ CPU impact minimal (<1% additional)

---

### Task 1.2: Unison Oscillator (Week 2-3)

**Purpose**: Implement detuned voice aggregation  
**Depends On**: Task 1.0 (voice allocator) + Task 1.1 (PWM saw)  
**Status**: Not Started

#### Deliverables

1. **New File: `dsp/unison_oscillator.h`** (250-350 lines)
   ```cpp
   class UnisonOscillator {
   public:
       void Init(float sample_rate);
       void SetVoiceCount(uint8_t count);
       void SetDetuneAmount(float cents);
       void SetPitch(float freq_hz);
       void SetWaveform(uint8_t wave);
       void SetPulseWidth(float pw);
       void Render(float* left, float* right, uint32_t frames);
       
   private:
       static constexpr uint8_t kMaxVoices = 7;
       JupiterDCO voices_[kMaxVoices];
       float detune_cents_[kMaxVoices];
       float pan_[kMaxVoices];
       uint8_t num_voices_;
       
       void CalculateDetuneCents();
       void CalculatePanning();
   };
   ```

2. **New File: `dsp/unison_oscillator.cc`** (300-400 lines)
   - Voice initialization and configuration
   - Detune pattern calculation (golden ratio phasing)
   - Pan distribution across stereo field
   - Parallel voice rendering

3. **Modify: `drupiter_synth.h`** (for unison mode)
   ```cpp
   #if DRUPITER_MODE == SYNTH_MODE_UNISON
   
   UnisonOscillator unison_osc1_;
   UnisonOscillator unison_osc2_;
   
   #endif
   ```

4. **Modify: `drupiter_synth.cc` - Unison render loop**
   - Initialize unison oscillators in `Init()`
   - Render both oscillators in parallel
   - Mix outputs with `osc1_level_` and `osc2_level_`
   - Apply shared filter to mixed output

#### Testing (Task 1.2)
```bash
# Desktop test
cd test/drupiter-synth
./drupiter_test --mode unison --voices 1 out_1v.wav
./drupiter_test --mode unison --voices 3 out_3v.wav
./drupiter_test --mode unison --voices 5 out_5v.wav
./drupiter_test --mode unison --voices 7 out_7v.wav

# Spectrum analysis (measure harmonic width)
python analyze_chorus.py out_*v.wav

# Hardware test
# - Build: make DRUPITER_MODE=UNISON UNISON_VOICES=5
# - Play single note (C3) and hold
# - Listen for progressive thickening with detune amount
```

#### Success Criteria
- ✅ 1-voice unison = same as monophonic
- ✅ 3-voice unison = noticeable chorus effect
- ✅ 5-voice unison = thick, characteristic hoover sound
- ✅ 7-voice unison = maximum thickness
- ✅ Detune pattern creates smooth, musical beating
- ✅ Stereo image widens with voice count
- ✅ CPU stays within budget (50-65% for 5v)

---

### Task 1.3: Waveform Parameter Expansion (Week 3)

**Purpose**: Expose SAW_PWM in parameter system  
**Depends On**: Task 1.1 (PWM sawtooth)  
**Status**: Not Started

#### Deliverables

1. **Modify: `header.c`** (parameter descriptors)
   - Update DCO1 and DCO2 waveform max values (3 → 4)
   - Add parameter strings for new waveform

2. **Modify: `unit.cc` - `unit_set_param_value()`**
   ```cpp
   case PARAM_DCO1_WAVE:
       // value = 0-4 (SAW, SQUARE, PULSE, TRIANGLE, SAW_PWM)
       dco1_.SetWaveform(value);
       break;
       
   case PARAM_DCO2_WAVE:
       // value = 0-4 (SAW, NOISE, PULSE, SINE, SAW_PWM)
       dco2_.SetWaveform(value);
       break;
   ```

3. **Desktop test harness**
   - Add command-line options for waveform selection
   - Verify parameter updates reach DSP layer

#### Testing (Task 1.3)
```bash
# Test parameter boundaries
# - Set DCO1 waveform to 0 (SAW)
# - Set DCO1 waveform to 4 (SAW_PWM)
# - Verify smooth transitions

# Backward compatibility
# - Load factory preset (uses waveforms 0-3)
# - Verify sounds identical to v1.x
```

#### Success Criteria
- ✅ Parameter max value increased to 4
- ✅ SAW_PWM selectable from drumlogue UI
- ✅ Factory presets unaffected (still use 0-3)
- ✅ Smooth parameter transitions

---

### Task 1.4: MOD HUB Expansion (Week 3)

**Purpose**: Add unison/polyphonic controls via modulation  
**Depends On**: Tasks 1.0-1.2  
**Status**: Not Started

#### Deliverables

1. **Modify: `drupiter_synth.h`**
   ```cpp
   enum ModDestination {
       MOD_DEST_PITCH = 0,           // Existing
       MOD_DEST_FILTER = 1,          // Existing
       // ... existing 0-14 ...
       MOD_DEST_UNISON_VOICES = 15,   // NEW: Voice count (0-100%)
       MOD_DEST_UNISON_DETUNE = 16,   // NEW: Detune spread (0-100%)
       MOD_DEST_POLY_VOICES = 17,     // NEW: Active poly voices (0-100%)
       
       MOD_NUM_DESTINATIONS
   };
   ```

2. **Modify: `drupiter_synth.cc`**
   - Get modulation values for new destinations
   - Map to runtime voice counts and detune amounts
   - Update allocator per-frame

3. **Modify: `header.c`** (MOD HUB parameter descriptors)
   - Update MOD_NUM_SLOTS if needed
   - Add friendly names for new destinations

#### Testing (Task 1.4)
```bash
# Test modulation routing
# - Assign LFO → MOD_DEST_UNISON_DETUNE
# - Play note and listen for evolving chorus
# - Verify smooth, real-time modulation

# Test with different LFO rates
# - Slow LFO (0.5 Hz): Gentle opening/closing effect
# - Medium LFO (2 Hz): Typical chorus speed
# - Fast LFO (8 Hz): Tremolo-like character
```

#### Success Criteria
- ✅ New MOD destinations addressable from drumlogue
- ✅ Smooth real-time modulation of voice counts/detune
- ✅ No CPU impact (< 2% overhead)
- ✅ Backward compatible (existing MOD routings unchanged)

---

### Task 1.5: Comprehensive Testing (Week 3-4)

**Purpose**: Validate all three modes for release  
**Depends On**: Tasks 1.0-1.4  
**Status**: Not Started

#### Desktop Testing

```bash
# Unit tests
cd test/drupiter-synth
make test-monophonic      # Single voice baseline
make test-polyphonic      # 4-voice chords
make test-unison          # 5-voice hoover

# Spectrum analysis
python analyze_spectra.py

# Performance profiling
time ./drupiter_test *.wav
# Expected: <2 seconds for 10 second test file
```

#### Hardware Testing

1. **Monophonic Mode Compatibility**
   ```
   Test: Load all 6 factory presets
   Verify: Identical sound to v1.x
   Pass: ✅ All presets unchanged
   ```

2. **Polyphonic Mode**
   ```
   Test: Play MIDI chords (C-E-G-B) all simultaneously
   Verify: All 4 notes audible and independent
   Pass: ✅ CPU <75%, no stuttering
   ```

3. **Unison Mode**
   ```
   Test: Play single note (C3) and hold 3 seconds
   Verify: Thick, chorused hoover sound
   Pass: ✅ Recognizably "hoover" character
   ```

4. **Stress Test**
   ```
   Test: 10 minutes continuous rapid MIDI
   Verify: No glitches, clicks, stuck voices
   Pass: ✅ Rock solid stability
   ```

#### Success Criteria (Phase 1 Complete)
- ✅ All three modes compile without warnings
- ✅ Monophonic = v1.x backward compatible
- ✅ Polyphonic = 4 independent voices
- ✅ Unison = characteristic hoover sound
- ✅ CPU budgets met (20%, 75%, 65%)
- ✅ All tests pass (desktop + hardware)
- ✅ No audio artifacts or glitches
- ✅ Ready for Phase 2 enhancements

---

## Phase 2: Enhancements (2-3 weeks after Phase 1)

### Task 2.1: Expand Unison Voice Count (3-5 voices → 7)
- Profile current 5-voice implementation
- Optimize hot path
- Enable 6-7 voice configurations
- NEON acceleration if needed

### Task 2.2: Pitch Envelope (Per-Voice Modulation)
- Add pitch envelope to voice state
- Map to MOD HUB destination
- Characteristic "zap" sound for hoover

### Task 2.3: Phase-Aligned Dual Output (Optional)
- Align waveform phases between DCO1/DCO2
- Richer harmonic texture
- Complex sounds from minimal oscillators

### Task 2.4: NEON SIMD Optimization
- Vectorize 4-voice oscillator processing
- 2.5-3.5× speedup expected
- Enable 6-7 voice configurations

---

## Build System Integration

### Makefile Structure

```makefile
# config.mk
DRUPITER_MODE ?= MONOPHONIC
POLYPHONIC_VOICES ?= 4
UNISON_VOICES ?= 5

# Make targets
build: all
test: test-monophonic test-polyphonic test-unison
clean: clean-all

# Mode-specific builds
build-mono: DRUPITER_MODE=MONOPHONIC make all
build-poly: DRUPITER_MODE=POLYPHONIC make all
build-unison: DRUPITER_MODE=UNISON make all
```

### CI/CD Integration

```yaml
# .github/workflows/dsp-test.yml
- name: Build all modes
  run: |
    make DRUPITER_MODE=MONOPHONIC clean build
    make DRUPITER_MODE=POLYPHONIC POLYPHONIC_VOICES=4 clean build
    make DRUPITER_MODE=UNISON UNISON_VOICES=5 clean build

- name: Run desktop tests
  run: |
    cd test/drupiter-synth
    make test
```

---

## Code Review Checklist

Before merging Phase 1:

- [ ] All code follows C++ coding standards (cpp.instructions.md)
- [ ] No dynamic allocation in real-time path
- [ ] Voice allocator abstraction clean and extensible
- [ ] CPU budgets verified (desktop + QEMU ARM)
- [ ] All backward compatibility tests pass
- [ ] No memory leaks (valgrind clean)
- [ ] Code documented with meaningful comments
- [ ] Tests comprehensive and passing
- [ ] Performance profiling complete
- [ ] Hardware testing on actual drumlogue

---

## Risk Mitigation

| Risk | Probability | Impact | Mitigation |
|------|-----------|--------|-----------|
| CPU budget exceeded | Medium | High | NEON optimization, voice limiting |
| Parameter conflicts | Low | High | Careful parameter mapping review |
| Backward compat breaks | Low | Critical | Comprehensive preset testing |
| Memory corruption | Low | Critical | Thorough testing, bounds checking |
| Audio glitches | Medium | Medium | Anti-aliasing, envelope smoothing |

---

## Success Metrics (Phase 1 Final)

1. **Functional** 
   - ✅ Three modes fully working
   - ✅ All 24 parameters accessible
   - ✅ MIDI routing correct in all modes

2. **Performance**
   - ✅ CPU: Mono 20%, Poly 65-75%, Unison 50-65%
   - ✅ Latency <1.5ms
   - ✅ No buffer underruns

3. **Quality**
   - ✅ No audio artifacts
   - ✅ Recognizable hoover sound (unison)
   - ✅ Independent voice behavior (polyphonic)

4. **Compatibility**
   - ✅ All 6 factory presets unchanged
   - ✅ v1.x presets load identically
   - ✅ No parameter reuse or removal

5. **Documentation**
   - ✅ SYNTHESIS_MODES.md complete
   - ✅ Code comments clear
   - ✅ Build instructions documented

---

## Next Steps (After Phase 1)

1. Code review and approval
2. Hardware validation on drumlogue
3. Performance profiling and optimization
4. Phase 2 planning (pitch envelope, NEON)
5. Release planning (v2.0.0)

