#!/bin/bash
set -e

# Simple test script for drumlogue units in QEMU ARM emulation
# Usage: ./test-unit.sh <unit-name> [--profile] [--perf-mon]
# Example: ./test-unit.sh clouds-revfx --profile --perf-mon

UNIT_NAME="$1"
PROFILE_FLAG=""
PERF_MON_FLAG=""
BUILD_FLAGS=""

# Parse flags
shift
for arg in "$@"; do
    case "$arg" in
        --profile)
            PROFILE_FLAG="--profile"
            ;;
        --perf-mon)
            PERF_MON_FLAG="--perf-mon"
            BUILD_FLAGS="PERF_MON=1 __QEMU_ARM__=1"
            ;;
    esac
done

if [ -z "$UNIT_NAME" ]; then
    echo "Usage: $0 <unit-name> [--profile] [--perf-mon]"
    echo ""
    echo "Options:"
    echo "  --profile    Enable CPU profiling (time measurements)"
    echo "  --perf-mon   Enable PERF_MON (DSP cycle counting)"
    echo ""
    echo "Available units:"
    ls -1 ../../drumlogue/ | grep -v common
    exit 1
fi

# Convert hyphens to underscores for .drmlgunit filename
UNIT_FILE_NAME="${UNIT_NAME//-/_}"

# Special case for drumpler (multi-ROM project) - default to internal ROM
if [ "$UNIT_NAME" = "drumpler" ]; then
    UNIT_FILE_NAME="drumpler_internal"
fi

UNIT_FILE="../../drumlogue/${UNIT_NAME}/${UNIT_FILE_NAME}.drmlgunit"
INPUT_WAV="fixtures/sine_440hz.wav"
OUTPUT_WAV="build/output_${UNIT_NAME}.wav"

# Build or rebuild unit if PERF_MON is requested
if [ -n "$BUILD_FLAGS" ]; then
    echo "⚙️  Rebuilding unit with PERF_MON enabled..."
    (cd ../.. && ./build.sh ${UNIT_NAME} build ${BUILD_FLAGS})
    if [ $? -ne 0 ]; then
        echo "❌ Failed to build unit with PERF_MON"
        exit 1
    fi
elif [ ! -f "$UNIT_FILE" ]; then
    echo "❌ Unit file not found: $UNIT_FILE"
    echo ""
    echo "Build it first:"
    echo "  cd ../.. && ./build.sh ${UNIT_NAME}"
    exit 1
fi

# Check if ARM host exists
if [ ! -f "unit_host_arm" ]; then
    echo "⚙️  Building ARM unit host..."
    # Use appropriate Makefile based on OS
    if [[ "$(uname -s)" == "Darwin" ]]; then
        # macOS: Use Podman-based build
        make -f Makefile.podman podman-build
    else
        # Linux: Use native ARM cross-compilation
        make -f Makefile.arm
    fi
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

# Run test - different approach for macOS (podman) vs Linux (native)
if [[ "$(uname -s)" == "Darwin" ]]; then
    # macOS: Use Podman container with qemu-user-static
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
            --verbose $PROFILE_FLAG $PERF_MON_FLAG
    "
else
    # Linux: Use native qemu-arm directly
    qemu-arm -cpu cortex-a7 -L /usr/arm-linux-gnueabihf \
        ./unit_host_arm \
        ../../drumlogue/${UNIT_NAME}/${UNIT_FILE_NAME}.drmlgunit \
        ./${INPUT_WAV} \
        ./${OUTPUT_WAV} \
        --verbose $PROFILE_FLAG $PERF_MON_FLAG
fi

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
