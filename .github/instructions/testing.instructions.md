---
applyTo: "test/**/*.{c,cc,cpp,h}"
description: "Testing standards for drumlogue DSP units"
---

# Testing Standards for Drumlogue DSP Units

## Testing Strategy

### Three-Phase Testing
1. **Desktop testing** - Offline WAV processing (fast iteration)
2. **QEMU ARM testing** - ARM emulation with realistic execution environment
3. **Hardware testing** - On-device validation (final verification)

## Desktop Test Harness

### Purpose
- Validate DSP algorithms without hardware
- Rapid development iteration
- Automated CI/CD testing
- WAV file analysis and debugging

## QEMU ARM Testing

### Purpose
- Test actual `.drmlgunit` files in ARM environment
- Validate unit loading and SDK API interactions  
- ARM-specific execution and memory behavior
- Performance characteristics closer to hardware

### Setup
```bash
# Install ARM cross-compilation tools
# macOS:
brew install arm-linux-gnueabihf-binutils arm-linux-gnueabihf-gcc qemu libsndfile

# Linux:
sudo apt-get install gcc-arm-linux-gnueabihf qemu-system-arm libsndfile1-dev
```

### Usage
```bash
cd test/qemu-arm

# Build ARM unit host
make

# Test a unit in QEMU
./run_qemu.sh ../../drumlogue/clouds-revfx/clouds_revfx.drmlgunit \
              input.wav output.wav --param-0 75 --verbose

# Run complete test suite
./test_demo.sh
```

### Benefits over Desktop Testing
- **ARM execution**: Tests actual ARM code paths, NEON usage, memory alignment
- **Real unit loading**: Tests the actual `.drmlgunit` binary loading mechanism  
- **SDK integration**: Validates unit callbacks and SDK API interactions
- **Performance**: ARM-realistic performance characteristics
- **Memory layout**: ARM-specific memory access patterns and cache behavior

### Architecture
```
┌─────────────────┐    ┌──────────────────┐    ┌─────────────────┐
│ Host (macOS)    │    │ QEMU ARM        │    │ .drmlgunit      │
│                 │    │                 │    │                 │
│ Input WAV   ────┼────┼──> unit_host ───┼────┼──> unit_render  │
│ Output WAV  ◄───┼────┼─── (dlopen)     │    │    unit_init    │
│ QEMU runner     │    │ ARM Cortex-A7   │    │    etc.         │
└─────────────────┘    └──────────────────┘    └─────────────────┘
```

The QEMU ARM framework includes:
- `unit_host.c` - Main unit host that loads and runs .drmlgunit files
- `sdk_stubs.c` - Minimal implementations of logue SDK runtime functions  
- `wav_file.c` - WAV file I/O utilities (ARM-compatible)
- `run_qemu.sh` - QEMU runner script for ARM emulation

### Structure
```
test/<unit-name>/
├── Makefile           # Host build configuration
├── README.md          # Test documentation
├── main.cc            # Test harness entry point
├── unit.h             # Unit-specific test utilities
├── wav_file.h         # WAV I/O helper
├── fixtures/          # Test input/output files
│   ├── impulse.wav
│   ├── sine_440.wav
│   ├── noise.wav
│   └── drums.wav
└── build/             # Build artifacts

test/qemu-arm/         # QEMU ARM testing framework
├── Makefile           # ARM cross-compilation
├── unit_host.c        # ARM unit host (loads .drmlgunit)
├── sdk_stubs.c        # Logue SDK runtime stubs
├── wav_file.c         # WAV I/O for ARM
├── run_qemu.sh        # QEMU runner script
└── test_demo.sh       # Complete test demonstration
```

### Makefile Template
```makefile
# Compiler for host machine
CXX := g++
CXXFLAGS := -std=c++17 -O2 -Wall -Wextra
CXXFLAGS += -DTEST  # Enable test-specific code paths

# Platform detection
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Darwin)
    BREW_PREFIX := $(shell brew --prefix 2>/dev/null)
    ifdef BREW_PREFIX
        CXXFLAGS += -I$(BREW_PREFIX)/include
        LDFLAGS := -L$(BREW_PREFIX)/lib -lsndfile
    endif
else
    LDFLAGS := -lsndfile
endif

# Paths to source code
REPO_ROOT := $(shell cd ../.. && pwd)
DSP_SRC := $(REPO_ROOT)/drumlogue/<unit-name>
EURORACK := $(REPO_ROOT)/eurorack

# Includes
INCLUDES := -I$(DSP_SRC) -I$(EURORACK) -I.

# Sources (exclude unit.cc which has hardware dependencies)
DSP_SOURCES := $(DSP_SRC)/<unit>_dsp.cc
TEST_SOURCES := main.cc

# Build
TARGET := <unit>_test
OBJECTS := $(BUILD_DIR)/main.o $(BUILD_DIR)/<unit>_dsp.o

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

clean:
	rm -rf build $(TARGET) fixtures/*.wav

# Test targets
fixtures:
	mkdir -p fixtures

generate-fixtures: $(TARGET) fixtures
	./$(TARGET) --generate-impulse fixtures/impulse.wav
	./$(TARGET) --generate-sine fixtures/sine_440.wav 440

test: $(TARGET) generate-fixtures
	@echo "=== Running DSP Tests ==="
	./$(TARGET) fixtures/impulse.wav fixtures/out_impulse.wav
	./$(TARGET) fixtures/sine_440.wav fixtures/out_sine.wav
	@echo "✓ All tests passed"
```

