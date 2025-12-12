#!/bin/bash
# macOS-friendly player - uses test-unit.sh directly which handles container execution

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
QEMU_DIR="$SCRIPT_DIR/../qemu-arm"
UNIT_PATH="$1"

if [ -z "$UNIT_PATH" ]; then
    echo "Usage: $0 <unit.drmlgunit>"
    echo "Example: $0 ../../drumlogue/pepege-synth/pepege_synth.drmlgunit"
    exit 1
fi

# Extract unit name from path (e.g., pepege-synth from path or pepege_synth from filename)
UNIT_DIR=$(dirname "$UNIT_PATH")
UNIT_NAME=$(basename "$UNIT_DIR")

echo "🎵 Drumlogue Unit Player (macOS)"
echo "Unit: $UNIT_NAME"
echo ""
echo "1) Quick test (with audio)"
echo "2) CPU profiling (10-second test)"  
echo "3) Unit info"
echo "q) Quit"
echo ""
echo -n "Choice: "
read choice

case "$choice" in
    1)
        echo "Testing unit..."
        cd "$QEMU_DIR"
        ./test-unit.sh "$UNIT_NAME"
        if command -v play &> /dev/null; then
            echo "Playing audio..."
            play "build/output_$UNIT_NAME.wav"
        else
            echo "Install sox to play audio: brew install sox"
            echo "Output saved to: $QEMU_DIR/build/output_$UNIT_NAME.wav"
        fi
        ;;
    2)
        echo "Running profiling test..."
        cd "$QEMU_DIR"
        ./test-unit.sh "$UNIT_NAME" --profile
        ;;
    3)
        echo "Unit information:"
        cd "$QEMU_DIR"
        ./test-unit.sh "$UNIT_NAME" --verbose 2>&1 | grep -A 25 "Unit Information"
        ;;
    *)
        echo "Goodbye!"
        ;;
esac
