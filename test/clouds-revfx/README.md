# Clouds RevFX Local Test Harness

This directory contains a test harness for locally testing the Clouds reverb/FX DSP code without uploading to the Drumlogue hardware.

## Overview

The test harness compiles the DSP core (`CloudsFx` class) for your host machine (macOS/Linux) and allows you to:

1. Process WAV files through the reverb/effects chain
2. Verify the algorithm behavior
3. Run automated tests in CI

## Prerequisites

- A C++17 compatible compiler (g++ or clang++)
- GNU Make
- libsndfile (for WAV file I/O)

### Installing libsndfile

**macOS:**

```bash
brew install libsndfile
```

**Ubuntu/Debian:**

```bash
sudo apt-get install libsndfile1-dev
```

## Building

```bash
cd test/clouds-revfx
make
```

## Running the Test Harness

### Process a WAV file

```bash
./clouds_revfx_test input.wav output.wav [options]
```

**Options:**

- `--dry-wet <0-100>`: Dry/wet mix (default: 50)
- `--time <0-100>`: Reverb time (default: 50)
- `--diffusion <0-100>`: Diffusion amount (default: 50)
- `--lp <0-100>`: Low-pass filter (default: 70)
- `--input-gain <0-100>`: Input gain (default: 20)
- `--texture <0-100>`: Diffuser texture (default: 0)
- `--grain-amt <0-100>`: Granular mix amount (default: 0)
- `--shift-amt <0-100>`: Pitch shifter amount (default: 0)

### Generate test signals

```bash
./clouds_revfx_test --generate-impulse impulse.wav
./clouds_revfx_test --generate-sine sine.wav [frequency_hz]
./clouds_revfx_test --generate-noise noise.wav
```

### Run automated tests

```bash
make test
```

## CI/CD

This test harness runs automatically on GitHub Actions whenever changes are pushed to:

- `drumlogue/clouds-revfx/**`
- `eurorack/**`
- `test/clouds-revfx/**`

The workflow runs on both Ubuntu and macOS to ensure cross-platform compatibility.

## Directory Structure

```text
test/clouds-revfx/
├── Makefile           # Build configuration
├── main.cc            # Test harness entry point
├── wav_file.h         # Simple WAV file I/O using libsndfile
├── unit.h             # Stubs for logue SDK types
└── fixtures/          # Test WAV files (generated)
```

## How It Works

The test harness provides:

1. **Stub implementations** for drumlogue SDK types (`unit.h`, `unit_runtime_desc_t`, etc.)
2. **WAV file I/O** via libsndfile
3. **Main harness** that initializes the `CloudsFx` class and processes audio

The DSP code (`clouds_fx.cc`, `clouds_fx.h`, and the effect headers) is compiled directly from the `drumlogue/clouds-revfx/` directory without modification.

## CI Integration

The GitHub Actions workflow runs:

1. Build the test harness
2. Generate test signals (impulse, sine, noise)
3. Process through reverb with various settings
4. Verify output files are valid (non-zero, correct length)
5. Optionally run comparison tests against reference outputs
