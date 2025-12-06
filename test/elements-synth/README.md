# Elements Synth Test Harness

Local testing harness for the elements-synth DSP code. Compiles the synthesis
engine for desktop (macOS/Linux) to enable offline testing and debugging without
drumlogue hardware.

## Prerequisites

### macOS
```bash
brew install libsndfile
```

### Linux (Debian/Ubuntu)
```bash
sudo apt-get install libsndfile1-dev
```

## Building

```bash
cd test/elements-synth
make
```

For debug build with symbols:
```bash
make DEBUG=1
```

## Usage

### Basic usage
```bash
# Generate a 2-second note with default settings
./elements_synth_test output.wav

# Use a preset
./elements_synth_test output.wav --preset MARIMBA

# Specify note and velocity
./elements_synth_test output.wav --preset PLUCK --note 48 --velocity 120

# Play a sequence of notes
./elements_synth_test scale.wav --preset VIBES --notes 60,62,64,65,67,69,71,72 --duration 4
```

### Available presets
- 0: INIT - Basic initialization
- 1: MARIMBA - Mallet percussion
- 2: VIBES - Vibraphone-like
- 3: PLUCK - Plucked string
- 4: BOW - Bowed string
- 5: FLUTE - Wind instrument
- 6: STRING - Karplus-Strong string
- 7: MSTRING - Multi-string (sympathetic)

### Command-line options
```
--preset <name|num>     Use a preset (0-7 or name)
--note <0-127>          MIDI note number (default: 60)
--velocity <1-127>      Note velocity (default: 100)
--duration <seconds>    Duration in seconds (default: 2.0)
--notes <n1,n2,...>     Play a sequence of notes
--analyze               Check output for NaN/Inf/clipping

# Individual parameters:
--bow <0-127>           Bow level
--blow <0-127>          Blow level
--strike <0-127>        Strike level
--mallet <0-11>         Mallet type
--geometry <-64 to 63>  Resonator geometry
--brightness <-64 to 63> Resonator brightness
--damping <-64 to 63>   Resonator damping
--cutoff <0-127>        Filter cutoff
--resonance <0-127>     Filter resonance
--model <0-2>           Model (0=MODAL, 1=STRING, 2=MSTRING)
--attack <0-127>        Envelope attack
--decay <0-127>         Envelope decay
--release <0-127>       Envelope release
```

## Testing for DSP Problems

The `--analyze` flag checks the output for:
- NaN (Not a Number) samples
- Inf (Infinity) samples
- Clipping (samples > 0.99)

```bash
# Test a single preset
./elements_synth_test test.wav --preset MARIMBA --analyze

# Test all presets for stability
make test-analyze
```

Exit codes:
- 0: Success
- 1: Argument error
- 2: DSP problems detected (NaN/Inf)

## Debugging

For debugging DSP issues:

```bash
# Build with debug symbols
make clean
make DEBUG=1

# Run under debugger
make debug

# Or manually:
lldb ./elements_synth_test -- test.wav --preset INIT --analyze
```

## Test Targets

```bash
make test          # Quick test with INIT preset
make test-preset   # Test several presets
make test-notes    # Test note sequences
make test-analyze  # Test all presets for stability
make test-all      # Run all tests
```

## Output

The test harness writes stereo 48kHz float WAV files that can be played
in any audio player or analyzed in a DAW or audio editor like Audacity.

## Troubleshooting

### "libsndfile not found"
Make sure libsndfile is installed:
- macOS: `brew install libsndfile`
- Linux: `sudo apt-get install libsndfile1-dev`

### Compilation errors
The DSP code is header-only. Make sure you're building from the
`test/elements-synth` directory where the stub `unit.h` is located.

### NaN/Inf detected
This indicates a DSP bug (division by zero, filter instability, etc.).
Use `--analyze` and debug build to track down the issue.
