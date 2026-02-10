#!/usr/bin/env bash
#
# Build all drumpler ROM variants as native shared libraries for presets editor
# Each variant gets embedded ROM data and unique naming
#
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
EDITOR_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
REPO_ROOT="$(cd "${EDITOR_DIR}/../.." && pwd)"
DRUMPLER_DIR="${REPO_ROOT}/drumlogue/drumpler"
RESOURCES_DIR="${DRUMPLER_DIR}/resources"
MANIFEST="${DRUMPLER_DIR}/roms.manifest"
BUILD_NATIVE="${SCRIPT_DIR}/build-native.sh"
MAKEFILE_NATIVE="${SCRIPT_DIR}/Makefile.native"

# Backups
HEADER_BACKUP="${DRUMPLER_DIR}/header.c.bak.native"
CONFIG_BACKUP="${DRUMPLER_DIR}/config.mk.bak.native"

BASE_UNIT_ID=0x00000005

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

log_info() { echo -e "${BLUE}>>${NC} $*"; }
log_success() { echo -e "${GREEN}>>✓${NC} $*"; }
log_error() { echo -e "${RED}>>✗${NC} $*" >&2; }
log_warn() { echo -e "${YELLOW}>>!${NC} $*"; }

# Cleanup function
cleanup() {
    if [ -f "$HEADER_BACKUP" ]; then
        mv "$HEADER_BACKUP" "${DRUMPLER_DIR}/header.c"
    fi
    if [ -f "$CONFIG_BACKUP" ]; then
        mv "$CONFIG_BACKUP" "${DRUMPLER_DIR}/config.mk"
    fi
    rm -f "${DRUMPLER_DIR}/rom_data.cc"
    rm -f "${DRUMPLER_DIR}/.rom_"*.bin
}

trap cleanup EXIT ERR INT TERM

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