### Test Harness Implementation

**main.cc template:**
```cpp
#include <iostream>
#include <cstring>
#include <cstdlib>
#include "wav_file.h"
#include "<unit>_dsp.h"

void PrintUsage(const char* prog) {
  std::cerr << "Usage: " << prog << " input.wav output.wav [options]\n";
  std::cerr << "Options:\n";
  std::cerr << "  --param1 <value>    Set parameter 1 (0-100)\n";
  std::cerr << "  --param2 <value>    Set parameter 2 (0-100)\n";
  std::cerr << "\nTest signal generation:\n";
  std::cerr << "  --generate-impulse out.wav\n";
  std::cerr << "  --generate-sine out.wav <freq_hz>\n";
  std::cerr << "  --generate-noise out.wav\n";
}

// Generate test signals
void GenerateImpulse(const char* output, uint32_t sample_rate = 48000) {
  const size_t size = sample_rate;  // 1 second
  std::vector<float> buffer(size, 0.0f);
  buffer[0] = 1.0f;  // Unit impulse
  
  WavFile::Write(output, buffer.data(), size, sample_rate, 1);
  std::cout << "Generated impulse: " << output << std::endl;
}

void GenerateSine(const char* output, float freq, 
                  uint32_t sample_rate = 48000) {
  const size_t size = sample_rate;
  std::vector<float> buffer(size);
  
  for (size_t i = 0; i < size; i++) {
    float phase = 2.0f * M_PI * freq * i / sample_rate;
    buffer[i] = 0.5f * sinf(phase);
  }
  
  WavFile::Write(output, buffer.data(), size, sample_rate, 1);
  std::cout << "Generated sine " << freq << "Hz: " << output << std::endl;
}

void GenerateNoise(const char* output, uint32_t sample_rate = 48000) {
  const size_t size = sample_rate;
  std::vector<float> buffer(size);
  
  for (size_t i = 0; i < size; i++) {
    buffer[i] = (rand() / float(RAND_MAX)) * 2.0f - 1.0f;
  }
  
  WavFile::Write(output, buffer.data(), size, sample_rate, 1);
  std::cout << "Generated white noise: " << output << std::endl;
}

int main(int argc, char** argv) {
  if (argc < 2) {
    PrintUsage(argv[0]);
    return 1;
  }
  
  // Handle test signal generation
  if (strcmp(argv[1], "--generate-impulse") == 0 && argc >= 3) {
    GenerateImpulse(argv[2]);
    return 0;
  }
  if (strcmp(argv[1], "--generate-sine") == 0 && argc >= 4) {
    GenerateSine(argv[2], atof(argv[3]));
    return 0;
  }
  if (strcmp(argv[1], "--generate-noise") == 0 && argc >= 3) {
    GenerateNoise(argv[2]);
    return 0;
  }
  
  // Normal processing mode
  if (argc < 3) {
    PrintUsage(argv[0]);
    return 1;
  }
  
  const char* input_file = argv[1];
  const char* output_file = argv[2];
  
  // Load input WAV
  WavFile input(input_file);
  if (!input.IsValid()) {
    std::cerr << "Error: Could not load " << input_file << std::endl;
    return 1;
  }
  
  std::cout << "Input: " << input_file << std::endl;
  std::cout << "  Samples: " << input.frames() << std::endl;
  std::cout << "  Channels: " << input.channels() << std::endl;
  std::cout << "  Sample rate: " << input.sample_rate() << std::endl;
  
  // Initialize DSP
  MyDSP dsp;
  dsp.Init(input.sample_rate());
  
  // Parse parameters from command line
  for (int i = 3; i < argc; i += 2) {
    if (strcmp(argv[i], "--param1") == 0 && i + 1 < argc) {
      int value = atoi(argv[i + 1]);
      dsp.SetParam1(value);
      std::cout << "  Param1: " << value << std::endl;
    }
  }
  
  // Process audio
  std::vector<float> output(input.frames() * input.channels());
  dsp.Process(input.data(), output.data(), 
              input.frames(), input.channels());
  
  // Write output WAV
  WavFile::Write(output_file, output.data(), input.frames(),
                 input.sample_rate(), input.channels());
  
  std::cout << "Output: " << output_file << std::endl;
  
  return 0;
}
```

