---
applyTo: "drumlogue/**/*.{c,cc,cpp,h,hpp}"
description: "C/C++ coding standards for drumlogue DSP development"
---

# C/C++ Coding Standards for Drumlogue Units

## Language Standards

- **C++17** for implementation files (`.cc`, `.cpp`)
- **C11** for header metadata (`header.c`)
- Use ARM GCC toolchain (arm-none-eabi-gcc)
- Compile with `-std=c++17 -O2 -Wall -Wextra`

## Code Style

### Naming Conventions
- Classes: `PascalCase` (e.g., `GranularProcessor`, `CloudsFx`)
- Functions/methods: `snake_case` (e.g., `process_audio`, `set_param`)
- Constants: `kPascalCase` (e.g., `kSampleRate`, `kMaxGrainSize`)
- Macros: `SCREAMING_SNAKE_CASE` (e.g., `SAMPLE_RATE`, `BUFFER_SIZE`)
- Member variables: `snake_case_` with trailing underscore

### File Organization
```cpp
// Standard order in header files:
1. Include guard or #pragma once
2. System includes (<...>)
3. SDK includes ("drumlogue/...")
4. Project includes ("...")
5. Constants and macros
6. Forward declarations
7. Class/struct definitions
8. Inline function implementations
```

### Example Header:
```cpp
#pragma once

#include <cstdint>
#include <cmath>

#include "drumlogue/unit.h"

namespace dsp {

constexpr uint32_t kSampleRate = 48000;
constexpr size_t kMaxDelay = 96000;

class DelayLine {
 public:
  void Init(float* buffer, size_t size);
  float Read(float delay_time);
  void Write(float sample);
  
 private:
  float* buffer_;
  size_t size_;
  size_t write_pos_;
};

}  // namespace dsp
```

## Real-Time Audio Constraints

### Critical Rules
- **NO dynamic allocation** in `unit_render()` (no `new`, `malloc`, `std::vector::push_back`)
- **NO blocking operations** (no mutexes, file I/O, network)
- **NO virtual functions** in hot paths (prefer templates or function pointers)
- **NO exceptions** (compile with `-fno-exceptions`)
- **NO recursion** in audio callback
- **ALWAYS return from rendering functions** - missing returns cause silent failures

### Memory Management
```cpp
// ✅ GOOD: Pre-allocate in unit_init()
static float delay_buffer[kMaxDelay];

void unit_init(const unit_runtime_desc_t* runtime) {
  memset(delay_buffer, 0, sizeof(delay_buffer));
}

// ❌ BAD: Allocate in unit_render()
void unit_render(...) {
  float* buffer = new float[size];  // NEVER DO THIS
}

// ✅ GOOD: Use std::nothrow for rare init-time allocations
void Init() {
  buffer_ = new (std::nothrow) float[size];
  if (buffer_ == nullptr) {
    // Handle allocation failure gracefully
    size_ = 0;
    return;
  }
}
```

### Fixed-Size Buffers
```cpp
// Use static arrays or member arrays
class Reverb {
 private:
  static constexpr size_t kBufferSize = 8192;
  float buffer_[kBufferSize];
};
```

## DSP Best Practices

### Initialize All State
```cpp
void unit_init(const unit_runtime_desc_t* runtime) {
  // Zero all buffers
  memset(&state, 0, sizeof(state));
  
  // Initialize parameters to defaults
  gain_ = 1.0f;
  mix_ = 0.5f;
  
  // Initialize oscillator phase
  phase_ = 0.0f;
}
```

### Parameter Smoothing
```cpp
// Avoid zipper noise
class SmoothedParam {
 public:
  void SetTarget(float target) { target_ = target; }
  
  float Process() {
    // One-pole filter: ~10ms smoothing at 48kHz
    const float alpha = 0.999f;
    current_ = current_ * alpha + target_ * (1.0f - alpha);
    return current_;
  }
  
 private:
  float current_ = 0.0f;
  float target_ = 0.0f;
};
```

### Numerical Stability
```cpp
// Check for division by zero
float SafeDivide(float num, float denom) {
  if (fabsf(denom) < 1e-9f) return 0.0f;
  return num / denom;
}

// Clamp to valid range
float Clamp(float value, float min, float max) {
  if (value < min) return min;
  if (value > max) return max;
  return value;
}

// Prevent denormals
inline float FlushDenormal(float x) {
  if (fabsf(x) < 1e-15f) return 0.0f;
  return x;
}
```

