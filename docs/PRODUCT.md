# Mynah X3 and Mynah X4

## Product

Mynah X3 and Mynah X4 are Castalia-linked household devices based on the
Xteink X3 and X4 hardware. Castalia intends to work with Xteink on licensing
and product use. eInq is their quiet household console: it shows what matters
now and offers a few useful actions without becoming another
general-purpose screen.

Both devices install CrossPoint as their base system. eInq is the default
Mynah experience on boot, while CrossPoint remains available for reader
features, settings, file transfer, recovery, and board support. The product
should contribute clean integration points upstream where practical rather
than maintaining an unnecessarily deep fork.

One Mynah firmware build should support both devices when CrossPoint's board
abstraction permits it. Hardware-specific display, button, storage, and power
behavior belongs behind a board profile.

## Core experience

The home face is a glanceable summary:

- current time and the next calendar event
- inferred room and room-relevant light state
- one scheduled Castalia item: eINQ card, astrology, fortune, or reflection
- compact playback state when Spotify is active

Buttons open a shallow face carousel. A face may expose one primary action and
a small number of secondary actions. The e-paper display should never require
fast animation or continuous refresh.

| Face | Glance | Actions |
|------|--------|---------|
| Today | date, next event, household cue | next/previous event |
| eINQ | card title, glyph, prompt | another card, save |
| Astrolabe | daily astrology summary | detail page |
| Fortune | daily draw or oracle | draw, reveal |
| Lights | inferred room, lights on/off | toggle, scene, brightness steps |
| Spotify | track and playback state | play/pause, next, volume steps |
| Settings | profile, room confidence, sync | open setup, rescan room |

## Parent and kid profiles

Profiles are configuration and authorization policy, not separate firmware.
The server and device both enforce the policy.

| Capability | Parent | Kid |
|------------|--------|-----|
| Personal calendar | yes | curated family events |
| Household calendar | yes | yes |
| Light toggle / approved scenes | yes | yes |
| Edit rooms, devices, and scenes | yes | no |
| Spotify play/pause / approved content | yes | yes |
| Arbitrary Spotify search or account changes | optional | no |
| Astrology, fortunes, eINQ cards | full | age-appropriate catalog |
| Service connection and device administration | yes | no |

Kid mode must not expose calendar descriptions marked private, access tokens,
service credentials, setup administration, or unrestricted smart-home device
control. A local profile switch is insufficient for privilege escalation:
administrative changes require a parent session in the setup PWA.

## System boundary

The ESP32-C3 is a presentation and input client. OAuth, vendor secrets, token
refresh, service normalization, and policy enforcement belong in a Castalia
gateway.

```text
Mynah X3 / Mynah X4
  |-- CrossPoint: board support, reader, settings, recovery
  |-- eInq: default Castalia face and household apps
  |-- local: display, buttons, cache, BLE scan, room inference
  |-- HTTPS: compact device API
  `-- local HTTP: setup PWA

Castalia device gateway
  |-- Castalia sign-in and household policy
  |-- calendar provider
  |-- Spotify Web API
  |-- Home Assistant REST API
  |     `-- Tuya integration
  `-- eINQ, astrology, and fortune services
```

Use the existing `mynah-castalia-link` QR pairing flow. Reuse the
`mynah-tuya` architecture: Home Assistant owns the Tuya integration, while
Einq receives only allow-listed rooms, lights, scenes, and commands. Tuya
credentials must not be stored on the device.

Spotify control also goes through the gateway. Einq is a remote for an
existing Spotify Connect playback target; it is not an audio renderer.

## Setup PWA

The device tries its primary and backup networks, then creates `x3-XXXX` and
serves a captive setup PWA if neither connects. On the home network it remains
available read-only at `http://x3.local/`. Mutations require physical setup
mode so a LAN client cannot redirect device credentials or elevate local
configuration.

The PWA is installable when served from the Castalia HTTPS origin. Browsers do
not generally install service workers from an ESP32's plain HTTP LAN origin,
so the captive/local build remains a responsive web app with the same UI and
the hosted build supplies the manifest and offline shell.

Setup flow:

1. Select WiFi and verify connectivity.
2. Name the device and choose X3/X4 board profile if auto-detection is unsure.
3. Scan Castalia QR and assign the device to a household.
4. Choose parent or kid profile and, for kids, an age/content policy.
5. Connect calendar and Spotify through hosted OAuth.
6. Select the Home Assistant instance and allow-listed Tuya lights/scenes.
7. Add room beacons, walk the device into each room, and calibrate RSSI.
8. Preview faces, refresh cadence, quiet hours, and privacy settings.

Configuration is versioned JSON. Secret material is stored separately from
display preferences. The device API should expose:

```text
GET  /api/v1/device
GET  /api/v1/config
PUT  /api/v1/config
GET  /api/v1/wifi/scan
POST /api/v1/wifi
GET  /api/v1/rooms/scan
POST /api/v1/rooms/calibrate
POST /api/v1/pairing/start
GET  /api/v1/pairing/status
POST /api/v1/restart
```

Mutating local endpoints require a short-lived setup token shown on the
physical device. The captive portal may bootstrap WiFi without that token
only before the device has been claimed.

## Room inference with BLE RSSI

Small BLE beacons are assigned to rooms. The device scans in short windows,
smooths each beacon's RSSI, and picks the strongest calibrated room.

RSSI is noisy and should not be treated as distance. The resolver therefore:

- uses an exponentially weighted moving average per beacon
- ignores samples older than a configured TTL
- requires a minimum RSSI floor
- requires a lead over the runner-up before selecting a room
- requires repeated wins before switching away from the current room
- reports `unknown` rather than guessing at low confidence

Room inference remains on-device. The gateway may receive the selected room
and confidence for command routing, but raw scan histories are not uploaded by
default. Users can disable room history entirely.

WiFi and BLE share the ESP32-C3 radio. Scans should be short and scheduled
away from HTTPS sync. A practical starting point is a 2-second scan on user
wake, then a slower scan while the device is active; do not keep the radio on
during e-paper idle.

## Refresh and offline behavior

- Cache the last successful home payload and each face's compact data on SD.
- Fetch on explicit wake, meaningful room change, and a coarse schedule.
- Update only the changed e-paper region where supported.
- Queue light/playback commands briefly and show a clear pending/failure state.
- Calendar, eINQ, astrology, and fortune remain readable offline from cache.
- Controls that need the gateway are visibly unavailable when offline.

## Delivery order

1. Install current CrossPoint and verify its X3/X4 board profiles, display,
   buttons, SD, WiFi, BLE coexistence, sleep, and recovery paths.
2. Setup PWA shell, versioned configuration, and Castalia claim flow.
3. BLE room scanning, calibration, and room-aware home/light face.
4. Gateway home payload plus calendar and eINQ faces.
5. Home Assistant/Tuya light controls.
6. Spotify remote controls.
7. Astrology and fortune faces.
8. Kid policy, parent administration, privacy review, and field testing.

## First milestone

The first useful vertical slice is:

- boots on X3 and X4
- joins WiFi through the setup web app
- pairs with a Castalia household
- identifies one of two calibrated rooms
- shows the next calendar item and eINQ card
- toggles an allow-listed Home Assistant light in the inferred room

That milestone exercises every important boundary without requiring the full
face catalog.
