#!/usr/bin/env bash
set -euo pipefail

# This script builds and uploads the specified Arduino sketch.
# Run this from within the edge/heltec directory.
#
# Usage:
#   ./heltec.sh build <sketch_name>
#   ./heltec.sh upload <sketch_name> [port]
#   ./heltec.sh build-upload <sketch_name> [port]
#
# Examples:
#   ./heltec.sh build relay
#   ./heltec.sh upload remote                    # Auto-detect port
#   ./heltec.sh upload remote /dev/ttyUSB0      # Manual port
#   ./heltec.sh build-upload relay              # Auto-detect port
#   ./heltec.sh build-upload relay /dev/ttyUSB0 # Manual port

if [ "$#" -lt 2 ] || [ "$#" -gt 3 ]; then
  echo "Usage: $0 {build|upload|build-upload} <sketch_name> [port]"
  exit 1
fi

ACTION=$1
SKETCH_NAME=$2
PORT=${3:-}  # Optional port parameter, empty if not provided
FQBN="Heltec-esp32:esp32:heltec_wifi_lora_32_V3"
SKETCH_DIR="${SKETCH_NAME}"

if [ ! -d "$SKETCH_DIR" ]; then
  echo "Error: Sketch directory not found at '$SKETCH_DIR'"
  exit 1
fi

# Ensure the lib symlink exists
./arduino-include.sh apply

# Function to get port - uses provided port or auto-detects
get_port() {
    if [ -n "$PORT" ]; then
        echo "$PORT"
        return 0
    fi

    # Auto-detect first available USB port
    local detected_port
    detected_port=$(arduino-cli board list | awk '/USB/{print $1; exit}')
    echo "$detected_port"
}

case "$ACTION" in
  build)
    echo "Building sketch '$SKETCH_NAME' for board '$FQBN'..."
    arduino-cli compile --fqbn "$FQBN" "$SKETCH_DIR"
    echo "Build complete."
    ;;
  upload)
    # Get the serial port (manual or auto-detected)
    PORT=$(get_port)

    if [ -z "$PORT" ]; then
        echo "Error: Could not find a connected board. Please ensure it is connected and drivers are installed."
        exit 1
    fi

    echo "Found board on port: $PORT"
    echo "Building and uploading sketch '$SKETCH_NAME' for board '$FQBN'..."
    arduino-cli upload -p "$PORT" --fqbn "$FQBN" "$SKETCH_DIR"
    echo "Upload complete."
    ;;
  build-upload)
    echo "Building sketch '$SKETCH_NAME' for board '$FQBN'..."
    arduino-cli compile --fqbn "$FQBN" "$SKETCH_DIR"

    # Get the serial port (manual or auto-detected)
    PORT=$(get_port)

    if [ -z "$PORT" ]; then
        echo "Error: Could not find a connected board. Please ensure it is connected and drivers are installed."
        exit 1
    fi

    echo "Found board on port: $PORT"
    echo "Uploading sketch '$SKETCH_NAME' to board '$FQBN'..."
    arduino-cli upload -p "$PORT" --fqbn "$FQBN" "$SKETCH_DIR"
    echo "Build and upload complete."
    ;;
  *)
    echo "Error: Invalid action '$ACTION'. Use 'build', 'upload', or 'build-upload'." >&2
    exit 1
    ;;
esac

