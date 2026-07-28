# Einq firmware notes

Shared board definitions, display helpers, and integration notes for Castalia e-paper targets.

**Devices:** Mynah X3 and Mynah X4, based on Xteink ESP32-C3 hardware and
shipping with [CrossPoint Reader](https://github.com/crosspoint-reader/crosspoint-reader)
as the installed base system.

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

## WiFi (`einq-wifi/`)

Dual-network NVS credential store plus setup PWA (`x3-XXXX` fallback AP and
`x3.local` on the home network). See [apps/inq-face/WIFI.md](../apps/inq-face/WIFI.md).

## Device configuration (`einq-config/`)

Validated, versioned JSON for Mynah model, parent/kid policy, enabled faces,
Castalia gateway, and calibrated room beacons. WiFi and service credentials
are deliberately stored outside this document.

## Room inference (`einq-room/`)

Hardware-independent BLE RSSI smoothing and room-change hysteresis. The
CrossPoint/NimBLE scanner feeds observations into this resolver.

## Household payload (`einq-home/`)

Bounded Castalia gateway client and offline cache for calendar, astrology,
fortune, eINQ, Spotify playback, room lights, and parent/kid permissions.

## Castalia pairing (`einq-auth/`)

QR pairing, SD-only session persistence, and gateway-proxied token refresh,
reusing the deployed Pocket Mynah `mynah-castalia-link` flow.

## Versioning

Version in [`einq-version`](einq-version). Patched builds pull updates from [CastaliaInstitute/einq releases](https://github.com/CastaliaInstitute/einq/releases).

Build with `PIO=/opt/homebrew/bin/pio ./scripts/build-firmware.sh`, then package
the image for the X3/X4 SD updater with `./scripts/package-sd-update.sh`. This
creates `dist/mynah-einq-<version>/update.bin` and its SHA-256 checksum.

Put `update.bin` at the root of a FAT32 SD card, power the device fully off,
then hold the left-side Up/page-turn button and top-right Power for about three
seconds until the update screen appears.
