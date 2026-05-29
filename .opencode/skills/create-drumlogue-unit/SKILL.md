---
name: create-drumlogue-unit
description: Scaffold a new Korg drumlogue DSP unit from template. Gathers unit name, type (synth/delfx/revfx/masterfx), developer ID, unit ID, and description, then creates all required files (config.mk, header.c, unit.cc, Makefile) in drumlogue/<unit-name>/ and optionally a desktop test harness in test/<unit-name>/.
license: MIT
compatibility: opencode
metadata:
  audience: developers
  workflow: new-unit
---

# Create Drumlogue Unit Skill

You are scaffolding a new Korg drumlogue DSP unit. Follow every step in order; do not skip steps.

## Step 1 — Gather Information

Ask the user for these details if not already provided:

| Field | Description | Rules |
|---|---|---|
| **Unit directory name** | e.g. `my-reverb` | lowercase, hyphens OK, no spaces |
| **Unit type** | `synth`, `delfx`, `revfx`, or `masterfx` | pick one |
| **Developer ID** | 32-bit hex, e.g. `0x434C444DU` | must not be `0x00000000`, `0x4B4F5247`, or `0x6B6F7267`; check `logue-sdk/developer_ids.md` |
| **Unit ID** | 0–127 integer unique within your dev_id | check existing headers for taken IDs |
| **Display name** | Shown on device, ≤13 chars, 7-bit ASCII only | must fit on drumlogue screen |
| **Version** | Semantic `x.y.z` | converted to `0xMMNNPP` in header.c |
| **Create test harness?** | yes/no | recommended; creates `test/<unit-name>/` |

Derive `PROJECT` (the Makefile variable) from the directory name by replacing hyphens with underscores (e.g. `my-reverb` → `my_reverb`).

Derive the `k_unit_module_*` constant from the unit type:
- `synth` → `k_unit_module_synth`
- `delfx` → `k_unit_module_delfx`
- `revfx` → `k_unit_module_revfx`
- `masterfx` → `k_unit_module_masterfx`

Also set the `UNIT_TARGET_PLATFORM`:
- `synth` → `UNIT_TARGET_PLATFORM | k_unit_module_synth`
- `delfx` → `UNIT_TARGET_PLATFORM | k_unit_module_delfx`
- `revfx` → `UNIT_TARGET_PLATFORM | k_unit_module_revfx`
- `masterfx` → `UNIT_TARGET_PLATFORM | k_unit_module_masterfx`

## Step 2 — Create Directory

```bash
mkdir -p drumlogue/<unit-name>
```

## Step 3 — Copy Makefile from SDK Template

The SDK template Makefile must be copied **as-is** — never modify it.

```bash
cp logue-sdk/platform/drumlogue/dummy-<type>/Makefile drumlogue/<unit-name>/Makefile
```

## Step 4 — Create `config.mk`

Create `drumlogue/<unit-name>/config.mk`:

```makefile
PROJECT := <project_name>
PROJECT_TYPE := <synth|delfx|revfx|masterfx>

# C sources
CSRC = header.c

# C++ sources
CXXSRC = unit.cc

# Include paths
UINCDIR =

# Common drumlogue utilities (absolute path required — unit lives outside SDK)
COMMON_INC_PATH = /workspace/drumlogue/common
COMMON_SRC_PATH = /workspace/drumlogue/common

# Library paths and flags
ULIBDIR =
ULIBS   = -lm

# Preprocessor defines
UDEFS =
```

## Step 5 — Create `header.c`

Create `drumlogue/<unit-name>/header.c` with the correct `k_unit_module_*` target, developer ID, unit ID, version, and name. Start with 2 placeholder parameters (one per-type-appropriate default, one `none` filler) — the user can add more later.

Version encoding: `1.0.0` → `0x010000U`, `1.2.3` → `0x010203U`.

