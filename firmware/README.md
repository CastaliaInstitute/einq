# Einq firmware notes

Shared board definitions, display helpers, and integration notes for Castalia e-paper targets.

**Primary device:** Xteink X4 (ESP32-C3) with [CrossPoint Reader](https://github.com/crosspoint-reader/crosspoint-reader).

App-specific code belongs under `../apps/`. This folder is for cross-app utilities as the platform grows.

## BLE (`einq-ble/`)

NimBLE GATT surface used by the clock-face patch:

- **Read / notify** current display as JSON (`display` characteristic)
- **Write** a clock or message face (`display_cmd` characteristic)

See [einq-ble/README.md](einq-ble/README.md) and `scripts/ble-einq-probe.py`.

## OTA

Version in [`einq-version`](einq-version). Patched builds pull updates from [CastaliaInstitute/einq releases](https://github.com/CastaliaInstitute/einq/releases). See [docs/OTA.md](../docs/OTA.md).
