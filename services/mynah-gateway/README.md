# Mynah gateway

This edge service implements the two APIs consumed by Mynah X3/X4:

- `GET /api/v1/health`
- `GET /api/v1/device/daily?room=Kitchen`
- `GET /api/v1/device/home?room=Kitchen`
- `POST /api/v1/device/actions`

It combines Castalia calendar/astrology/fortune/eINQ content with room-specific
Home Assistant state. Tuya lights are exposed to Mynah through their Home
Assistant `light.*` entities; Spotify uses a `media_player.*` entity.

`device/daily` is the canonical synchronization endpoint; `device/home`
remains a compatibility alias. Its `castalia.device.daily.v1` response may
contain Castalia CalDAV-derived events and tasks, news, art, quote and
mindfulness selections, the user's EPUB catalog revision, personal face
settings, and OTA metadata. The device caches this bounded bundle on SD.

## Configuration

Set ordinary variables:

- `HOME_ASSISTANT_URL`
- `CASTALIA_CONTENT_URL`
- `CASTALIA_NEWS_URL` (optional when news is already in Castalia content)

Set secrets:

- `HOME_ASSISTANT_TOKEN`
- `CASTALIA_SERVICE_TOKEN`
- `DEVICE_SESSIONS`
- `SUPABASE_URL`
- `SUPABASE_ANON_KEY`

The production Worker is routed at `mynah.castalia.institute` by
`wrangler.toml`. Set Worker variables and secrets in Cloudflare before using
the manual **Deploy Mynah gateway** workflow. `keep_vars = true` prevents a
source deployment from erasing values managed in the Cloudflare dashboard.

`DEVICE_SESSIONS` is a JSON object keyed by device bearer token:

```json
{
  "replace-with-a-random-device-token": {
    "profile": "kid",
    "ageBand": "tween",
    "calendarEntity": "calendar.family",
    "weatherEntity": "weather.home",
    "calendarTitleMode": "busy",
    "permissions": {
      "calendar": true,
      "astrology": true,
      "fortune": true,
      "cards": true,
      "spotifyControl": true,
      "lightControl": true,
      "administration": false
    },
    "rooms": {
      "Kitchen": {
        "spotifyEntity": "media_player.spotify_kitchen",
        "lightEntities": ["light.kitchen_ceiling", "light.kitchen_counter"],
        "scene": "Evening"
      }
    }
  }
}
```

For configurable household calendars, prefer an allow-list:

```json
{
  "defaultCalendarId": "family@example.com",
  "calendars": {
    "family@example.com": "calendar.family"
  }
}
```

The device sends the selected calendar ID, but the gateway reads only its
mapped Home Assistant entity. Unknown IDs return no events.

Start from `device-policy.example.json` for the Daniel & Camille household.
Replace every `replace_with_*` Home Assistant entity before storing the policy
in `DEVICE_SESSIONS` or `DEVICE_USERS`.

The Castalia content response may provide `day.aphorism`, detailed
`selfWeather`, detailed `synastryWeather`, and up to six `{name,status}` family
entries. Astrology and synastry are delivered in dedicated bounded segments so
multi-page readings do not exceed the X3 TLS receive window. Physical
weather comes from the allow-listed Home Assistant `weatherEntity`.

The primary private overlay is `castalia_device_daily_editions` in Castalia
Supabase, keyed by paired `individual` and issue date. Gazetteer publishes only
the bounded device fields to that RLS-protected table; full charts remain in
the private family archive. A policy-supplied `familyRepository` (or
`FAMILY_RHYTHM_REPO_MAP_JSON`) and read-only `FAMILY_RHYTHM_GITHUB_TOKEN`
remain a migration fallback. Credentials never leave the gateway.

The `scriptorium` segment reads `library/catalog.json` from the paired
individual's private repository (for example,
`CastaliaInstitute/castalia-dcmcshan`). Set `GITHUB_LIBRARY_TOKEN` to a
least-privilege token with read-only Contents access to those repositories.
Only catalog entries under `library/books/*.epub` are returned, and the token
remains gateway-side; it is never sent to the X3/X4. A session may override the
default repository with `libraryRepository`, but the gateway only accepts
repositories matching `CastaliaInstitute/castalia-*`.

For QR-paired Mynah sessions, set `DEVICE_USERS` to the same policy objects
keyed by Supabase user ID. The gateway validates the bearer JWT with Supabase
before selecting that policy. `DEVICE_SESSIONS` remains useful for development
and dedicated non-user device credentials.

`POST /api/v1/device/session/refresh` exchanges a device refresh token through
Supabase without embedding the public project key into firmware.

Kid sessions default to calendar title redaction unless
`calendarTitleMode` is explicitly `full`. They can never receive
`administration`, regardless of the configured permission.

Run the dependency-free contract tests with:

```sh
npm test
```

The Home Assistant calls use its documented REST calendar, entity-state, and
service endpoints. Home Assistant remains the owner of Tuya and Spotify
credentials; they are never stored on the Mynah.

Supported room-scoped actions are:

| Device action | Home Assistant service |
|---|---|
| `spotify.toggle` | `media_player.media_play_pause` |
| `spotify.previous` | `media_player.media_previous_track` |
| `spotify.next` | `media_player.media_next_track` |
| `lights.toggle` | `light.toggle` |
| `lights.dimmer` | `light.turn_on` with `brightness_step_pct: -10` |
| `lights.brighter` | `light.turn_on` with `brightness_step_pct: 10` |

Short side-button presses navigate faces. On Spotify and Lights, long side
presses invoke the previous/next or dimmer/brighter actions respectively.