```c
/**
 *  @file header.c
 *  @brief drumlogue SDK unit header for <Display Name>
 */

#include "unit.h"

const __unit_header unit_header_t unit_header = {
    .header_size = sizeof(unit_header_t),
    .target      = UNIT_TARGET_PLATFORM | <k_unit_module_TYPE>,
    .api         = UNIT_API_VERSION,
    .dev_id      = <DEV_ID>,
    .unit_id     = <UNIT_ID>,
    .version     = <VERSION_HEX>,
    .name        = "<Display Name>",
    .num_presets = 0,
    .num_params  = 2,
    .params = {
        // Format: min, max, center, default, type, fractional, frac_type, reserved, name
        // Page 1
        {0, 100, 0, 50, k_unit_param_type_percent, 0, 0, 0, {"PARAM1"}},
        {0, 0, 0, 0, k_unit_param_type_none, 0, 0, 0, {""}},

        // Pages 2–6 (blank — add params as needed, max 24 total)
        {0, 0, 0, 0, k_unit_param_type_none, 0, 0, 0, {""}},
        {0, 0, 0, 0, k_unit_param_type_none, 0, 0, 0, {""}},
        {0, 0, 0, 0, k_unit_param_type_none, 0, 0, 0, {""}},
        {0, 0, 0, 0, k_unit_param_type_none, 0, 0, 0, {""}},
        {0, 0, 0, 0, k_unit_param_type_none, 0, 0, 0, {""}},
        {0, 0, 0, 0, k_unit_param_type_none, 0, 0, 0, {""}},
        {0, 0, 0, 0, k_unit_param_type_none, 0, 0, 0, {""}},
        {0, 0, 0, 0, k_unit_param_type_none, 0, 0, 0, {""}},
        {0, 0, 0, 0, k_unit_param_type_none, 0, 0, 0, {""}},
        {0, 0, 0, 0, k_unit_param_type_none, 0, 0, 0, {""}},
        {0, 0, 0, 0, k_unit_param_type_none, 0, 0, 0, {""}},
        {0, 0, 0, 0, k_unit_param_type_none, 0, 0, 0, {""}},
        {0, 0, 0, 0, k_unit_param_type_none, 0, 0, 0, {""}},
        {0, 0, 0, 0, k_unit_param_type_none, 0, 0, 0, {""}},
        {0, 0, 0, 0, k_unit_param_type_none, 0, 0, 0, {""}},
        {0, 0, 0, 0, k_unit_param_type_none, 0, 0, 0, {""}},
        {0, 0, 0, 0, k_unit_param_type_none, 0, 0, 0, {""}},
        {0, 0, 0, 0, k_unit_param_type_none, 0, 0, 0, {""}},
        {0, 0, 0, 0, k_unit_param_type_none, 0, 0, 0, {""}},
        {0, 0, 0, 0, k_unit_param_type_none, 0, 0, 0, {""}},
        {0, 0, 0, 0, k_unit_param_type_none, 0, 0, 0, {""}},
        {0, 0, 0, 0, k_unit_param_type_none, 0, 0, 0, {""}},
    },
};
```

## Step 6 — Create `unit.cc`

Create `drumlogue/<unit-name>/unit.cc`. The template varies by unit type.

### For `synth`:

```cpp
/**
 *  @file unit.cc
 *  @brief drumlogue SDK unit implementation for <Display Name> (synth)
 */

#include <cstddef>
#include <cstdint>
#include <cstring>

#include "unit.h"

static unit_runtime_desc_t s_desc;

// Parameters (mirrors header.c order)
static float s_param1 = 0.5f;

__unit_callback int8_t unit_init(const unit_runtime_desc_t* desc) {
  if (!desc)
    return k_unit_err_undef;
  if (desc->target != unit_header.target)
    return k_unit_err_target;
  if (!UNIT_API_IS_COMPAT(desc->api))
    return k_unit_err_api_version;

  s_desc = *desc;
  s_param1 = 0.5f;
  return k_unit_err_none;
}

__unit_callback void unit_teardown() {}
__unit_callback void unit_reset() {}
__unit_callback void unit_resume() {}
__unit_callback void unit_suspend() {}

__unit_callback void unit_render(const float* in, float* out, uint32_t frames) {
  (void)in;
  // TODO: implement synthesis
  for (uint32_t i = 0; i < frames * 2; i++) {
    out[i] = 0.f;
  }
}

__unit_callback void unit_set_param_value(uint8_t id, int32_t value) {
  switch (id) {
    case 0: s_param1 = value / 100.f; break;
    default: break;
  }
}

__unit_callback int32_t unit_get_param_value(uint8_t id) {
  switch (id) {
    case 0: return (int32_t)(s_param1 * 100.f);
    default: return 0;
  }
}

__unit_callback const char* unit_get_param_str_value(uint8_t id, int32_t value) {
  (void)id; (void)value;
  return nullptr;
}

__unit_callback const uint8_t* unit_get_param_bmp_value(uint8_t id, int32_t value) {
  (void)id; (void)value;
  return nullptr;
}

__unit_callback void unit_set_tempo(uint32_t tempo) { (void)tempo; }
__unit_callback void unit_tempo_4ppqn_tick(uint32_t counter) { (void)counter; }
__unit_callback void unit_note_on(uint8_t note, uint8_t velocity) { (void)note; (void)velocity; }
__unit_callback void unit_note_off(uint8_t note) { (void)note; }
__unit_callback void unit_all_note_off() {}
__unit_callback void unit_pitch_bend(uint16_t bend) { (void)bend; }
__unit_callback void unit_channel_pressure(uint8_t pressure) { (void)pressure; }
__unit_callback void unit_aftertouch(uint8_t note, uint8_t aftertouch) { (void)note; (void)aftertouch; }
__unit_callback void unit_load_preset(uint8_t idx) { (void)idx; }
__unit_callback uint8_t unit_get_preset_index() { return 0; }
__unit_callback const char* unit_get_preset_name(uint8_t idx) { (void)idx; return nullptr; }
```

### For `revfx` / `delfx` / `masterfx` (effects process stereo in→out):

