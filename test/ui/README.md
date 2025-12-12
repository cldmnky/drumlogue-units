# Drumlogue Unit Player

Interactive player for testing drumlogue units in ARM emulation with real-time audio output.

## Quick Start

### macOS (recommended)
```bash
./play-macos.sh <path-to-unit.drmlgunit>
```

### Linux  
```bash
./player.sh <path-to-unit.drmlgunit>
```

Example:
```bash
./play-macos.sh ../../drumlogue/pepege-synth/pepege_synth.drmlgunit
```

## Requirements

### Linux
- `qemu-user-static` - ARM user-mode emulation
- `sox` - Audio playback

```bash
sudo apt-get install qemu-user-static sox
```

### macOS (Apple Silicon)
The player requires `qemu-arm-static` which isn't available natively on macOS ARM. 

**Alternative on macOS:** Use the profiling tests directly:
```bash
cd ../qemu-arm
./test-unit.sh <unit-name> --profile
```

Or test with specific signals and play the output:
```bash
./test-unit.sh pepege-synth
play build/output_pepege-synth.wav
```

This approach works on macOS since the test-unit.sh handles the container execution automatically.

## Features

- Play with different input signals (sine, noise, sweep, impulse)
- CPU profiling and performance metrics
- View unit information and parameters
- Real-time audio output
- All processing in ARM emulation

## Menu

1. Play with sine wave (440Hz)
2. Play with white noise  
3. Play with frequency sweep
4. Play with impulse
5. Run CPU profiling
6. Show unit info
7. Quit

## How It Works

The player uses QEMU ARM emulation to run the actual .drmlgunit DSP code, processes test signals, and pipes the output audio to your sound system via sox/aplay.

This tests units exactly as they would run on drumlogue hardware!
