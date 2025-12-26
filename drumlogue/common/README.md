# Common Utilities for Drumlogue Units

This directory contains reusable components for developing drumlogue units. These utilities help reduce code duplication and ensure consistent behavior across all units.

## Available Utilities

### Core DSP & Audio
- **`dsp_utils.h`**: Lightweight DSP helpers (clamp, lerp, crossfade) and one-pole dezipper.
- **`fixed_mathq.h`**: Fixed-point arithmetic (Q formats) optimized for Cortex-A7 using ARM intrinsics.
- **`neon_dsp.h`**: ARM NEON SIMD utilities for buffer operations, mixing, and gain control.
- **`arm_intrinsics.h`**: Low-level ARM DSP intrinsics (smmul, smlawb, etc.) for Cortex-A7/M4/M7.
- **`simd_utils.h`**: NEON SIMD helpers for common audio buffer operations (load/store, MAC, clamp).
- **`smoothed_value.h`**: One-pole smoothed parameter class for zipper-free parameter changes.

### Oscillators & Effects
- **`wavetable_osc.h`**: Anti-aliased wavetable oscillator with smooth morphing and integration.
- **`ppg_osc.h`**: PPG Wave style 8-bit wavetable oscillator with key-wave interpolation and NEON support.
- **`stereo_widener.h`**: NEON-optimized Mid/Side stereo processing and width enhancement.

### UI & Parameter Management
- **`hub_control.h`**: Hub system to control multiple parameters using a single selector + value pair.
- **`param_format.h`**: Consistent string formatting for UI (Percent, Frequency, Time, dB, Pitch).
- **`preset_manager.h`**: Generic template-based system for preset storage, loading, and validation.
- **`midi_helper.h`**: MIDI conversion utilities (Note-to-Freq, velocity scaling, pitch bend).

### Performance Monitoring
- **`perf_mon.h`**: Cycle counting and performance metrics with `-DPERF_MON` flag control. Compiles to zero overhead when disabled.

---

## Usage Examples

### Hub Control System

Compress multiple related parameters into a single selector + value pair:

```cpp
#include "common/hub_control.h"

// Define hub destinations
static constexpr common::HubControl<8>::Destination kModDests[] = {
  {"LFO>PWM", "%",  0, 100, 0,  false},  // LFO to pulse width
  {"ENV>VCF", "%",  0, 100, 50, true},   // ENV to filter (bipolar)
  {"HPF",     "Hz", 0, 100, 0,  false},  // High-pass filter
};

class MySynth {
 private:
  common::HubControl<8> mod_hub_{kModDests};
  
 public:
  void SetParameter(uint8_t id, int32_t value) {
    switch (id) {
      case PARAM_MOD_SEL:
        mod_hub_.SetDestination(value);
        break;
      case PARAM_MOD_VAL:
        mod_hub_.SetValue(value);
        break;
    }
  }
  
  const char* GetParameterStr(uint8_t id, int32_t value) {
    static char buf[16];
    switch (id) {
      case PARAM_MOD_SEL:
        return mod_hub_.GetDestinationName(value);
      case PARAM_MOD_VAL:
        return mod_hub_.GetValueString(buf, sizeof(buf));
    }
    return "";
  }
  
  void Render(float* out, uint32_t frames) {
    // Get values for each destination
    float lfo_to_pwm = mod_hub_.GetValue(0) / 100.0f;
    float env_to_vcf = (mod_hub_.GetValue(1) - 50) / 50.0f;  // bipolar
    float hpf_cutoff = mod_hub_.GetValue(2) / 100.0f;
    // ... use in DSP
  }
};
```

### Parameter Formatting

Consistent string formatting for UI display:

```cpp
#include "common/param_format.h"

const char* GetParameterStr(uint8_t id, int32_t value) {
  static char buf[16];
  
  switch (id) {
    case PARAM_CUTOFF:
      float freq = cutoff_to_frequency(value);
      return common::ParamFormat::Frequency(buf, sizeof(buf), freq);
      // Returns: "440Hz" or "4.4kHz"
    
    case PARAM_ATTACK:
      float time_ms = value * 50.0f;  // Example scaling
      return common::ParamFormat::Time(buf, sizeof(buf), time_ms);
      // Returns: "50ms" or "1.5s"
    
    case PARAM_DETUNE:
      float cents = (value - 50) * 4.0f;
      return common::ParamFormat::Pitch(buf, sizeof(buf), cents);
      // Returns: "+25c" or "-2st"
    
    case PARAM_ENV_AMT:
      return common::ParamFormat::BipolarPercent(buf, sizeof(buf), 
                                                  value, 0, 100);
      // Returns: "-50%" or "+75%"
  }
  return "";
}
```

### Preset Management

Standardized preset system:

```cpp
#include "common/preset_manager.h"

// Define factory presets
static constexpr common::PresetManager<24>::Preset kPresets[] = {
  {"Brass Lead", {1, 0, 50, 25, /* ... 24 params */}},
  {"Fat Bass",   {0, 0, 50, 0,  /* ... 24 params */}},
  {"Soft Pad",   {1, 2, 75, 50, /* ... 24 params */}},
};

class MySynth {
 private:
  common::PresetManager<24> preset_mgr_{kPresets, 3};
  
 public:
  void LoadPreset(uint8_t idx) {
    if (preset_mgr_.LoadPreset(idx)) {
      // Apply all parameters from loaded preset
      for (uint8_t i = 0; i < 24; i++) {
        SetParameter(i, preset_mgr_.GetParam(i));
      }
    }
  }
  
  const char* GetPresetName(uint8_t idx) {
    return preset_mgr_.GetPresetName(idx);
  }
  
  uint8_t GetPresetIndex() {
    return preset_mgr_.GetCurrentIndex();
  }
};
```

