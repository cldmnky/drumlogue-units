---
agent: 'agent'
model: Claude Sonnet 4
tools: ['codebase', 'edit', 'runCommands', 'search', 'problems']
description: 'Debug drumlogue unit build or runtime issues'
---

# Debug Drumlogue Unit

You are helping debug a drumlogue unit. First, identify the issue type:

## Issue Types

### 1. Build Failures
**Symptoms:** `./build.sh <unit>` fails with compiler errors

**Debug steps:**
1. Check podman/docker container logs from build output
2. Verify `config.mk`:
   - Correct PROJECT name
   - Valid PROJECT_TYPE (synth|delfx|revfx|masterfx)
   - All source files listed in CSRC/CXXSRC exist
   - Include paths are correct (use /workspace/ prefix)
3. Check `Makefile` matches SDK template
4. Verify all source files compile independently
5. Look for missing includes or undefined symbols

**Common fixes:**
- Add missing source files to `config.mk`
- Fix include paths (UINCDIR, COMMON_INC_PATH)
- Add missing library paths (ULIBDIR, ULIBS)
- Check for C++17 compatibility issues

### 2. Linker Errors / Undefined Symbols
**Symptoms:** Build succeeds but `.drmlgunit` has undefined symbols

**Debug steps:**
```bash
# Check for undefined symbols
objdump -T drumlogue/<unit>/<unit>.drmlgunit | grep "UND"

# Compare with working unit
objdump -T drumlogue/elementish-synth/elementish_synth.drmlgunit | grep "UND"
```

**Common causes:**
1. **Undefined `static constexpr` members:**
   ```cpp
   // In header.h
   class Foo {
     static constexpr float kValues[] = {1.0, 2.0};
   };
   
   // MUST add in unit.cc
   constexpr float Foo::kValues[];
   ```

2. **Missing library functions:** Check if symbol is from standard library (expected) or your code (error)

3. **Incorrect function signatures:** Verify unit callbacks match SDK API

**Fix:**
- Add out-of-class definitions for static constexpr members
- Link required libraries in `config.mk`
- Check function prototypes match SDK headers

### 3. Hardware Load Errors
**Symptoms:** Unit fails to load on drumlogue (error message on device)

**Debug steps:**
1. Check symbol table: `objdump -T <unit>.drmlgunit | grep "UND"`
2. Verify `header.c`:
   - Correct module type (`k_unit_module_synth`, etc.)
   - Valid developer ID (not reserved)
   - Valid unit ID (0-127)
   - Unit name is 7-bit ASCII, ≤13 chars
   - Parameter names are 7-bit ASCII
3. Check version encoding: `0xMMNNPPU` format
4. Ensure parameter array size ≤24 descriptors

**Common fixes:**
- Fix non-ASCII characters in strings
- Correct module type in header.c
- Add missing parameter descriptors
- Use proper parameter types

### 4. Runtime DSP Issues
**Symptoms:** Unit loads but sounds wrong, glitches, or crashes

**Debug approach:**
1. **Build desktop test harness:**
   ```bash
   cd test/<unit>
   make
   make generate-fixtures
   ```

2. **Test offline:**
   ```bash
   ./unit_test fixtures/sine_440.wav output.wav --param1 50
   ```

3. **Analyze output:**
   - Listen for artifacts
   - Check for silence, clicks, distortion
   - Use Audacity for visual inspection

4. **Check DSP implementation:**
   - Buffer bounds (frames_per_buffer)
   - Channel handling (mono vs stereo)
   - Parameter scaling/ranges
   - Sample rate assumptions (48kHz)
   - Division by zero, NaN, inf
   - Uninitialized state variables

**Common fixes:**
- Initialize all DSP state in `unit_init()`
- Add parameter smoothing to avoid zipper noise
- Clamp values to prevent overflow
- Check for denormal numbers
- Validate array indexing

## Debugging Checklist

- [ ] Build succeeds without warnings
- [ ] No undefined symbols (except standard library)
- [ ] `header.c` validated (names, IDs, parameters)
- [ ] Desktop test harness runs without errors
- [ ] DSP sounds correct in offline tests
- [ ] All parameters work as expected
- [ ] Hardware loads without errors
- [ ] Hardware runtime performs correctly

## Advanced Debugging

**Memory/performance issues:**
- Check stack usage (limited on ARM)
- Avoid dynamic allocation in `unit_render()`
- Profile CPU usage (aim for <80% at 48kHz)
- Use fixed-point where appropriate

**MIDI/tempo issues:**
- Implement MIDI handlers if needed
- Verify tempo sync callback
- Check note on/off handling

**Preset issues:**
- Implement preset load/save callbacks
- Validate preset data structures
- Test preset recall on hardware
