# Card of the Day (cards.castalia.institute)

Einq faces can pull the daily **iNQ card** from the same schedule as [cards.castalia.institute](https://cards.castalia.institute), aligned with the **7-year developmental arc** and **Candlemas seasons** (Castalia’s seasonal year — Spring from Feb 1, then Summer / Fall / Winter).

## API

Static JSON per calendar day (built in the [cards](https://github.com/CastaliaInstitute/cards) repo):

```
GET https://cards.castalia.institute/card-of-the-day/YYYY-MM-DD.json
```

Example (`2026-05-29`):

```json
{
  "date": "2026-05-29",
  "month": {
    "month": "2026-05",
    "year": 1,
    "season": 2,
    "topic": "Home Spaces",
    "focus": "Home Spaces"
  },
  "card": {
    "slug": "rain",
    "title": "Rain",
    "domain": "weather_sky",
    "season": 2,
    "topic": "Water Cycle",
    "image": "https://cards.castalia.institute/_astro/rain_….png"
  }
}
```

- **year** — developmental year (1–7)
- **season** — Candlemas season (1 Spring Feb–Apr, 2 Summer, 3 Fall, 4 Winter)
- **topic** — monthly theme from `editorial/arc/monthly-topics.json`
- **card** — that day’s card from the month’s 12-card manifest

## Einq firmware (v0)

`firmware/einq-cotd/`:

- Fetches JSON when WiFi is up (boot after NTP, midnight wake)
- Caches raw JSON on SD: `/.einq/cotd/YYYY-MM-DD.json`
- Offline: reads cache from the last successful sync

The clock-face patch shows **title + topic** with a domain-based glyph (`people` → person, `locations` → place, else thing). Card PNGs are not rendered on e-paper yet.

## Deploy note

The JSON routes are generated at build time in **cards** (`src/pages/card-of-the-day/[date].json.ts`). Ensure a **cards** deploy includes `dist/card-of-the-day/*.json` before devices can fetch live data. Until then, Einq falls back to cached SD files or the local day-of-year glyph.

## Related

- [AUTH.md](AUTH.md) — QR sign-in for per-user / household cards (planned)
- [CARD-ART.md](CARD-ART.md) — future Noto Emoji / bitmap art
- [cards/editorial/arc/README.md](https://github.com/CastaliaInstitute/cards/blob/main/editorial/arc/README.md) — arc, manifests, Candlemas seasons
