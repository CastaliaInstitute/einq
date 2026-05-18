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