### Lookup Tables
```cpp
// Use for expensive operations (sin, exp, log)
static constexpr size_t kTableSize = 2048;
static float sin_table[kTableSize];

void InitSinTable() {
  for (size_t i = 0; i < kTableSize; i++) {
    sin_table[i] = sinf(2.0f * M_PI * i / kTableSize);
  }
}

float FastSin(float phase) {
  // phase in [0, 1)
  size_t index = static_cast<size_t>(phase * kTableSize) % kTableSize;
  return sin_table[index];
}
```

## Platform-Specific Code

### ARM Optimization
```cpp
// Use ARM NEON intrinsics when beneficial
#ifdef __ARM_NEON
#include <arm_neon.h>

void ProcessBlock(float* buffer, size_t size) {
  // NEON vectorized processing
  for (size_t i = 0; i < size; i += 4) {
    float32x4_t vec = vld1q_f32(&buffer[i]);
    vec = vmulq_n_f32(vec, 0.5f);  // Example: scale by 0.5
    vst1q_f32(&buffer[i], vec);
  }
}
#endif
```

### NEON Class Safety
```cpp
// Classes with NEON state must delete copy operations
class NeonProcessor {
 public:
  NeonProcessor() = default;
  ~NeonProcessor() = default;
  
  // Prevent accidental copies (NEON registers are expensive to copy)
  NeonProcessor(const NeonProcessor&) = delete;
  NeonProcessor& operator=(const NeonProcessor&) = delete;
  
  // Allow moves if needed
  NeonProcessor(NeonProcessor&&) = default;
  NeonProcessor& operator=(NeonProcessor&&) = default;
  
 private:
  float32x4_t state_[16];  // NEON registers
};
```

### Desktop Test Code
```cpp
// Conditionally compile test-only code
#ifdef TEST
  #include <iostream>
  #define DEBUG_PRINT(x) std::cout << x << std::endl
#else
  #define DEBUG_PRINT(x)
#endif
```

### QEMU ARM Testing
```cpp
// Hardware register access won't work in QEMU user-mode
// Use fallback for cycle counting in testing
#include <chrono>

static inline uint32_t GetCycleCount() {
#if defined(TEST) || defined(__QEMU_ARM__)
  // Fallback: use std::chrono for testing
  using namespace std::chrono;
  static auto start_time = high_resolution_clock::now();
  auto now = high_resolution_clock::now();
  auto elapsed_us = duration_cast<microseconds>(now - start_time).count();
  return static_cast<uint32_t>(elapsed_us * 900);  // 900MHz simulation
#else
  // Real hardware: ARM DWT PMCCNTR register
  volatile uint32_t* pmccntr = (volatile uint32_t*)0xE0001004;
  return *pmccntr;
#endif
}
```

## Common Patterns

### Unit Render Loop
```cpp
void unit_render(const float* in, float* out, 
                 uint32_t frames, uint8_t channels) {
  for (uint32_t i = 0; i < frames; i++) {
    // Read input(s)
    float in_l = in[i * channels];
    float in_r = (channels > 1) ? in[i * channels + 1] : in_l;
    
    // Process
    float out_l = ProcessSample(in_l);
    float out_r = ProcessSample(in_r);
    
    // Write output(s)
    out[i * channels] = out_l;
    if (channels > 1) {
      out[i * channels + 1] = out_r;
    }
  }
}
```

### Renderer Functions Must Return Values
```cpp
// ❌ BAD: Missing return statement causes silent failure
void Render(const float* in, float* out, uint32_t frames) {
  // ... process audio ...
  // Oops, forgot to return - compiler won't catch this!
}

// ✅ GOOD: Always return, even if void
void Render(const float* in, float* out, uint32_t frames) {
  // ... process audio ...
  return;  // Explicit return for clarity
}

// ✅ BETTER: Use [[nodiscard]] for non-void returns
[[nodiscard]] bool Render(const float* in, float* out, uint32_t frames) {
  // ... process audio ...
  return true;  // Compiler warns if caller ignores return
}
```

