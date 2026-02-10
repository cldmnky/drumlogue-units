# Drumpler ROM Resources

This directory contains JV-880 ROM files for the drumpler unit build system.

## ROM Files

### Base System ROMs
- `jv880_rom1.bin` (32 KB) - Main ROM 1
- `jv880_rom2.bin` (256 KB) - Main ROM 2
- `jv880_waverom1.bin` (2 MB) - Wave ROM 1
- `jv880_waverom2.bin` (2 MB) - Wave ROM 2
- `jv880_nvram.bin` (32 KB) - NVRAM

### SR-JV80 Expansion ROMs (8 MB each)
- `SR-JV80-01 Pop - CS 0x3F1CF705.bin`
- `SR-JV80-02 Orchestral - CS 0x3F0E09E2.BIN`
- `SR-JV80-03 Piano - CS 0x3F8DB303.bin`
- `SR-JV80-04 Vintage Synth - CS 0x3E23B90C.BIN`
- `SR-JV80-05 World - CS 0x3E8E8A0D.bin`
- `SR-JV80-06 Dance - CS 0x3EC462E0.bin`
- `SR-JV80-07 Super Sound Set - CS 0x3F1EE208.bin`
- `SR-JV80-08 Keyboards of the 60s and 70s - CS 0x3F1E3F0A.BIN`
- `SR-JV80-09 Session - CS 0x3F381791.BIN`
- `SR-JV80-10 Bass & Drum - CS 0x3D83D02A.BIN`
- `SR-JV80-11 Techno - CS 0x3F046250.bin`
- `SR-JV80-12 HipHop - CS 0x3EA08A19.BIN`
- `SR-JV80-13 Vocal - CS 0x3ECE78AA.bin`
- `SR-JV80-14 Asia - CS 0x3C8A1582.bin`
- `SR-JV80-15 Special FX - CS 0x3F591CE4.bin`
- `SR-JV80-16 Orchestral II - CS 0x3F35B03B.bin`
- `SR-JV80-17 Country - CS 0x3ED75089.bin`
- `SR-JV80-18 Latin - CS 0x3EA51033.BIN`
- `SR-JV80-19 House - CS 0x3E330C41.BIN`

### RD-500 ROMs
- `rd500_expansion.bin` (8 MB)
- `rd500_patches.bin` (128 KB)

## Usage

To create a drumpler unit for a specific ROM:

```bash
cd /path/to/drumlogue-units
./drumlogue/drumpler/build_rom_unit.sh drumlogue/drumpler/resources/<rom_file.bin> [rom_tag] [unit_id] [display_name]
```

Example:
```bash
./drumlogue/drumpler/build_rom_unit.sh drumlogue/drumpler/resources/SR-JV80-01\ Pop\ -\ CS\ 0x3F1CF705.bin pop 0x06 "Pop Drums"
```

This will create a new unit at `drumlogue/drumpler-pop/` with the ROM embedded.

## Source

ROMs downloaded from: https://archive.org/download/jv880_rompack_v1

## Licensing Note

These ROM files are protected by copyright. They are for use with the JV-880 emulator for personal, non-commercial use only. Distribution of the ROM files themselves is not permitted.

## .gitignore

This directory is excluded from git via `.gitignore` to avoid committing large binary files to version control.
