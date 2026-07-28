# Mynah Castalia pairing

`EinqAuth` reuses Pocket Mynah's deployed `mynah-castalia-link` QR flow:

1. `POST /start` creates a 20-minute single-use pairing.
2. The X3/X4 displays the Castalia sign-in URL as a QR code.
3. The device polls `/poll` until the phone completes sign-in.
4. Tokens are stored only at `/.einq/session.json` on the SD card.
5. Near expiry, the device uses the configured Mynah gateway's
   `/api/v1/device/session/refresh` endpoint.

The gateway proxies refresh so the Supabase project key does not need to be
compiled into the firmware.
