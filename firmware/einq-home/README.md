# Mynah eInq household payload

`EinqHome` fetches one compact personalized payload from the configured
Castalia gateway and caches the last valid response at `/.einq/home.json`.

```http
GET /api/v1/device/home?room=Kitchen
Authorization: Bearer <Castalia access token>
```

```json
{
  "generatedAt": "2026-07-28T16:20:00Z",
  "profile": "kid",
  "today": {
    "nextEvent": {
      "title": "Piano",
      "start": "2026-07-28T17:00:00-06:00",
      "end": "2026-07-28T18:00:00-06:00",
      "allDay": false
    }
  },
  "astrology": {
    "title": "Moon in Aquarius",
    "summary": "Notice the shape of a new idea."
  },
  "fortune": {
    "title": "The Lantern",
    "summary": "Carry enough light for the next step."
  },
  "card": {
    "title": "Threshold",
    "summary": "What changes when you cross?",
    "domain": "place"
  },
  "spotify": {
    "connected": true,
    "playing": true,
    "track": "An Ending (Ascent)",
    "artist": "Brian Eno",
    "device": "Kitchen",
    "volume": 34
  },
  "lights": {
    "available": true,
    "room": "Kitchen",
    "on": true,
    "brightness": 60,
    "scene": "Evening"
  },
  "permissions": {
    "calendar": true,
    "astrology": true,
    "fortune": true,
    "cards": true,
    "spotifyControl": true,
    "lightControl": true,
    "administration": false
  }
}
```

The gateway redacts private calendar fields and filters content before a kid
profile receives the payload. Firmware separately checks `permissions` before
showing or sending an action.

Interactive faces send:

```http
POST /api/v1/device/actions
Content-Type: application/json

{"action":"spotify.toggle","room":"Kitchen"}
```

The supported device actions are `spotify.toggle` and `lights.toggle`. The
gateway remains authoritative: it must validate the device session, profile,
room, and household policy even though firmware also rejects actions not
granted in the home payload.
