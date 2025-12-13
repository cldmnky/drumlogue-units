# Drumlogue Presets Editor - Implementation Plan

## Executive Summary

Create a native macOS/Linux application that:
1. Loads drumlogue units compiled as shared libraries (.dylib/.so)
2. Provides a UI mimicking the drumlogue hardware interface
3. Enables preset creation, editing, and testing
4. Processes audio in real-time with native NEON support (on ARM64)
5. Exports presets compatible with hardware

**Key Insight:** Since you're on Apple Silicon (arm64), NEON intrinsics will work natively without translation, enabling 1:1 performance characteristics with the drumlogue hardware (also ARM).

### Current Status (Dec 2025)

- Phase 1 (native host scaffold, dlopen loader, runtime stubs, native unit builds) ✅ complete
- Phase 2 (PortAudio engine, ring buffer, CPU load probe, CLI RT path) ✅ complete
- Phase 3 (GUI) ✅ complete — ImGui-based parameter panel with page navigation, audio controls, menu bar
- Phase 4 (Presets) 🚧 started — JSON preset format, save/load, preset browser

---

## Architecture Overview

```
┌─────────────────────────────────────────────────────────────┐
│                    Preset Editor GUI                         │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐      │
│  │  Parameter   │  │    Preset    │  │   Audio I/O  │      │
│  │   Controls   │  │   Manager    │  │   Routing    │      │
│  └──────────────┘  └──────────────┘  └──────────────┘      │
└─────────────────────────────────────────────────────────────┘
                            │
                            ▼
┌─────────────────────────────────────────────────────────────┐
│                   Unit Host Layer                            │
│  • Load/unload units (dlopen)                               │
│  • SDK runtime stubs (sample access, etc.)                  │
│  • Parameter mapping                                         │
│  • Audio buffer management                                   │
└─────────────────────────────────────────────────────────────┘
                            │
                            ▼
┌─────────────────────────────────────────────────────────────┐
│              Drumlogue Unit (.dylib/.so)                     │
│  • Compiled with NEON extensions enabled                     │
│  • Linked with SDK stubs                                     │
│  • Same source code as hardware build                        │
└─────────────────────────────────────────────────────────────┘
                            │
                            ▼
┌─────────────────────────────────────────────────────────────┐
│                  Audio Backend                               │
│  • PortAudio or CoreAudio (macOS)                           │
│  • Real-time audio thread                                    │
│  • Low-latency buffer management                             │
└─────────────────────────────────────────────────────────────┘
```

---

## Phase 1: Native Unit Host Foundation

### 1.1 Build System for Native Units

**Goal:** Compile drumlogue units as native shared libraries (.dylib on macOS, .so on Linux)

**Location:** `test/presets-editor/build-system/`

**Files to create:**
```
build-system/
├── Makefile.native          # Native compilation rules
├── build-native.sh          # Build script for units
└── config/
    ├── macos-arm64.mk       # macOS ARM64 config (NEON enabled)
    ├── macos-x86_64.mk      # macOS x86_64 config (SSE translation)
    └── linux-arm64.mk       # Linux ARM64 config
```

**Makefile.native template:**
```makefile
# Native unit compilation for preset editor
CC := clang
CXX := clang++
CXXFLAGS := -std=c++17 -O2 -Wall -Wextra -fPIC -DTEST -DNATIVE_BUILD

# Platform detection
UNAME_S := $(shell uname -s)
UNAME_M := $(shell uname -m)

# macOS ARM64 (Apple Silicon) - Native NEON
ifeq ($(UNAME_S),Darwin)
ifeq ($(UNAME_M),arm64)
    CXXFLAGS += -march=armv8-a -DARM_MATH_NEON
    LDFLAGS := -dynamiclib -framework Accelerate
    OUTPUT_EXT := .dylib
endif
endif

# macOS x86_64 - Use SSE2NEON translation
ifeq ($(UNAME_S),Darwin)
ifeq ($(UNAME_M),x86_64)
    CXXFLAGS += -msse4.2 -DSSE2NEON
    INCLUDES += -Ivendor/sse2neon
    LDFLAGS := -dynamiclib -framework Accelerate
    OUTPUT_EXT := .dylib
endif
endif

# Linux ARM64 - Native NEON
ifeq ($(UNAME_S),Linux)
ifeq ($(UNAME_M),aarch64)
    CXXFLAGS += -march=armv8-a -DARM_MATH_NEON
    LDFLAGS := -shared -lm
    OUTPUT_EXT := .so
endif
endif

# SDK includes
INCLUDES += -I../../../logue-sdk/platform/drumlogue/inc
INCLUDES += -I../../../logue-sdk/platform/drumlogue/inc/dsp
INCLUDES += -I../../../logue-sdk/platform/drumlogue/inc/utils

# Build target
$(PROJECT)$(OUTPUT_EXT): $(CXXSRC) $(CSRC)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o $@ $^ $(LDFLAGS)
```

