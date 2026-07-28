# Einq

**Mynah eInq** is Castalia Institute’s e-paper experience for **Mynah X3**
and **Mynah X4** — calm, low-power surfaces for information that should live
in the room, not on a phone.

**Site:** [https://einq.castalia.institute/](https://einq.castalia.institute/)

## Supported hardware

| Device | Status | Notes |
|--------|--------|--------|
| Mynah X3 | **Target** | Based on the Xteink X3; licensing in progress |
| Mynah X4 | **Supported** | Based on the [Xteink X4](https://www.xteink.com/); licensing in progress |

Mynah X3 and X4 install
[CrossPoint](https://github.com/crosspoint-reader/crosspoint-reader) as their
base system. eInq is the default Castalia household experience; CrossPoint
continues to provide board support, settings, recovery, and its reader
capabilities.

## Goals

- **Day-long inq surface** — through the day the screen shows **inq cards** (person, place, thing), plus quotes, mindfulness reminders, and light time context — rotated by schedule, not by notifications. See [docs/VISION.md](docs/VISION.md) (details TBD).
- **Time-aware refresh** — what appears changes with part of day; e-paper updates on a timer, WiFi only when syncing content.
- **Castalia apps** — first-party logic under `apps/` and `firmware/` (clock demo today; **inq-face** next).
- **Developer-friendly** — CrossPoint as the installed base system; eInq owns
  the default ambient face.
- **Room-aware controls** — BLE RSSI selects the likely room so lighting and
  household actions stay relevant to where the device is.
- **Two household modes** — parent and kid profiles share the same firmware,
  with capabilities enforced by policy rather than by separate builds.

The current product architecture and staged feature plan are in
[docs/PRODUCT.md](docs/PRODUCT.md).

## Repository layout

```
docs/           # GitHub Pages site (einq.castalia.institute)
scripts/        # Cloudflare DNS for GitHub Pages
apps/           # Einq applications (screen logic, assets)
firmware/       # Shared firmware helpers / integration notes
```

## Firmware OTA

Einq-patched images update over WiFi from [GitHub Releases](https://github.com/CastaliaInstitute/einq/releases) (manual) or [einq.castalia.institute](https://einq.castalia.institute/firmware.json) (midnight auto-update on the Einq face). See [docs/OTA.md](docs/OTA.md).

## Site & deploy

Static pages live in `docs/`. GitHub Actions deploys to GitHub Pages on push to `main`.

1. **Settings → Pages → Build and deployment:** GitHub Actions (`Deploy GitHub Pages` workflow).
2. **Custom domain:** `einq.castalia.institute` (see `docs/CNAME`).

## DNS (Cloudflare)

`einq.castalia.institute` → `castaliainstitute.github.io` (DNS only, not proxied).

```bash
set -a && source ../castalia.institute/.env && set +a
[[ -z "${CLOUDFLARE_API_TOKEN:-}" && -n "${CLOUDFLARE_TOKEN:-}" ]] && export CLOUDFLARE_API_TOKEN="$CLOUDFLARE_TOKEN"
./scripts/cf-dns-einq-github-pages.sh
```

Or run the **Sync einq DNS** workflow (requires repo/org `CLOUDFLARE_API_TOKEN`).

## Install / bring-up (Mynah X4)

The build installs CrossPoint with the Mynah eInq modules and default face.
Unmodified community CrossPoint remains useful for board validation.

```bash
# Optional: flash unmodified community CrossPoint for board validation
EINQ_PATCH=0 ./scripts/flash-x4.sh
```

Restore or install images via
[CrossPoint flash tools](https://crosspointreader.com/#flash-tools) if needed.

## Develop an Einq app

See [develop](https://einq.castalia.institute/develop.html) on the site and `apps/README.md` in this repo.

## License

Documentation and site: Castalia Institute. Third-party firmware (CrossPoint) remains under its upstream license (MIT).
