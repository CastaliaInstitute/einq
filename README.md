# Einq

**Einq** is Castalia Institute’s e-paper platform — calm, low-power surfaces for information that should live in the room, not on a phone.

**Site:** [https://einq.castalia.institute/](https://einq.castalia.institute/)

## Supported hardware

| Device | Status | Notes |
|--------|--------|--------|
| [Xteink X4](https://www.xteink.com/) | **Supported** | 4.3″ e-paper reader; primary development target |

We run **[CrossPoint Reader](https://github.com/crosspoint-reader/crosspoint-reader)** — open-source firmware for the X4 (ESP32-C3, PlatformIO). It replaces the stock reader OS with EPUB support, WiFi upload, OTA updates, and a path toward custom apps.

## Goals

- **Time-aware surfaces** — apps that refresh the display by day, hour, or schedule (calendar, quotes, household rhythm, ambient information).
- **Castalia apps** — first-party sketches and libraries shared in this repo under `apps/` and `firmware/`.
- **Developer-friendly** — document flashing, CrossPoint builds, and how to ship a screen that updates on a timer.

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

## Flash CrossPoint on X4

See [CrossPoint Reader](https://github.com/crosspoint-reader/crosspoint-reader) — web flasher or `esptool`. Official Xteink firmware can be restored if needed.

## Develop an Einq app

See [develop](https://einq.castalia.institute/develop.html) on the site and `apps/README.md` in this repo.

## License

Documentation and site: Castalia Institute. Third-party firmware (CrossPoint) remains under its upstream license (MIT).
