# Multi-ROM Build System Plan for Drumpler

## Goal
Build all 19 SR-JV80 expansion ROM variants with a single command: `./build.sh drumpler`

All `.drmlgunit` files will be stored in `drumlogue/drumpler/` with ROM-specific names.

## Current System Analysis

### Available ROMs (19 expansion boards)
Located in `drumlogue/drumpler/resources/`:
- SR-JV80-01 Pop
- SR-JV80-02 Orchestral
- SR-JV80-03 Piano
- SR-JV80-04 Vintage Synth
- SR-JV80-05 World
- SR-JV80-06 Dance
- SR-JV80-07 Super Sound Set
- SR-JV80-08 Keyboards of the 60s and 70s
- SR-JV80-09 Session
- SR-JV80-10 Bass & Drum
- SR-JV80-11 Techno
- SR-JV80-12 HipHop
- SR-JV80-13 Vocal
- SR-JV80-14 Asia
- SR-JV80-15 Special FX
- SR-JV80-16 Orchestral II
- SR-JV80-17 Country
- SR-JV80-18 Latin
- SR-JV80-19 House

### Base ROM Files
- `jv880_rom1.bin` (32KB)
- `jv880_rom2.bin` (256KB)
- `jv880_waverom1.bin` (2MB)
- `jv880_waverom2.bin` (2MB)
- `jv880_nvram.bin` (32KB)

### Current Build Constraints
1. **Unit ID uniqueness**: Each ROM variant needs a unique `unit_id` in `header.c` (currently `0x00000005U`)
2. **Display name**: Maximum 13 ASCII chars, shown on drumlogue screen
3. **ROM embedding**: ROM data must be embedded via `xxd -i` → `rom_data.cc`
4. **Build directory**: SDK expects all files in project directory during build
5. **Container isolation**: Build runs in podman container with read-only source mounts

## Proposed Solution

### Architecture: Orchestrated Build Script

Create `drumlogue/drumpler/build_all_roms.sh` that:
1. Scans `resources/` for base ROM + expansion ROMs
2. For each expansion ROM:
   - Generates combined ROM binary (base + expansion)
   - Creates `rom_data.cc` via `xxd -i`
   - Temporarily patches `header.c` with unique unit_id and name
   - Invokes SDK build via container
   - Copies output `.drmlgunit` with ROM-specific name
   - Restores original `header.c` and cleans up `rom_data.cc`

### Naming Convention

**Unit IDs**: Base `0x00000005U` + offset
- `0x00000005U` - Internal ROM only (no expansion) - "drumpler"
- `0x00000006U` - SR-JV80-01 Pop - "Pop"
- `0x00000007U` - SR-JV80-02 Orchestral - "Orchestral"
- ... (sequential)
- `0x00000018U` - SR-JV80-19 House - "House"

**Display Names** (truncated to 13 chars):
- "Pop"
- "Orchestral"
- "Piano"
- "VintageSynth"
- "World"
- "Dance"
- "SuperSoundSet"
- "Keyboards6070" (truncated from "Keyboards of the 60s and 70s")
- "Session"
- "Bass&Drum"
- "Techno"
- "HipHop"
- "Vocal"
- "Asia"
- "SpecialFX"
- "OrchestralII"
- "Country"
- "Latin"
- "House"

**Output Files**:
- `drumpler.drmlgunit` (internal ROM only)
- `drumpler_pop.drmlgunit` (SR-JV80-01)
- `drumpler_orchestral.drmlgunit` (SR-JV80-02)
- `drumpler_piano.drmlgunit` (SR-JV80-03)
- ... etc.

### ROM Manifest File

Create `drumlogue/drumpler/roms.manifest` to define ROM metadata:

```bash
# ROM_TAG|UNIT_ID_OFFSET|DISPLAY_NAME|EXPANSION_FILE
internal|0|drumpler|NONE
pop|1|Pop|SR-JV80-01 Pop - CS 0x3F1CF705.bin
orchestral|2|Orchestral|SR-JV80-02 Orchestral - CS 0x3F0E09E2.BIN
piano|3|Piano|SR-JV80-03 Piano - CS 0x3F8DB303.bin
vintagesynth|4|VintageSynth|SR-JV80-04 Vintage Synth - CS 0x3E23B90C.BIN
world|5|World|SR-JV80-05 World - CS 0x3E8E8A0D.bin
dance|6|Dance|SR-JV80-06 Dance - CS 0x3EC462E0.bin
supersound|7|SuperSoundSet|SR-JV80-07 Super Sound Set - CS 0x3F1EE208.bin
keyboards|8|Keyboards6070|SR-JV80-08 Keyboards of the 60s and 70s - CS 0x3F1E3F0A.BIN
session|9|Session|SR-JV80-09 Session - CS 0x3F381791.BIN
bassdrum|10|Bass&Drum|SR-JV80-10 Bass & Drum - CS 0x3D83D02A.BIN
techno|11|Techno|SR-JV80-11 Techno - CS 0x3F046250.bin
hiphop|12|HipHop|SR-JV80-12 HipHop - CS 0x3EA08A19.BIN
vocal|13|Vocal|SR-JV80-13 Vocal - CS 0x3ECE78AA.bin
asia|14|Asia|SR-JV80-14 Asia - CS 0x3C8A1582.bin
specialfx|15|SpecialFX|SR-JV80-15 Special FX - CS 0x3F591CE4.bin
orchestral2|16|OrchestralII|SR-JV80-16 Orchestral II - CS 0x3F35B03B.bin
country|17|Country|SR-JV80-17 Country - CS 0x3ED75089.bin
latin|18|Latin|SR-JV80-18 Latin - CS 0x3EA51033.BIN
house|19|House|SR-JV80-19 House - CS 0x3E330C41.BIN
```