# Create combined ROM binary with pre-unscrambled waveroms
create_rom_binary() {
    local expansion_file="$1"
    local output_file="$2"
    
    # Ensure unscramble tool is built
    build_unscramble_tool
    
    # Unscramble base waveroms (Roland JV-880 address+data bit scrambling)
    unscramble_waverom "${RESOURCES_DIR}/jv880_waverom1.bin" "${RESOURCES_DIR}/.jv880_waverom1_unscrambled.bin"
    unscramble_waverom "${RESOURCES_DIR}/jv880_waverom2.bin" "${RESOURCES_DIR}/.jv880_waverom2_unscrambled.bin"
    
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

# Patch header.c
patch_header() {
    local unit_id="$1"
    local display_name="$2"
    
    cp "${DRUMPLER_DIR}/header.c" "$HEADER_BACKUP"
    
    perl -pi -e "s/\.unit_id = 0x[0-9A-Fa-f]+U,/.unit_id = ${unit_id},/" "${DRUMPLER_DIR}/header.c"
    perl -pi -e "s/\.name = \"[^\"]*\",/.name = \"${display_name}\",/" "${DRUMPLER_DIR}/header.c"
}

# Patch config.mk
patch_config() {
    local project_name="$1"
    
    cp "${DRUMPLER_DIR}/config.mk" "$CONFIG_BACKUP"
    
    perl -pi -e "s/^PROJECT := .*/PROJECT := ${project_name}/" "${DRUMPLER_DIR}/config.mk"
    
    if ! grep -q "rom_data.cc" "${DRUMPLER_DIR}/config.mk"; then
        perl -pi -e 's/^CXXSRC = unit.cc$/CXXSRC = unit.cc rom_data.cc/' "${DRUMPLER_DIR}/config.mk"
    fi
    
    # Add DRUMPLER_ROM_EMBEDDED define
    if ! grep -q "DRUMPLER_ROM_EMBEDDED" "${DRUMPLER_DIR}/config.mk"; then
        echo "" >> "${DRUMPLER_DIR}/config.mk"
        echo "# Enable embedded ROM data" >> "${DRUMPLER_DIR}/config.mk"
        echo "UDEFS += -DDRUMPLER_ROM_EMBEDDED" >> "${DRUMPLER_DIR}/config.mk"
    fi
    
    # Add DEBUG define for presets editor
    if ! grep -q "UDEFS += -DDEBUG" "${DRUMPLER_DIR}/config.mk"; then
        echo "UDEFS += -DDEBUG" >> "${DRUMPLER_DIR}/config.mk"
    fi
}

# Build single ROM variant
build_rom_variant() {
    local rom_tag="$1"
    local unit_id_offset="$2"
    local display_name="$3"
    local expansion_file="$4"
    
    local unit_id=$(printf "0x%08XU" $((BASE_UNIT_ID + unit_id_offset)))
    local combined_rom="${DRUMPLER_DIR}/.rom_${rom_tag}.bin"
    local project_name="drumpler_${rom_tag}"
    
    log_info "Building native library: ${rom_tag}"
    log_info "  Display name: ${display_name}"
    log_info "  Unit ID:      ${unit_id}"
    log_info "  Expansion:    ${expansion_file}"
    
    # Create ROM binary
    log_info "  Creating ROM binary..."
    if ! create_rom_binary "$expansion_file" "$combined_rom"; then
        log_error "Failed to create ROM binary"
        return 1
    fi
    
    local rom_size=$(stat -f%z "$combined_rom" 2>/dev/null || stat -c%s "$combined_rom" 2>/dev/null)
    log_info "  ROM size: $(numfmt --to=iec-i --suffix=B $rom_size 2>/dev/null || echo $rom_size bytes)"
    
    # Generate rom_data.cc
    log_info "  Generating rom_data.cc..."
    xxd -i "$combined_rom" > "${DRUMPLER_DIR}/rom_data.cc"
    perl -pi -e 's/unsigned char [A-Za-z0-9_]+\[\]/extern const unsigned char g_drumpler_rom[]/' "${DRUMPLER_DIR}/rom_data.cc"
    perl -pi -e 's/unsigned int [A-Za-z0-9_]+_len/extern const unsigned int g_drumpler_rom_size/' "${DRUMPLER_DIR}/rom_data.cc"
    
    # Patch files
    log_info "  Patching header.c and config.mk..."
    patch_header "$unit_id" "$display_name"
    patch_config "$project_name"
    
    # Build native library
    log_info "  Building native library..."
    if ! make -C "$EDITOR_DIR" -f "$MAKEFILE_NATIVE" UNIT=drumpler --no-print-directory 2>&1 | tail -5; then
        log_error "Native build failed"
        cleanup
        return 1
    fi
    
    # Verify output
    local lib_ext="$(uname -s | grep -qi darwin && echo dylib || echo so)"
    local output_lib="${EDITOR_DIR}/units/${project_name}.${lib_ext}"
    
    if [ -f "$output_lib" ]; then
        local lib_size=$(stat -f%z "$output_lib" 2>/dev/null || stat -c%s "$output_lib" 2>/dev/null)
        log_success "Built: ${project_name}.${lib_ext} ($(numfmt --to=iec-i --suffix=B $lib_size 2>/dev/null || echo $lib_size bytes))"
    else
        log_error "Output not found: $output_lib"
        cleanup
        return 1
    fi
    
    # Cleanup for next iteration
    mv "$HEADER_BACKUP" "${DRUMPLER_DIR}/header.c" 2>/dev/null || true
    mv "$CONFIG_BACKUP" "${DRUMPLER_DIR}/config.mk" 2>/dev/null || true
    rm -f "${DRUMPLER_DIR}/rom_data.cc"
    rm -f "$combined_rom"
    
    return 0
}

# Main
main() {
    log_info "=== Building drumpler ROM variants for native host ==="
    
    # Verify manifest
    if [ ! -f "$MANIFEST" ]; then
        log_error "Manifest not found: ${MANIFEST}"
        log_warn "Run: cd ${DRUMPLER_DIR} && make -f Makefile.roms all"
        exit 1
    fi
    
    # Verify base ROMs
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
            log_error "Base ROM not found: ${file}"
            missing_roms=1
        fi
    done
    
    if [ $missing_roms -eq 1 ]; then
        log_warn "Download ROMs: cd ${DRUMPLER_DIR} && make -f Makefile.roms roms"
        exit 1
    fi
    
    local built_count=0
    local failed_count=0
    
    # Build each ROM variant
    while IFS='|' read -r rom_tag unit_id_offset display_name expansion_file; do
        [[ "$rom_tag" =~ ^#.*$ ]] && continue
        [[ -z "$rom_tag" ]] && continue
        
        echo ""
        
        if build_rom_variant "$rom_tag" "$unit_id_offset" "$display_name" "$expansion_file"; then
            built_count=$((built_count + 1))
        else
            log_error "Failed: ${rom_tag}"
            failed_count=$((failed_count + 1))
        fi
        
    done < "$MANIFEST"
    
    echo ""
    log_info "=== Build Summary ==="
    log_success "Successfully built: ${built_count} native libraries"
    [ $failed_count -gt 0 ] && log_error "Failed: ${failed_count} libraries"
    
    log_info "Output: ${EDITOR_DIR}/units/"
    ls -lh "${EDITOR_DIR}/units/drumpler_"*.{dylib,so} 2>/dev/null || log_warn "No drumpler libraries found"
    
    [ $failed_count -gt 0 ] && exit 1
}

main "$@"
