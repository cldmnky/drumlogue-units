#!/usr/bin/env bash
#
# Build all drumpler ROM variants from roms.manifest
# Each variant gets a unique unit_id and is built as a separate .drmlgunit file
#
# Usage: build_all_roms.sh [PERF_MON=1] [__QEMU_ARM__=1] [...]
#
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"
RESOURCES_DIR="${SCRIPT_DIR}/resources"
MANIFEST="${SCRIPT_DIR}/roms.manifest"
HEADER_BACKUP="${SCRIPT_DIR}/header.c.bak"
CONFIG_BACKUP="${SCRIPT_DIR}/config.mk.bak"
BASE_UNIT_ID=0x00000005
BASE_PROJECT_NAME="drumpler"

# Capture extra build flags to pass through (e.g., PERF_MON=1, __QEMU_ARM__=1)
BUILD_FLAGS="$*"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

log_info() {
    echo -e "${BLUE}>>${NC} $*"
}

log_success() {
    echo -e "${GREEN}>>✓${NC} $*"
}

log_warn() {
    echo -e "${YELLOW}>>⚠${NC} $*"
}

log_error() {
    echo -e "${RED}>>✗${NC} $*"
}

# Build unscramble tool if needed
UNSCRAMBLE_TOOL="${RESOURCES_DIR}/unscramble_waverom"

build_unscramble_tool() {
    if [ ! -f "$UNSCRAMBLE_TOOL" ] || [ "${RESOURCES_DIR}/unscramble_waverom.c" -nt "$UNSCRAMBLE_TOOL" ]; then
        log_info "Compiling waverom unscramble tool..."
        cc -O2 -Wall -o "$UNSCRAMBLE_TOOL" "${RESOURCES_DIR}/unscramble_waverom.c"
    fi
}

# Unscramble a waverom file (cached: skip if output already exists and is newer than input)
unscramble_waverom() {
    local input_file="$1"
    local output_file="$2"

    if [ -f "$output_file" ] && [ "$output_file" -nt "$input_file" ]; then
        log_info "  Using cached unscrambled: $(basename "$output_file")"
        return 0
    fi

    log_info "  Unscrambling: $(basename "$input_file")..."
    "$UNSCRAMBLE_TOOL" "$input_file" "$output_file"
}

# Function: create_rom_binary
# Combines base ROM files + optional expansion into single binary
# Waveroms are pre-unscrambled at build time (Roland address+data bit scrambling)
create_rom_binary() {
    local expansion_file="$1"
    local output_file="$2"
    
    # Ensure unscramble tool is built
    build_unscramble_tool
    
    # Unscramble base waveroms (Roland JV-880 address+data bit scrambling)
    unscramble_waverom "${RESOURCES_DIR}/jv880_waverom1.bin" "${RESOURCES_DIR}/.jv880_waverom1_unscrambled.bin"
    unscramble_waverom "${RESOURCES_DIR}/jv880_waverom2.bin" "${RESOURCES_DIR}/.jv880_waverom2_unscrambled.bin"
    
    # Concatenate: rom1 + rom2 + waverom1(unscrambled) + waverom2(unscrambled) + nvram + expansion(if present)
    cat "${RESOURCES_DIR}/jv880_rom1.bin" \
        "${RESOURCES_DIR}/jv880_rom2.bin" \
        "${RESOURCES_DIR}/.jv880_waverom1_unscrambled.bin" \
        "${RESOURCES_DIR}/.jv880_waverom2_unscrambled.bin" \
        "${RESOURCES_DIR}/jv880_nvram.bin" \
        > "$output_file"
    
    if [ "$expansion_file" != "NONE" ]; then
        if [ ! -f "${RESOURCES_DIR}/${expansion_file}" ]; then
            log_error "Expansion ROM not found: ${expansion_file}"
            return 1
        fi
        # Unscramble expansion ROM too (SR-JV80 series use same scrambling)
        local exp_unscrambled="${RESOURCES_DIR}/.${expansion_file%.bin}_unscrambled.bin"
        unscramble_waverom "${RESOURCES_DIR}/${expansion_file}" "$exp_unscrambled"
        cat "$exp_unscrambled" >> "$output_file"
    fi
}