**build-native.sh:**
```bash
#!/bin/bash
# Build a drumlogue unit as native shared library

set -e

UNIT_NAME=$1
UNIT_DIR="../../drumlogue/${UNIT_NAME}"

if [ ! -d "$UNIT_DIR" ]; then
    echo "Error: Unit directory not found: $UNIT_DIR"
    exit 1
fi

# Read config.mk to get sources
source $UNIT_DIR/config.mk

# Build native shared library
echo "Building ${UNIT_NAME} for native platform..."
make -f Makefile.native \
    PROJECT="${UNIT_NAME}" \
    PROJECT_DIR="${UNIT_DIR}" \
    CXXSRC="${CXXSRC}" \
    CSRC="${CSRC}"

echo "✓ Built: build/${UNIT_NAME}.dylib"
```

**Key differences from ARM build:**
- Output: `.dylib`/`.so` instead of `.drmlgunit`
- Compiler: Native clang/gcc instead of arm-none-eabi-gcc
- NEON: Native ARM64 or SSE2NEON translation for x86_64
- Optimization: Same flags as hardware build for consistency

### 1.2 SDK Runtime Stubs (Enhanced)

**Goal:** Provide all SDK runtime functions units expect

**Location:** `test/presets-editor/sdk/`

**Files:**
```
sdk/
├── runtime_stubs.h          # SDK runtime function declarations
├── runtime_stubs.c          # Implementation
├── sample_manager.h         # Sample access interface
└── sample_manager.c         # Mock sample provider
```

**Reuse from qemu-arm:**
- `sdk_stubs.c` provides basic runtime (already done)
- Enhance with preset management
- Add MIDI event handling
- Implement tempo sync

**Additions needed:**
```c
// runtime_stubs.h
typedef struct {
    const char* name;
    uint8_t param_values[24];  // Store up to 24 params
} preset_data_t;

// Preset management
int32_t stub_save_preset(uint8_t index, const preset_data_t* preset);
int32_t stub_load_preset(uint8_t index, preset_data_t* preset);
const char* stub_get_preset_name(uint8_t index);

// MIDI event injection
void stub_send_note_on(uint8_t note, uint8_t velocity);
void stub_send_note_off(uint8_t note);
void stub_send_gate_on(void);
void stub_send_gate_off(void);

// Tempo control
void stub_set_tempo(float bpm);
```

### 1.3 Unit Loader

**Goal:** Load units dynamically and manage lifecycle

**Location:** `test/presets-editor/core/`

**Files:**
```
core/
├── unit_loader.h            # Unit loading interface
├── unit_loader.c            # dlopen/dlsym management
├── unit_callbacks.h         # Callback function pointers
└── parameter_mapper.h       # Map UI to unit params
```

**unit_loader.h:**
```c
#ifndef UNIT_LOADER_H
#define UNIT_LOADER_H

#include <stdint.h>
#include <stdbool.h>

typedef struct unit_instance unit_instance_t;

// Load unit from shared library
unit_instance_t* unit_loader_load(const char* library_path);

// Unload unit
void unit_loader_unload(unit_instance_t* instance);

// Get unit metadata
const char* unit_loader_get_name(unit_instance_t* instance);
uint8_t unit_loader_get_param_count(unit_instance_t* instance);
const char* unit_loader_get_param_name(unit_instance_t* instance, uint8_t id);

// Initialize unit
int unit_loader_init(unit_instance_t* instance, uint32_t sample_rate);

// Process audio
void unit_loader_render(unit_instance_t* instance,
                        const float* input,
                        float* output,
                        uint32_t frames);

// Set parameter
void unit_loader_set_param(unit_instance_t* instance,
                           uint8_t param_id,
                           int32_t value);

// Get parameter value (if available)
int32_t unit_loader_get_param(unit_instance_t* instance, uint8_t param_id);

#endif
```

