# Einq firmware notes

Shared board definitions, display helpers, and integration notes for Castalia e-paper targets.

**Primary device:** Xteink X4 (ESP32-C3) with [CrossPoint Reader](https://github.com/crosspoint-reader/crosspoint-reader).

App-specific code belongs under `../apps/`. This folder is for cross-app utilities as the platform grows.

## BLE (`einq-ble/`)

NimBLE GATT surface used by the clock-face patch:

- **Read / notify** current display as JSON (`display` characteristic)
- **Write** a clock or message face (`display_cmd` characteristic)

See [einq-ble/README.md](einq-ble/README.md) and `scripts/ble-einq-probe.py`.

## Schedule (`einq-schedule/`)

Light-sleep timer helpers: wake at the next minute boundary or midnight for glyph rotation. Used by the clock-face patch.

## Glyph (`einq-glyph/`)

Simple monochrome person / place / thing glyphs for the inquiry face (v0 stand-in for Noto Emoji). See `apps/inq-face/CARD-ART.md`.

## Card of the Day (`einq-cotd/`)

Fetches and caches daily cards from [cards.castalia.institute](https://cards.castalia.institute). See `apps/inq-face/COTD.md`.

## OTA (`einq-ota/`)

Midnight auto-update from [einq.castalia.institute/firmware.json](https://einq.castalia.institute/firmware.json) (GitHub Pages). See [docs/OTA.md](../docs/OTA.md).

## OTA

Version in [`einq-version`](einq-version). Patched builds pull updates from [CastaliaInstitute/einq releases](https://github.com/CastaliaInstitute/einq/releases). See [docs/OTA.md](../docs/OTA.md).
