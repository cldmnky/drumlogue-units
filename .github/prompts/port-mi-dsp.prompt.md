---
agent: 'agent'
model: Auto
tools: ['codebase', 'edit', 'search', 'fetch']
description: 'Port Mutable Instruments eurorack DSP to drumlogue'
---

# Port Mutable Instruments DSP to Drumlogue

You are porting DSP code from Mutable Instruments eurorack modules to a drumlogue unit.

## Prerequisites

Ask for:
1. **Target eurorack module** (e.g., clouds, braids, elements, plaits)
2. **Unit type** (synth, delfx, revfx, masterfx)
3. **Desired features** (which algorithms/modes to include)

## Porting Strategy

### 1. Study Upstream Module

**Location:** `eurorack/<module>/`

**Key files:**
- DSP classes (e.g., `clouds/dsp/granular_processor.h`)
- Resource files (`resources.cc`, `resources.h`)
- Test harnesses (`test/` directory)
- Hardware specs (README, sample rates, buffer sizes)

**Understand:**
- Original sample rate (often 32kHz, 48kHz, or 96kHz)
- Audio format (int16, float, fixed-point)
- Buffer sizes and latency
- Parameter ranges and encoding
- Calibration/scaling factors

### 2. Identify Core DSP

**Include:**
- DSP processing classes
- Lookup tables and resources
- Utility functions (filters, oscillators)

**Exclude:**
- Hardware drivers (ADC, DAC, GPIO)
- UI code (switches, LEDs, displays)
- MIDI/CV handling specific to eurorack
- Bootloader code

**Example (clouds):**
```
INCLUDE:
- dsp/granular_processor.h/cc
- dsp/fx/*.h (reverb, pitch_shifter, diffuser)
- resources.h/cc (lookup tables)
- stmlib/utils/*.h (utilities)

EXCLUDE:
- ui.h/cc (hardware UI)
- cv_scaler.h (CV calibration)
- settings.h (EEPROM storage)
```

### 3. Create Desktop Test Harness First

**Purpose:** Validate DSP without hardware dependency

**Steps:**
1. Create `test/<unit-name>/` directory
2. Write `Makefile` for host compilation:
   ```makefile
   CXX := g++
   CXXFLAGS := -std=c++17 -O2 -DTEST
   INCLUDES := -I../../drumlogue/<unit> -I../../eurorack
   LDFLAGS := -lsndfile
   ```
3. Implement `main.cc` for WAV processing
4. Build and test: `make && ./unit_test input.wav output.wav`

**Benefits:**
- Fast iteration (no container builds)
- Easy debugging (gdb, valgrind)
- Automated testing (CI)
- WAV file analysis

### 4. Adapt DSP to Drumlogue

**Sample rate:** Drumlogue is fixed at 48kHz
- If source is different, resample or adjust coefficients
- Update filter cutoffs, delay lengths, etc.

**Audio format:** Drumlogue uses float32
- Convert from int16/fixed-point to float
- Scale values appropriately (typically ±1.0)

**Buffer size:** Variable (`frames_per_buffer` from runtime descriptor)
- Don't assume fixed size
- Process in loops

**Channels:** Check `input_channels`, `output_channels`
- Most eurorack is mono or stereo
- Drumlogue synths are mono, effects are stereo

**Example adaptation:**
```cpp
// Original (eurorack): 32kHz, int16, 32 samples
void Process(int16_t* input, int16_t* output, size_t size) {
  for (size_t i = 0; i < size; i++) {
    output[i] = process_sample(input[i]);
  }
}

// Adapted (drumlogue): 48kHz, float, variable samples
void unit_render(unit_runtime_desc_t* runtime, /* ... */) {
  const float* in = runtime->input_buffer;
  float* out = runtime->output_buffer;
  const uint32_t frames = runtime->frames_per_buffer;
  
  for (uint32_t i = 0; i < frames; i++) {
    out[i] = process_sample(in[i]);
  }
}
```

### 5. Map Parameters

**Eurorack parameters** are typically:
- Potentiometers (0-4095, 12-bit ADC)
- CV inputs (-5V to +5V)

**Drumlogue parameters** are:
- Percentage (0-100)
- Integer ranges (min-max)
- Strings (indexed selection)
- dB (gain controls)

**Mapping strategy:**
```cpp
// header.c
{
  .id = 0,
  .min = 0,
  .max = 100,
  .center = 50,
  .type = k_unit_param_type_percent,
  .name = "GRAIN SIZE"
}

// unit.cc
void unit_set_param_value(uint8_t id, int32_t value) {
  switch (id) {
    case 0:  // GRAIN SIZE
      // Map 0-100 to eurorack 0-4095 range
      uint16_t grain_size = (value * 4095) / 100;
      dsp.set_grain_size(grain_size);
      break;
  }
}
```

### 6. Handle Resources/Lookup Tables

**Many MI modules use lookup tables:**
- Waveforms, windows
- Exponential curves
- Filter coefficients

**Resource generation:**
1. Include resource scripts from `eurorack/<module>/resources/`
2. Adapt to drumlogue build (config.mk)
3. Or pre-generate and include as static data

**Example:**
```makefile
# config.mk
RESOURCES = resources.cc
PYTHON = python3

resources.cc: resources.py
	$(PYTHON) resources.py
```

### 7. Build for Drumlogue

1. **Create unit structure:**
   ```
   drumlogue/<unit>/
   ├── config.mk
   ├── header.c
   ├── unit.cc
   ├── Makefile
   └── dsp/              # Ported MI DSP files
       ├── processor.h
       ├── processor.cc
       └── ...
   ```

2. **Update `config.mk`:**
   ```makefile
   PROJECT := mi_clouds
   PROJECT_TYPE := revfx
   
   CXXSRC = unit.cc dsp/processor.cc
   
   UINCDIR = dsp
   UINCDIR += /repo/eurorack
   UINCDIR += /repo/eurorack/stmlib
   ```

3. **Build:**
   ```bash
   ./build.sh <unit-name>
   ```

### 8. Test Thoroughly

**Desktop testing:**
- Process various input signals (impulse, sine, noise, drums)
- Verify parameter sweeps
- Check for artifacts, glitches

**Hardware testing:**
- Load to drumlogue
- Test all parameters
- Check CPU usage (should be <80%)
- Validate MIDI, tempo sync if applicable

## Common Issues

**Compiler differences:**
- ARM vs x86_64
- GCC versions
- C++ standard (use C++17)

**Hardware dependencies:**
- Remove ARM intrinsics unless using NEON
- Replace platform-specific code

**Performance:**
- Optimize hot paths
- Use lookup tables
- Consider fixed-point for expensive operations
- Profile on hardware

**Licensing:**
- Respect MI license (MIT for STM32 code)
- Attribute properly
- Don't use "Mutable Instruments" trademark

## Reference Examples

Study existing ports in this repository:
- `drumlogue/clouds-revfx/` - Clouds reverb/granular
- `drumlogue/elementish-synth/` - Elements modal synthesis
- `drumlogue/drupiter-synth/` - Plaits macro oscillator

Each has:
- Desktop test harness (`test/<unit>/`)
- Clean DSP abstraction
- Parameter mapping
- Build configuration
