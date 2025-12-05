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
$ENGINE run --rm --entrypoint "" \
    -v "${SDK_PLATFORM}:/workspace" \
    -v "${SCRIPT_DIR}:/repo" \
    "$IMAGE" /bin/bash -c "ln -sfn /repo/drumlogue/${PROJECT} /workspace/drumlogue/${PROJECT} && ${CMD} ; rm -f /workspace/drumlogue/${PROJECT}"

# Copy output to project dir if build succeeded
if [ "$ACTION" != "clean" ] && [ -f "${SDK_PLATFORM}/drumlogue/${PROJECT}/build/${PROJECT//-/_}.drmlgunit" ]; then
    echo ">> Output available in logue-sdk/platform/drumlogue/${PROJECT}/build/"
fi
