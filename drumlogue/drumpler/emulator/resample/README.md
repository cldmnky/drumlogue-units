# libresample

This directory contains source files from [minorninth/libresample](https://github.com/minorninth/libresample).

## License

Dual-licensed as LGPL and BSD. See:
- https://github.com/minorninth/libresample/blob/master/LICENSE-LGPL.txt
- https://github.com/minorninth/libresample/blob/master/LICENSE-BSD.txt

## Files

- `libresample.h` - Public API header
- `resample.c` - Main API implementation
- `resamplesubs.c` - Core resampling subroutines
- `filterkit.c` - Kaiser-windowed sinc filter implementation
- `filterkit.h` - Filter function declarations
- `resample_defs.h` - Internal definitions and constants
- `config.h` - Build configuration

## Usage

This library provides high-quality audio resampling using windowed sinc interpolation (Kaiser window).

For the JV-880 emulator, we use:
- `highQuality=1` - 35 filter taps for best quality
- `factor=0.75` - Fixed ratio for 64kHz → 48kHz conversion