## Test Signal Types

### Impulse Response
**Purpose:** Reveals reverb tails, filter responses, system characteristics

```bash
./unit_test --generate-impulse fixtures/impulse.wav
./unit_test fixtures/impulse.wav fixtures/impulse_response.wav
```

**Analysis:** Import to Audacity, view spectrogram

### Sine Waves
**Purpose:** Test harmonic distortion, aliasing, filter cutoffs

```bash
# Low frequency
./unit_test --generate-sine fixtures/sine_100.wav 100

# Mid frequency
./unit_test --generate-sine fixtures/sine_1000.wav 1000

# High frequency (near Nyquist)
./unit_test --generate-sine fixtures/sine_20000.wav 20000
```

### White Noise
**Purpose:** Test frequency response, noise floor, stability

```bash
./unit_test --generate-noise fixtures/noise.wav
./unit_test fixtures/noise.wav fixtures/noise_filtered.wav
```

### Sweep
**Purpose:** Analyze frequency response over full spectrum

```cpp
void GenerateSweep(const char* output, uint32_t sample_rate = 48000) {
  const size_t size = 5 * sample_rate;  // 5 seconds
  std::vector<float> buffer(size);
  
  float start_freq = 20.0f;
  float end_freq = 20000.0f;
  
  for (size_t i = 0; i < size; i++) {
    float t = float(i) / size;
    // Exponential sweep
    float freq = start_freq * powf(end_freq / start_freq, t);
    float phase = 2.0f * M_PI * freq * i / sample_rate;
    buffer[i] = 0.5f * sinf(phase);
  }
  
  WavFile::Write(output, buffer.data(), size, sample_rate, 1);
}
```

## Automated Testing

### Makefile Test Target
```makefile
test: $(TARGET) generate-fixtures
	@echo "=== Test 1: Impulse response ==="
	./$(TARGET) fixtures/impulse.wav fixtures/out_impulse.wav
	
	@echo "=== Test 2: Sine wave ==="
	./$(TARGET) fixtures/sine_440.wav fixtures/out_sine.wav --freq 1000
	
	@echo "=== Test 3: White noise ==="
	./$(TARGET) fixtures/noise.wav fixtures/out_noise.wav
	
	@echo "=== All tests passed ==="
```

### CI Integration
See `.github/workflows/dsp-test.yml`:

```yaml
- name: Build test harness
  working-directory: test/<unit-name>
  run: make

- name: Run DSP tests
  working-directory: test/<unit-name>
  run: make test

- name: Upload test artifacts
  uses: actions/upload-artifact@v4
  with:
    name: test-wav-files
    path: test/<unit-name>/fixtures/*.wav
```

## Hardware Testing

### Checklist
- [ ] Unit loads without errors
- [ ] All parameters respond correctly
- [ ] Parameter ranges are appropriate
- [ ] No audio glitches or clicks
- [ ] CPU usage acceptable (<80%)
- [ ] Presets save/recall correctly
- [ ] MIDI handling works (if applicable)
- [ ] Tempo sync accurate (if applicable)
- [ ] No memory corruption (long-running stability)

### Test Procedure
1. Build: `./build.sh <unit-name>`
2. Copy `.drmlgunit` to drumlogue via USB (Units/ folder)
3. Load unit on device
4. Test each parameter across full range
5. Test with various input signals (drums, synth, external audio)
6. Check for artifacts (distortion, aliasing, zipper noise)
7. Stress test (extreme parameter values, polyphony)
8. Long-running stability (30+ minutes)

## Debugging Tests

### Print Debugging (Desktop Only)
```cpp
#ifdef TEST
  #include <iostream>
  #define DEBUG_PRINT(msg) std::cout << msg << std::endl
  #define DEBUG_VALUE(name, val) \
    std::cout << name << ": " << val << std::endl
#else
  #define DEBUG_PRINT(msg)
  #define DEBUG_VALUE(name, val)
#endif

void ProcessSample(float input) {
  DEBUG_VALUE("Input", input);
  float output = filter_.Process(input);
  DEBUG_VALUE("Output", output);
  return output;
}
```

### Valgrind for Memory Issues
```bash
valgrind --leak-check=full --show-leak-kinds=all \
  ./unit_test input.wav output.wav
```

### GDB for Crashes
```bash
gdb ./unit_test
(gdb) run fixtures/input.wav fixtures/output.wav
(gdb) backtrace
```

## Best Practices

- Write tests early (test-driven DSP development)
- Test on both macOS and Linux (CI does this automatically)
- Generate diverse test signals (impulse, sine, noise, sweeps)
- Analyze outputs visually (Audacity) and aurally
- Keep test runtime short (<5 seconds per test)
- Upload test artifacts to CI for inspection
- Test parameter sweeps, not just static values
- Validate numerical stability (no NaN, Inf, denormals)
- Check DC offset (should be near zero)