**Key features:**
- dlopen/dlsym wrapper for cross-platform compatibility
- Symbol resolution for all unit callbacks
- Error handling for missing symbols
- Thread-safe parameter updates

---

## Phase 2: Audio Engine

### 2.1 Audio Backend

**Goal:** Real-time audio processing with low latency

**Location:** `test/presets-editor/audio/`

**Technology choice:** PortAudio (cross-platform) or CoreAudio (macOS only)

**Files:**
```
audio/
├── audio_engine.h           # Audio engine API
├── audio_engine.c           # Implementation
├── ring_buffer.h            # Lock-free audio buffer
└── ring_buffer.c
```

**audio_engine.h:**
```c
#ifndef AUDIO_ENGINE_H
#define AUDIO_ENGINE_H

#include <stdint.h>
#include "unit_loader.h"

typedef struct audio_engine audio_engine_t;

// Configuration
typedef struct {
    uint32_t sample_rate;     // 48000 Hz (match drumlogue)
    uint16_t buffer_size;     // 128 or 256 samples
    uint8_t input_channels;   // 1 or 2
    uint8_t output_channels;  // 1 or 2
} audio_config_t;

// Create audio engine
audio_engine_t* audio_engine_create(const audio_config_t* config);

// Destroy audio engine
void audio_engine_destroy(audio_engine_t* engine);

// Load unit into audio engine
int audio_engine_load_unit(audio_engine_t* engine, unit_instance_t* unit);

// Unload current unit
void audio_engine_unload_unit(audio_engine_t* engine);

// Start/stop audio processing
int audio_engine_start(audio_engine_t* engine);
void audio_engine_stop(audio_engine_t* engine);

// Thread-safe parameter update
void audio_engine_set_param(audio_engine_t* engine, uint8_t id, int32_t value);

// Get current CPU usage
float audio_engine_get_cpu_load(audio_engine_t* engine);

#endif
```

**Audio callback (real-time safe):**
```c
static int audio_callback(const void* input_buffer,
                         void* output_buffer,
                         unsigned long frames,
                         const PaStreamCallbackTimeInfo* time_info,
                         PaStreamCallbackFlags status_flags,
                         void* user_data) {
    audio_engine_t* engine = (audio_engine_t*)user_data;
    const float* in = (const float*)input_buffer;
    float* out = (float*)output_buffer;
    
    // Lock-free parameter updates from ring buffer
    apply_pending_param_updates(engine);
    
    // Call unit_render
    if (engine->unit) {
        unit_loader_render(engine->unit, in, out, frames);
    } else {
        // Passthrough or silence
        memset(out, 0, frames * sizeof(float) * engine->channels);
    }
    
    // Update CPU load
    update_cpu_stats(engine);
    
    return paContinue;
}
```

**Requirements:**
- 48kHz sample rate (drumlogue standard)
- Low latency (128-256 sample buffer)
- Lock-free parameter updates (ring buffer)
- CPU usage monitoring
- Graceful underrun handling

### 2.2 Audio Routing

**Features:**
- Input source selection (mic, line in, test signals)
- Output monitoring
- Dry/wet mix for effects units
- Internal test signal generator (sine, saw, noise)

---

## Phase 3: User Interface

**Goals for initial drop (WIP):**
- Bring up Dear ImGui with SDL2 or GLFW backend on macOS/Linux.
- Render a single window with: unit name, parameter table (id, name, current/default), and live value editing via sliders/combos.
- Wire value edits to `unit_set_param_value` through the existing loader/runtime.
- Provide a minimal transport strip: load unit (.dylib), start/stop audio (reuse CLI engine), and a dropdown for buffer size/sample rate.
- Keep all UI work in a new `gui/` module to avoid disturbing CLI build.

