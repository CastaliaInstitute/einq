# Einq apps

Each subdirectory is one **Mynah eInq application** — firmware or assets for
the default Castalia experience on Mynah X3 and Mynah X4.

**Product direction:** [docs/VISION.md](../docs/VISION.md) — inq cards, quotes, mindfulness reminders, scheduled through the day. Schema and APIs later.

## Apps

| App | Status | Role |
|-----|--------|------|
| `clock-face/` | Demo | Boot face, time + weekday; BLE read/write display surface |
| `inq-face/` | Planned | Day-long rotation: inq cards, quotes, reminders |

## Conventions

- **PlatformIO** / ESP32-C3; install CrossPoint and extend it with first-class
  Mynah activities
- Document refresh policy (partial vs full) per app
- Content packs: prefer SD or synced JSON later; no secrets in repo

## Scheduling

Apps choose the next wake from wall clock (and eventually NTP):

- Card / quote rotation (hourly or few times per day)
- Part-of-day layout (morning / day / evening)
- Minute tick only when showing live time

See [develop.html](https://einq.castalia.institute/develop.html).
