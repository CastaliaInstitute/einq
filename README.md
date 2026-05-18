# Einq

**Einq** is Castalia Institute’s e-paper platform — calm, low-power surfaces for information that should live in the room, not on a phone.

**Site:** [https://einq.castalia.institute/](https://einq.castalia.institute/)

## Supported hardware

| Device | Status | Notes |
|--------|--------|--------|
| [Xteink X4](https://www.xteink.com/) | **Supported** | 4.3″ e-paper display (ESP32-C3); primary hardware |

**Einq is not an e-reader.** We use the X4 as an ambient inquiry surface (inq cards, quotes, reminders). Firmware is **Einq-only** — built on the open X4 hardware/SDK ecosystem, not a fork of reader UX.

## Goals

- **Day-long inq surface** — through the day the screen shows **inq cards** (person, place, thing), plus quotes, mindfulness reminders, and light time context — rotated by schedule, not by notifications. See [docs/VISION.md](docs/VISION.md) (details TBD).
- **Time-aware refresh** — what appears changes with part of day; e-paper updates on a timer, WiFi only when syncing content.
- **Castalia apps** — first-party logic under `apps/` and `firmware/` (clock demo today; **inq-face** next).
- **Developer-friendly** — CrossPoint as OS; Einq app owns the ambient face.

## Repository layout

```
docs/           # GitHub Pages site (einq.castalia.institute)
scripts/        # Cloudflare DNS for GitHub Pages
apps/           # Einq applications (screen logic, assets)
firmware/       # Shared firmware helpers / integration notes
```

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

## Flash / bring-up (X4)

Early validation used [CrossPoint](https://github.com/crosspoint-reader/crosspoint-reader) only to prove USB flash and the panel (`EINQ_PATCH=0 ./scripts/flash-x4.sh`). **Product firmware** will be **inq-face** — see `apps/inq-face/`.

```bash
# Optional: flash community CrossPoint image (bring-up / lab only)
EINQ_PATCH=0 ./scripts/flash-x4.sh
```

Restore stock or community images via [CrossPoint flash tools](https://crosspointreader.com/#flash-tools) if needed.

## Develop an Einq app

See [develop](https://einq.castalia.institute/develop.html) on the site and `apps/README.md` in this repo.

## License

Documentation and site: Castalia Institute. Third-party firmware (CrossPoint) remains under its upstream license (MIT).
