#!/usr/bin/env bash
set -euo pipefail

# This script builds and uploads the specified Arduino sketch.
# Run this from within the edge/heltec directory.
#
# Usage:
#   ./heltec.sh build <sketch_name>
#   ./heltec.sh upload <sketch_name>
#
# Example:
#   ./heltec.sh build relay
#   ./heltec.sh upload remote

if [ "$#" -lt 2 ]; then
  echo "Usage: $0 {build|upload} <sketch_name>"
  exit 1
fi

ACTION=$1
SKETCH_NAME=$2
FQBN="Heltec-esp32:esp32:heltec_wifi_lora_32_V2"
SKETCH_DIR="${SKETCH_NAME}"

if [ ! -d "$SKETCH_DIR" ]; then
  echo "Error: Sketch directory not found at '$SKETCH_DIR'"
  exit 1
fi

# Ensure the lib symlink exists
./arduino-include.sh apply

case "$ACTION" in
  build)
    echo "Building sketch '$SKETCH_NAME' for board '$FQBN'..."
    arduino-cli compile --fqbn "$FQBN" "$SKETCH_DIR"
    echo "Build complete."
    ;;
  upload)
    # Find the serial port
    PORT=$(arduino-cli board list | awk '/USB/{print $1; exit}')

    if [ -z "$PORT" ]; then
        echo "Error: Could not find a connected board. Please ensure it is connected and drivers are installed."
        exit 1
    fi

    echo "Found board on port: $PORT"
    echo "Building and uploading sketch '$SKETCH_NAME' for board '$FQBN'..."
    arduino-cli upload -p "$PORT" --fqbn "$FQBN" "$SKETCH_DIR"
    echo "Upload complete."
    ;;
  *)
    echo "Error: Invalid action '$ACTION'. Use 'build' or 'upload'." >&2
    exit 1
    ;;
esac