**Progress (Dec 2025):**
- Added `gui/` module with SDL2 + ImGui app skeleton (`presets-editor-gui` target).
- GUI renders unit name/param sliders, writes changes through `unit_set_param_value`, and shows string values when available.
- Audio start/stop wired through existing PortAudio engine (guarded; disabled if PortAudio missing).
- Makefile gains optional `make gui` target with SDL2/ImGui detection (pkg-config or Homebrew fallbacks).
- Page-based parameter navigation (4 params/page) matching drumlogue hardware layout.
- Menu bar with File/Audio menus, unit name display, and CPU load indicator.
- Vendored ImGui support for systems without pkg-config formula.
- Fixed synth vs effects unit input channel handling (synths=0, effects=2).

**Phase 3 Complete ✅**

## Phase 4: Preset System

**Goals:**
- JSON-based preset format for editor (separate from hardware .drmlgpreset)
- Save current parameter state to named preset
- Load preset and apply all parameters
- Preset browser UI panel
- Preset library management (list, rename, delete)
- Import/export preset files

### 3.1 UI Framework Choice

**Options:**

1. **Dear ImGui** (Recommended)
   - Immediate mode GUI (easy to iterate)
   - Cross-platform (macOS, Linux, Windows)
   - Low overhead
   - Good for audio tools
   - C++ API

2. **Qt**
   - Native look and feel
   - More complex, higher overhead
   - Better for polished apps

3. **Native (Cocoa/GTK)**
   - Platform-specific
   - More work to implement

**Recommendation:** Start with Dear ImGui for rapid prototyping, consider Qt later for polish.

### 3.2 UI Layout - Drumlogue Mimicry

**Location:** `test/presets-editor/gui/`

```
gui/
├── main_window.h            # Main window
├── main_window.cpp
├── parameter_panel.h        # Parameter controls
├── parameter_panel.cpp
├── preset_browser.h         # Preset management
├── preset_browser.cpp
├── audio_io_panel.h         # Audio settings
└── audio_io_panel.cpp
```

**Main Window Layout:**
```
┌─────────────────────────────────────────────────────────────┐
│ File  Edit  Unit  Preset  Audio  Help                       │
├─────────────────────────────────────────────────────────────┤
│                                                              │
│  ┌────────────────┐  ┌──────────────────────────────────┐  │
│  │  Unit Info     │  │     Parameter Controls           │  │
│  │                │  │                                   │  │
│  │ Name: CLOUDS   │  │  POSITION    [========|-----]    │  │
│  │ Type: RevFX    │  │  SIZE        [====|----------]   │  │
│  │ Ver:  1.2.0    │  │  PITCH       [--------|-------]  │  │
│  │                │  │  DENSITY     [===========|---]   │  │
│  │ CPU: 45%       │  │  TEXTURE     [====|----------]   │  │
│  │                │  │  DRY/WET     [-----------|===]   │  │
│  │                │  │  REVERB      [=======|---------] │  │
│  │                │  │  FEEDBACK    [===|------------]  │  │
│  └────────────────┘  │                                   │  │
│                      │  [Randomize] [Reset] [Copy]      │  │
│                      └──────────────────────────────────┘  │
│                                                              │
│  ┌────────────────────────────────────────────────────────┐ │
│  │  Preset Browser                                        │ │
│  │  ┌──────────┬──────────┬──────────┬──────────┐        │ │
│  │  │ INIT     │ SHIMMER  │ GRANULAR │ LOFI     │ ...   │ │
│  │  └──────────┴──────────┴──────────┴──────────┘        │ │
│  │  [Save] [Save As...] [Delete] [Import] [Export]       │ │
│  └────────────────────────────────────────────────────────┘ │
│                                                              │
│  ┌────────────────────────────────────────────────────────┐ │
│  │  Audio I/O                                             │ │
│  │  Input: [Built-in Mic ▼]  Output: [Built-in ▼]       │ │
│  │  Sample Rate: 48000 Hz     Buffer: 256 samples        │ │
│  │  [●] Audio Active          Latency: 5.3 ms            │ │
│  └────────────────────────────────────────────────────────┘ │
│                                                              │
│  Status: Ready                                     v1.0.0   │
└─────────────────────────────────────────────────────────────┘
```

