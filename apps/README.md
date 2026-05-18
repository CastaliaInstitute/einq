# Einq apps

Each subdirectory is one installable or flashable **Einq application** — firmware that owns the e-paper surface on a supported device.

## Planned apps

| App | Purpose |
|-----|---------|
| `clock-face/` | Time + weekday; partial refresh each minute, full refresh at day change |
| *(your app)* | Fork this table row when you add a folder |

## Conventions

- **PlatformIO** / ESP32-C3 unless noted otherwise
- Target **CrossPoint Reader** display APIs where possible, or document fork points
- Document refresh policy (partial vs full) in each app’s README
- No secrets in repo — use `secrets.ini` locally (gitignored) for WiFi if needed

## Time-based updates

Apps should compute the next wake time from wall clock or NTP-synced RTC:

- Minute tick for clocks
- Hour boundary for coarser faces
- Calendar rules (weekday vs weekend layout)
- Optional external sync (Castalia API) on a longer interval

See [develop.html](https://einq.castalia.institute/develop.html) on the site.
