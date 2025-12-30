# Copilot Instructions for this Repo

- **Purpose & layout**: This repo combines (a) Mutable Instruments eurorack module sources (`eurorack/…`, STM32/AVR) and (b) Korg logue SDK materials (`logue-sdk/…`) for building drumlogue user units.
- **Main targets**: Most day-to-day work here is drumlogue unit development. Custom units live in `drumlogue/` at the repo root (not in the logue-sdk submodule). Templates for reference are in `logue-sdk/platform/drumlogue/` (`dummy-synth`, `dummy-delfx`, `dummy-revfx`, `dummy-masterfx`).

## Quick Reference

### Build & Test
```bash
./build.sh <unit-name>              # Build unit
./build.sh <unit-name> clean        # Clean build
cd test/<unit-name> && make test    # Desktop testing
```

### Release Management
```bash
make build UNIT=<name>                    # Build
make version UNIT=<name> VERSION=1.0.0    # Update version
make release UNIT=<name> VERSION=1.0.0    # Prepare release
make tag UNIT=<name> VERSION=1.0.0        # Create git tag
make list-units                           # List all units
```

## Specialized Instructions

The following instruction files provide detailed guidance for specific areas:

- <a>C/C++ Coding Standards for Drumlogue Units</a> - DSP code style, real-time constraints, ARM optimization
- <a>Build System Guidelines</a> - Makefiles, config.mk, container builds
- <a>Testing Standards for Drumlogue DSP Units</a> - Desktop test harnesses, hardware validation

## Reusable Prompts

Use these prompts for common development tasks:

- <a>Create New Drumlogue Unit</a> - Template-based unit creation
- <a>Debug Drumlogue Unit</a> - Build, runtime, and DSP debugging
- <a>Port Mutable Instruments DSP to Drumlogue</a> - Porting Mutable Instruments modules
- <a>Release Drumlogue Unit</a> - Complete release workflow

## Specialized Agents

- <a>Drumlogue DSP Expert Agent</a> - Expert mode for DSP development

## Building Units

- **Build script**: Use `./build.sh <project-name>` from the repo root:
  ```bash
  ./build.sh clouds-revfx        # Build the clouds-revfx unit
  ./build.sh clouds-revfx clean  # Clean the build
  ```
- **Container image**: The build uses the `localhost/logue-sdk-dev-env:latest` podman image. To rebuild it:
  ```bash
  cd logue-sdk/docker/docker-app
  BUILD_ID=$(git rev-parse --short HEAD)
  VERSION=$(cat VERSION)
  podman build --build-arg build=$BUILD_ID --build-arg version=$VERSION -t logue-sdk-dev-env:$VERSION -t logue-sdk-dev-env:latest .
  ```
- **Final artifact**: `<project>.drmlgunit` appears in `drumlogue/<project>/` after a successful build.
- **Alternative**: The SDK also provides `logue-sdk/docker/run_interactive.sh` for an interactive shell approach.

## Project Structure

- **Project layout**: Each unit in `drumlogue/<project>/` needs:
  - `Makefile` – Copy from `logue-sdk/platform/drumlogue/dummy-revfx/Makefile` (or appropriate template)
  - `config.mk` – Project config: `PROJECT`, `PROJECT_TYPE`, `CSRC`, `CXXSRC`, includes
  - `header.c` – Unit metadata (dev_id, unit_id, name, parameters)
  - `unit.cc` – SDK callbacks (`unit_init`, `unit_render`, `unit_set_param_value`, etc.)
  - Additional `.cc/.h` files as needed

- **config.mk critical settings**: Since our projects live outside the SDK and are symlinked in, you MUST set explicit paths:
  ```makefile
  COMMON_INC_PATH = /workspace/drumlogue/common
  COMMON_SRC_PATH = /workspace/drumlogue/common
  ```

- **Project config**: `config.mk` declares `PROJECT`, `PROJECT_TYPE` (`synth|delfx|revfx|masterfx`), `CSRC`, `CXXSRC`, include/library paths and extra `UDEFS`. Avoid editing the shared Makefiles directly.

## Unit API

- **Metadata header**: `header.c` must stay aligned with the target module (`k_unit_module_*`), developer ID, unit ID, version encoding, and 7-bit ASCII name (<=13 chars). Parameters: up to 24 descriptors; blank slots use `{0,0,0,0,k_unit_param_type_none,...}` to control paging.
- **Unit callbacks**: Implement in `unit.cc` (`unit_init`, `unit_render`, `unit_set_param_value`, preset getters/loaders, tempo, MIDI handlers). Drumlogue uses 48kHz float buffers; check `frames_per_buffer`, `input_channels`, `output_channels` from `unit_runtime_desc_t`.
- **Samples API**: Use runtime callbacks (`get_num_sample_banks`, `get_num_samples_for_bank`, `get_sample`) for sample access; `sample_wrapper_t` provides `frames`, `channels`, `sample_ptr`.
- **UI strings/bitmaps**: `k_unit_param_type_strings` and `k_unit_param_type_bitmaps` must return short 7-bit ASCII strings or 16x16 1bpp bitmaps; bitmap API needs drumlogue FW ≥1.1.0.
- **Developer IDs**: Choose a unique 32-bit `dev_id`; reserved: `0x00000000`, `0x4B4F5247`, `0x6B6F7267`. See `logue-sdk/developer_ids.md`.

## Mutable Instruments Code

- **eurorack/**: Contains upstream modules (e.g., `braids`, `clouds`). Resource generation scripts live in `resources/` and run during make to emit lookup tables (`resources.cc/h`). DSP sources are designed to compile both on hardware and desktop for testing (`test/` emits WAVs). Bootloader code sits in `bootloader/`.
- **Licensing**: Eurorack code is GPLv3 (AVR) / MIT (STM32); hardware CC-BY-SA. Respect the "Mutable Instruments" trademark notice in `eurorack/README.md` when producing derivatives.

## Desktop Testing

- **Test harnesses**: Each unit can have a desktop test harness in `test/<unit>/` for offline DSP validation
- **Purpose**: Process WAV files through algorithms without hardware, enabling fast iteration and automated testing
- **Build**: `cd test/<unit> && make`
- **Run**: `./unit_test input.wav output.wav [options]`
- **CI**: Automated tests run via `.github/workflows/dsp-test.yml`

## Guidelines

- **Naming/ASCII**: Keep unit names and parameter strings to the allowed ASCII set noted in drumlogue docs; avoid non-ASCII in code unless already present.
- **When adding new units**: Create a new directory in `drumlogue/`, copy Makefile from a template, create `config.mk`, `header.c`, and `unit.cc`. Keep source lists minimal and prefer adding includes/libs via config vars rather than editing shared build scripts.
- **Debugging**: For quick desktop-side checks in Mutable code, run the module's `test/` harness to generate WAVs; for drumlogue, rely on build logs and load `.drmlgunit` onto hardware via USB mass storage.
- **Deployment**: Copy `.drmlgunit` files to the drumlogue's Units/* folder via USB mass storage.
