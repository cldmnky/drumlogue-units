#!/usr/bin/env bash
# Build a drumlogue unit as a native shared library for the presets editor
# Usage: ./build-native.sh <unit-name> [DEBUG=1]

set -euo pipefail

if [[ $# -lt 1 ]]; then
  echo "Usage: $0 <unit-name> [DEBUG=1]" >&2
  exit 1
fi

UNIT_NAME="$1"
DEBUG="${2:-0}"
SCRIPT_DIR="$(cd -- "$(dirname "$0")" && pwd)"
ROOT_DIR="$(cd -- "$SCRIPT_DIR/../../.." && pwd)"
EDITOR_DIR="$ROOT_DIR/test/presets-editor"
MAKEFILE="$SCRIPT_DIR/Makefile.native"
UNIT_DIR="$ROOT_DIR/drumlogue/$UNIT_NAME"
PROJECT_NAME=""

if [[ ! -d "$UNIT_DIR" ]]; then
  echo "Error: unit directory not found: $UNIT_DIR" >&2
  exit 1
fi

echo "Building unit '$UNIT_NAME' for native host..."
make -f "$MAKEFILE" UNIT="$UNIT_NAME" DEBUG="$DEBUG" --no-print-directory

LIB_EXT="$(uname -s | grep -qi darwin && echo dylib || echo so)"

# Prefer PROJECT from config.mk if available, else fall back to UNIT_NAME
if [[ -f "$UNIT_DIR/config.mk" ]]; then
  PROJECT_NAME=$(grep -E '^PROJECT *:=' "$UNIT_DIR/config.mk" | head -n1 | awk '{print $3}') || PROJECT_NAME=""
fi
LIB_BASENAME="${PROJECT_NAME:-$UNIT_NAME}"

OUTPUT_LIB="$EDITOR_DIR/units/${LIB_BASENAME}.${LIB_EXT}"
ALT_OUTPUT_LIB="$EDITOR_DIR/units/${UNIT_NAME//-/_}.${LIB_EXT}"

if [[ -f "$OUTPUT_LIB" ]]; then
  echo "✓ Output: $OUTPUT_LIB"
elif [[ -f "$ALT_OUTPUT_LIB" ]]; then
  echo "✓ Output: $ALT_OUTPUT_LIB"
else
  echo "Build completed, but output library not found. Checked: $OUTPUT_LIB and $ALT_OUTPUT_LIB" >&2
fi
