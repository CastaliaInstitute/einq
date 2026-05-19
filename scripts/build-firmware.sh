#!/usr/bin/env bash
# Build Einq-patched CrossPoint firmware (boot → Einq clock; Back → CrossPoint home).
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
CP="${CROSSPOINT_DIR:-$ROOT/.vendor/crosspoint-reader}"
PIO="${PIO:-$ROOT/.venv-pio/bin/pio}"

if [[ ! -x "$PIO" ]]; then
  echo "error: PlatformIO not found at $PIO — create venv:" >&2
  echo "  python3 -m venv $ROOT/.venv-pio && $ROOT/.venv-pio/bin/pip install -U https://github.com/pioarduino/platformio-core/archive/refs/tags/v6.1.19.zip" >&2
  exit 1
fi

# ESP32 platform builder deps (see pioarduino platform penv_setup.py)
"$ROOT/.venv-pio/bin/pip" install -q 'littlefs-python>=0.16.0' 'fatfs-ng>=0.1.14' 'pyyaml>=6.0.2' \
  'rich-click>=1.8.6' 'zopfli>=0.2.2' intelhex 'rich>=14.0.0' 'urllib3<2' cryptography certifi ecdsa \
  'bitstring>=4.3.1' 'reedsolo>=1.5.3,<1.8' 2>/dev/null || true

if [[ ! -d "$CP/src" ]]; then
  git clone --depth 1 --recursive https://github.com/crosspoint-reader/crosspoint-reader.git "$CP"
fi

bash "$ROOT/scripts/apply-einq-clock-patch.sh"
"$PIO" run -d "$CP" -e gh_release
echo "Built: $CP/.pio/build/gh_release/firmware.bin"