### Parameter Handling
```cpp
void unit_set_param_value(uint8_t id, int32_t value) {
  switch (id) {
    case 0:  // TIME (0-100%)
      // Convert to DSP range
      float time = value / 100.0f;
      delay_time_.SetTarget(time * kMaxDelayTime);
      break;
      
    case 1:  // FEEDBACK (0-100%)
      float fb = value / 100.0f;
      feedback_.SetTarget(fb);
      break;
  }
}
```

## Avoid These Patterns

### Don't Use iostream in Audio Code
```cpp
// ❌ BAD
#include <iostream>
void unit_render(...) {
  std::cout << "Processing frame" << std::endl;  // TOO SLOW
}

// ✅ GOOD: Use debug flags for desktop testing only
#ifdef TEST
  #include <iostream>
#endif
```

### Don't Use STL Containers Dynamically
```cpp
// ❌ BAD
std::vector<float> buffer;
void unit_render(...) {
  buffer.push_back(sample);  // Dynamic allocation
}

// ✅ GOOD
static constexpr size_t kMaxSize = 1024;
std::array<float, kMaxSize> buffer;
size_t buffer_pos = 0;
```

### Don't Use Virtual Functions in Hot Paths
```cpp
// ❌ BAD: Virtual call overhead
class Oscillator {
 public:
  virtual float Process() = 0;
};

// ✅ GOOD: Template or function pointer
template <typename OscType>
class Voice {
  float Process() {
    return osc_.Process();  // Inlined
  }
  OscType osc_;
};
```

## Testing Guidelines

### Unit Tests (Desktop)
```cpp
// test/<unit>/main.cc
#include "unit.h"
#include "wav_file.h"

int main(int argc, char** argv) {
  // Load input WAV
  WavFile input(argv[1]);
  
  // Initialize DSP
  MyDSP dsp;
  dsp.Init(48000);
  
  // Process
  std::vector<float> output(input.size());
  dsp.Process(input.data(), output.data(), input.size());
  
  // Write output WAV
  WavFile::Write(argv[2], output.data(), output.size(), 48000);
  
  return 0;
}
```

### Compile for Testing
```makefile
# test/<unit>/Makefile
CXX := g++
CXXFLAGS := -std=c++17 -O2 -Wall -Wextra -DTEST
INCLUDES := -I../../drumlogue/<unit> -I../../eurorack
LDFLAGS := -lsndfile

test_binary: main.cc $(DSP_SOURCES)
	$(CXX) $(CXXFLAGS) $(INCLUDES) $^ $(LDFLAGS) -o $@
```

## Documentation

### Comment Complex Algorithms
```cpp
// Explain DSP math, cite papers if applicable
// Moog ladder filter, based on Huovilainen 2004
// https://dafx.de/paper-archive/2004/P_061.pdf
float MoogFilter(float input) {
  // Thermal feedback path
  float thermal = 0.5f * tanh(input - 4.0f * resonance_ * state_[3]);
  
  // 4 cascaded one-pole filters
  for (int i = 0; i < 4; i++) {
    state_[i] = state_[i] + cutoff_ * (thermal - state_[i]);
    thermal = state_[i];
  }
  
  return state_[3];
}
```

### Document Parameter Ranges
```cpp
// In header.c, be explicit about ranges
{
  .id = 0,
  .min = 0,
  .max = 100,      // 0-100%
  .center = 50,
  .type = k_unit_param_type_percent,
  .name = "CUTOFF"   // Controls filter cutoff frequency
}
```

## Performance Tips

- Prefer `float` over `double` on ARM (native 32-bit FPU)
- Use `constexpr` for compile-time constants
- Mark functions `inline` when appropriate
- Avoid branching in tight loops (use branchless techniques)
- Profile on actual hardware (ARM performance differs from x86)
- Use `-O2` or `-O3` optimization
- Consider lookup tables for transcendental functions
- Use `powf()` not `pow()` for single-precision (ARM has hardware sqrtf/powf)
- Normalize MIDI velocity: `velocity / 127.0f` not `/128.0f`

## Security

- Validate all input parameters (bounds check)
- Check array indices before access
- Avoid buffer overflows
- Use `size_t` for array sizes, not `int`
- Clear sensitive data (e.g., `memset` after use)
