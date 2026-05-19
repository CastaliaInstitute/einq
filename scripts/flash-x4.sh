#!/usr/bin/env bash
# Flash CrossPoint (+ optional Einq patch build) to Xteink X4 (ESP32-C3).
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
CP="${CROSSPOINT_DIR:-$ROOT/.vendor/crosspoint-reader}"
PORT="${EINQ_PORT:-}"
BAUD="${EINQ_BAUD:-921600}"
USE_PATCH="${EINQ_PATCH:-1}"
FW_BIN="${EINQ_FIRMWARE:-}"

find_c3_port() {
  local p chip
  for p in /dev/cu.usbmodem*; do
    [[ -e "$p" ]] || continue
    chip="$(esptool.py -p "$p" chip_id 2>/dev/null | awk '/Chip is/{print $3; exit}')" || true
    if [[ "$chip" == "ESP32-C3" ]]; then
      echo "$p"
      return 0
    fi
  done
  return 1
}

if [[ -z "$PORT" ]]; then
  PORT="$(find_c3_port)" || {
    echo "error: no ESP32-C3 serial port found (plug in X4, wake screen)" >&2
    ls /dev/cu.usbmodem* 2>/dev/null || true
    exit 1
  }
fi

echo "Using port: $PORT"

if [[ "$USE_PATCH" == "1" ]]; then
  bash "$ROOT/scripts/apply-einq-clock-patch.sh"
  echo "Building CrossPoint + Einq Clock (gh_release)…"
  (cd "$CP" && pio run -e gh_release)
  FW_BIN="$CP/.pio/build/gh_release/firmware.bin"
elif [[ -z "$FW_BIN" ]]; then
  FW_BIN="$ROOT/.cache/firmware/crosspoint-1.3.0.bin"
  if [[ ! -f "$FW_BIN" ]]; then
    mkdir -p "$ROOT/.cache/firmware"
    curl -fsSL -o "$FW_BIN" \
      "https://github.com/crosspoint-reader/crosspoint-reader/releases/download/1.3.0/firmware.bin"
  fi
fi

if [[ ! -f "$FW_BIN" ]]; then
  echo "error: firmware not found: $FW_BIN" >&2
  exit 1
fi

echo "Flashing $FW_BIN @ 0x10000 …"
esptool.py --chip esp32c3 --port "$PORT" --baud "$BAUD" write_flash 0x10000 "$FW_BIN"

if [[ "$USE_PATCH" == "1" ]]; then
  echo "Done. Boots to Einq clock; OTA via Settings → System → Check for updates"
else
  echo "Done. CrossPoint 1.3.0 installed. Copy apps/clock-face/sd/sleep.bmp to SD (see apps/clock-face/README.md)."
fi