### Implementation Steps

#### 1. Create `drumlogue/drumpler/roms.manifest`
Defines all ROM variants to build with metadata.

#### 2. Create `drumlogue/drumpler/build_all_roms.sh`

```bash
#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
RESOURCES_DIR="${SCRIPT_DIR}/resources"
MANIFEST="${SCRIPT_DIR}/roms.manifest"
HEADER_BACKUP="${SCRIPT_DIR}/header.c.bak"
BASE_UNIT_ID=0x00000005

# Function: create_rom_binary
# Combines base ROM files + optional expansion into single binary
create_rom_binary() {
    local expansion_file="$1"
    local output_file="$2"
    
    # Concatenate: rom1 + rom2 + waverom1 + waverom2 + nvram + expansion(if present)
    cat "${RESOURCES_DIR}/jv880_rom1.bin" \
        "${RESOURCES_DIR}/jv880_rom2.bin" \
        "${RESOURCES_DIR}/jv880_waverom1.bin" \
        "${RESOURCES_DIR}/jv880_waverom2.bin" \
        "${RESOURCES_DIR}/jv880_nvram.bin" \
        > "$output_file"
    
    if [ "$expansion_file" != "NONE" ]; then
        cat "${RESOURCES_DIR}/${expansion_file}" >> "$output_file"
    fi
}

# Function: patch_header
# Temporarily modify header.c with unique unit_id and name
patch_header() {
    local unit_id="$1"
    local display_name="$2"
    local header_file="${SCRIPT_DIR}/header.c"
    
    cp "$header_file" "$HEADER_BACKUP"
    
    perl -pi -e "s/\.unit_id = 0x[0-9A-Fa-f]+U,/.unit_id = ${unit_id},/" "$header_file"
    perl -pi -e "s/\.name = \"[^\"]*\",/.name = \"${display_name}\",/" "$header_file"
}

# Function: restore_header
restore_header() {
    if [ -f "$HEADER_BACKUP" ]; then
        mv "$HEADER_BACKUP" "${SCRIPT_DIR}/header.c"
    fi
}

# Function: build_rom_variant
build_rom_variant() {
    local rom_tag="$1"
    local unit_id_offset="$2"
    local display_name="$3"
    local expansion_file="$4"
    
    local unit_id=$(printf "0x%08XU" $((BASE_UNIT_ID + unit_id_offset)))
    local combined_rom="${SCRIPT_DIR}/.rom_${rom_tag}.bin"
    local output_name="drumpler_${rom_tag}.drmlgunit"
    
    echo ">> Building ROM variant: ${rom_tag} (${display_name})"
    echo ">>   Unit ID: ${unit_id}"
    echo ">>   Expansion: ${expansion_file}"
    
    # Create combined ROM binary
    create_rom_binary "$expansion_file" "$combined_rom"
    
    # Generate rom_data.cc
    xxd -i "$combined_rom" > "${SCRIPT_DIR}/rom_data.cc"
    perl -pi -e 's/unsigned char [A-Za-z0-9_]+\[\]/extern const unsigned char g_drumpler_rom[]/' \
        "${SCRIPT_DIR}/rom_data.cc"
    perl -pi -e 's/unsigned int [A-Za-z0-9_]+_len/extern const unsigned int g_drumpler_rom_size/' \
        "${SCRIPT_DIR}/rom_data.cc"
    
    # Patch header.c
    patch_header "$unit_id" "$display_name"
    
    # Build via SDK (invoke build.sh from repo root)
    cd "${SCRIPT_DIR}/../.."
    ./build.sh drumpler build 2>&1 | tail -30
    
    # Copy output with ROM-specific name
    if [ -f "${SCRIPT_DIR}/drumpler.drmlgunit" ]; then
        mv "${SCRIPT_DIR}/drumpler.drmlgunit" "${SCRIPT_DIR}/${output_name}"
        echo ">>   ✓ Built: ${output_name}"
    else
        echo ">>   ✗ Build failed for ${rom_tag}"
        return 1
    fi
    
    # Cleanup
    restore_header
    rm -f "$combined_rom" "${SCRIPT_DIR}/rom_data.cc"
}

# Main build loop
echo "=== Building all ROM variants from ${MANIFEST} ==="

# Trap to ensure header is restored on error
trap restore_header EXIT ERR INT TERM

while IFS='|' read -r rom_tag unit_id_offset display_name expansion_file; do
    # Skip comments and empty lines
    [[ "$rom_tag" =~ ^#.*$ ]] && continue
    [[ -z "$rom_tag" ]] && continue
    
    build_rom_variant "$rom_tag" "$unit_id_offset" "$display_name" "$expansion_file" || {
        echo ">> ERROR: Failed building ${rom_tag}"
        exit 1
    }
    
done < "$MANIFEST"

echo "=== All ROM variants built successfully ==="
ls -lh "${SCRIPT_DIR}"/*.drmlgunit
```

