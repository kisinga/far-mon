#!/usr/bin/env bash
set -euo pipefail

# Run this from within edge/heltec
# Usage:
#   ./arduino-include.sh apply   # create sketch-local lib symlinks
#   ./arduino-include.sh revert  # remove sketch-local lib symlinks

VERBOSE=false

# Parse arguments
while [[ $# -gt 0 ]]; do
  case $1 in
    --verbose)
      VERBOSE=true
      shift
      ;;
    -*)
      echo "Unknown option: $1" >&2
      echo "Usage: $0 [--verbose] {apply|revert}"
      exit 1
      ;;
    *)
      break
      ;;
  esac
done

ROOT="$(pwd)"

is_heltec_dir() {
  [[ -d "lib" && -d "relay" && -d "remote" ]]
}

apply_changes() {
  if [ "$VERBOSE" = true ]; then
    echo "[apply] Checking if we're in the correct directory..."
  fi
  if ! is_heltec_dir; then
    echo "Error: run this from edge/heltec (must contain lib/, relay/, remote/)" >&2
    exit 1
  fi

  if [ "$VERBOSE" = true ]; then
    echo "[apply] Creating sketch-local lib symlinks..."
    echo "[apply] Processing sketches: relay, remote"
  fi

  for d in relay remote; do
    if [[ -L "$d/lib" ]]; then
      echo " - $d/lib already a symlink; ok"
    elif [[ -e "$d/lib" && ! -L "$d/lib" ]]; then
      echo "Error: $d/lib exists and is not a symlink. Move it aside then re-run." >&2
      exit 1
    else
      if [ "$VERBOSE" = true ]; then
        echo " - creating symlink: $d/lib -> ../lib"
      fi
      ln -s ../lib "$d/lib"
      echo " - created $d/lib -> ../lib"
    fi
  done

  echo "[apply] Done."
  if [ "$VERBOSE" = true ]; then
    echo "[apply] To revert changes, run: $0 revert"
  fi
}

revert_changes() {
  if [ "$VERBOSE" = true ]; then
    echo "[revert] Checking if we're in the correct directory..."
  fi
  if ! is_heltec_dir; then
    echo "Error: run this from edge/heltec (must contain lib/, relay/, remote/)" >&2
    exit 1
  fi

  if [ "$VERBOSE" = true ]; then
    echo "[revert] Removing symlinks (if present)..."
    echo "[revert] Processing sketches: relay, remote"
  fi

  for d in relay remote; do
    if [[ -L "$d/lib" ]]; then
      if [ "$VERBOSE" = true ]; then
        echo " - removing symlink: $d/lib"
      fi
      rm "$d/lib"
      echo " - removed symlink $d/lib"
    else
      echo " - skip (no symlink): $d/lib"
    fi
  done
  echo "[revert] Done."
}

# Check if we have a command after parsing options
if [ $# -lt 1 ]; then
  echo "Usage: $0 [--verbose] {apply|revert}"
  exit 2
fi

cmd="$1"
case "$cmd" in
  apply)  apply_changes ;;
  revert) revert_changes ;;
  *) echo "Usage: $0 [--verbose] {apply|revert}"; exit 2 ;;
esac