#!/bin/bash
# Run UI player in QEMU ARM emulation with audio output

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$SCRIPT_DIR/../.."

# Check arguments
if [ $# -lt 1 ]; then
    echo "Usage: $0 <unit.drmlgunit>"
    echo ""
    echo "Example:"
    echo "  $0 ../../drumlogue/pepege-synth/pepege_synth.drmlgunit"
    exit 1
fi

UNIT_PATH="$1"

# Check if unit exists
if [ ! -f "$UNIT_PATH" ]; then
    echo "Error: Unit file not found: $UNIT_PATH"
    exit 1
fi

# Check if ui_player_arm exists
if [ ! -f "$SCRIPT_DIR/ui_player_arm" ]; then
    echo "Error: ui_player_arm not built. Run 'make -f Makefile.podman' first"
    exit 1
fi

# Check for required tools
if ! command -v qemu-arm-static &> /dev/null; then
    echo "Error: qemu-arm-static not found. Please install qemu-user-static"
    exit 1
fi

# Check for audio player
AUDIO_PLAYER=""
if command -v aplay &> /dev/null; then
    AUDIO_PLAYER="aplay -f FLOAT_LE -r 48000 -c 2"
elif command -v sox &> /dev/null; then
    AUDIO_PLAYER="play -t wav -"
else
    echo "Warning: No audio player found (aplay or sox). Audio will not be played."
    AUDIO_PLAYER="cat > /dev/null"
fi

echo "🎵 Starting Drumlogue UI Player"
echo "   Unit: $(basename $UNIT_PATH)"
echo "   Audio: $AUDIO_PLAYER"
echo ""
echo "Controls:"
echo "  TAB       - Switch between Pattern/Parameters mode"
echo "  ←/→       - Navigate steps/parameters"
echo "  ↑/↓       - Adjust values"
echo "  SPACE     - Play/Stop"
echo "  +/-       - Adjust tempo"
echo "  R         - Reset pattern"
echo "  Q         - Quit"
echo ""
echo "Press any key to start..."
read -n 1 -s

# Run in QEMU with audio piped to player
qemu-arm-static -L /usr/arm-linux-gnueabihf \
    -E LD_LIBRARY_PATH=/usr/arm-linux-gnueabihf/lib:/workspace/lib \
    "$SCRIPT_DIR/ui_player_arm" "$UNIT_PATH" 2>/dev/null | $AUDIO_PLAYER

echo ""
echo "✓ UI player stopped"