# Function: patch_header
# Temporarily modify header.c with unique unit_id and name
patch_header() {
    local unit_id="$1"
    local display_name="$2"
    local header_file="${SCRIPT_DIR}/header.c"
    
    # Backup original
    cp "$header_file" "$HEADER_BACKUP"
    
    # Patch unit_id and name
    perl -pi -e "s/\.unit_id = 0x[0-9A-Fa-f]+U,/.unit_id = ${unit_id},/" "$header_file"
    perl -pi -e "s/\.name = \"[^\"]*\",/.name = \"${display_name}\",/" "$header_file"
}

# Function: patch_config
# Temporarily modify config.mk with ROM-specific PROJECT name and add rom_data.cc
patch_config() {
    local project_name="$1"
    local config_file="${SCRIPT_DIR}/config.mk"
    
    # Backup original
    cp "$config_file" "$CONFIG_BACKUP"
    
    # Patch PROJECT name
    perl -pi -e "s/^PROJECT := .*/PROJECT := ${project_name}/" "$config_file"
    
    # Add rom_data.cc to CXXSRC if not already present
    if ! grep -q "rom_data.cc" "$config_file"; then
        perl -pi -e 's/^CXXSRC = unit.cc$/CXXSRC = unit.cc rom_data.cc/' "$config_file"
    fi
}

# Function: restore_files
# Restore original header.c and config.mk
restore_files() {
    if [ -f "$HEADER_BACKUP" ]; then
        mv "$HEADER_BACKUP" "${SCRIPT_DIR}/header.c"
    fi
    if [ -f "$CONFIG_BACKUP" ]; then
        mv "$CONFIG_BACKUP" "${SCRIPT_DIR}/config.mk"
    fi
}

# Function: build_rom_variant
# Build a single ROM variant
build_rom_variant() {
    local rom_tag="$1"
    local unit_id_offset="$2"
    local display_name="$3"
    local expansion_file="$4"
    
    local unit_id=$(printf "0x%08XU" $((BASE_UNIT_ID + unit_id_offset)))
    local combined_rom="${SCRIPT_DIR}/.rom_${rom_tag}.bin"
    local project_name="${BASE_PROJECT_NAME}_${rom_tag}"
    local output_name="${project_name}.drmlgunit"
    
    log_info "Building ROM variant: ${rom_tag}"
    log_info "  Display name: ${display_name}"
    log_info "  Unit ID:      ${unit_id}"
    log_info "  Expansion:    ${expansion_file}"
    
    # Create combined ROM binary
    log_info "  Creating combined ROM binary..."
    if ! create_rom_binary "$expansion_file" "$combined_rom"; then
        log_error "Failed to create ROM binary for ${rom_tag}"
        return 1
    fi
    
    local rom_size=$(stat -f%z "$combined_rom" 2>/dev/null || stat -c%s "$combined_rom" 2>/dev/null)
    log_info "  ROM size: $(numfmt --to=iec-i --suffix=B $rom_size 2>/dev/null || echo $rom_size bytes)"
    
    # Generate rom_data.cc
    log_info "  Generating rom_data.cc..."
    xxd -i "$combined_rom" > "${SCRIPT_DIR}/rom_data.cc"
    perl -pi -e 's/unsigned char [A-Za-z0-9_]+\[\]/extern const unsigned char g_drumpler_rom[]/' \
        "${SCRIPT_DIR}/rom_data.cc"
    perl -pi -e 's/unsigned int [A-Za-z0-9_]+_len/extern const unsigned int g_drumpler_rom_size/' \
        "${SCRIPT_DIR}/rom_data.cc"
    
    # Patch header.c and config.mk
    log_info "  Patching header.c and config.mk..."
    patch_header "$unit_id" "$display_name"
    patch_config "$project_name"
    
    # Build via SDK (invoke build.sh from repo root)
    # Set _DRUMPLER_INTERNAL_BUILD to prevent recursion
    log_info "  Building via SDK..."
    cd "${REPO_ROOT}"
    export _DRUMPLER_INTERNAL_BUILD=1
    # Pass through build flags (PERF_MON, __QEMU_ARM__, etc.)
    if ! ./build.sh drumpler build ${BUILD_FLAGS} 2>&1 | tail -40; then
        log_error "SDK build failed for ${rom_tag}"
        restore_files
        rm -f "$combined_rom" "${SCRIPT_DIR}/rom_data.cc"
        return 1
    fi
    unset _DRUMPLER_INTERNAL_BUILD
    
    # Check if output exists
    if [ -f "${SCRIPT_DIR}/${output_name}" ]; then
        local output_size=$(stat -f%z "${SCRIPT_DIR}/${output_name}" 2>/dev/null || stat -c%s "${SCRIPT_DIR}/${output_name}" 2>/dev/null)
        log_success "Built: ${output_name} ($(numfmt --to=iec-i --suffix=B $output_size 2>/dev/null || echo $output_size bytes))"
    else
        log_error "Build artifact not found: ${output_name}"
        restore_files
        rm -f "$combined_rom" "${SCRIPT_DIR}/rom_data.cc"
        return 1
    fi
    
    # Cleanup
    restore_files
    rm -f "$combined_rom" "${SCRIPT_DIR}/rom_data.cc"
    
    return 0
}

