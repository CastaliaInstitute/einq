#include "EinqWifiStore.h"

#include <Preferences.h>
#include <WString.h>

#include <cstring>

namespace {

constexpr char kNamespace[] = "einq";
constexpr char kSsidKey[] = "wifi_ssid";
constexpr char kPassKey[] = "wifi_pass";
constexpr char kBackupSsidKey[] = "wifi_ssid_2";
constexpr char kBackupPassKey[] = "wifi_pass_2";

void copyOut(char* dest, size_t destLen, const String& src) {
  if (dest == nullptr || destLen == 0) {
    return;
  }
  strncpy(dest, src.c_str(), destLen - 1);
  dest[destLen - 1] = '\0';
}

bool loadPair(const char* ssidKey, const char* passKey, EinqWifiStore::Credentials& out) {
  out = {};
  Preferences prefs;
  if (!prefs.begin(kNamespace, true)) return false;
  const String ssid = prefs.getString(ssidKey, "");
  const String pass = prefs.getString(passKey, "");
  prefs.end();
  if (ssid.isEmpty()) return false;
  copyOut(out.ssid, sizeof(out.ssid), ssid);
  copyOut(out.password, sizeof(out.password), pass);
  out.valid = true;
  return true;
}

}  // namespace

namespace EinqWifiStore {

bool hasCredentials() {
  Preferences prefs;
  if (!prefs.begin(kNamespace, true)) {
    return false;
  }
  const bool ok = prefs.isKey(kSsidKey) && prefs.getString(kSsidKey, "").length() > 0;
  prefs.end();
  return ok;
}

bool load(char* ssidOut, size_t ssidLen, char* passOut, size_t passLen) {
  if (ssidOut == nullptr || ssidLen == 0) {
    return false;
  }

  Preferences prefs;
  if (!prefs.begin(kNamespace, true)) {
    return false;
  }

  const String ssid = prefs.getString(kSsidKey, "");
  const String pass = prefs.getString(kPassKey, "");
  prefs.end();

  if (ssid.isEmpty()) {
    return false;
  }

  copyOut(ssidOut, ssidLen, ssid);
  if (passOut != nullptr && passLen > 0) {
    copyOut(passOut, passLen, pass);
  }
  return true;
}

bool loadPrimary(Credentials& out) { return loadPair(kSsidKey, kPassKey, out); }

bool loadBackup(Credentials& out) { return loadPair(kBackupSsidKey, kBackupPassKey, out); }

bool save(const char* ssid, const char* password) {
  if (ssid == nullptr || ssid[0] == '\0') {
    return false;
  }

  Preferences prefs;
  if (!prefs.begin(kNamespace, false)) {
    return false;
  }

  const bool ok = prefs.putString(kSsidKey, ssid) > 0 && prefs.putString(kPassKey, password != nullptr ? password : "") > 0;
  prefs.end();
  return ok;
}

bool saveNetworks(const char* primarySsid, const char* primaryPassword, const char* backupSsid,
                  const char* backupPassword) {
  if (primarySsid == nullptr || primarySsid[0] == '\0') return false;
  Preferences prefs;
  if (!prefs.begin(kNamespace, false)) return false;
  bool ok = prefs.putString(kSsidKey, primarySsid) > 0 &&
            prefs.putString(kPassKey, primaryPassword == nullptr ? "" : primaryPassword) > 0;
  if (backupSsid != nullptr && backupSsid[0] != '\0') {
    ok = prefs.putString(kBackupSsidKey, backupSsid) > 0 &&
         prefs.putString(kBackupPassKey, backupPassword == nullptr ? "" : backupPassword) > 0 && ok;
  } else {
    prefs.remove(kBackupSsidKey);
    prefs.remove(kBackupPassKey);
  }
  prefs.end();
  return ok;
}

void clear() {
  Preferences prefs;
  if (!prefs.begin(kNamespace, false)) {
    return;
  }
  prefs.remove(kSsidKey);
  prefs.remove(kPassKey);
  prefs.remove(kBackupSsidKey);
  prefs.remove(kBackupPassKey);
  prefs.end();
}

}  // namespace EinqWifiStore