### MIDI Helpers

Common MIDI conversions:

```cpp
#include "common/midi_helper.h"

void NoteOn(uint8_t note, uint8_t velocity) {
  // Convert MIDI note to frequency
  float freq = common::MidiHelper::NoteToFreq(note);
  
  // Convert velocity with exponential curve
  float vel = common::MidiHelper::VelocityToFloatCurved(velocity, 2.0f);
  
  osc_.SetFrequency(freq);
  env_.Trigger(vel);
}

void PitchBend(uint16_t bend) {
  // Convert pitch bend to semitones (±2 semitone range)
  float semitones = common::MidiHelper::PitchBendToSemitones(bend, 2.0f);
  
  // Or directly to frequency multiplier
  float multiplier = common::MidiHelper::PitchBendToMultiplier(bend, 2.0f);
  
  osc_.SetPitchBend(multiplier);
}

// Display note name
const char* GetNoteName(uint8_t note) {
  static char buf[8];
  snprintf(buf, sizeof(buf), "%s%d", 
           common::MidiHelper::NoteName(note),
           common::MidiHelper::NoteOctave(note));
  return buf;  // e.g., "C4", "F#5"
}
```

### Parameter Smoothing

Anti-zipper noise for parameter changes:

```cpp
#include "common/smoothed_value.h"

class MySynth {
 private:
  dsp::SmoothedValue cutoff_smooth_;
  
 public:
  void Init(float sample_rate) {
    cutoff_smooth_.Init(0.5f, 0.005f);  // 5ms smoothing time
  }
  
  void SetParameter(uint8_t id, int32_t value) {
    if (id == PARAM_CUTOFF) {
      float normalized = value / 100.0f;
      cutoff_smooth_.SetTarget(normalized);
    }
  }
  
  void Render(float* out, uint32_t frames) {
    for (uint32_t i = 0; i < frames; i++) {
      // Smooth parameter updates every sample
      float cutoff = cutoff_smooth_.Process();
      
      // Use smoothed value in DSP
      filter_.SetCutoff(cutoff);
      out[i] = filter_.Process(in[i]);
    }
  }
};
```

### Performance Monitoring

Measure DSP performance with cycle counting (enabled with `-DPERF_MON` flag):

```cpp
#include "common/perf_mon.h"

class MySynth {
 private:
  uint8_t perf_osc_, perf_filter_, perf_effects_;
  
 public:
  void Init() {
    PERF_MON_INIT();
    perf_osc_ = PERF_MON_REGISTER("OSC");
    perf_filter_ = PERF_MON_REGISTER("FILTER");
    perf_effects_ = PERF_MON_REGISTER("EFFECTS");
  }
  
  void Render(float* out, uint32_t frames) {
    for (uint32_t i = 0; i < frames; i++) {
      // Oscillator
      PERF_MON_START(perf_osc_);
      float sig = ProcessOscillator();
      PERF_MON_END(perf_osc_);
      
      // Filter
      PERF_MON_START(perf_filter_);
      sig = ProcessFilter(sig);
      PERF_MON_END(perf_filter_);
      
      // Effects
      PERF_MON_START(perf_effects_);
      out[i] = ProcessEffects(sig);
      PERF_MON_END(perf_effects_);
    }
  }
  
  void PrintStats() {
    for (uint8_t i = 0; i < 3; i++) {
      uint32_t avg = PERF_MON_GET_AVG(i);
      uint32_t peak = PERF_MON_GET_PEAK(i);
      uint32_t frames = PERF_MON_GET_FRAMES(i);
      printf("%s: avg=%u peak=%u frames=%u\n", 
             PERF_MON_GET_NAME(i), avg, peak, frames);
    }
  }
};
```

**Build with performance monitoring:**
```bash
# In your unit's config.mk
UDEFS = -DPERF_MON
```

**When disabled (default), all `PERF_MON_*` macros compile to nothing - zero overhead.**

---

## Design Principles

### Real-Time Safe
- No dynamic allocation (no `new`, `malloc`)
- No blocking operations
- Fixed-size buffers
- Suitable for audio callback context

### Header-Only
- All utilities are header-only templates or inline functions
- No separate compilation required
- Easy to integrate into build system

### Type Safety
- Templates provide compile-time type checking
- Range validation built-in
- Const-correctness throughout

### Zero Overhead
- Inline functions where appropriate
- Template specialization avoids runtime dispatch
- Compiler optimization friendly

---

## Integration

### In config.mk

Already included via default SDK paths:
```makefile
COMMON_INC_PATH = /workspace/drumlogue/common
COMMON_SRC_PATH = /workspace/drumlogue/common
```

### In Source Files

Simply include the header:
```cpp
#include "common/hub_control.h"
#include "common/param_format.h"
#include "common/preset_manager.h"
#include "common/midi_helper.h"
#include "common/smoothed_value.h"
```

---

## Contributing

When adding new common utilities:

1. **Make it generic** - Should be useful for multiple units
2. **Keep it header-only** - Avoid separate .cc files
3. **Document thoroughly** - Include usage examples
4. **Real-time safe** - No allocations, no blocking
5. **Test it** - Add desktop test if appropriate

---

## License

Common utilities follow the same license as the drumlogue-units repository.
