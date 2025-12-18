#!/bin/bash
set -e

# Simple test script for drumlogue units in QEMU ARM emulation
# Usage: ./test-unit.sh <unit-name> [--profile]
# Example: ./test-unit.sh clouds-revfx --profile

UNIT_NAME="$1"
PROFILE_FLAG=""

# Check for --profile flag
if [ "$2" == "--profile" ]; then
    PROFILE_FLAG="--profile"
fi

if [ -z "$UNIT_NAME" ]; then
    echo "Usage: $0 <unit-name> [--profile]"
    echo ""
    echo "Options:"
    echo "  --profile    Enable CPU profiling"
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

# Check if test signals exist
if [ ! -f "$INPUT_WAV" ]; then
    echo "⚙️  Generating test signals..."
    python3 generate_signals.py
fi

# Create output directory
mkdir -p build

echo "🧪 Testing $UNIT_NAME in QEMU ARM emulation..."
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
        --verbose $PROFILE_FLAG
"

if [ $? -eq 0 ]; then
    echo ""
    echo "✅ Test passed!"
    echo "📁 Output: $OUTPUT_WAV"
    ls -lh "$OUTPUT_WAV"
else
    echo ""
    echo "❌ Test failed"
    exit 1
fi
