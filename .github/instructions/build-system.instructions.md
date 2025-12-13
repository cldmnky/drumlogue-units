---
applyTo: "**/Makefile,**/*.mk,build.sh"
description: "Build system guidelines for drumlogue units"
---

# Build System Guidelines

## Overview

The drumlogue-units repository uses a multi-layered build system:
- **Root Makefile** - Release management (version, tag, release)
- **build.sh** - Container-based ARM builds
- **Unit Makefiles** - SDK templates (copied, not modified)
- **config.mk** - Project-specific configuration

## Build Script (build.sh)

### Usage
```bash
./build.sh <unit-name>          # Build unit
./build.sh <unit-name> clean    # Clean build
./build.sh --build-image        # Rebuild SDK container
```

### How It Works
1. Detects project name from `config.mk`
2. Creates container with mounted volumes:
   - `/sdk-platform:ro` - SDK platform (read-only)
   - `/project-src:ro` - Unit source (read-only)
   - `/output` - Build output (writable)
   - `/repo/eurorack:ro` - Mutable Instruments DSP (read-only)
3. Copies SDK and sources to writable `/workspace` in container
4. Runs SDK build command
5. Copies `.drmlgunit` artifact back to host

### Environment Variables
- `ENGINE` - Container engine (default: podman, can use docker)
- `IMAGE` - Container image (default: localhost/logue-sdk-dev-env:latest)

**Example:**
```bash
ENGINE=docker ./build.sh clouds-revfx
```

## Root Makefile

### Purpose
Provides release management commands for multi-unit repository.

### Targets

**Build:**
```bash
make build UNIT=<name>
```

**Version management:**
```bash
make version UNIT=<name> VERSION=1.0.0
# Updates header.c with hex-encoded version
```

**Release preparation:**
```bash
make release UNIT=<name> VERSION=1.0.0
# Runs: version + build + shows next steps
```

**Git tagging:**
```bash
make tag UNIT=<name> VERSION=1.0.0
# Creates tag: <name>/v<version>
```

**List units:**
```bash
make list-units
```

### Version Encoding
Semantic version (x.y.z) → Hex format:
- `0xMMNNPPU` where MM=major, NN=minor, PP=patch, U=unsigned suffix
- Example: `1.2.3` → `0x010203U`
- Pre-releases use base version: `1.0.0-pre` → `0x010000U`

## Unit config.mk

### Purpose
Project-specific configuration for SDK build system.

### Required Variables
```makefile
PROJECT := unit_name          # Must match output .drmlgunit name
PROJECT_TYPE := synth         # synth|delfx|revfx|masterfx
```

### Source Files
```makefile
# C sources
CSRC = resource.c

# C++ sources (space-separated)
CXXSRC = unit.cc dsp/processor.cc dsp/filter.cc
```

### Include Paths
```makefile
# Project includes (relative to unit directory)
UINCDIR = dsp
UINCDIR += /repo/eurorack/stmlib

# Common utilities (MUST use absolute /workspace path)
COMMON_INC_PATH = /workspace/drumlogue/common
COMMON_SRC_PATH = /workspace/drumlogue/common
```

**Critical:** Since units live outside SDK, use `/workspace/` prefix for common includes.

### Libraries
```makefile
# Library paths
ULIBDIR = 

# Libraries to link
ULIBS = -lm

# Additional defines
UDEFS = -DARM_MATH_CM4
```

### Example config.mk
```makefile
PROJECT := clouds_revfx
PROJECT_TYPE := revfx

# Sources
CSRC = 
CXXSRC = unit.cc clouds_fx.cc

# Eurorack dependencies
CXXSRC += /repo/eurorack/stmlib/utils/random.cc

# Includes
UINCDIR = .
UINCDIR += dsp
UINCDIR += /repo/eurorack
UINCDIR += /repo/eurorack/stmlib

COMMON_INC_PATH = /workspace/drumlogue/common
COMMON_SRC_PATH = /workspace/drumlogue/common

# Libraries
ULIBDIR = 
ULIBS = -lm

# Defines
UDEFS = 
```

## Unit Makefile

### Source
Copy from SDK template:
- `logue-sdk/platform/drumlogue/dummy-synth/Makefile`
- `logue-sdk/platform/drumlogue/dummy-revfx/Makefile`
- etc.

### Rules
- **Never modify SDK Makefiles directly**
- **Copy to your unit directory**
- **Don't edit the copied Makefile** - use `config.mk` instead

The Makefile includes SDK build rules and references `config.mk` for customization.

## Desktop Test Harness Makefile

### Purpose
Build DSP code for host machine (x86_64/ARM64) for offline testing.

