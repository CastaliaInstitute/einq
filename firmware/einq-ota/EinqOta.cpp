#include "EinqOta.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <Logging.h>
#include <NetworkClientSecure.h>
#include <WiFi.h>
#include <esp_crt_bundle.h>
#include <esp_http_client.h>
#include <esp_https_ota.h>
#include <esp_wifi.h>

#include <cstring>
#include <string>

namespace {

esp_err_t httpClientSetHeader(esp_http_client_handle_t httpClient) {
  return esp_http_client_set_header(httpClient, "User-Agent", "Einq-X4-" CROSSPOINT_VERSION);
}

bool parseSemver(const char* version, int& major, int& minor, int& patch) {
  if (version == nullptr || version[0] == '\0') {
    return false;
  }
  return sscanf(version, "%d.%d.%d", &major, &minor, &patch) == 3;
}

void copyField(char* dest, size_t destLen, const char* src) {
  if (src == nullptr) {
    dest[0] = '\0';
    return;
  }
  strncpy(dest, src, destLen - 1);
  dest[destLen - 1] = '\0';
}

}  // namespace

namespace EinqOta {

bool isNewerThanRunning(const char* latestVersion) {
  if (latestVersion == nullptr || latestVersion[0] == '\0') {
    return false;
  }

  const char* currentVersion = CROSSPOINT_VERSION;
  if (strcmp(latestVersion, currentVersion) == 0) {
    return false;
  }

  int latestMajor = 0;
  int latestMinor = 0;
  int latestPatch = 0;
  int currentMajor = 0;
  int currentMinor = 0;
  int currentPatch = 0;
  if (!parseSemver(latestVersion, latestMajor, latestMinor, latestPatch) ||
      !parseSemver(currentVersion, currentMajor, currentMinor, currentPatch)) {
    return false;
  }

  if (latestMajor != currentMajor) {
    return latestMajor > currentMajor;
  }
  if (latestMinor != currentMinor) {
    return latestMinor > currentMinor;
  }
  if (latestPatch != currentPatch) {
    return latestPatch > currentPatch;
  }

  if (strstr(currentVersion, "-rc") != nullptr) {
    return true;
  }
  return false;
}

Result fetchManifest(char* versionOut, size_t versionLen, char* urlOut, size_t urlLen, size_t* sizeOut) {
  if (WiFi.status() != WL_CONNECTED) {
    return Result::HttpError;
  }

  NetworkClientSecure client;
  client.setInsecure();
  HTTPClient http;
  http.begin(client, kManifestUrl);
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  http.addHeader("User-Agent", "Einq-X4-" CROSSPOINT_VERSION);
  http.setTimeout(15000);

  const int code = http.GET();
  if (code != HTTP_CODE_OK) {
    LOG_DBG("EOTA", "manifest HTTP %d", code);
    http.end();
    return Result::HttpError;
  }

  const std::string body = http.getString().c_str();
  http.end();
  if (body.empty()) {
    return Result::ParseError;
  }

  JsonDocument doc;
  if (deserializeJson(doc, body) != DeserializationError::Ok) {
    return Result::ParseError;
  }

  const char* version = doc["version"] | "";
  if (version[0] == '\0') {
    return Result::ParseError;
  }

  copyField(versionOut, versionLen, version);
  const char* url = doc["url"] | kDefaultFirmwareUrl;
  copyField(urlOut, urlLen, url);
  if (sizeOut != nullptr) {
    *sizeOut = doc["size"] | 0;
  }
  return Result::Ok;
}

Result installFromUrl(const char* url) {
  if (url == nullptr || url[0] == '\0') {
    return Result::InstallError;
  }

  esp_https_ota_handle_t otaHandle = nullptr;
  esp_http_client_config_t clientConfig = {
      .url = url,
      .timeout_ms = 60000,
      .buffer_size = 8192,
      .buffer_size_tx = 8192,
      .skip_cert_common_name_check = true,
      .crt_bundle_attach = esp_crt_bundle_attach,
      .keep_alive_enable = true,
  };

  esp_https_ota_config_t otaConfig = {
      .http_config = &clientConfig,
      .http_client_init_cb = httpClientSetHeader,
  };

  esp_wifi_set_ps(WIFI_PS_NONE);

  esp_err_t err = esp_https_ota_begin(&otaConfig, &otaHandle);
  if (err != ESP_OK) {
    LOG_ERR("EOTA", "ota begin: %s", esp_err_to_name(err));
    esp_wifi_set_ps(WIFI_PS_MIN_MODEM);
    return Result::InstallError;
  }

  do {
    err = esp_https_ota_perform(otaHandle);
    delay(50);
  } while (err == ESP_ERR_HTTPS_OTA_IN_PROGRESS);

  esp_wifi_set_ps(WIFI_PS_MIN_MODEM);

  if (err != ESP_OK) {
    LOG_ERR("EOTA", "ota perform: %s", esp_err_to_name(err));
    esp_https_ota_finish(otaHandle);
    return Result::InstallError;
  }

  if (!esp_https_ota_is_complete_data_received(otaHandle)) {
    LOG_ERR("EOTA", "incomplete OTA image");
    esp_https_ota_finish(otaHandle);
    return Result::InstallError;
  }

  err = esp_https_ota_finish(otaHandle);
  if (err != ESP_OK) {
    LOG_ERR("EOTA", "ota finish: %s", esp_err_to_name(err));
    return Result::InstallError;
  }

  LOG_INF("EOTA", "OTA complete, rebooting");
  return Result::Ok;
}

Result checkAndInstallIfNewer() {
  char version[24];
  char url[160];
  size_t size = 0;
  const Result manifest = fetchManifest(version, sizeof(version), url, sizeof(url), &size);
  if (manifest != Result::Ok) {
    return manifest;
  }

  if (!isNewerThanRunning(version)) {
    LOG_DBG("EOTA", "up to date (%s)", CROSSPOINT_VERSION);
    return Result::NoUpdate;
  }

  LOG_INF("EOTA", "installing %s (current %s)", version, CROSSPOINT_VERSION);
  const Result installed = installFromUrl(url);
  if (installed != Result::Ok) {
    return Result::InstallError;
  }
  return Result::Ok;
}

}  // namespace EinqOta