#### 3. Modify root `build.sh`

Add detection for `drumpler` project to invoke multi-ROM build:

```bash
PROJECT="${1:-clouds-revfx}"
ACTION="${2:-build}"

# Special handling for drumpler: build all ROM variants
if [ "$PROJECT" = "drumpler" ] && [ "$ACTION" = "build" ]; then
    echo ">> Detected drumpler project: building all ROM variants..."
    exec "${SCRIPT_DIR}/drumlogue/drumpler/build_all_roms.sh"
fi

# ... rest of existing build.sh code
```

#### 4. Update `.gitignore`

```gitignore
# Drumpler multi-ROM build artifacts
drumlogue/drumpler/*.drmlgunit
drumlogue/drumpler/rom_data.cc
drumlogue/drumpler/.rom_*.bin
drumlogue/drumpler/header.c.bak
```

### Build Workflow

**User runs:**
```bash
./build.sh drumpler
```

**System does:**
1. Detects `drumpler` in `build.sh`
2. Invokes `drumlogue/drumpler/build_all_roms.sh`
3. Reads `roms.manifest`
4. For each ROM entry:
   - Combines base ROM + expansion into temporary `.rom_*.bin`
   - Generates `rom_data.cc` via `xxd -i`
   - Backs up `header.c`
   - Patches `header.c` with unique unit_id and display_name
   - Runs SDK build via `./build.sh drumpler build` (actual SDK build)
   - Renames output to `drumpler_<tag>.drmlgunit`
   - Restores `header.c`, removes `rom_data.cc`
5. All `.drmlgunit` files remain in `drumlogue/drumpler/`

**Output:**
```
drumlogue/drumpler/
├── drumpler_internal.drmlgunit      (4.3 MB, no expansion)
├── drumpler_pop.drmlgunit           (12.9 MB)
├── drumpler_orchestral.drmlgunit    (12.9 MB)
├── drumpler_piano.drmlgunit         (12.9 MB)
... (19 total expansion variants)
```

### Alternative Build Options

**Build single ROM variant:**
```bash
# Still works for individual ROMs
./build_rom_unit.sh resources/SR-JV80-10.bin pop 6 "Pop"
./build.sh drumpler-pop
```

**Clean all ROM artifacts:**
```bash
rm -f drumlogue/drumpler/*.drmlgunit
```

## Testing Strategy

1. **Dry-run test**: Run `build_all_roms.sh` with echo-only mode to verify logic
2. **Single ROM test**: Build one ROM variant (SR-JV80-10 Bass & Drum)
3. **Full build test**: Build all 19 ROM variants (~4 hours estimated)
4. **Symbol check**: Verify no undefined symbols in any `.drmlgunit`
5. **Hardware test**: Load 3-4 variants onto drumlogue and verify:
   - Unique unit IDs recognized
   - Correct preset names displayed
   - Audio playback works

## Pros & Cons

### Pros
✅ Single command builds all variants  
✅ No SDK Makefile modifications needed  
✅ Clear separation of concerns  
✅ Manifest-driven (easy to add/remove ROMs)  
✅ All outputs in one directory  
✅ Preserves original `header.c` and files  
✅ Compatible with existing build_rom_unit.sh workflow  

### Cons
⚠️ Serial build (no parallelization) - ~4 hours for all 19 ROMs  
⚠️ Temporary file juggling (rom_data.cc, header.c backup)  
⚠️ Each build is ~13MB = ~250MB total storage  
⚠️ Need to rebuild all variants if wrapper code changes  

## Future Enhancements

1. **Parallel builds**: Use GNU parallel or xargs to build multiple ROMs concurrently
2. **Incremental builds**: Track ROM checksums, skip if `.drmlgunit` is up-to-date
3. **ROM validation**: Verify expansion ROM checksums before build
4. **Preset name extraction**: Pre-generate preset name tables for each ROM
5. **Build cache**: Cache unscrambled expansion ROMs to speed up builds

## Estimated Timeline

- **Step 1-2** (manifest + script): 2 hours
- **Step 3** (build.sh integration): 30 minutes
- **Testing**: 6-8 hours (full build cycle)
- **Total**: 8-10 hours

## Questions for User

1. Should we build the "internal ROM only" variant (`drumpler_internal.drmlgunit`)?
2. Acceptable to have ~250MB of `.drmlgunit` files in `drumlogue/drumpler/`?
3. Should these be tracked in git or added to `.gitignore`?
4. Prefer parallel builds (complex) or serial builds (simple)?
5. Any specific ROM naming conventions preferred for drumlogue display?
