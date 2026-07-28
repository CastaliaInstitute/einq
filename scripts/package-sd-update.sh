#!/usr/bin/env bash
# Package the current Mynah CrossPoint image for Xteink's SD-card updater.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
FIRMWARE="${EINQ_FIRMWARE:-$ROOT/.vendor/crosspoint-reader/.pio/build/gh_release/firmware.bin}"
VERSION="$(tr -d '[:space:]' <"$ROOT/firmware/einq-version")"
OUTPUT_DIR="${EINQ_OUTPUT_DIR:-$ROOT/dist/mynah-einq-$VERSION}"

if [[ ! -f "$FIRMWARE" ]]; then
  echo "error: firmware not found at $FIRMWARE; run scripts/build-firmware.sh first" >&2
  exit 1
fi

mkdir -p "$OUTPUT_DIR"
cp "$FIRMWARE" "$OUTPUT_DIR/update.bin"
shasum -a 256 "$OUTPUT_DIR/update.bin" >"$OUTPUT_DIR/update.bin.sha256"

echo "Packaged: $OUTPUT_DIR/update.bin"
echo "Copy update.bin to the root of a FAT32 SD card."
