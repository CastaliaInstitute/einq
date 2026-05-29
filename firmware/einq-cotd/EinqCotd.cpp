#include "EinqCotd.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <HalStorage.h>
#include <NetworkClientSecure.h>
#include <WiFi.h>

#include <cstring>

namespace {

constexpr char kCotdBaseUrl[] = "https://cards.castalia.institute/card-of-the-day/";

void copyField(char* dest, size_t destLen, const char* src) {
  if (src == nullptr) {
    dest[0] = '\0';
    return;
  }
  strncpy(dest, src, destLen - 1);
  dest[destLen - 1] = '\0';
}

bool parsePayload(const char* json, size_t len, EinqCotdCard& out) {
  JsonDocument doc;
  if (deserializeJson(doc, json, len) != DeserializationError::Ok) {
    return false;
  }

  copyField(out.date, sizeof(out.date), doc["date"] | "");
  const JsonObjectConst month = doc["month"].as<JsonObjectConst>();
  const JsonObjectConst card = doc["card"].as<JsonObjectConst>();
  if (card.isNull()) {
    return false;
  }

  copyField(out.title, sizeof(out.title), card["title"] | card["slug"] | "");
  copyField(out.slug, sizeof(out.slug), card["slug"] | "");
  copyField(out.domain, sizeof(out.domain), card["domain"] | "");
  copyField(out.topic, sizeof(out.topic), card["topic"] | month["topic"] | card["focus"] | "");

  out.season = card["season"] | month["season"] | 0;
  out.year = card["year"] | month["year"] | 0;
  out.valid = out.title[0] != '\0';
  return out.valid;
}

std::string cachePath(const char* dateYmd) {
  return std::string("/.einq/cotd/") + dateYmd + ".json";
}

bool ensureCacheDir() {
  if (!Storage.exists("/.einq")) {
    if (!Storage.mkdir("/.einq")) {
      return false;
    }
  }
  if (!Storage.exists("/.einq/cotd")) {
    return Storage.mkdir("/.einq/cotd");
  }
  return true;
}

bool readFileToString(const std::string& path, std::string& out) {
  FsFile file;
  if (!Storage.openFileForRead("COTD", path, file)) {
    return false;
  }
  out.clear();
  out.reserve(file.size());
  while (file.available()) {
    const int ch = file.read();
    if (ch < 0) {
      break;
    }
    out.push_back(static_cast<char>(ch));
  }
  file.close();
  return !out.empty();
}

bool writeStringToFile(const std::string& path, const std::string& body) {
  if (!ensureCacheDir()) {
    return false;
  }
  FsFile file;
  if (!Storage.openFileForWrite("COTD", path, file)) {
    return false;
  }
  const size_t written = file.write(reinterpret_cast<const uint8_t*>(body.data()), body.size());
  file.close();
  return written == body.size();
}

}  // namespace

namespace EinqCotd {

bool loadCached(const char* dateYmd, EinqCotdCard& out) {
  out = {};
  std::string body;
  if (!readFileToString(cachePath(dateYmd), body)) {
    return false;
  }
  return parsePayload(body.c_str(), body.size(), out);
}

bool fetchAndCache(const char* dateYmd, EinqCotdCard& out) {
  out = {};
  if (WiFi.status() != WL_CONNECTED) {
    return false;
  }

  const std::string url = std::string(kCotdBaseUrl) + dateYmd + ".json";
  NetworkClientSecure client;
  client.setInsecure();
  HTTPClient http;
  http.begin(client, url.c_str());
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  http.addHeader("User-Agent", "Einq-X4/1.4");
  http.setTimeout(15000);

  const int code = http.GET();
  if (code != HTTP_CODE_OK) {
    http.end();
    return false;
  }

  const std::string body = http.getString().c_str();
  http.end();
  if (body.empty()) {
    return false;
  }

  if (!parsePayload(body.c_str(), body.size(), out)) {
    return false;
  }

  writeStringToFile(cachePath(dateYmd), body);
  return true;
}

bool syncForDate(const char* dateYmd, EinqCotdCard& out) {
  if (WiFi.status() == WL_CONNECTED && fetchAndCache(dateYmd, out)) {
    return true;
  }
  return loadCached(dateYmd, out);
}

}  // namespace EinqCotd
