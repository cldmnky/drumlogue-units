#!/usr/bin/env bash
# Build script for drumlogue units using podman
# Usage: ./build.sh [project-name] [action]
#   e.g.: ./build.sh clouds-revfx        # Build the unit
#         ./build.sh clouds-revfx clean  # Clean the build
#         ./build.sh --build-image       # Build the SDK container image

set -e

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" &> /dev/null && pwd)"
SDK_PLATFORM="${SCRIPT_DIR}/logue-sdk/platform"
SDK_DOCKER="${SCRIPT_DIR}/logue-sdk/docker/docker-app"
ENGINE="${ENGINE:-podman}"
IMAGE="${IMAGE:-localhost/logue-sdk-dev-env:latest}"

# Build SDK container image
build_image() {
    echo ">> Building SDK container image..."
    cd "${SDK_DOCKER}"
    BUILD_ID=$(git rev-parse --short HEAD)
    VERSION=$(cat VERSION)
    $ENGINE build \
        --build-arg build="$BUILD_ID" \
        --build-arg version="$VERSION" \
        -t "logue-sdk-dev-env:${VERSION}" \
        -t "logue-sdk-dev-env:latest" \
        .
    echo ">> SDK image built: logue-sdk-dev-env:${VERSION}"
}

# Handle --build-image flag
if [ "$1" = "--build-image" ]; then
    build_image
    exit 0
fi

PROJECT="${1:-clouds-revfx}"
ACTION="${2:-build}"

case "$ACTION" in
    clean)
        CMD="/app/commands/build --clean drumlogue/${PROJECT}"
        ;;
    *)
        CMD="/app/commands/build drumlogue/${PROJECT}"
        ;;
esac

echo ">> Building ${PROJECT}..."

# Get actual project name from config.mk (the SDK uses this to name the artifact)
CONFIG_MK="${SCRIPT_DIR}/drumlogue/${PROJECT}/config.mk"
if [ -f "${CONFIG_MK}" ]; then
    SDK_PROJECT_NAME=$(grep "^PROJECT *:=" "${CONFIG_MK}" | sed 's/^PROJECT *:= *//' | tr -d ' \t')
    ARTIFACT_NAME="${SDK_PROJECT_NAME}.drmlgunit"
else
    # Fallback: convert hyphens to underscores
    ARTIFACT_NAME="${PROJECT//-/_}.drmlgunit"
fi
OUTPUT_DIR="${SCRIPT_DIR}/drumlogue/${PROJECT}"

# Mount the project directory directly into the workspace to avoid symlink permission issues
# Also mount eurorack directory for Mutable Instruments DSP code
# Pre-create build directories to avoid permission issues in container
BUILD_DIR="${SCRIPT_DIR}/drumlogue/${PROJECT}/build"
mkdir -p "${BUILD_DIR}/obj"
mkdir -p "${BUILD_DIR}/lst"
mkdir -p "${BUILD_DIR}/.dep"
# Make build dirs world-writable for container user compatibility
chmod -R 777 "${BUILD_DIR}"

BUILD_EXIT_CODE=0
# Create a build script that:
# 1. Copies SDK platform drumlogue to a writable /workspace
# 2. Copies project files from mounted read-only source
# 3. Builds the project
# 4. Copies artifact to a separately mounted output directory
OUTPUT_MOUNT="${SCRIPT_DIR}/drumlogue/${PROJECT}"
mkdir -p "${OUTPUT_MOUNT}"
chmod 777 "${OUTPUT_MOUNT}"

$ENGINE run --rm --entrypoint "" \
    -e HOME=/tmp \
    -v "${SDK_PLATFORM}:/sdk-platform:ro" \
    -v "${SCRIPT_DIR}/drumlogue/${PROJECT}:/project-src:ro" \
    -v "${SCRIPT_DIR}/drumlogue/common:/project-common:ro" \
    -v "${OUTPUT_MOUNT}:/output" \
    -v "${SCRIPT_DIR}/eurorack:/repo/eurorack:ro" \
    "$IMAGE" /bin/bash -c "
        rm -rf /workspace/drumlogue && \
        cp -r /sdk-platform/drumlogue /workspace/ && \
        mkdir -p /workspace/drumlogue/common && \
        cp -r /project-common/* /workspace/drumlogue/common/ && \
        rm -rf /workspace/drumlogue/${PROJECT} && \
        mkdir -p /workspace/drumlogue/${PROJECT} && \
        cp -r /project-src/* /workspace/drumlogue/${PROJECT}/ && \
        rm -rf /workspace/drumlogue/${PROJECT}/build && \
        mkdir -p /workspace/drumlogue/${PROJECT}/build/obj && \
        mkdir -p /workspace/drumlogue/${PROJECT}/build/lst && \
        mkdir -p /workspace/drumlogue/${PROJECT}/build/.dep && \
        ${CMD} && \
        if [ "${ACTION}" != "clean" ]; then cp /workspace/drumlogue/${PROJECT}/${ARTIFACT_NAME} /output/; fi
    " || BUILD_EXIT_CODE=$?

if [ $BUILD_EXIT_CODE -ne 0 ]; then
    echo ">> Error: Build failed with exit code ${BUILD_EXIT_CODE}"
    exit $BUILD_EXIT_CODE
fi

# The SDK deploys the artifact directly to the project directory
if [ "$ACTION" != "clean" ]; then
    if [ -f "${OUTPUT_DIR}/${ARTIFACT_NAME}" ]; then
        echo ">> Build artifact: ${OUTPUT_DIR}/${ARTIFACT_NAME}"
        ls -la "${OUTPUT_DIR}/${ARTIFACT_NAME}"
    else
        echo ">> Error: Build artifact not found at ${OUTPUT_DIR}/${ARTIFACT_NAME}"
        exit 1
    fi
fi