**Parameter Control Widget:**
- Slider with value display
- MIDI learn capability
- Modulation visualization
- Right-click for fine control

### 3.3 Preset Management UI

**Features:**
- Grid view of presets (4x8 = 32 presets to match drumlogue)
- Preset name editing
- Drag and drop to reorder
- Import/export single presets
- Import/export preset banks
- Preset comparison (A/B)

---

## Phase 4: Preset System

### 4.1 Preset File Format

**Location:** `test/presets-editor/presets/`

**Format:** JSON for human readability, binary for hardware compatibility

**JSON Format (editor):**
```json
{
  "version": 1,
  "unit": {
    "name": "clouds_revfx",
    "developer_id": "0x12345678",
    "unit_id": 42,
    "version": "1.2.0"
  },
  "preset": {
    "index": 0,
    "name": "SHIMMER",
    "parameters": [
      { "id": 0, "name": "POSITION", "value": 50, "min": 0, "max": 100 },
      { "id": 1, "name": "SIZE", "value": 75, "min": 0, "max": 100 },
      { "id": 2, "name": "PITCH", "value": 50, "min": 0, "max": 100 },
      ...
    ]
  },
  "metadata": {
    "created": "2024-12-13T10:30:00Z",
    "modified": "2024-12-13T11:45:00Z",
    "author": "Your Name",
    "tags": ["reverb", "shimmer", "ambient"]
  }
}
```

**Binary Format (hardware):**
- Must match drumlogue's preset structure
- Investigate via:
  1. Analyze `.drmlgpreset` files from hardware
  2. Check SDK documentation
  3. Reverse engineer if needed

### 4.2 Preset Manager

```c
// preset_manager.h
typedef struct preset_manager preset_manager_t;

// Create preset manager for a unit
preset_manager_t* preset_manager_create(const char* unit_name);

// Load preset bank (JSON or binary)
int preset_manager_load_bank(preset_manager_t* mgr, const char* path);

// Save preset bank
int preset_manager_save_bank(preset_manager_t* mgr, const char* path);

// Get preset
const preset_data_t* preset_manager_get(preset_manager_t* mgr, uint8_t index);

// Set preset
void preset_manager_set(preset_manager_t* mgr, uint8_t index, const preset_data_t* preset);

// Export to hardware format
int preset_manager_export_hardware(preset_manager_t* mgr, const char* path);

// Import from hardware format
int preset_manager_import_hardware(preset_manager_t* mgr, const char* path);
```

---

## Phase 5: Testing & Validation

### 5.1 Comparison with Hardware

**Goal:** Ensure native builds produce identical output to hardware

**Test strategy:**
1. Record parameter sweeps on hardware
2. Replay same inputs through native unit
3. Compare outputs (should be bit-exact on ARM, nearly identical on x86)

**Test harness:**
```bash
# Record on hardware
drumlogue-recorder --unit clouds-revfx --preset 0 --output hw_output.wav

# Replay in editor
presets-editor --unit clouds-revfx.dylib --preset 0 --input test.wav --output editor_output.wav

# Compare
audio-diff hw_output.wav editor_output.wav --tolerance 0.0001
```

### 5.2 Performance Validation

**Metrics:**
- CPU usage vs hardware (should be similar)
- Latency (should be < 10ms)
- NEON instruction usage (verify with profiler)

**Tools:**
- Instruments (macOS)
- perf (Linux)
- objdump to verify NEON codegen

---

## Phase 6: Advanced Features

### 6.1 MIDI Integration

**Features:**
- MIDI controller support (map knobs to parameters)
- MIDI learn
- Note on/off for synth units
- Program change for preset switching

### 6.2 Modulation Matrix

**Visual modulation routing:**
- LFOs
- Envelope followers
- MIDI CC
- Audio-rate modulation

### 6.3 Preset Randomization

**Smart randomization:**
- Per-parameter constraints
- Preserve certain parameters
- Genetic algorithm for evolution

### 6.4 A/B Comparison

- Store two presets
- Crossfade between them
- Difference visualization

---

## Implementation Phases & Timeline

