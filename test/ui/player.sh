#!/bin/bash
# Simple interactive player using existing QEMU test infrastructure

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
QEMU_DIR="$SCRIPT_DIR/../qemu-arm"

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

# Check arguments
if [ $# -lt 1 ]; then
    echo -e "${RED}Usage: $0 <unit.drmlgunit>${NC}"
    echo ""
    echo "Example:"
    echo "  $0 ../../drumlogue/pepege-synth/pepege_synth.drmlgunit"
    exit 1
fi

UNIT_PATH="$1"
UNIT_NAME=$(basename "$UNIT_PATH" .drmlgunit)

# Check if unit exists
if [ ! -f "$UNIT_PATH" ]; then
    echo -e "${RED}Error: Unit file not found: $UNIT_PATH${NC}"
    exit 1
fi

# Check for qemu-arm (macOS) or qemu-arm-static (Linux)
QEMU_ARM=""
if command -v qemu-arm &> /dev/null; then
    QEMU_ARM="qemu-arm"
elif command -v qemu-arm-static &> /dev/null; then
    QEMU_ARM="qemu-arm-static"
else
    echo -e "${RED}Error: QEMU ARM emulator not found${NC}"
    echo "Install with:"
    echo "  macOS: brew install qemu"
    echo "  Linux: sudo apt-get install qemu-user-static"
    exit 1
fi

echo -e "${GREEN}✓ Using: $QEMU_ARM${NC}"

# Check for audio player
AUDIO_CMD=""
if command -v play &> /dev/null; then
    AUDIO_CMD="play -q -t wav -"
elif command -v sox &> /dev/null; then
    AUDIO_CMD="sox -q -t wav - -d"
elif command -v aplay &> /dev/null; then
    AUDIO_CMD="aplay -q -f FLOAT_LE -r 48000 -c 2"
else
    echo -e "${YELLOW}Warning: No audio player found (sox or aplay)${NC}"
    echo "Install with: brew install sox"
    echo "Audio will be saved to files instead"
fi

echo -e "${CYAN}╔════════════════════════════════════════════════════════════════╗${NC}"
echo -e "${CYAN}║         DRUMLOGUE UNIT PLAYER - ARM Emulation Mode           ║${NC}"
echo -e "${CYAN}╚════════════════════════════════════════════════════════════════╝${NC}"
echo ""
echo -e "Unit: ${GREEN}$UNIT_NAME${NC}"
echo ""

# Create temporary directory for output
TMP_DIR=$(mktemp -d)
trap "rm -rf $TMP_DIR" EXIT

# Function to play a test with audio
play_test() {
    local input_signal="$1"
    local description="$2"
    local extra_args="${3:-}"
    
    echo -e "${BLUE}▶${NC} Playing: $description"
    
    OUTPUT_WAV="$TMP_DIR/output.wav"
    
    # Run test
    cd "$QEMU_DIR"
    ./test-unit.sh "$UNIT_NAME" --input "$input_signal" $extra_args > /dev/null 2>&1
    
    # Copy output
    cp "build/output_$UNIT_NAME.wav" "$OUTPUT_WAV" 2>/dev/null || {
        echo -e "${RED}Error: Failed to generate audio${NC}"
        return 1
    }
    
    # Play audio if player available
    if [ -n "$AUDIO_CMD" ]; then
        echo -e "${GREEN}Playing audio...${NC}"
        $AUDIO_CMD < "$OUTPUT_WAV" 2>/dev/null || true
    else
        echo -e "${YELLOW}Audio saved to: $OUTPUT_WAV${NC}"
        echo "Play with: play $OUTPUT_WAV"
    fi
}

# Main menu loop
while true; do
    echo ""
    echo -e "${CYAN}═══════════════════════════════════════════════════════════════${NC}"
    echo -e "${GREEN}Choose an action:${NC}"
    echo ""
    echo "  1) Play with sine wave input"
    echo "  2) Play with noise input"
    echo "  3) Play with sweep input"
    echo "  4) Play with impulse input"
    echo "  5) Run profiling test"
    echo "  6) Show unit info"
    echo "  q) Quit"
    echo ""
    echo -n "Selection: "
    read choice
    
    case "$choice" in
        1)
            play_test "fixtures/sine_440hz.wav" "440Hz Sine Wave"
            ;;
        2)
            play_test "fixtures/noise.wav" "White Noise"
            ;;
        3)
            play_test "fixtures/sweep.wav" "Frequency Sweep"
            ;;
        4)
            play_test "fixtures/impulse.wav" "Impulse Response"
            ;;
        5)
            echo -e "${BLUE}▶${NC} Running profiling test..."
            cd "$QEMU_DIR"
            ./test-unit.sh "$UNIT_NAME" --profile
            ;;
        6)
            echo -e "${BLUE}▶${NC} Unit Information:"
            cd "$QEMU_DIR"
            ./test-unit.sh "$UNIT_NAME" --verbose 2>&1 | grep -A 25 "Unit Information" | head -30
            ;;
        q|Q)
            echo -e "${GREEN}✓ Goodbye!${NC}"
            exit 0
            ;;
        *)
            echo -e "${RED}Invalid selection${NC}"
            ;;
    esac
done