# Main build loop
main() {
    log_info "=== Building all ROM variants from ${MANIFEST} ==="
    
    # Verify manifest exists
    if [ ! -f "$MANIFEST" ]; then
        log_error "Manifest not found: ${MANIFEST}"
        log_warn "Run: make -f Makefile.roms manifest"
        exit 1
    fi
    
    # Verify base ROM files exist
    local required_files=(
        "jv880_rom1.bin"
        "jv880_rom2.bin"
        "jv880_waverom1.bin"
        "jv880_waverom2.bin"
        "jv880_nvram.bin"
    )
    
    local missing_roms=0
    for file in "${required_files[@]}"; do
        if [ ! -f "${RESOURCES_DIR}/${file}" ]; then
            log_error "Required base ROM file not found: ${file}"
            missing_roms=1
        fi
    done
    
    if [ $missing_roms -eq 1 ]; then
        log_warn "ROM files not found. Download them with:"
        log_warn "  cd ${SCRIPT_DIR} && make -f Makefile.roms roms"
        exit 1
    fi
    
    # Trap to ensure files are restored on error
    trap restore_files EXIT ERR INT TERM
    
    local built_count=0
    local failed_count=0
    
    # Read manifest and build each ROM variant
    while IFS='|' read -r rom_tag unit_id_offset display_name expansion_file; do
        # Skip comments and empty lines
        [[ "$rom_tag" =~ ^#.*$ ]] && continue
        [[ -z "$rom_tag" ]] && continue
        
        echo ""  # Blank line between builds
        
        if build_rom_variant "$rom_tag" "$unit_id_offset" "$display_name" "$expansion_file"; then
            built_count=$((built_count + 1))
        else
            log_error "Failed building ${rom_tag}"
            failed_count=$((failed_count + 1))
            # Continue with next ROM instead of exiting
        fi
        
    done < "$MANIFEST"
    
    echo ""
    log_info "=== Build Summary ==="
    log_success "Successfully built: ${built_count} variants"
    if [ $failed_count -gt 0 ]; then
        log_error "Failed: ${failed_count} variants"
    fi
    
    log_info "Output files in: ${SCRIPT_DIR}"
    ls -lh "${SCRIPT_DIR}"/*.drmlgunit 2>/dev/null || log_warn "No .drmlgunit files found"
    
    if [ $failed_count -gt 0 ]; then
        exit 1
    fi
}

# Run main
main "$@"
