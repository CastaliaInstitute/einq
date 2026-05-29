# Einq firmware OTA

Two update paths:

| Path | When | Source |
|------|------|--------|
| **Manual** | Settings → System → Check for updates | [GitHub Releases](https://github.com/CastaliaInstitute/einq/releases) API (`firmware.bin` asset) |
| **Scheduled** | Midnight wake (Einq clock face) | [GitHub Pages](https://einq.castalia.institute) `firmware.json` + `firmware.bin` |

## Requirements on device

- Einq-patched firmware.
- WiFi connected (saved credentials on SD — see [apps/clock-face/README.md](../apps/clock-face/README.md)).
- Release version **newer** than `CROSSPOINT_VERSION` baked into the running image (semver `major.minor.patch`).

## GitHub Pages manifest

Published at **https://einq.castalia.institute/firmware.json** (with matching **firmware.bin**):

```json
{
  "version": "1.4.1",
  "url": "https://einq.castalia.institute/firmware.bin",
  "size": 6091799
}
```

The midnight auto-update reads this manifest, compares semver to the running build, and installs silently if newer (brief “Updating firmware” on screen, then reboot).

`scripts/bundle-pages-site.sh` assembles `docs/` + firmware for Pages deploy. It runs on:

- **Release Einq firmware** (tags) — ships the new binary immediately
- **Deploy GitHub Pages** (docs pushes) — re-bundles the latest release asset when available

## Publish a release (maintainers)

1. Bump [`firmware/einq-version`](../firmware/einq-version) (e.g. `1.4.0` → `1.4.1`).
2. Commit, tag, and push:

```bash
git add firmware/einq-version
git commit -m "Release firmware 1.4.1"
git tag 1.4.1
git push origin main --tags
```

3. Workflow **Release Einq firmware** builds `gh_release`, creates the GitHub Release, and deploys Pages with OTA files.

Or run the workflow manually (**Actions → Release Einq firmware → Run workflow**) and pass the version.

## Install on device

**Automatic:** leave the device on the Einq face with WiFi configured; at midnight it checks Pages OTA after syncing the card of the day.

**Manual:**

1. **Back** from Einq clock → CrossPoint home → **Settings** → **System** → **Check for updates**.
2. Confirm WiFi when prompted.
3. Install when offered; device reboots into the new image.

## USB flash (first time or recovery)

```bash
gh run download --repo CastaliaInstitute/einq -n einq-firmware
EINQ_PATCH=0 EINQ_FIRMWARE="$PWD/firmware.bin" ./scripts/flash-x4.sh
```
