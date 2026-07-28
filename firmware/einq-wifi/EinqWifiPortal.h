#pragma once

#include <cstddef>
#include <string>

/**
 * Einq WiFi captive portal: soft AP, DNS redirect, and /settings form.
 * Credentials are written to NVS via EinqWifiStore on POST /wifi.
 */
namespace EinqWifiPortal {

struct Info {
  std::string apSsid;
  std::string apIp;
  std::string settingsUrl;
  std::string wifiQrPayload;
};

/** Start open AP + mDNS + DNS captive portal + HTTP server. */
bool start(Info& out);

/** Serve the same PWA on the connected LAN as http://x3.local/. */
bool startStation();

/** Process DNS and HTTP (call from activity loop). */
void loop();

bool isRunning();
bool isAccessPoint();

/** True after the phone submits SSID/password (saved to NVS). */
bool credentialsSaved();

void clearCredentialsSaved();

void stop();

}  // namespace EinqWifiPortal
