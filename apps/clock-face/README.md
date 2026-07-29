# Mynah eInq CrossPoint app

Daily household experience for Xteink X3 and X4.

## A. Built into CrossPoint (recommended)

Patches CrossPoint with the Mynah face carousel and starts it on boot.

```bash
./scripts/flash-x4.sh              # EINQ_PATCH=1 (default): build + flash
EINQ_PATCH=0 ./scripts/flash-x4.sh # official CrossPoint release only
```

Requires PlatformIO + [pioarduino](https://github.com/pioarduino/pioarduino) per upstream CrossPoint docs if the build fails on stock PlatformIO.

Patch sources: `apps/clock-face/patch/`

### Corner themes

The front-page ornaments are manifest-driven. Add a transparent master corner
PNG to `assets/`, register it in `assets/corner-themes/manifest.json`, and run:

```bash
python3 scripts/generate-corner-art.py
```

Each unique image is packed to 1bpp once. A theme can provide one `source` that
is flipped into all four corners, independent images under `corners`, or a
master plus selected overrides:

```json
{
  "id": "winter-holly",
  "source": "assets/holly-master.png",
  "corners": {
    "top_right": "assets/holly-top-right.png",
    "bottom_left": "assets/holly-bottom-left.png"
  },
  "seasons": ["winter"]
}
```

Supported corner keys are `top_left`, `top_right`, `bottom_left`, and
`bottom_right`. Themes may target `winter`, `spring`, `summer`, `autumn`, or
`all`. When several themes are eligible for the current season, the firmware
rotates them deterministically by day of year.

### BLE

When built with the patch, the device advertises as **Einq** and exposes a GATT service to read the current face (clock or message) and push a simple text display. See [firmware/einq-ble/README.md](../../firmware/einq-ble/README.md).

```bash
pip install bleak
python3 scripts/ble-einq-probe.py
python3 scripts/ble-einq-probe.py --show '{"mode":"message","title":"Einq","line1":"Hello"}'
```

## WiFi

Einq stores WiFi in **ESP32 NVS** (not SD). On-device setup:

1. Long-press the forward side button to open setup.
2. Join the device-specific `x3-XXXX` hotspot when neither saved network connects.
3. Open `http://x3.local/` or `http://192.168.4.1/`.
4. Save primary and backup WiFi networks in NVS.

See [apps/inq-face/WIFI.md](../inq-face/WIFI.md).

Legacy SD WiFi (`scripts/setup-wifi-sd.py`) is only for upstream CrossPoint file transfer, not the Einq clock patch.

### Daily faces

The app light-sleeps between updates and wakes for clock, content, and room changes.
Short presses on either side button move through:

Today, Calendar, Self Weather, Synastry, Family, Fortune, eINQ Card,
Spotify, and room Lights. Unavailable or disallowed faces are skipped.

**Midnight OTA:** checks `https://einq.castalia.institute/firmware.json` once per day at the day boundary; installs newer firmware automatically from GitHub Pages. Manual updates still use Settings → System → Check for updates (GitHub Releases). See [docs/OTA.md](../../docs/OTA.md).

**Note:** X4 deep sleep cuts MCU power on battery, so timer wake only works in this light-sleep path — not in CrossPoint’s stock deep sleep screen.

## OTA updates

Patched firmware checks **CastaliaInstitute/einq** GitHub Releases (not upstream CrossPoint). On device: **Settings → System → Check for updates** (WiFi required).

Maintainers: see [docs/OTA.md](../../docs/OTA.md) (tag `1.4.0`, asset `firmware.bin`).

## B. SD card sleep demo (stock CrossPoint)

Works on the **CrossPoint 1.3.0** image already flashed without the patch.

1. Insert the X4 SD card (or mount over USB if exposed).
2. Generate assets:

```bash
python3 scripts/generate-demo-sleep-bmp.py
cp apps/clock-face/sd/sleep.bmp /Volumes/YOUR_SD/sleep.bmp
mkdir -p /Volumes/YOUR_SD/.sleep
cp apps/clock-face/sd/sleep.bmp /Volumes/YOUR_SD/.sleep/einq-demo.bmp
```

3. On device: **Settings → Sleep screen → Custom** (or Cover + Custom).
4. Put the device to sleep to see the Einq clock BMP.

Re-run the generator (or `scripts/einq-clock-sync.sh` when added) to update the image from your Mac.

## Hardware

- Flash/upload port: ESP32-C3 USB serial (`/dev/cu.usbmodem*`, not ESP32-S3).
