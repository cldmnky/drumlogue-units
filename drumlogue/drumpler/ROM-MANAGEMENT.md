# Drumpler ROM Management Guide

## Overview

ROM files are **not** checked into the repository due to their size (~180MB total). Instead, they are downloaded on-demand from archive.org and auto-extracted into the `resources/` directory.

## Quick Start

```bash
# One-time setup: Download ROMs and generate manifest
cd drumlogue/drumpler
make -f Makefile.roms all

# Build all ROM variants
cd ../..
./build.sh drumpler
```

## ROM Management Commands

All commands are run from the `drumlogue/drumpler/` directory:

| Command | Description |
|---------|-------------|
| `make -f Makefile.roms all` | Download ROMs + generate manifest (default) |
| `make -f Makefile.roms roms` | Download and extract ROM pack only |
| `make -f Makefile.roms manifest` | Generate roms.manifest from ROMs |
| `make -f Makefile.roms verify` | Verify ROM integrity |
| `make -f Makefile.roms clean-roms` | Remove ROM files (saves ~180MB) |
| `make -f Makefile.roms clean` | Remove ROMs + generated files |
| `make -f Makefile.roms help` | Show help |

## ROM Source

ROMs are downloaded from:
```
https://ia800705.us.archive.org/26/items/jv880_rompack_v1/jv880_rompack_v1.zip
```

This archive contains:
- **5 base ROMs** (jv880_rom1.bin, jv880_rom2.bin, jv880_waverom1.bin, jv880_waverom2.bin, jv880_nvram.bin)
- **19 SR-JV80 expansion boards** (SR-JV80-01 through SR-JV80-19)

Total size: ~154MB compressed, ~180MB extracted

## Manifest Generation

The `roms.manifest` file is **auto-generated** from ROM files in `resources/`:

```bash
make -f Makefile.roms manifest
```

This scans for `SR-JV80-*.bin` files and generates manifest entries with:
- **ROM tag**: Lowercase identifier for filename (e.g., `pop`, `bassdrum`)
- **Unit ID offset**: Unique offset (1-19) added to base unit_id `0x00000005U`
- **Display name**: Truncated to 13 chars for drumlogue screen
- **Expansion file**: Full filename in `resources/`

### Manifest Format

```
# Format: ROM_TAG|UNIT_ID_OFFSET|DISPLAY_NAME|EXPANSION_FILE
internal|0|drumpler|NONE
pop|1|Pop|SR-JV80-01 Pop - CS 0x3F1CF705.bin
orchestral|2|Orchestral|SR-JV80-02 Orchestral - CS 0x3F0E09E2.BIN
```

## Adding/Removing ROMs

1. Add/remove `.bin` files in `resources/`
2. Regenerate manifest: `make -f Makefile.roms manifest`
3. Rebuild units: `./build.sh drumpler` from repo root

## Verification

Check ROM integrity:

```bash
make -f Makefile.roms verify
```

This verifies:
- All 5 base ROM files present
- Counts expansion ROM files
- Shows file sizes

Example output:
```
>> Verifying ROM pack integrity...
✓ Found: resources/jv880_rom1.bin (32K)
✓ Found: resources/jv880_rom2.bin (256K)
✓ Found: resources/jv880_waverom1.bin (2.0M)
✓ Found: resources/jv880_waverom2.bin (2.0M)
✓ Found: resources/jv880_nvram.bin (32K)
>> Expansion ROMs found: 19
```

## Git Ignore

ROM files are explicitly ignored in `.gitignore`:

```gitignore
# ROM files (downloaded, not checked in)
drumlogue/drumpler/resources/*.bin
drumlogue/drumpler/resources/*.BIN
drumlogue/drumpler/resources/*.zip
drumlogue/drumpler/roms.manifest

# Temporary build artifacts
drumlogue/drumpler/rom_data.cc
drumlogue/drumpler/.rom_*.bin
drumlogue/drumpler/header.c.bak
drumlogue/drumpler/config.mk.bak
```

Only `resources/README.md` is tracked in git as documentation.

## Clean Workflow

Remove ROMs to save disk space:

```bash
make -f Makefile.roms clean-roms  # Remove ROM files only
make -f Makefile.roms clean       # Remove ROMs + manifest + artifacts
```

Re-download when needed:

```bash
make -f Makefile.roms all
```

## Build Integration

The root `build.sh` automatically checks for ROM availability:

```bash
./build.sh drumpler
```

If ROMs are missing, you'll see:

```
ERROR: ROM files not found in drumlogue/drumpler/resources

To download and prepare ROM files:
  cd drumlogue/drumpler && make -f Makefile.roms all

This will download the JV-880 ROM pack from archive.org and generate the manifest.
```

## Troubleshooting

### Download fails with "curl: command not found"

Install curl:
```bash
# macOS (via Homebrew)
brew install curl

# Linux (Ubuntu/Debian)
sudo apt-get install curl
```

Or use wget (automatically detected):
```bash
# Linux
sudo apt-get install wget
```

### Extraction fails with "unzip: command not found"

Install unzip:
```bash
# macOS (via Homebrew)
brew install unzip

# Linux (Ubuntu/Debian)
sudo apt-get install unzip
```

### Manifest generation shows "No SR-JV80 ROMs found"

Check that ROM files were extracted correctly:
```bash
ls -l resources/*.bin resources/*.BIN
```

Re-download if needed:
```bash
make -f Makefile.roms clean
make -f Makefile.roms roms
```

### Build fails with "Expansion ROM not found"

Manifest may reference a file that doesn't exist. Regenerate manifest:
```bash
make -f Makefile.roms manifest
```

## Development Workflow

**First-time setup:**
```bash
cd drumlogue/drumpler
make -f Makefile.roms all
cd ../..
./build.sh drumpler  # Build all 20 variants
```

**After pulling repo updates:**
```bash
cd drumlogue/drumpler
make -f Makefile.roms verify  # Check if ROMs present
# If ROMs missing:
make -f Makefile.roms all
```

**Testing single ROM:**
```bash
# Edit roms.manifest to keep only one entry
./build.sh drumpler  # Faster build for testing
```

**Cleaning up disk space:**
```bash
cd drumlogue/drumpler
make -f Makefile.roms clean-roms  # Saves ~180MB
# Re-download later when needed
```

## File Sizes

| Item | Size | Location |
|------|------|----------|
| ROM pack (ZIP) | ~154MB | `resources/jv880_rompack_v1.zip` |
| Extracted ROMs | ~180MB | `resources/*.bin` |
| Single .drmlgunit | ~13MB | `drumlogue/drumpler/*.drmlgunit` |
| All 20 units | ~250MB | Build output |

## Related Documentation

- [README-MULTI-ROM.md](README-MULTI-ROM.md) - Multi-ROM build system overview
- [resources/README.md](resources/README.md) - ROM pack documentation
- [../../.github/copilot-instructions.md](../../.github/copilot-instructions.md) - Repository guidelines
