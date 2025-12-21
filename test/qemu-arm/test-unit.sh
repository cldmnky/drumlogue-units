#!/bin/bash
set -e

# Simple test script for drumlogue units in QEMU ARM emulation
# Usage: ./test-unit.sh <unit-name> [options]
# Example: ./test-unit.sh clouds-revfx --profile --test-presets

UNIT_NAME="$1"
PROFILE_FLAG=""
TEST_PRESETS_FLAG=""

# Parse options
shift
while [ $# -gt 0 ]; do
    case "$1" in
        --profile)
            PROFILE_FLAG="--profile"
            shift
            ;;
        --test-presets)
            TEST_PRESETS_FLAG="--test-presets"
            shift
            ;;
        *)
            echo "Unknown option: $1"
            exit 1
            ;;
    esac
done

if [ -z "$UNIT_NAME" ]; then
    echo "Usage: $0 <unit-name> [options]"
    echo ""
    echo "Options:"
    echo "  --profile        Enable CPU profiling"
    echo "  --test-presets   Test preset loading and switching (requires preset support)"
    echo ""
    echo "Available units:"
    ls -1 ../../drumlogue/ | grep -v common
    exit 1
fi

# Convert hyphens to underscores for .drmlgunit filename
UNIT_FILE_NAME="${UNIT_NAME//-/_}"
UNIT_FILE="../../drumlogue/${UNIT_NAME}/${UNIT_FILE_NAME}.drmlgunit"
INPUT_WAV="fixtures/sine_440hz.wav"
OUTPUT_WAV="build/output_${UNIT_NAME}.wav"

# Check if unit file exists
if [ ! -f "$UNIT_FILE" ]; then
    echo "❌ Unit file not found: $UNIT_FILE"
    echo ""
    echo "Build it first:"
    echo "  cd ../.. && ./build.sh ${UNIT_NAME}"
    exit 1
fi

# Check if ARM host exists
if [ ! -f "unit_host_arm" ]; then
    echo "⚙️  Building ARM unit host..."
    make -f Makefile.podman podman-build
fi

# Check if test signals exist (skip if only testing presets)
if [ -z "$TEST_PRESETS_FLAG" ] && [ ! -f "$INPUT_WAV" ]; then
    echo "⚙️  Generating test signals..."
    python3 generate_signals.py
fi

# Create output directory
mkdir -p build

if [ -n "$TEST_PRESETS_FLAG" ]; then
    echo "🧪 Testing $UNIT_NAME presets in QEMU ARM emulation..."
else
    echo "🧪 Testing $UNIT_NAME in QEMU ARM emulation..."
fi
echo ""

# Run test
podman run --rm -it \
    -v "$(pwd):/workspace:Z" \
    -v "$(pwd)/../..:/repo:ro,Z" \
    -w /workspace \
    ubuntu:22.04 \
    bash -c "
    apt-get update -qq && \
    dpkg --add-architecture armhf && \
    apt-get update -qq && \
    apt-get install -y -qq qemu-user-static libsndfile1:armhf libstdc++6:armhf > /dev/null 2>&1 && \
    qemu-arm-static -cpu cortex-a7 -L /usr/arm-linux-gnueabihf \
        /workspace/unit_host_arm \
        /repo/drumlogue/${UNIT_NAME}/${UNIT_FILE_NAME}.drmlgunit \
        /workspace/${INPUT_WAV} \
        /workspace/${OUTPUT_WAV} \
        --verbose $PROFILE_FLAG $TEST_PRESETS_FLAG
"

if [ $? -eq 0 ]; then
    echo ""
    echo "✅ Test passed!"
    if [ -z "$TEST_PRESETS_FLAG" ]; then
        echo "📁 Output: $OUTPUT_WAV"
        ls -lh "$OUTPUT_WAV"
    fi
else
    echo ""
    echo "❌ Test failed"
    exit 1
fi
