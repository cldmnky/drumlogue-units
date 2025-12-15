# Common Utilities Quick Reference

Quick lookup for using `drumlogue/common/` utilities in your units.

---

## Hub Control

**Include**: `#include "common/hub_control.h"`

```cpp
// Define destinations (in .h file)
static constexpr common::HubControl<8>::Destination kModDests[] = {
  {"NAME", "unit", min, max, default, bipolar}
};

// Create instance
common::HubControl<8> hub_{kModDests};

// Set/Get
hub_.SetDestination(idx);           // Set selector
hub_.SetValue(value);               // Set value for current dest
int32_t val = hub_.GetValue(idx);   // Get value for dest idx

// Display
const char* name = hub_.GetDestinationName(idx);
const char* str = hub_.GetValueString(buf, size);
```

---

## Parameter Formatting

**Include**: `#include "common/param_format.h"`

```cpp
static char buf[16];

// Percentage: "50%"
ParamFormat::Percent(buf, sizeof(buf), 50);

// Bipolar: "-50%" to "+50%"
ParamFormat::BipolarPercent(buf, sizeof(buf), value, 0, 100);

// Frequency: "440Hz" or "4.4kHz"
ParamFormat::Frequency(buf, sizeof(buf), 440.0f);

// Time: "50ms" or "1.5s"
ParamFormat::Time(buf, sizeof(buf), 1500.0f);

// Decibels: "-12.0dB"
ParamFormat::Decibels(buf, sizeof(buf), -12.0f);

// Pitch: "+25c" or "-2st"
ParamFormat::Pitch(buf, sizeof(buf), 25.0f);

// Octave: "16'" / "8'" / "4'"
ParamFormat::OctaveRange(1);  // "8'"

// Note: "C4"
ParamFormat::NoteName(buf, sizeof(buf), 60);

// On/Off
ParamFormat::OnOff(true);  // "ON"
```

---

## Preset Manager

**Include**: `#include "common/preset_manager.h"`

```cpp
// Define presets (in .h file)
static constexpr common::PresetManager<24>::Preset kPresets[] = {
  {"Name", {param0, param1, ... param23}}
};

// Create manager
common::PresetManager<24> mgr_{kPresets, num_presets};

// Load
mgr_.LoadPreset(idx);

// Get parameter from current preset
int32_t val = mgr_.GetParam(param_id);

// Set parameter in current preset
mgr_.SetParam(param_id, value);

// Names
const char* name = mgr_.GetPresetName(idx);
uint8_t current = mgr_.GetCurrentIndex();
```

---

## MIDI Helper

**Include**: `#include "common/midi_helper.h"`

```cpp
// Note to frequency: A4=440Hz
float freq = MidiHelper::NoteToFreq(69);  // 440.0

// Velocity to 0-1
float vel = MidiHelper::VelocityToFloat(100);  // 0.787

// Velocity with curve
float vel = MidiHelper::VelocityToFloatCurved(100, 2.0f);

// Pitch bend to semitones (±2 default)
float st = MidiHelper::PitchBendToSemitones(8192 + 4096, 2.0f);  // +1.0

// Pitch bend to frequency multiplier
float mult = MidiHelper::PitchBendToMultiplier(bend, 2.0f);

// Cents/semitones to ratio
float ratio = MidiHelper::CentsToRatio(100.0f);      // 1.0595 (1 semitone)
float ratio = MidiHelper::SemitonesToRatio(12.0f);   // 2.0 (octave)

// Note name/octave
const char* name = MidiHelper::NoteName(60);    // "C"
int8_t octave = MidiHelper::NoteOctave(60);     // 4

// CC to float
float val = MidiHelper::CCToFloat(64);          // 0.503
float bipolar = MidiHelper::CCToBipolar(64);    // 0.0 (centered)
```

---

## Smoothed Value

**Include**: `#include "common/smoothed_value.h"`

```cpp
// Create smoother
common::SmoothedValue cutoff_;

// Initialize (initial_value, smoothing_time)
cutoff_.Init(0.5f, 0.005f);  // 5ms smoothing

// Set target
cutoff_.SetTarget(0.8f);

// Process (call once per sample)
float smooth = cutoff_.Process();

// Use in render loop
for (uint32_t i = 0; i < frames; i++) {
  float val = cutoff_.Process();
  // ... use smoothed value
}
```

---

## Common Patterns

### Hub Control in SetParameter

```cpp
void SetParameter(uint8_t id, int32_t value) {
  switch (id) {
    case PARAM_HUB_SEL:
      hub_.SetDestination(value);
      break;
    case PARAM_HUB_VAL:
      hub_.SetValue(value);
      break;
  }
}
```

### Hub Control in GetParameterStr

```cpp
const char* GetParameterStr(uint8_t id, int32_t value) {
  static char buf[16];
  
  switch (id) {
    case PARAM_HUB_SEL:
      return hub_.GetDestinationName(value);
    case PARAM_HUB_VAL:
      return hub_.GetValueString(buf, sizeof(buf));
  }
  return "";
}
```

### Hub Values in Render

```cpp
void Render(float* out, uint32_t frames) {
  // Extract hub values once per buffer
  float lfo_pwm = hub_.GetValue(0) / 100.0f;
  float env_vcf = (hub_.GetValue(1) - 50) / 50.0f;  // bipolar
  
  // Use in DSP...
}
```

### Preset Loading

```cpp
void LoadPreset(uint8_t idx) {
  if (preset_mgr_.LoadPreset(idx)) {
    // Apply all 24 parameters
    for (uint8_t i = 0; i < 24; i++) {
      SetParameter(i, preset_mgr_.GetParam(i));
    }
  }
}
```

### MIDI Note On

```cpp
void NoteOn(uint8_t note, uint8_t velocity) {
  float freq = MidiHelper::NoteToFreq(note);
  float vel = MidiHelper::VelocityToFloatCurved(velocity, 2.0f);
  
  osc_.SetFrequency(freq);
  env_.Trigger(vel);
}
```

---

## Full Documentation

See [drumlogue/common/README.md](../common/README.md) for comprehensive examples and API reference.
