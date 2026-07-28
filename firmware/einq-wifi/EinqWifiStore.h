#pragma once

#include <cstddef>

/** WiFi credentials in ESP32 NVS (Preferences namespace "einq"). */
namespace EinqWifiStore {

struct Credentials {
  char ssid[33] {};
  char password[65] {};
  bool valid = false;
};

bool hasCredentials();

/** Load SSID/password into caller buffers. Returns false when unset. */
bool load(char* ssidOut, size_t ssidLen, char* passOut, size_t passLen);
bool loadPrimary(Credentials& out);
bool loadBackup(Credentials& out);

bool save(const char* ssid, const char* password);
bool saveNetworks(const char* primarySsid, const char* primaryPassword, const char* backupSsid,
                  const char* backupPassword);

void clear();

}  // namespace EinqWifiStore
