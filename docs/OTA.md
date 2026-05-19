# Einq firmware OTA

Einq builds use CrossPoint’s **Check for updates** flow (`Settings → System → Check for updates`). The device downloads `firmware.bin` from the latest [GitHub Release](https://github.com/CastaliaInstitute/einq/releases) on **CastaliaInstitute/einq** (not the upstream CrossPoint repo).

## Requirements on device

- Einq-patched firmware (OTA URL points at this repo).
- WiFi connected (saved credentials on SD — see [apps/clock-face/README.md](../apps/clock-face/README.md)).
- Release version **newer** than `CROSSPOINT_VERSION` baked into the running image (semver `major.minor.patch`).

## Publish a release (maintainers)

1. Bump [`firmware/einq-version`](../firmware/einq-version) (e.g. `1.4.0` → `1.4.1`).
2. Commit, tag, and push:

```bash
git add firmware/einq-version
git commit -m "Release firmware 1.4.1"
git tag 1.4.1
git push origin main --tags
```

3. GitHub Actions workflow **Release Einq firmware** builds `gh_release` and creates a release with asset **`firmware.bin`** (exact name required).

Or run the workflow manually (**Actions → Release Einq firmware → Run workflow**) and pass the version.

## Install on device

1. **Back** from Einq clock → CrossPoint home → **Settings** → **System** → **Check for updates**.
2. Confirm WiFi when prompted.
3. Install when offered; device reboots into the new image.

## USB flash (first time or recovery)

```bash
gh run download --repo CastaliaInstitute/einq -n einq-firmware
EINQ_PATCH=0 EINQ_FIRMWARE="$PWD/firmware.bin" ./scripts/flash-x4.sh
```