```cpp
/**
 *  @file unit.cc
 *  @brief drumlogue SDK unit implementation for <Display Name> (<type>)
 */

#include <cstddef>
#include <cstdint>
#include <cstring>

#include "unit.h"

static unit_runtime_desc_t s_desc;
static float s_param1 = 0.5f;

__unit_callback int8_t unit_init(const unit_runtime_desc_t* desc) {
  if (!desc)
    return k_unit_err_undef;
  if (desc->target != unit_header.target)
    return k_unit_err_target;
  if (!UNIT_API_IS_COMPAT(desc->api))
    return k_unit_err_api_version;

  s_desc = *desc;
  s_param1 = 0.5f;
  return k_unit_err_none;
}

__unit_callback void unit_teardown() {}
__unit_callback void unit_reset() {}
__unit_callback void unit_resume() {}
__unit_callback void unit_suspend() {}

__unit_callback void unit_render(const float* in, float* out, uint32_t frames) {
  // Stereo pass-through — replace with your DSP
  for (uint32_t i = 0; i < frames * 2; i++) {
    out[i] = in[i];
  }
}

__unit_callback void unit_set_param_value(uint8_t id, int32_t value) {
  switch (id) {
    case 0: s_param1 = value / 100.f; break;
    default: break;
  }
}

__unit_callback int32_t unit_get_param_value(uint8_t id) {
  switch (id) {
    case 0: return (int32_t)(s_param1 * 100.f);
    default: return 0;
  }
}

__unit_callback const char* unit_get_param_str_value(uint8_t id, int32_t value) {
  (void)id; (void)value;
  return nullptr;
}

__unit_callback const uint8_t* unit_get_param_bmp_value(uint8_t id, int32_t value) {
  (void)id; (void)value;
  return nullptr;
}

__unit_callback void unit_set_tempo(uint32_t tempo) { (void)tempo; }
__unit_callback void unit_tempo_4ppqn_tick(uint32_t counter) { (void)counter; }
__unit_callback void unit_load_preset(uint8_t idx) { (void)idx; }
__unit_callback uint8_t unit_get_preset_index() { return 0; }
__unit_callback const char* unit_get_preset_name(uint8_t idx) { (void)idx; return nullptr; }
```

## Step 7 — Build Immediately

After creating all files, run the build to catch any errors early:

```bash
./build.sh <unit-name>
```

If the build fails, diagnose with:
```bash
# Check for undefined symbols
objdump -T drumlogue/<unit-name>/<project>.drmlgunit 2>/dev/null | grep "UND" || true
```

Common first-build failures:
- Wrong `k_unit_module_*` in header.c target line
- Typo in `num_params` (must equal the non-blank entries, or total declared)
- `COMMON_INC_PATH` typo (must be `/workspace/drumlogue/common`)

## Step 8 — (Optional) Create Desktop Test Harness

If the user requested a test harness, create `test/<unit-name>/`:

```bash
mkdir -p test/<unit-name>/fixtures
```

Create `test/<unit-name>/Makefile`:

```makefile
CXX      := g++
CXXFLAGS := -std=c++17 -O2 -Wall -Wextra -DTEST

UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Darwin)
    BREW_PREFIX := $(shell brew --prefix 2>/dev/null)
    ifdef BREW_PREFIX
        CXXFLAGS += -I$(BREW_PREFIX)/include
        LDFLAGS  := -L$(BREW_PREFIX)/lib -lsndfile
    else
        LDFLAGS  := -lsndfile
    endif
else
    LDFLAGS := -lsndfile
endif

REPO_ROOT := $(shell cd ../.. && pwd)
DSP_SRC   := $(REPO_ROOT)/drumlogue/<unit-name>
INCLUDES  := -I$(DSP_SRC) -I.

TARGET    := <project>_test
SOURCES   := main.cc

$(TARGET): $(SOURCES)
	$(CXX) $(CXXFLAGS) $(INCLUDES) $^ $(LDFLAGS) -o $@

clean:
	rm -f $(TARGET) fixtures/*.wav

fixtures:
	mkdir -p fixtures

generate-fixtures: $(TARGET) fixtures
	./$< --generate-impulse fixtures/impulse.wav
	./$< --generate-sine fixtures/sine_440.wav 440
	./$< --generate-noise fixtures/noise.wav

test: $(TARGET) generate-fixtures
	@echo "=== Impulse response ==="
	./$< fixtures/impulse.wav fixtures/out_impulse.wav
	@echo "=== Sine wave ==="
	./$< fixtures/sine_440.wav fixtures/out_sine.wav
	@echo "✓ All tests passed"
```

Create `test/<unit-name>/main.cc` with a minimal WAV-processing harness that exercises the unit's DSP.

## Step 9 — Summary

After completing all steps, report:
- Files created (list each path)
- Build result (success/failure with log excerpt on failure)
- Next steps: add DSP logic to `unit_render()`, add parameters in `header.c`, run `make test`

## Reference

- Existing units for comparison: `drumlogue/clouds-revfx/`, `drumlogue/elementish-synth/`
- SDK templates: `logue-sdk/platform/drumlogue/dummy-<type>/`
- Common utilities: `drumlogue/common/`
- Build: `./build.sh <unit-name>` — always use the containerized build, never cross-compile manually
- Release: `make release UNIT=<unit-name> VERSION=1.0.0`
