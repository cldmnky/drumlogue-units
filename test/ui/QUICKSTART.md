# UI Player - Quick Reference

## What Was Created

I've created an interactive player for testing drumlogue units with real-time audio output in ARM emulation. It's located in `test/ui/`.

## Files Created

- **`player.sh`** - Main interactive player script ✅ READY TO USE
- **`README.md`** - Usage documentation
- `pattern.h/c` - 16-step sequencer (for future ncurses UI)
- `ui_main.c` - Full ncurses UI (requires API updates)
- `Makefile` - Native build
- `Makefile.podman` - ARM cross-compilation build

## How To Use

```bash
cd test/ui
./player.sh ../../drumlogue/<unit-name>/<unit>.drmlgunit
```

### Example

```bash
./player.sh ../../drumlogue/pepege-synth/pepege_synth.drmlgunit
```

## Features

The interactive menu provides:

1. **Play with sine wave** - Test with 440Hz tone
2. **Play with noise** - White noise input
3. **Play with sweep** - Frequency sweep
4. **Play with impulse** - Impulse response
5. **CPU profiling** - Performance metrics with 10-second test
6. **Unit info** - View parameters and metadata
7. **Quit**

## Requirements

- ✅ `qemu-arm-static` (you have this)
- ⚠️ `sox` for audio playback

Install sox:
```bash
brew install sox
```

## How It Works

```
player.sh → unit_host (ARM) → .drmlgunit → QEMU → sox → 🔊
```

The player:
1. Loads your unit in ARM emulation (QEMU)
2. Processes test signals through the DSP
3. Outputs audio to your sound system
4. All in real-time!

## What's Next

The full ncurses UI with pattern sequencer and live parameter editing is already coded in `ui_main.c`, but needs the unit_host API to be refactored for better callback management. The current `player.sh` provides immediate functionality using the existing test infrastructure.

## Testing

Try it now with any unit:

```bash
cd test/ui

# Test a synth
./player.sh ../../drumlogue/pepege-synth/pepege_synth.drmlgunit

# Test an effect  
./player.sh ../../drumlogue/clouds-revfx/clouds_revfx.drmlgunit

# Test profiling (10 MIDI notes, parameter changes)
# Choose option 5 from the menu
```

The profiling test now:
- **For synths**: Triggers 10 different MIDI notes with varying velocities
- **For synths**: Changes 3-5 random parameters every second
- **For all units**: Runs for 10 seconds (synths) or 1 second (effects)
- **Shows**: CPU usage, real-time factor, headroom

Enjoy! 🎵