### Template
```makefile
CXX := g++
CXXFLAGS := -std=c++17 -O2 -Wall -Wextra
CXXFLAGS += -DTEST  # Enable test code paths

# Platform detection
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Darwin)
    # macOS with Homebrew
    BREW_PREFIX := $(shell brew --prefix 2>/dev/null)
    ifdef BREW_PREFIX
        CXXFLAGS += -I$(BREW_PREFIX)/include
        LDFLAGS := -L$(BREW_PREFIX)/lib -lsndfile
    else
        LDFLAGS := -lsndfile
    endif
else
    # Linux
    LDFLAGS := -lsndfile
endif

# Paths
REPO_ROOT := $(shell cd ../.. && pwd)
DSP_SRC := $(REPO_ROOT)/drumlogue/<unit>
EURORACK := $(REPO_ROOT)/eurorack

# Includes
INCLUDES := -I$(DSP_SRC) -I$(EURORACK) -I.

# Sources (exclude unit.cc which has hardware dependencies)
DSP_SOURCES := $(DSP_SRC)/dsp_core.cc
TEST_SOURCES := main.cc

# Target
TARGET := unit_test

# Build
$(TARGET): $(TEST_SOURCES) $(DSP_SOURCES)
	$(CXX) $(CXXFLAGS) $(INCLUDES) $^ $(LDFLAGS) -o $@

clean:
	rm -rf build $(TARGET) fixtures/*.wav

# Test targets
fixtures:
	mkdir -p fixtures

generate-fixtures: $(TARGET) fixtures
	./$(TARGET) --generate-impulse fixtures/impulse.wav
	./$(TARGET) --generate-sine fixtures/sine_440.wav 440

test: $(TARGET) generate-fixtures
	./$(TARGET) fixtures/impulse.wav fixtures/out_impulse.wav
	./$(TARGET) fixtures/sine_440.wav fixtures/out_sine.wav
```

## Container Image

### Building
```bash
cd logue-sdk/docker/docker-app
BUILD_ID=$(git rev-parse --short HEAD)
VERSION=$(cat VERSION)
podman build \
  --build-arg build=$BUILD_ID \
  --build-arg version=$VERSION \
  -t logue-sdk-dev-env:$VERSION \
  -t logue-sdk-dev-env:latest \
  .
```

### Contents
- ARM GCC toolchain (arm-none-eabi-gcc)
- Build scripts and utilities
- SDK dependencies

## Common Issues

### Build Fails with "No such file"
**Cause:** Incorrect paths in `config.mk`

**Fix:** Use absolute `/workspace/` or `/repo/` paths for includes/sources

### Wrong Output Filename
**Cause:** `PROJECT` in `config.mk` doesn't match expected name

**Fix:** Check `PROJECT := <name>` matches the expected `.drmlgunit` filename

### Missing Symbols
**Cause:** Source files not listed in `CXXSRC` or `CSRC`

**Fix:** Add all `.cc/.c` files to config.mk

### Container Permission Errors
**Cause:** Build directories not writable

**Fix:** `build.sh` handles this automatically; ensure not running as root

### Can't Find Common Files
**Cause:** `COMMON_INC_PATH` not set correctly

**Fix:**
```makefile
COMMON_INC_PATH = /workspace/drumlogue/common
COMMON_SRC_PATH = /workspace/drumlogue/common
```

## Best Practices

- **Never edit SDK Makefiles** - They're templates
- **Use config.mk for all customization**
- **Keep source lists minimal** - Only include what's needed
- **Test desktop build first** - Faster iteration
- **Use absolute paths** - Units live outside SDK structure
- **Clean before release builds** - `./build.sh <unit> clean && ./build.sh <unit>`
- **Check build logs** - Container outputs compilation warnings
- **Verify artifact** - Check `.drmlgunit` file size is reasonable (typically 50-200KB)

## Debugging Build Issues

### Check Container Logs
```bash
# build.sh shows all output
./build.sh <unit> 2>&1 | tee build.log
```

### Verify config.mk
```bash
# Check PROJECT name
grep "^PROJECT" drumlogue/<unit>/config.mk

# Check sources exist
grep "CXXSRC" drumlogue/<unit>/config.mk
```

### Test Desktop Build
```bash
cd test/<unit>
make clean && make
```

### Manual Container Build
```bash
podman run --rm -it \
  -v "$(pwd)/logue-sdk/platform:/sdk-platform:ro" \
  -v "$(pwd)/drumlogue/<unit>:/project:ro" \
  localhost/logue-sdk-dev-env:latest \
  /bin/bash

# Inside container:
cd /workspace
cp -r /sdk-platform/drumlogue .
cp -r /project/* drumlogue/<unit>/
cd drumlogue/<unit>
make
```

## Release Workflow

Complete release process:

```bash
# 1. Update version
make version UNIT=<name> VERSION=1.0.0

# 2. Build
make build UNIT=<name>

# 3. Test
# ... hardware testing ...

# 4. Update RELEASE_NOTES.md
# ... manual edit ...

# 5. Commit
git commit -am "Release <name> v1.0.0"

# 6. Tag
make tag UNIT=<name> VERSION=1.0.0

# 7. Push
git push && git push --tags
```

Or use one-step preparation:
```bash
make release UNIT=<name> VERSION=1.0.0
# Then manually: test, commit, tag, push
```
