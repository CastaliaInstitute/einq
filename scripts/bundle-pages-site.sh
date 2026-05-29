#!/usr/bin/env bash
# Assemble GitHub Pages site: docs/ + optional firmware.bin + firmware.json
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
OUT="${1:-$ROOT/_pages}"
FW_SRC="${EINQ_FIRMWARE:-}"
VERSION_FILE="$ROOT/firmware/einq-version"
SITE_BASE="${EINQ_PAGES_URL:-https://einq.castalia.institute}"

rm -rf "$OUT"
mkdir -p "$OUT"
cp -r "$ROOT/docs/." "$OUT/"

VERSION=""
if [[ -f "$VERSION_FILE" ]]; then
  VERSION="$(tr -d '[:space:]' <"$VERSION_FILE")"
fi

if [[ -z "$FW_SRC" && -n "${GH_TOKEN:-${GITHUB_TOKEN:-}}" ]]; then
  TAG="$(gh release list --repo CastaliaInstitute/einq --limit 1 --json tagName -q '.[0].tagName' 2>/dev/null || true)"
  if [[ -n "$TAG" ]]; then
    TMP_FW="$(mktemp)"
    if gh release download "$TAG" --repo CastaliaInstitute/einq --pattern firmware.bin -O "$TMP_FW" 2>/dev/null; then
      FW_SRC="$TMP_FW"
      VERSION="${VERSION:-$TAG}"
    fi
  fi
fi

if [[ -n "$FW_SRC" && -f "$FW_SRC" ]]; then
  cp "$FW_SRC" "$OUT/firmware.bin"
  SIZE=$(wc -c <"$OUT/firmware.bin" | tr -d ' ')
  VERSION="${VERSION:-0.0.0}"
  python3 - "$OUT/firmware.json" "$VERSION" "$SITE_BASE/firmware.bin" "$SIZE" <<'PY'
import json
import sys
path, version, url, size = sys.argv[1:5]
with open(path, "w", encoding="utf-8") as f:
    json.dump({"version": version, "url": url, "size": int(size)}, f, indent=2)
    f.write("\n")
print(f"wrote {path} version={version} size={size}")
PY
  echo "Bundled firmware.bin ($SIZE bytes) and firmware.json"
else
  echo "No firmware.bin bundled (set EINQ_FIRMWARE or publish a release)" >&2
fi

echo "Pages site ready at $OUT"
