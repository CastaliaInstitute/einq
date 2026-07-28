# Einq WiFi setup (captive portal)

Einq stores WiFi credentials in **ESP32 NVS** (not the SD card). Setup uses the same dual-QR pattern as CrossPoint’s hotspot mode: join the device AP, then open a settings page on your phone.

## Flow

```
┌─────────────┐   fallback AP: x3-XXXX       ┌──────────────┐
│ Mynah X3/X4 │ ───────────────────────────► │ Phone joins  │
│  (e-paper)  │      http://x3.local/         │ Mynah hotspot│
└──────┬──────┘         /settings            └──────┬───────┘
       │                                              │
       │  Soft AP + DNS captive portal + HTTP :80     │
       ▼                                              ▼
┌─────────────┐   POST /wifi (ssid, password)  ┌──────────────┐
│ EinqWifi    │ ◄───────────────────────────── │ Mobile form  │
│ Portal      │   → EinqWifiStore (NVS)        │ /settings    │
└─────────────┘                                └──────────────┘
```

1. The device tries the primary network for 15 seconds, then the backup.
2. If neither connects, it starts open AP **`x3-XXXX`** (`XXXX` = last 16 bits of MAC).
3. Captive DNS and `http://192.168.4.1/` open the embedded setup PWA.
4. **POST /wifi** saves both networks in NVS namespace `einq`.
5. On a home network, the same PWA is served read-only at
   **`http://x3.local/`** using mDNS.

WiFi and configuration writes are accepted only while the physical device is
in setup/AP mode. This prevents another client on the home LAN from replacing
the gateway URL or changing the parent/kid configuration.

## On device

| Entry | Action |
|-------|--------|
| **Long forward side button** | Open WiFi setup |
| **Short side button** | Move to the adjacent face |

**Back** cancels and stops the hotspot.

## Firmware modules

| Path | Role |
|------|------|
| `firmware/einq-wifi/EinqWifiStore.*` | NVS read/write |
| `firmware/einq-wifi/EinqWifiPortal.*` | AP, DNS, `/settings` HTML |
| `apps/clock-face/patch/EinqWifiSetupActivity.*` | E-paper QR UI |

The setup PWA is embedded in firmware; no external host is required.

## Migration from SD WiFi

CrossPoint’s `/.crosspoint/wifi.json` is **not** read by the Einq clock patch anymore. Re-run setup once via **Einq WiFi**, or use `scripts/setup-wifi-sd.py` only for upstream CrossPoint features (file transfer, etc.).

## Clearing credentials

Developers can erase NVS namespace `einq` with esptool/IDF. A parent-facing
factory-reset control is still pending.
