# Einq Clock (demo app)

Time-aware face for Xteink X4. Two ways to run it:

## A. Built into CrossPoint (recommended)

Patches CrossPoint with **Home → Einq Clock** (live clock, refreshes each minute).

```bash
./scripts/flash-x4.sh              # EINQ_PATCH=1 (default): build + flash
EINQ_PATCH=0 ./scripts/flash-x4.sh # official CrossPoint release only
```

Requires PlatformIO + [pioarduino](https://github.com/pioarduino/pioarduino) per upstream CrossPoint docs if the build fails on stock PlatformIO.

Patch sources: `apps/clock-face/patch/`

### BLE

When built with the patch, the device advertises as **Einq** and exposes a GATT service to read the current face (clock or message) and push a simple text display. See [firmware/einq-ble/README.md](../../firmware/einq-ble/README.md).

```bash
pip install bleak
python3 scripts/ble-einq-probe.py
python3 scripts/ble-einq-probe.py --show '{"mode":"message","title":"Einq","line1":"Hello"}'
```

## WiFi (demo)

CrossPoint stores WiFi on the **SD card** at `/.crosspoint/wifi.json` (not ESP NVS). For the Chateau demo:

```bash
chmod +x scripts/setup-wifi-sd.py
./scripts/setup-wifi-sd.py --from-device
# When the SD volume is mounted:
./scripts/setup-wifi-sd.py --from-device --mount /Volumes/YOUR_SD
```

Default SSID/password: **The Chateau** / **thechateau**. Staging file: `apps/clock-face/sd/.crosspoint/wifi.json`.

With the Einq clock patch, the face loads that file on boot, connects, and runs NTP so the clock can show real time.

### Scheduled wake (glyph + clock)

The Einq face **light-sleeps** between updates (CrossPoint deep sleep is disabled while the face is active). It wakes on:

- **Each minute** — refresh the clock
- **Midnight** — rotate the daily **iNQ glyph** (person / place / thing by day-of-year)
- **Power button** — same as any wake from light sleep

Requires NTP-synced wall time (WiFi once at boot). Long-press power still enters CrossPoint deep sleep (manual).

**Card of the Day:** when WiFi is available, the face syncs from `https://cards.castalia.institute/card-of-the-day/YYYY-MM-DD.json` (7-year arc + Candlemas seasons — same schedule as iNQ Cards). Cached on SD at `/.einq/cotd/`. See [apps/inq-face/COTD.md](../inq-face/COTD.md).

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
