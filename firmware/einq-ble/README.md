# Einq BLE GATT surface

Provisional BLE API for the X4 Einq face: read what is on screen and push a simple text face.

## Device

- **Name:** `Einq`
- **Service UUID:** `a1b2c3d4-e5f6-4789-a012-3456789abcde`

The connectable advertisement keeps the service UUID for discovery. Its scan
response name identifies the card currently shown as `iNQ: <card title>`
(truncated to the 29-byte BLE name limit), so a scanner can identify the card
without opening a GATT connection.

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
{"mode":"clock","title":"Einq","time":"14:30","day":"Monday","date":"2026-05-18","glyph":"person"}
```

Clock and message snapshots also include the selected decorative face theme,
for example `"theme":"summer-grapevine"`, so deployed artwork can be verified
without inferring it from the display.

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

Provision WiFi and start NTP synchronization (the password is persisted
obfuscated on SD and is never returned in the display snapshot):

```json
{"mode":"wifi","ssid":"The Chateau","password":"thechateau"}
```

The clock uses the America/Denver POSIX timezone rule
`MST7MDT,M3.2.0,M11.1.0`, including daylight-saving transitions.

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
