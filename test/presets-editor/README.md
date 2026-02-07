# Drumlogue Presets Editor

A native macOS/Linux application for creating, editing, and testing drumlogue unit presets.

## Overview

This tool allows you to:
- Load drumlogue units as native shared libraries (.dylib/.so)
- Edit all parameters in real-time with a GUI
- Create and manage presets
- Test units with live audio or test signals
- Export presets (future: compatible with hardware)

## Key Features

- **Native NEON Support**: On Apple Silicon (ARM64), uses native NEON instructions for 1:1 performance with hardware
- **Real-Time Audio**: Low-latency audio processing via PortAudio
- **No Code Changes**: Units compile as-is, same source as hardware builds
- **Cross-Platform**: macOS (ARM64/x86_64) and Linux (ARM64/x86_64)

## Architecture

```
┌──────────────┐
│  GUI (ImGui) │
└──────┬───────┘
       │
┌──────▼────────┐
│  Unit Loader  │ (dlopen/dlsym)
└──────┬────────┘
       │
┌──────▼────────┐
│ Unit (.dylib) │ (clouds-revfx, elementish-synth, etc.)
└──────┬────────┘
       │
┌──────▼────────┐
│ Audio Engine  │ (PortAudio, 48kHz, low-latency)
└───────────────┘
```

## Status

**Current Phase:** Planning  
**See:** [IMPLEMENTATION_PLAN.md](IMPLEMENTATION_PLAN.md) for detailed roadmap

## Quick Start (Phase 1)

```bash
# Build the native host CLI (phase 1)
cd test/presets-editor
make

# Build a unit as native library (example: clouds-revfx)
./build-system/build-native.sh clouds-revfx

# Sanity check load/init/render (one buffer)
./bin/presets-editor-cli --unit units/clouds_revfx.dylib

# Real-time test (PortAudio) for 5 seconds
./bin/presets-editor-cli --unit units/clouds_revfx.dylib --rt 5
# Note: requires PortAudio installed (brew install portaudio). Without it, the CLI builds
# but will skip real-time audio and print an error.

# Phase 3: GUI prototype (SDL2 + ImGui)
make gui  # builds bin/presets-editor-gui if SDL2 + ImGui are present (pkg-config imgui, sdl2)
./bin/presets-editor-gui --unit units/clouds_revfx.dylib
```

If SDL2/ImGui are missing, the gui target will print a dependency hint. On macOS:

```bash
brew install sdl2 imgui
```

## Building Drumpler ROM Variants

The **drumpler** unit has multiple ROM variants (internal + 19 SR-JV80 expansions). To build all variants as native libraries:

```bash
cd test/presets-editor

# Build all ROM variants from roms.manifest
./build-system/build-drumpler-roms.sh
```

This creates separate `.dylib` files for each ROM variant:
- `drumpler_internal.dylib` (4.6 MB) - Internal ROM only
- `drumpler_orchestral.dylib` (13 MB) - SR-JV80-02 Orchestral
- `drumpler_pop.dylib` (13 MB) - SR-JV80-01 Pop
- ...and 17 more expansion variants

**Prerequisites:** ROM files must be downloaded first:
```bash
cd ../../drumlogue/drumpler
make -f Makefile.roms all
cd ../../test/presets-editor
```

**Control which ROMs build:** Edit `drumlogue/drumpler/roms.manifest` and comment out unwanted variants with `#`.

**Note:** Each variant has unique:
- Unit ID (0x00000005 + offset)
- Display name (shown in GUI)
- Embedded ROM data (4-13 MB per library)

## Requirements

### System
- macOS 11+ (ARM64 or x86_64) or Linux (ARM64/x86_64)
- 8GB RAM recommended
- Audio interface (built-in or external)

### Development
- C++17 compiler (clang/gcc)
- CMake 3.15+
- PortAudio
- SDL2 or GLFW
- Dear ImGui (vendored)

### macOS Installation
```bash
brew install portaudio sdl2 cmake
```

### Linux Installation
```bash
sudo apt-get install libportaudio2 portaudio19-dev libsdl2-dev cmake build-essential
```

## Architecture Notes

### Why Native Builds Work

1. **ARM64 → ARM64**: On Apple Silicon, we compile units with the same ARM architecture and NEON intrinsics as the hardware
2. **SDK Stubs**: Provide minimal runtime functions (sample access, MIDI, etc.) via dlopen
3. **No Modifications**: Units use the exact same source code as hardware builds
4. **Shared Libraries**: Compile as .dylib/.so instead of .drmlgunit, but the code is identical

### Advantages

- **Fast Iteration**: Compile and test in seconds, no hardware needed
- **Debugging**: Use native debuggers (lldb/gdb) and profilers (Instruments/perf)
- **Automation**: Scriptable preset generation and testing
- **Development**: Test DSP algorithms before deploying to hardware