## Phase 1: Proof of Concept (1-2 weeks) — Status: Completed
- [x] Native build system for one unit (e.g., clouds-revfx)
- [x] Basic unit loader
- [ ] Simple audio engine (PortAudio)
- [x] Command-line test (no GUI)
- [ ] Verify NEON usage with profiler

**Deliverable:** CLI tool that loads clouds-revfx.dylib and processes audio (achieved)

**Artifacts:**
- build-system/Makefile.native, build-system/build-native.sh (shared-lib builds)
- sdk/runtime_stubs.{c,h} (runtime descriptor + sample stub)
- core/unit_loader.{c,h} (dlopen/dlsym loader)
- app/main.c (CLI host rendering one buffer)
**Next action:** Start Phase 2 (audio engine with PortAudio, lock-free param path)

### Phase 2: Core Audio Engine (1 week)
- [x] Complete audio engine with parameter updates
- [x] Ring buffer for lock-free communication
- [x] CPU usage monitoring (PortAudio CPU load)
- [ ] Test signal generator

**Deliverable:** Stable audio processing with low latency

**Notes:** PortAudio is autodetected; if missing, the build falls back to a stub (no real-time audio) and prints a clear message. Install PortAudio (e.g., `brew install portaudio`) to enable live audio.

### Phase 3: Basic GUI (2 weeks)
- [ ] ImGui setup
- [ ] Main window layout
- [ ] Parameter sliders (all 24 params)
- [ ] Unit loading dialog
- [ ] Audio device selection

**Deliverable:** Functional GUI for parameter control

### Phase 4: Preset System (1 week)
- [ ] Preset file format (JSON)
- [ ] Preset manager
- [ ] Save/load presets
- [ ] Preset browser UI

**Deliverable:** Full preset management

### Phase 5: Polish & Testing (1 week)
- [ ] Hardware comparison tests
- [ ] Performance optimization
- [ ] Bug fixes
- [ ] Documentation

**Deliverable:** Production-ready v1.0

### Phase 6: Advanced Features (2+ weeks)
- [ ] MIDI integration
- [ ] Preset randomization
- [ ] A/B comparison
- [ ] Export to hardware format

---

## Technical Challenges & Solutions

### Challenge 1: NEON Compatibility

**Problem:** Units use ARM NEON intrinsics, need to work on macOS/Linux

