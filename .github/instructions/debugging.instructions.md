---
applyTo: "**/*.{c,cc,cpp,h,mk,sh}"
description: "Debugging guidelines for drumlogue units"
---

# Debugging Drumlogue Units

## Build Issues

### Check for Undefined Symbols

When a unit fails to load on hardware, check for undefined symbols:

```bash
# Check for undefined symbols in the .drmlgunit file
objdump -T drumlogue/<unit>/<unit>.drmlgunit | grep "UND"

# Compare with a working unit
objdump -T drumlogue/elementish-synth/elementish_synth.drmlgunit | grep "UND"
```

**Expected undefined symbols** (provided by drumlogue runtime):
- `GLIBC_*` - Standard C library functions (sinf, cosf, memset, snprintf, etc.)
- `GCC_*` - GCC runtime (division, unwind, personality)
- `CXXABI_*` - C++ ABI functions (atexit, throw)
- `GLIBCXX_*` - C++ standard library (new/delete, iostream)

**Unexpected undefined symbols** indicate missing object files:
- Class methods from your code (e.g., `_ZN3dsp14VoiceAllocator...`)
- These mean the implementation wasn't compiled or linked

### Common Cause: Missing Object Files

**Symptom:** Class methods show as undefined, even though source file is in config.mk

**Root cause:** SDK Makefile may be compiling a different file with the same name

**Example:** If you have:
- `drumlogue/<unit>/dsp/voice_allocator.cc` (your implementation)
- `drumlogue/common/voice_allocator.cc` (stub file)

The SDK might compile the common stub instead of your implementation!

**Solution:**
1. Remove stub/deprecated files from `drumlogue/common/`
2. Use specific paths in config.mk: `dsp/voice_allocator.cc`
3. Rebuild and verify symbols are resolved

### Verify Build Artifacts

After building, check the build directory contains object files:

```bash
ls -la drumlogue/<unit>/build/obj/

# You should see .o files for each source file:
# - header.o
# - unit.o
# - your_dsp_file.o
# etc.
```

**Build artifacts preserved by build.sh:**
- `obj/*.o` - Individual compiled units
- `*.map` - Complete memory map and symbol table
- `*.list` - Human-readable assembly listing  
- `*.dmp` - Binary dump with section info
- `*.hex` - Intel HEX format
- `*.bin` - Raw binary image

### Analyze Symbol Sizes

Check if your code is actually in the binary:

```bash
# View symbol table from object files
nm drumlogue/<unit>/build/obj/your_file.o | grep YourClassName

# Check final binary size
ls -lh drumlogue/<unit>/<unit>.drmlgunit

# View memory map
less drumlogue/<unit>/build/<unit>.map
```

### Check Compilation Flags

Ensure required defines are set in config.mk:

```makefile
# Example: NEON optimization
UDEFS += -DUSE_NEON
UDEFS += -mfpu=neon
UDEFS += -mfloat-abi=hard

# Example: Performance monitoring
UDEFS += -DPERF_MON
CXXSRC += ../common/perf_mon.cc
```

### Static Constexpr Members

**Problem:** Class with `static constexpr` members fails to load

```cpp
class MyDSP {
    static constexpr float kValues[] = {1.0, 2.0};
};
```

**Cause:** C++14 requires out-of-class definitions for ODR-used members

**Solution:** Add definition in `.cc` file:
```cpp
// In my_dsp.cc
constexpr float MyDSP::kValues[];
```

## Runtime Issues

### Desktop Test Harness

Build and run desktop test harness for fast iteration:

```bash
cd test/<unit>
make clean && make

# Generate test signals
make generate-fixtures

# Process test signals
./unit_test fixtures/impulse.wav output.wav --param1 50

# Run full test suite
make test
```

### Enable Debug Logging

Add conditional debug output:

```cpp
#ifdef DEBUG
#include <cstdio>
#endif

void ProcessSample(float input) {
#ifdef DEBUG
    static int count = 0;
    if (count++ % 1000 == 0) {
        fprintf(stderr, "Sample %d: input=%.3f\n", count, input);
        fflush(stderr);
    }
#endif
    // ... processing ...
}
```

**Desktop:** Compile with `-DTEST` or `-DDEBUG`
**Hardware:** Not recommended (no stderr on device)

### Analyze Audio Output

**Visual analysis:**
1. Process WAV file through desktop test harness
2. Open output WAV in Audacity
3. View waveform and spectrogram
4. Check for:
   - Silence (DSP not running)
   - Clipping (exceeding ±1.0)
   - Distortion (unwanted harmonics)
   - Aliasing (frequency folding)
   - DC offset (non-zero mean)

**Numerical analysis:**
```bash
# Use sox to get statistics
sox input.wav -n stats

# Get peak, RMS, DC offset
# Max amplitude: should be ≤1.0
# DC offset: should be near 0
```

### Common DSP Issues

**Symptom:** Unit loads but produces silence

**Causes:**
- Uninitialized DSP state
- Gate/trigger not set
- Envelopes not triggered
- Output gain at zero

**Debug:**
```cpp
#ifdef DEBUG
fprintf(stderr, "Gate: %d, Note: %d, Env: %.3f\n", 
        gate_, current_note_, env_vca_.Process());
#endif
```

**Symptom:** Clicks or pops in audio

**Causes:**
- Missing parameter smoothing
- Uninitialized buffers
- Discontinuities in oscillators/filters
- Sample rate mismatch

