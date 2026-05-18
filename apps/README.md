# Einq apps

Each subdirectory is one **Einq application** — firmware or assets for the ambient e-paper face on X4.

**Product direction:** [docs/VISION.md](../docs/VISION.md) — inq cards, quotes, mindfulness reminders, scheduled through the day. Schema and APIs later.

## Apps

| App | Status | Role |
|-----|--------|------|
| `clock-face/` | Demo | Boot face, time + weekday; proves schedule + display |
| `inq-face/` | Planned | Day-long rotation: inq cards, quotes, reminders |

## Conventions

- **PlatformIO** / ESP32-C3; extend CrossPoint or ship a dedicated face firmware
- Document refresh policy (partial vs full) per app
- Content packs: prefer SD or synced JSON later; no secrets in repo

## Scheduling

Apps choose the next wake from wall clock (and eventually NTP):

- Card / quote rotation (hourly or few times per day)
- Part-of-day layout (morning / day / evening)
- Minute tick only when showing live time

See [develop.html](https://einq.castalia.institute/develop.html).