### Limitations

- **Not Bit-Exact (x86_64)**: On Intel Macs, NEON→SSE translation may have minor differences
- **Hardware Validation Still Needed**: Always test final presets on actual drumlogue
- **Sample Access**: Mock samples, not actual drumlogue sample bank (yet)

## Project Structure

```
test/presets-editor/
├── IMPLEMENTATION_PLAN.md   # Detailed technical plan
├── README.md                # This file
├── build-system/            # Native unit compilation
├── sdk/                     # SDK runtime stubs
├── core/                    # Unit loader, parameter mapping
├── audio/                   # Audio engine (PortAudio)
├── gui/                     # User interface (ImGui)
├── presets/                 # Preset management
├── test/                    # Unit tests
└── vendor/                  # Third-party dependencies
```

## Development Phases

1. **Phase 1: Proof of Concept** ✅
   - Native unit compilation
   - Basic audio processing (CLI)
   - Verify NEON usage

2. **Phase 2: Audio Engine** ✅
   - Real-time audio with PortAudio
   - Lock-free parameter updates
   - CPU monitoring

3. **Phase 3: GUI** ✅
   - ImGui-based interface
   - Parameter controls (sliders)
   - Unit loading

4. **Phase 4: Presets** ✅
   - JSON preset format
   - Save/load/manage presets
   - Preset browser
   - Factory preset support (unit-defined presets)

5. **Phase 5: Polish** (planned)
   - Hardware comparison tests
   - Performance tuning
   - Documentation

6. **Phase 6: Advanced** (planned)
   - MIDI integration
   - Hardware preset export
   - A/B comparison

## Contributing

This is an experimental tool. Contributions welcome!

### Roadmap
- [ ] Phase 1: POC
- [ ] Phase 2: Audio Engine
- [ ] Phase 3: Basic GUI
- [ ] Phase 4: Preset System
- [ ] Phase 5: Testing & Polish
- [ ] Phase 6: Advanced Features

## Technical Details

### NEON Support

**macOS ARM64** (Apple Silicon):
- Native NEON intrinsics ✅
- Same codegen as hardware ✅

**macOS x86_64** (Intel):
- Uses sse2neon translation layer
- Minor performance/precision differences possible

**Linux ARM64**:
- Native NEON intrinsics ✅

**Linux x86_64**:
- Uses sse2neon translation layer

### Audio Configuration

- **Sample Rate**: 48000 Hz (drumlogue standard)
- **Buffer Size**: 128-256 samples (configurable)
- **Latency Target**: < 10ms
- **Channels**: 1-2 (mono/stereo)

### Unit Loading

Units are loaded via dlopen/dlsym:
1. Load .dylib/.so
2. Resolve symbols (unit_init, unit_render, etc.)
3. Initialize with runtime descriptor
4. Call unit_render in audio callback

Same approach as `test/qemu-arm/unit_host.c`.

### Preset System

Two types of presets are supported:

**User Presets** (JSON files in `presets/` directory):
- Create: Edit parameters, enter name, click "Save Preset"
- Load: Click "Load" in preset browser
- Delete: Click "Delete" button
- Format: JSON with parameter names and values

**Factory Presets** (built into units):
- Read-only presets defined by unit developer
- Loaded via unit callbacks (`unit_get_preset_name`, `unit_load_preset`)
- Example: pepege-synth has 6 factory presets (Default, Bass, Lead, Pad, Pluck, FX)
- Serve as starting points for custom presets

See [FACTORY_PRESETS.md](FACTORY_PRESETS.md) for details.

## FAQ

**Q: Why not just use the existing test harnesses?**  
A: They process WAV files offline. This tool provides real-time interaction and preset management.

**Q: Can I use this to create hardware presets?**  
A: Phase 4 uses JSON (editor-only). Phase 6 will add hardware export (.drmlgpreset format).

**Q: Does this work on Intel Macs?**  
A: Yes, via sse2neon translation, but may have minor differences from hardware.

**Q: Can I use MIDI controllers?**  
A: Planned for Phase 6.

**Q: Will units sound identical to hardware?**  
A: On ARM64: Yes (same architecture, NEON). On x86_64: Very close, but not bit-exact.

## License

Same as drumlogue-units repository (see top-level LICENSE).

## References

- [Implementation Plan](IMPLEMENTATION_PLAN.md) - Detailed technical design
- [Logue SDK](https://github.com/korginc/logue-sdk)
- [PortAudio](http://www.portaudio.com/)
- [Dear ImGui](https://github.com/ocornut/imgui)

---

**Status**: Phase 4 (Preset system). See [IMPLEMENTATION_PLAN.md](IMPLEMENTATION_PLAN.md) for details.