**Fix:**
```cpp
// Add smoothing
SmoothedValue cutoff_;
cutoff_.Init(1000.0f, 0.01f);  // Init with starting value and time constant

// In render loop
float target = calculate_cutoff();
cutoff_.SetTarget(target);
float smooth = cutoff_.Process();  // Use smoothed value
```

**Symptom:** Distortion or harsh sound

**Causes:**
- Output clipping (exceeding ±1.0)
- Missing anti-aliasing
- Incorrect filter coefficients
- Denormal numbers

**Debug:**
```cpp
// Clamp output
output = std::clamp(output, -1.0f, 1.0f);

// Check for NaN/Inf
if (!std::isfinite(output)) {
    output = 0.0f;
#ifdef DEBUG
    fprintf(stderr, "WARNING: Non-finite output!\n");
#endif
}

// Flush denormals
inline float FlushDenormal(float x) {
    if (fabsf(x) < 1e-15f) return 0.0f;
    return x;
}
```

### Memory Debugging

**Desktop only** - use valgrind:

```bash
cd test/<unit>
make
valgrind --leak-check=full ./unit_test input.wav output.wav
```

Check for:
- Memory leaks
- Use after free
- Buffer overruns
- Uninitialized variables

### Performance Profiling

**Check CPU usage on device:**
- Monitor frame processing time
- Should be <80% of available cycles
- Use performance monitor if enabled

**Desktop profiling:**
```bash
# Build with profiling
cd test/<unit>
make CXXFLAGS="-g -O2 -pg"

# Run test
./unit_test input.wav output.wav

# View profile
gprof unit_test gmon.out
```

### Voice Allocation Issues

**Symptom:** Monophonic mode doesn't switch back to previous note

**Cause:** Not handling `NoteOffResult::retrigger` signal

**Debug:**
```cpp
void NoteOff(uint8_t note) {
    auto result = allocator_.NoteOff(note);
    
#ifdef DEBUG
    fprintf(stderr, "NoteOff: %d, retrigger: %d, note: %d\n",
            note, result.retrigger, result.retrigger_note);
#endif
    
    if (result.retrigger) {
        // Update to retriggered note
        current_note_ = result.retrigger_note;
        current_freq_ = NoteToFreq(result.retrigger_note);
    }
}
```

## UI Issues

### Flickering Display

**Symptom:** Screen flickers when navigating to parameter page

**Cause:** Excessive string formatting (snprintf) called on every frame

**Solution:** Cache formatted strings
```cpp
// In class
mutable char cached_str_[16] = "";
uint8_t last_value_ = 255;  // Invalid/uninitialized

const char* GetParameterStr(uint8_t id, int32_t value) {
    // Only reformat if value changed
    if (value == last_value_) {
        return cached_str_;  // Cache hit
    }
    
    // Cache miss - reformat
    last_value_ = value;
    snprintf(cached_str_, sizeof(cached_str_), "%d", value);
    return cached_str_;
}
```

### Parameter Not Responding

**Check parameter handler:**
```cpp
void SetParameter(uint8_t id, int32_t value) {
#ifdef DEBUG
    fprintf(stderr, "SetParam: id=%d, value=%d\n", id, value);
#endif
    
    switch (id) {
        case PARAM_CUTOFF:
            // Clamp and apply
            value = std::clamp(value, 0, 100);
            cutoff_ = value / 100.0f;
            break;
    }
}
```

## Hardware Debugging

### Unit Won't Load

**Check:**
1. Undefined symbols (see above)
2. File size reasonable (typically 50-200KB)
3. Version number valid (0xMMNNPPU format)
4. Developer ID not reserved
5. Unit name is 7-bit ASCII, ≤13 chars
6. Parameter descriptors valid (≤24 params)

### Unit Crashes Device

**Possible causes:**
- Stack overflow (deeply nested calls)
- Heap corruption (buffer overrun)
- Division by zero
- Invalid pointer access
- Infinite loop in render callback

**Prevention:**
```cpp
// Check buffer bounds
if (index >= buffer_size) return;

// Safe division
float SafeDivide(float num, float denom) {
    if (fabsf(denom) < 1e-9f) return 0.0f;
    return num / denom;
}

// Avoid recursion
// Use iterative algorithms instead
```

### Testing Checklist

Before deploying to hardware:

- [ ] Desktop tests pass
- [ ] No undefined symbols (except runtime)
- [ ] Binary size reasonable
- [ ] All parameters tested in UI
- [ ] Audio processed without artifacts
- [ ] No malloc/new in render callback
- [ ] All buffers initialized
- [ ] Parameters clamped to valid ranges
- [ ] Numerical stability verified (no NaN/Inf)

## Debugging Tools Summary

| Tool | Purpose | Platform |
|------|---------|----------|
| `objdump -T` | Check undefined symbols | All |
| `nm` | Analyze symbol table | All |
| Desktop test harness | Fast DSP iteration | Host |
| `valgrind` | Memory debugging | Linux/macOS |
| `gdb`/`lldb` | Interactive debugging | Host |
| Audacity | Audio visualization | All |
| `.map` file | Memory layout | All |
| `.list` file | Assembly inspection | All |
| `gprof` | Performance profiling | Host |

## When to Ask for Help

If you've tried the above and still stuck:

1. **Build issues:** Check SDK Makefile compatibility, path resolution
2. **Linking errors:** Verify all source files in config.mk, no name conflicts
3. **Runtime crashes:** May need ARM-specific debugging (JTAG, OpenOCD)
4. **DSP artifacts:** May need algorithm-specific expertise
5. **Performance:** May need NEON optimization or algorithm changes
