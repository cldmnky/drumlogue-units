# Drumpler Multi-ROM Build System

Automatically builds all 20 drumpler ROM variants (1 internal + 19 SR-JV80 expansion boards) with a single command.

## Prerequisites

ROM files are **not** included in this repository (they're ~180MB and pulled from archive.org). You must download them before building.

### Download ROM Files

```bash
cd drumlogue/drumpler
make -f Makefile.roms all
```

This downloads the JV-880 ROM pack from archive.org, extracts all ROM files into `resources/`, and auto-generates the `roms.manifest` file.

**ROM Management Commands:**

```bash
# Download and extract ROMs + generate manifest
make -f Makefile.roms all

# Just download/extract ROMs (skip manifest generation)
make -f Makefile.roms roms

# Just generate manifest from existing ROMs
make -f Makefile.roms manifest

# Verify ROM files are present
make -f Makefile.roms verify

# Remove downloaded ROMs (saves ~180MB disk space)
make -f Makefile.roms clean-roms

# Remove ROMs + all generated files
make -f Makefile.roms clean

# Show help
make -f Makefile.roms help
```

## Quick Start

```bash
# 1. Download ROM files (one-time setup)
cd drumlogue/drumpler && make -f Makefile.roms all && cd ../..

# 2. Build all ROM variants (~4 hours)
./build.sh drumpler

# Output: 20 .drmlgunit files in drumlogue/drumpler/
# - drumpler_internal.drmlgunit (4.3 MB)
# - drumpler_pop.drmlgunit (12.9 MB)
# - drumpler_orchestral.drmlgunit (12.9 MB)
# ... (19 expansion ROM variants)
```

## How It Works

1. **Manifest-driven**: `roms.manifest` defines all ROM variants with:
   - ROM tag (for output filename)
   - Unit ID offset (each ROM gets unique unit_id)
   - Display name (shown on drumlogue screen)
   - Expansion ROM filename

2. **Orchestrated build**: `build_all_roms.sh` loops through manifest:
   - Combines base ROM files + expansion ROM → temporary binary
   - Generates `rom_data.cc` via `xxd -i`
   - Patches `header.c` with unique unit_id and display name
   - Patches `config.mk` to add `rom_data.cc` to sources
   - Builds via SDK container
   - Renames output to `drumpler_<tag>.drmlgunit`
   - Restores original files

3. **Automatic detection**: Root `build.sh` detects `drumpler` project and invokes multi-ROM build

## Unit IDs

Each ROM variant gets a unique unit_id starting from `0x00000005U` (base):

- `0x00000005U` - internal (no expansion) - "drumpler"
- `0x00000006U` - pop (SR-JV80-01) - "Pop"
- `0x00000007U` - orchestral (SR-JV80-02) - "Orchestral"
- ...
- `0x00000018U` - house (SR-JV80-19) - "House"

## Display Names

Truncated to 13 ASCII characters for drumlogue screen:

- Pop, Orchestral, Piano, VintageSynth, World, Dance
- SuperSoundSet, Keyboards6070, Session, Bass&Drum
- Techno, HipHop, Vocal, Asia, SpecialFX
- OrchestralII, Country, Latin, House

## Managing ROMs

### Manifest Format

The `roms.manifest` file is auto-generated from ROM files in `resources/`:

```
# Format: ROM_TAG|UNIT_ID_OFFSET|DISPLAY_NAME|EXPANSION_FILE
internal|0|drumpler|
pop|1|Pop|SR-JV80-01 Pop - CS 0x3F1CF705.bin
orchestral|2|Orchestral|SR-JV80-02 Orchestral - CS 0x7D8CCAD6.bin
```

**Regenerate manifest** (if you add/remove ROM files):

```bash
cd drumlogue/drumpler
make -f Makefile.roms manifest
```

### Add/Remove ROM Variants

1. Add/remove `.bin` files in `resources/` (or download different ROM pack)
2. Regenerate manifest: `make -f Makefile.roms manifest`
3. Rebuild: `./build.sh drumpler` from repo root

### Test Single ROM

Create `roms.manifest.test` with one ROM entry, then:

```bash
cd drumlogue/drumpler
mv roms.manifest roms.manifest.full
mv roms.manifest.test roms.manifest
cd ../..
./build.sh drumpler
# ... test build ...
cd drumlogue/drumpler
mv roms.manifest roms.manifest.test
mv roms.manifest.full roms.manifest
```

### Build Single ROM Manually

For development/testing of individual ROMs:

```bash
./drumlogue/drumpler/build_rom_unit.sh \
  drumlogue/drumpler/resources/SR-JV80-10\ Bass\ \&\ Drum\ -\ CS\ 0x3D83D02A.BIN \
  bassdrum 10 "Bass&Drum"

./build.sh drumpler-bassdrum
```

## Output Files

All `.drmlgunit` files are stored in `drumlogue/drumpler/`:

```
drumpler_internal.drmlgunit    (4.3 MB - internal ROM only)
drumpler_pop.drmlgunit        (12.9 MB - SR-JV80-01)
drumpler_orchestral.drmlgunit (12.9 MB - SR-JV80-02)
...
drumpler_house.drmlgunit      (12.9 MB - SR-JV80-19)
```

Total: ~250 MB (20 units × ~12.5 MB average)

## Git Ignore

**ROM files** and temporary build artifacts are ignored:

```
# ROM files (downloaded, not checked in)
drumlogue/drumpler/resources/*.bin
drumlogue/drumpler/resources/*.zip
drumlogue/drumpler/roms.manifest

# Temporary build artifacts
drumlogue/drumpler/rom_data.cc
drumlogue/drumpler/.rom_*.bin
drumlogue/drumpler/header.c.bak
drumlogue/drumpler/config.mk.bak
```

Output `.drmlgunit` files are **also ignored** by default (via `*.drmlgunit` pattern).

ROM files are large (~180MB total) and pulled from archive.org on-demand via `make -f Makefile.roms`.

To track specific ROM units in git, you can force-add them:

```bash
git add -f drumlogue/drumpler/drumpler_bassdrum.drmlgunit
```

## Deployment

Copy `.drmlgunit` files to drumlogue via USB mass storage:

```bash
# Mount drumlogue, then:
cp drumlogue/drumpler/*.drmlgunit /Volumes/drumlogue/Units/
```

Each ROM variant appears as a separate synth unit with its own unique ID.

## Troubleshooting

### Build fails with "ROM files not found"

ROM files must be downloaded before building:

```bash
cd drumlogue/drumpler && make -f Makefile.roms all
```

This downloads the ~180MB JV-880 ROM pack from archive.org and extracts it into `resources/`.

### Build fails with "Expansion ROM not found"

Ensure expansion ROM files exist in `drumlogue/drumpler/resources/` with exact filenames from manifest.

If manifest is outdated, regenerate it: `make -f Makefile.roms manifest`

### Build produces small .drmlgunit (~65KB)

ROM data wasn't embedded. Check that `rom_data.cc` exists and is listed in `config.mk` CXXSRC.

### Infinite recursion / build hangs

Ensure `_DRUMPLER_INTERNAL_BUILD` environment variable is exported in `build_all_roms.sh` before calling `./build.sh drumpler`.

### Symbol errors: "undefined reference to g_drumpler_rom"

`rom_data.cc` wasn't compiled or linked. Verify it's in CXXSRC and check build logs.

## Performance

- **Serial builds**: ~10-15 minutes per ROM × 20 ROMs = **3-4 hours** total
- **Disk space**: ~250 MB for all 20 `.drmlgunit` files
- **Parallel optimization**: Not yet implemented (would require more complex dependency management)

## Future Enhancements

- [ ] Parallel builds (GNU parallel / make -j)
- [ ] Incremental builds (skip if ROM unchanged)
- [ ] Pre-unscrambled ROM cache
- [ ] Build progress bar UI
- [ ] Automatic preset name extraction
