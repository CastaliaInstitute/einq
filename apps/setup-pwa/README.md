# Mynah eInq setup PWA

Responsive setup and configuration UI served by the X3/X4 captive portal and
by the hosted Castalia setup origin.

Run a local preview:

```bash
python3 -m http.server 4173 -d apps/setup-pwa
```

The preview falls back to defaults when `/api/v1/config` is unavailable. On
device, assets are embedded by `scripts/generate-setup-assets.py`.
