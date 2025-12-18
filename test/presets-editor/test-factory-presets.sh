#!/bin/bash
# Test script to verify factory preset support

set -e

cd "$(dirname "$0")"

echo "=== Factory Preset Support Test ==="
echo

# Check if GUI binary exists
if [ ! -f bin/presets-editor-gui ]; then
    echo "Error: GUI binary not found. Run 'make gui' first."
    exit 1
fi

# Check if pepege-synth unit exists
if [ ! -f units/pepege_synth.dylib ]; then
    echo "Error: pepege_synth.dylib not found. Run '../build-native.sh pepege-synth' first."
    exit 1
fi

echo "✓ GUI binary found"
echo "✓ pepege_synth.dylib found"
echo

# Show unit info
echo "Unit info:"
otool -L units/pepege_synth.dylib | head -5
echo

# Check for factory preset symbols
echo "Checking for factory preset symbols:"
nm units/pepege_synth.dylib | grep -E "(unit_get_preset_name|unit_load_preset)" || echo "  (symbols not exported, but may be present)"
echo

# Launch GUI
echo "Launching GUI..."
echo "Expected behavior:"
echo "  1. Unit loads successfully"
echo "  2. Preset browser shows 'Factory Presets' section"
echo "  3. Six factory presets are listed (Default, Bass, Lead, Pad, Pluck, FX)"
echo "  4. Clicking 'Load' on any factory preset updates parameters"
echo
echo "Press Ctrl+C to exit when done testing."
echo

./bin/presets-editor-gui --unit units/pepege_synth.dylib
