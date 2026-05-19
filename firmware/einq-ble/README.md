# Einq BLE GATT surface

Provisional BLE API for the X4 Einq face: read what is on screen and push a simple text face.

## Device

- **Name:** `Einq`
- **Service UUID:** `a1b2c3d4-e5f6-4789-a012-3456789abcde`

## Characteristics

| UUID suffix | Name | Properties | Payload |
|-------------|------|------------|---------|
| `…abc01` | `display` | READ, NOTIFY | JSON snapshot of current face |
| `…abc02` | `display_cmd` | WRITE, WRITE_NR | JSON command to change face |

Full UUIDs:

- Service: `a1b2c3d4-e5f6-4789-a012-3456789abcde`
- Display state: `a1b2c3d4-e5f6-4789-a012-3456789abc01`
- Display command: `a1b2c3d4-e5f6-4789-a012-3456789abc02`

## Display state (read / notify)

**Clock mode** (default):

```json
{"mode":"clock","title":"Einq","time":"14:30","day":"Monday","date":"2026-05-18"}
```

When time is unsynced, `line1` may be `"Connect WiFi to sync time"`.

**Message mode** (after a BLE write):

```json
{"mode":"message","title":"Hello","line1":"First line","line2":"Second"}
```

Subscribe to NOTIFY on `display` to get updates when the face changes.

## Display command (write)

Return to the live clock:

```json
{"mode":"clock"}
```

Show up to three centered lines under a header title:

```json
{"mode":"message","title":"Einq","line1":"Castalia","line2":"inquiry"}
```

Alternate body format:

```json
{"mode":"message","title":"Einq","lines":["Line one","Line two"]}
```

## Test from a Mac (bleak)

```bash
pip install bleak
python3 scripts/ble-einq-probe.py
python3 scripts/ble-einq-probe.py --show '{"mode":"message","title":"Einq","line1":"Hello from BLE"}'
```
