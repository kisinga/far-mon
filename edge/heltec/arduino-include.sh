#!/usr/bin/env bash
set -euo pipefail

# Run this from within edge/heltec
# Usage:
#   ./arduino-include.sh apply   # create sketch-local lib symlinks
#   ./arduino-include.sh revert  # remove sketch-local lib symlinks

ROOT="$(pwd)"

is_heltec_dir() {
  [[ -d "lib" && -d "relay" && -d "remote" ]]
}

apply_changes() {
  echo "[apply] Checking directory..."
  if ! is_heltec_dir; then
    echo "Error: run this from edge/heltec (must contain lib/, relay/, remote/)" >&2
    exit 1
  fi

  echo "[apply] Creating sketch-local lib symlinks..."
  for d in relay remote; do
    if [[ -L "$d/lib" ]]; then
      echo " - $d/lib already a symlink; ok"
    elif [[ -e "$d/lib" && ! -L "$d/lib" ]]; then
      echo "Error: $d/lib exists and is not a symlink. Move it aside then re-run." >&2
      exit 1
    else
      ln -s ../lib "$d/lib"
      echo " - created $d/lib -> ../lib"
    fi
  done

  echo "[apply] Done."
  echo "        To revert: $0 revert"
}

revert_changes() {
  echo "[revert] Checking directory..."
  if ! is_heltec_dir; then
    echo "Error: run this from edge/heltec (must contain lib/, relay/, remote/)" >&2
    exit 1
  fi

  echo "[revert] Removing symlinks (if present)..."
  for d in relay remote; do
    if [[ -L "$d/lib" ]]; then
      rm "$d/lib"
      echo " - removed symlink $d/lib"
    else
      echo " - skip (no symlink): $d/lib"
    fi
  done
  echo "[revert] Done."
}

cmd="${1:-}"
case "$cmd" in
  apply)  apply_changes ;;
  revert) revert_changes ;;
  *) echo "Usage: $0 {apply|revert}"; exit 2 ;;
esac