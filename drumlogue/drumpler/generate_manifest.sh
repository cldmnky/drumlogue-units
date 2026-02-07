#!/usr/bin/env bash
#
# Generate roms.manifest from downloaded ROM files
# Scans resources/ directory for SR-JV80 expansion ROMs and creates manifest entries
#
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
RESOURCES_DIR="${SCRIPT_DIR}/resources"
BASE_UNIT_ID=0x00000005

# Display name mapping (truncated to 13 chars for drumlogue)
declare -A DISPLAY_NAMES=(
    ["01"]="Pop"
    ["02"]="Orchestral"
    ["03"]="Piano"
    ["04"]="VintageSynth"
    ["05"]="World"
    ["06"]="Dance"
    ["07"]="SuperSoundSet"
    ["08"]="Keyboards6070"
    ["09"]="Session"
    ["10"]="Bass&Drum"
    ["11"]="Techno"
    ["12"]="HipHop"
    ["13"]="Vocal"
    ["14"]="Asia"
    ["15"]="SpecialFX"
    ["16"]="OrchestralII"
    ["17"]="Country"
    ["18"]="Latin"
    ["19"]="House"
)

# ROM tag mapping (for output filename)
declare -A ROM_TAGS=(
    ["01"]="pop"
    ["02"]="orchestral"
    ["03"]="piano"
    ["04"]="vintagesynth"
    ["05"]="world"
    ["06"]="dance"
    ["07"]="supersound"
    ["08"]="keyboards"
    ["09"]="session"
    ["10"]="bassdrum"
    ["11"]="techno"
    ["12"]="hiphop"
    ["13"]="vocal"
    ["14"]="asia"
    ["15"]="specialfx"
    ["16"]="orchestral2"
    ["17"]="country"
    ["18"]="latin"
    ["19"]="house"
)

# Generate manifest header
cat <<'EOF'
# Drumpler ROM Variants Manifest
# Format: ROM_TAG|UNIT_ID_OFFSET|DISPLAY_NAME|EXPANSION_FILE
#
# ROM_TAG: Unique tag for output file naming (e.g., drumpler_<tag>.drmlgunit)
# UNIT_ID_OFFSET: Offset added to base unit_id 0x00000005U
# DISPLAY_NAME: Shown on drumlogue screen (max 13 chars, 7-bit ASCII)
# EXPANSION_FILE: Filename in resources/ (or NONE for internal ROM only)
#
# Auto-generated from downloaded ROM files
# Base unit_id: 0x00000005U (CLDM developer ID)
# Unit IDs: 0x00000005 + offset

# Internal ROM only (no expansion board)
internal|0|drumpler|NONE

# SR-JV80 Series Expansion Boards (01-19)
EOF

# Scan for SR-JV80 ROM files and generate entries
# Look for files matching pattern: SR-JV80-NN *
find "${RESOURCES_DIR}" -maxdepth 1 -type f \( -name "SR-JV80-*.bin" -o -name "SR-JV80-*.BIN" \) 2>/dev/null | sort | while read -r rom_file; do
    basename_file=$(basename "$rom_file")
    
    # Extract ROM number from filename (e.g., "SR-JV80-10 Bass & Drum - CS 0x3D83D02A.BIN" -> "10")
    if [[ "$basename_file" =~ SR-JV80-([0-9]{2}) ]]; then
        rom_num="${BASH_REMATCH[1]}"
        
        # Get display name and tag
        display_name="${DISPLAY_NAMES[$rom_num]:-ROM${rom_num}}"
        rom_tag="${ROM_TAGS[$rom_num]:-rom${rom_num}}"
        
        # Calculate unit_id offset (ROM 01 = offset 1, ROM 02 = offset 2, etc.)
        unit_id_offset=$((10#$rom_num))
        
        # Output manifest entry
        echo "${rom_tag}|${unit_id_offset}|${display_name}|${basename_file}"
    fi
done