**Solution:**
- **macOS ARM64:** Native NEON support ✅
- **macOS x86_64:** Use [sse2neon](https://github.com/DLTcollab/sse2neon) translation layer
- **Linux ARM64:** Native NEON support ✅
- **Linux x86_64:** Use sse2neon

**Implementation:**
```cpp
// In build system
#ifdef __ARM_NEON
  #include <arm_neon.h>
#else
  #include "sse2neon.h"  // Translation layer
#endif
```

### Challenge 2: Dynamic Library Symbol Resolution

**Problem:** Units export symbols, need to load them dynamically

**Solution:**
- Use dlopen/dlsym (POSIX)
- Export all unit callbacks with extern "C"
- Create symbol table at load time

**Already implemented in qemu-arm unit_host.c** ✅

### Challenge 3: Real-Time Audio Thread Safety

**Problem:** GUI runs on main thread, audio on real-time thread

**Solution:**
- Lock-free ring buffer for parameter updates
- No allocations in audio callback
- Pre-allocate all buffers
- Use atomic operations for state

### Challenge 4: Sample Access

**Problem:** Some units (e.g., drupiter) access drumlogue samples

**Solution:**
- Mock sample manager (already in sdk_stubs.c)
- Allow loading custom samples
- Provide default samples (sine, saw, noise)

### Challenge 5: Preset Format Compatibility

**Problem:** Need to export presets usable on hardware

**Solution:**
- Phase 1: Use JSON (editor-only)
- Phase 2: Reverse engineer `.drmlgpreset` format
- Phase 3: Implement binary export
- Validate on hardware

---

## File Structure

```
test/presets-editor/
├── IMPLEMENTATION_PLAN.md       # This file
├── README.md                    # User documentation
├── CMakeLists.txt               # Build system (CMake recommended)
│
├── build-system/                # Native unit compilation
│   ├── Makefile.native
│   ├── build-native.sh
│   └── config/
│       ├── macos-arm64.mk
│       └── linux-arm64.mk
│
├── sdk/                         # SDK runtime stubs
│   ├── runtime_stubs.h
│   ├── runtime_stubs.c
│   ├── sample_manager.h
│   └── sample_manager.c
│
├── core/                        # Core functionality
│   ├── unit_loader.h
│   ├── unit_loader.c
│   ├── unit_callbacks.h
│   └── parameter_mapper.h
│
├── audio/                       # Audio engine
│   ├── audio_engine.h
│   ├── audio_engine.c
│   ├── ring_buffer.h
│   └── ring_buffer.c
│
├── presets/                     # Preset management
│   ├── preset_manager.h
│   ├── preset_manager.c
│   ├── preset_format.h
│   └── preset_io.c
│
├── gui/                         # User interface
│   ├── main_window.h
│   ├── main_window.cpp
│   ├── parameter_panel.h
│   ├── parameter_panel.cpp
│   ├── preset_browser.h
│   ├── preset_browser.cpp
│   ├── audio_io_panel.h
│   └── audio_io_panel.cpp
│
├── test/                        # Tests
│   ├── test_unit_loader.c
│   ├── test_audio_engine.c
│   └── test_presets.c
│
├── vendor/                      # Third-party dependencies
│   ├── imgui/                   # Dear ImGui
│   ├── portaudio/               # Or git submodule
│   └── sse2neon/                # For x86_64 NEON translation
│
├── units/                       # Built native units (.dylib/.so)
│   ├── clouds_revfx.dylib
│   ├── elementish_synth.dylib
│   └── ...
│
├── presets-data/                # Preset storage
│   ├── clouds-revfx/
│   │   ├── bank1.json
│   │   └── ...
│   └── ...
│
└── bin/                         # Build output
    └── presets-editor           # Main executable
```

---

## Dependencies

### Core
- **C/C++ Compiler:** clang (macOS), gcc (Linux)
- **Build System:** CMake 3.15+
- **C++ Standard:** C++17

### Audio
- **PortAudio** - Cross-platform audio I/O
  - macOS: `brew install portaudio`
  - Linux: `apt-get install libportaudio2 portaudio19-dev`

### GUI
- **Dear ImGui** - Immediate mode GUI
  - Git submodule or vendored
- **SDL2** or **GLFW** - Windowing/OpenGL context
  - macOS: `brew install sdl2`
  - Linux: `apt-get install libsdl2-dev`

### Optional
- **sse2neon** - NEON to SSE translation (x86_64 only)
  - Git submodule: https://github.com/DLTcollab/sse2neon
- **RtMidi** - MIDI I/O (Phase 6)
  - macOS: `brew install rtmidi`

---

## Build Instructions (Preview)

```bash
# Phase 1: Build a native unit
cd test/presets-editor/build-system
./build-native.sh clouds-revfx

# Phase 3: Build the editor (once GUI is implemented)
cd test/presets-editor
mkdir build && cd build
cmake ..
make

# Run
./presets-editor --unit ../units/clouds_revfx.dylib
```

---

## Testing Strategy

### Unit Tests
- Unit loader (dlopen/dlsym)
- Audio engine (buffer processing)
- Preset manager (save/load)
- Parameter mapping

### Integration Tests
- Load each unit and verify all callbacks
- Process test signals (impulse, sine, noise)
- Compare with QEMU ARM output
- Save and reload presets

### Hardware Validation
- Export presets to .drmlgpreset
- Load on actual drumlogue
- Verify parameters match
- Record and compare audio output

---

## Success Criteria

### Phase 1 (POC)
- [x] Can compile clouds-revfx as .dylib
- [x] Can load and initialize unit
- [x] Can process audio through unit
- [x] NEON intrinsics work natively (arm64)

### Phase 3 (GUI)
- [ ] Can load any drumlogue unit
- [ ] Can adjust all parameters in real-time
- [ ] Audio latency < 10ms
- [ ] No audio dropouts

### Phase 4 (Presets)
- [ ] Can save/load presets to JSON
- [ ] Can browse and select presets
- [ ] Preset switching is instant

### Phase 5 (Production)
- [ ] Tested with all existing units
- [ ] CPU usage < 50% (on modern hardware)
- [ ] Stable for extended use (no memory leaks)
- [ ] Comprehensive user documentation

### Phase 6 (Advanced)
- [ ] MIDI controller integration works
- [ ] Can export presets to hardware format
- [ ] Presets work identically on hardware

---

## Risks & Mitigations

### Risk 1: NEON Code Doesn't Compile Natively
**Likelihood:** Low (you're on ARM64)  
**Impact:** High  
**Mitigation:** Already verified uname -m = arm64, NEON will work natively

### Risk 2: Symbol Loading Issues
**Likelihood:** Medium  
**Impact:** Medium  
**Mitigation:** Reuse proven qemu-arm unit_host.c approach

### Risk 3: Audio Dropouts/Latency
**Likelihood:** Medium  
**Impact:** High  
**Mitigation:** Use PortAudio's proven low-latency path, lock-free design

### Risk 4: Preset Format Incompatibility
**Likelihood:** High (unknown format)  
**Impact:** Medium  
**Mitigation:** Phase 1 uses JSON (editor-only), Phase 6 tackles hardware export

### Risk 5: UI Performance
**Likelihood:** Low  
**Impact:** Low  
**Mitigation:** ImGui is lightweight, designed for real-time apps

---

## Future Enhancements

### Version 2.0
- Plugin hosting (VST3/AU wrapper for units)
- Multi-unit chains (serial effects)
- Spectrum analyzer / oscilloscope
- Preset sharing / online library

### Version 3.0
- Cloud sync for presets
- Collaborative preset editing
- AI-assisted preset generation
- Mobile companion app (iOS/Android)

---

## References

### Code to Reuse
1. **test/qemu-arm/unit_host.c** - Unit loading, dlopen/dlsym ✅
2. **test/qemu-arm/sdk_stubs.c** - SDK runtime stubs ✅
3. **test/qemu-arm/wav_file.c** - WAV I/O utilities ✅
4. **test/*/main.cc** - Desktop test harnesses for reference

### External Resources
1. [PortAudio Documentation](http://www.portaudio.com/docs/v19-doxydocs/)
2. [Dear ImGui](https://github.com/ocornut/imgui)
3. [sse2neon](https://github.com/DLTcollab/sse2neon) (for x86_64 support)
4. [Logue SDK Documentation](https://github.com/korginc/logue-sdk)

### ARM NEON
1. [ARM NEON Intrinsics Reference](https://developer.arm.com/architectures/instruction-sets/intrinsics/)
2. [ARM NEON Optimization Guide](https://developer.arm.com/documentation/den0018/a/)

---

## Next Steps

1. **Review this plan** - Confirm approach aligns with your goals
2. **Set up development environment** - Install dependencies (PortAudio, SDL2)
3. **Start Phase 1** - Build clouds-revfx as native .dylib
4. **Create minimal CLI tool** - Test unit loading and audio processing
5. **Validate NEON** - Use Instruments to verify native NEON usage
6. **Proceed to Phase 2** - Implement audio engine once POC works

---

## Questions to Answer

Before starting implementation:

1. **Preset format:** Can you dump a `.drmlgpreset` file from hardware? This would help reverse engineer the format.

2. **Target units:** Which units do you want to prioritize for testing?
   - clouds-revfx (complex, uses eurorack DSP)
   - elementish-synth (synth unit)
   - pepege-synth (simpler synth)
   - drupiter-synth (sample-based)

3. **UI preferences:** ImGui (fast, programmer-friendly) vs Qt (polished, more work)?

4. **Audio I/O:** Real-time processing only, or also offline rendering?

5. **Platform support:** macOS only initially, or Linux too?

---

## Conclusion

**This is absolutely feasible.** Key advantages:

✅ You're on Apple Silicon (arm64) - NEON works natively  
✅ Already have unit_host infrastructure from qemu-arm  
✅ Units compile without modification  
✅ Clear path from POC to production  

**Estimated time to working prototype:** 2-3 weeks part-time

The hardest part will be Phase 6 (hardware preset format), but that's optional - a JSON-based editor is valuable on its own for:
- Rapid preset creation
- Parameter exploration
- Unit testing
- Documentation (screenshot presets)

Ready to start implementation?
