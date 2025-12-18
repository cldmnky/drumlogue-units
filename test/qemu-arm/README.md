# QEMU ARM Testing for Drumlogue Units

Test `.drmlgunit` files in QEMU ARM emulation before deploying to hardware. This validates units in an environment that closely matches the drumlogue's ARM Cortex-A7 processor.

## Quick Start

```bash
# Test a unit (auto-builds if needed)
./test-unit.sh clouds-revfx

# Or use Makefile
make test-unit UNIT=clouds-revfx
```

That's it! The script handles all dependencies automatically.

## What This Tests

The test harness:
- ✅ Loads `.drmlgunit` files as shared libraries (`dlopen`)
- ✅ Emulates SDK runtime with minimal stubs
- ✅ Runs in QEMU ARM (Cortex-A7 emulation)
- ✅ Processes audio through actual ARM code paths
- ✅ Validates unit initialization and parameter handling

## Benefits Over Desktop Testing

| Desktop Test | QEMU ARM Test |
|-------------|---------------|
| x86_64 code | **ARM code paths** |
| Native libs | **Actual .drmlgunit binary** |
| Approximate behavior | **ARM-realistic execution** |
| Fast iteration | **Hardware validation** |

**Use both:** Desktop for rapid DSP iteration, QEMU for pre-hardware validation.

## Architecture

```
macOS/Linux Host           Podman Container              QEMU ARM
┌──────────────┐          ┌──────────────┐            ┌──────────────┐
│ test-unit.sh │  runs    │ qemu-arm     │  executes  │ unit_host    │
│              │  ──────> │              │  ────────> │  ↓           │
│ Input WAV    │          │ ARM libs     │            │ .drmlgunit   │
│ Output WAV   │  <────── │              │  <──────── │  ↓           │
│              │  results │              │  audio     │ unit_render  │
└──────────────┘          └──────────────┘            └──────────────┘
```

## Directory Structure

```
test/qemu-arm/
├── test-unit.sh           # Simple test script (use this!)
├── unit_host.c/.h         # ARM unit host implementation
├── sdk_stubs.c/.h         # SDK runtime stubs
├── wav_file.c/.h          # WAV I/O utilities
├── Makefile               # Auto-detects macOS/Linux
├── Makefile.podman        # Podman/container build (macOS)
├── Makefile.arm           # Native ARM build (Linux)
├── generate_signals.py    # Test signal generator
├── fixtures/              # Test input signals
│   ├── sine_440hz.wav
│   ├── impulse.wav
│   ├── noise.wav
│   └── sweep.wav
├── build/                 # Output directory
└── unit_host_arm          # Compiled ARM binary
```

## Manual Testing

If you need more control:

```bash
# 1. Build ARM binary (once)
make -f Makefile.podman podman-build

# 2. Generate test signals (once)
python3 generate_signals.py

# 3. Run custom test
podman run --rm -it \
  -v "$(pwd):/workspace:Z" \
  -v "../../drumlogue:/units:ro,Z" \
  ubuntu:22.04 bash -c "
    apt-get update -qq && dpkg --add-architecture armhf && \
    apt-get update -qq && apt-get install -y -qq \
      qemu-user-static libsndfile1:armhf libstdc++6:armhf && \
    qemu-arm-static -L /usr/arm-linux-gnueabihf \
      /workspace/unit_host_arm \
      /units/clouds-revfx/clouds_revfx.drmlgunit \
      /workspace/fixtures/sine_440hz.wav \
      /workspace/build/output.wav \
      --param-0 75 --verbose
  "
```

## Requirements

**macOS:**
- Podman (installed automatically by test script)
- Python 3 (for signal generation)

**Linux:**
- ARM cross-toolchain: `apt-get install gcc-arm-linux-gnueabihf`
- QEMU user mode: `apt-get install qemu-user-static`
- libsndfile: `apt-get install libsndfile1-dev:armhf`

## Troubleshooting

**"Unit file not found"**
→ Build the unit first: `cd ../.. && ./build.sh <unit-name>`

**"ARM host not found"**
→ The script auto-builds it, but you can run: `make -f Makefile.podman podman-build`

**"Unit initialization failed: -2"**
→ API version mismatch. See [FIXES.md](FIXES.md) for details on SDK stub fixes.

**Podman errors on macOS**
→ Ensure Podman machine is running: `podman machine start`

## CI Integration

QEMU ARM tests run automatically on pull requests via GitHub Actions:
- **Workflow:** `.github/workflows/qemu-test.yml`
- **Trigger:** Any PR that modifies drumlogue units or build system
- **Tests:** All units (clouds-revfx, elementish-synth, pepege-synth, drupiter-synth)
- **Artifacts:** WAV output files available for download (7 day retention)

The workflow:
1. Builds each unit using the containerized build system
2. Sets up QEMU ARM environment with dependencies
3. Runs `test-unit.sh` for each unit
4. Uploads test results and output WAV files

To view results:
1. Go to your PR on GitHub
2. Click "Checks" tab
3. Select "QEMU ARM Test" job for the unit you want to inspect
4. Download artifacts to listen to output audio

## Performance

QEMU ARM emulation is slower than native:
- Desktop test: ~0.1s for 1s audio
- QEMU ARM: ~2-5s for 1s audio (20-50x slower)

Use desktop tests for rapid iteration, QEMU for validation.

## Advanced Usage

**Test all units:**
```bash
for unit in ../../drumlogue/*/; do
  name=$(basename "$unit")
  [ "$name" = "common" ] && continue
  ./test-unit.sh "$name"
done
```

**Custom parameters:**
Edit `test-unit.sh` to add parameter flags like:
```bash
--param-0 50 --param-1 75 --verbose
```

**Compare output:**
```bash
./test-unit.sh clouds-revfx
# Process with desktop test too
../../test/clouds-revfx/clouds_revfx_test \
  fixtures/sine_440hz.wav build/desktop_output.wav
# Compare
diff build/output_clouds-revfx.wav build/desktop_output.wav
```

## Technical Details

See [FIXES.md](FIXES.md) for information about SDK stub implementation and bug fixes.

**Memory Layout:**
- CPU: ARM Cortex-A7 (matches drumlogue's i.MX 6ULZ @ 900MHz)
- Sample rate: 48kHz (fixed)
- Buffer size: 256 frames (configurable)
- Channels: 1 (synth) or 2 (effects)
- Format: float32

**Limitations:**
- No hardware peripherals
- Approximate timing (QEMU not real-time)
- No drumlogue firmware interaction
- Limited to unit callbacks only