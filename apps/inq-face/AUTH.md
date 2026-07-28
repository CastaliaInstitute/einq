# Castalia sign-in (QR pairing)

Einq devices have no keyboard. **Scan a QR code with your phone** is the right way to sign in to Castalia and receive a session the device can use for personalized content (per-user card of the day, household settings, entitlements).

## Existing pattern (Pocket Mynah)

Castalia already ships this flow for the Mynah watch. Reuse it for Einq rather than inventing a second auth system.

```
┌─────────────┐     POST /start      ┌──────────────────────────┐
│  Einq X4    │ ──────────────────► │ mynah-castalia-link       │
│  (e-paper)  │ ◄── pair_id, secret │ (Supabase Edge Function)  │
└──────┬──────┘                     └────────────┬─────────────┘
       │ QR: castalia.institute/signin           │
       │     → redirect → /auth/mynah-device/    │
       ▼                                         │
┌─────────────┐     POST /complete              │
│  Phone      │ ────────────────────────────────►│
│  (Google)   │   access_token, refresh_token   │
└─────────────┘                                 │
       ▲                                         │
       │ GET /poll?pair_id&pair_secret          │
┌──────┴──────┐ ◄────────────────────────────────┘
│  Einq X4    │   status: ready → tokens (once)
└─────────────┘
```

### API (same as Mynah)

Base: `https://pilmscrodlitdrygabvo.supabase.co/functions/v1/mynah-castalia-link`

| Step | Method | Path | Who |
|------|--------|------|-----|
| Start pairing | `POST` | `/start` | Device |
| Poll for tokens | `GET` | `/poll?pair_id=&pair_secret=` | Device |
| Hand off session | `POST` | `/complete` | Phone (after Google sign-in) |

Pairing rows live in `mynah_device_pairings` (20-minute TTL). Implementation: [mynah/supabase/functions/mynah-castalia-link](https://github.com/CastaliaInstitute/mynah).

### Phone UX

After scan, user lands on Google sign-in, then a handoff page posts Supabase session tokens:

- Today: [castalia.institute/auth/mynah-device](https://castalia.institute/auth/mynah-device)
- Einq: add **`/auth/einq-device/`** (same logic, Einq-branded copy) — or reuse `mynah-device` with `?device=einq` for v0

Reference: `castalia.institute/app/auth/mynah-device/page.tsx`

### QR payload

Mynah encodes:

```
https://castalia.institute/auth/signin/?provider=google&redirect=/auth/mynah-device/?pair=UUID&key=SECRET
```

CrossPoint already has `QrUtils::drawQrCode()` — same approach on Einq.

## Einq firmware

Mirror [mynah/pocketwatch/.../pm_castalia_auth.cpp](https://github.com/CastaliaInstitute/mynah):

1. **Pairing screen** — Back or menu → “Sign in to Castalia” → `POST /start` → draw QR
2. **Poll loop** — every ~2s, `GET /poll` until `status: ready`
3. **Persist session** on SD at `/.einq/session.json`:

```json
{
  "access_token": "…",
  "refresh_token": "…",
  "expires_at_ms": 1717000000000
}
```

4. **Refresh** — before expiry, `POST` Supabase auth refresh (or re-pair)
5. **Use** — `Authorization: Bearer <access_token>` on personalized APIs

Store tokens only on SD (not repo). Never log tokens.

## What auth unlocks

| Feature | Without sign-in | With sign-in |
|---------|-----------------|--------------|
| Card of the day | Global editorial schedule | Per-user / per-household arc |
| Quotes, reminders | Public or cached | Entitlements, preferences |
| OTA | Anonymous (Pages manifest) | Unchanged |

Card API sketch (future):

```
GET https://cards.castalia.institute/api/cotd/today
Authorization: Bearer <access_token>
```

Until that exists, signed-in devices can still use the global `/card-of-the-day/YYYY-MM-DD.json` fallback.

## Security notes

- `pair_secret` is single-use, hashed server-side; poll consumes tokens
- Pairings expire in ~20 minutes
- QR is shown only on the physical device (short-lived)
- Prefer refresh tokens over long-lived access tokens on device

## Implementation status

- [x] `firmware/einq-auth/` starts and polls pairing, persists the session, refreshes through the Mynah gateway, and supplies bearer authentication.
- [x] `EinqAuthActivity` presents the QR and pairing status.
- [x] The QR uses the existing `auth/mynah-device` handoff with `device=einq`.
- [x] The home payload gateway provides personalized eINQ cards and household content.
- [ ] Deploy and configure the gateway/session policy for the production household.
