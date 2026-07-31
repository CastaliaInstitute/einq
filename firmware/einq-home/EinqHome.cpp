#include "EinqHome.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <HalStorage.h>
#include <NetworkClientSecure.h>
#include <WiFi.h>

#include <cctype>
#include <cstring>
#include <string>
#include <type_traits>

#include "einq-config/EinqConfigStore.h"

namespace {

constexpr char kCachePath[] = "/.einq/home.json";
constexpr char kSessionPath[] = "/.einq/session.json";
constexpr size_t kMaximumPayloadBytes = 16384;

void copyField(char* destination, size_t length, const char* source);

// EinqHomePayload is several kilobytes. Assigning `out = {}` makes GCC build a
// value-initialized temporary on Arduino's 16 KiB loopTask stack before copying
// it, which overflows during startup on X3/X4. Reset the POD fields in place and
// then restore the handful of non-zero defaults.
void resetPayload(EinqHomePayload& out) {
  static_assert(std::is_trivially_copyable_v<EinqHomePayload>);
  std::memset(&out, 0, sizeof(out));
  copyField(out.schema, sizeof(out.schema), "castalia.device.daily.v1");
  copyField(out.profile, sizeof(out.profile), "parent");
  out.permissions.calendar = true;
  out.permissions.astrology = true;
  out.permissions.fortune = true;
  out.permissions.cards = true;
}

void copyField(char* destination, size_t length, const char* source) {
  if (destination == nullptr || length == 0) {
    return;
  }
  if (source == nullptr) {
    destination[0] = '\0';
    return;
  }
  std::strncpy(destination, source, length - 1);
  destination[length - 1] = '\0';
}

bool parseReading(JsonObjectConst object, EinqHomeReading& output) {
  if (object.isNull()) {
    return false;
  }
  copyField(output.title, sizeof(output.title), object["title"] | "");
  copyField(output.summary, sizeof(output.summary), object["summary"] | "");
  output.valid = output.title[0] != '\0' || output.summary[0] != '\0';
  return output.valid;
}

const char* firstText(JsonObjectConst object, const char* first, const char* second,
                      const char* third = nullptr) {
  const char* value = object[first] | "";
  if (value[0] != '\0') return value;
  value = object[second] | "";
  if (value[0] != '\0' || third == nullptr) return value;
  return object[third] | "";
}

bool parseAttribution(JsonObjectConst object, EinqHomeAttribution& output) {
  if (object.isNull()) return false;
  copyField(output.title, sizeof(output.title), object["title"] | "");
  copyField(output.summary, sizeof(output.summary), firstText(object, "summary", "text"));
  copyField(output.byline, sizeof(output.byline), firstText(object, "byline", "artist", "author"));
  copyField(output.source, sizeof(output.source), object["source"] | "");
  copyField(output.assetUrl, sizeof(output.assetUrl), firstText(object, "assetUrl", "image"));
  copyField(output.sha256, sizeof(output.sha256), object["sha256"] | "");
  output.valid = output.title[0] != '\0' || output.summary[0] != '\0';
  return output.valid;
}

bool parsePayload(const char* json, size_t length, EinqHomePayload& output) {
  if (json == nullptr || length == 0 || length > kMaximumPayloadBytes) {
    return false;
  }

  JsonDocument document;
  if (deserializeJson(document, json, length)) {
    return false;
  }

  resetPayload(output);
  copyField(output.schema, sizeof(output.schema), document["schema"] | "castalia.device.daily.v1");
  copyField(output.date, sizeof(output.date), document["date"] | "");
  copyField(output.generatedAt, sizeof(output.generatedAt), document["generatedAt"] | "");
  copyField(output.profile, sizeof(output.profile), document["profile"] | "parent");

  const JsonObjectConst event = document["today"]["nextEvent"].as<JsonObjectConst>();
  if (!event.isNull()) {
    copyField(output.nextEvent.title, sizeof(output.nextEvent.title), event["title"] | "");
    copyField(output.nextEvent.start, sizeof(output.nextEvent.start), event["start"] | "");
    copyField(output.nextEvent.end, sizeof(output.nextEvent.end), event["end"] | "");
    output.nextEvent.allDay = event["allDay"] | false;
    output.nextEvent.valid = output.nextEvent.title[0] != '\0';
  }

  const JsonObjectConst weather = document["weather"].as<JsonObjectConst>();
  if (!weather.isNull()) {
    copyField(output.weather.condition, sizeof(output.weather.condition), weather["condition"] | "");
    copyField(output.weather.temperature, sizeof(output.weather.temperature), weather["temperature"] | "");
    copyField(output.weather.summary, sizeof(output.weather.summary), weather["summary"] | "");
    output.weather.valid = output.weather.condition[0] != '\0' || output.weather.temperature[0] != '\0';
  }

  parseReading(document["day"]["aphorism"].as<JsonObjectConst>(), output.dayAphorism);
  if (!parseReading(document["selfWeather"].as<JsonObjectConst>(), output.selfWeather)) {
    parseReading(document["astrology"].as<JsonObjectConst>(), output.selfWeather);
  }
  parseReading(document["synastryWeather"].as<JsonObjectConst>(), output.synastryWeather);
  const JsonArrayConst family = document["family"].as<JsonArrayConst>();
  for (const JsonObjectConst member : family) {
    if (output.familyCount >= 6) break;
    EinqHomeFamilyMember& next = output.family[output.familyCount];
    copyField(next.name, sizeof(next.name), member["name"] | "");
    copyField(next.status, sizeof(next.status), member["status"] | "");
    next.valid = next.name[0] != '\0' && next.status[0] != '\0';
    if (next.valid) output.familyCount++;
  }
  parseReading(document["fortune"].as<JsonObjectConst>(), output.fortune);
  parseAttribution(document["news"].as<JsonObjectConst>(), output.news);
  parseAttribution(document["art"].as<JsonObjectConst>(), output.art);
  parseAttribution(document["quote"].as<JsonObjectConst>(), output.quote);
  parseReading(document["mindfulness"].as<JsonObjectConst>(), output.mindfulness);

  const JsonArrayConst tasks = document["today"]["tasks"].as<JsonArrayConst>();
  for (const JsonObjectConst task : tasks) {
    if (output.taskCount >= 6) break;
    EinqHomeTask& next = output.tasks[output.taskCount];
    copyField(next.title, sizeof(next.title), task["title"] | "");
    copyField(next.due, sizeof(next.due), task["due"] | "");
    next.completed = task["completed"] | false;
    next.valid = next.title[0] != '\0';
    if (next.valid) output.taskCount++;
  }

  const JsonObjectConst codex = document["codex"].as<JsonObjectConst>();
  if (!codex.isNull()) {
    output.codex.revision = codex["revision"] | 0U;
    output.codex.selectedIndex = codex["selectedIndex"] | 0U;
    const JsonArrayConst codexTasks = codex["tasks"].as<JsonArrayConst>();
    for (const JsonObjectConst task : codexTasks) {
      if (output.codex.taskCount >= 12) break;
      EinqHomeCodexTask& next = output.codex.tasks[output.codex.taskCount];
      copyField(next.id, sizeof(next.id), task["id"] | "");
      copyField(next.host, sizeof(next.host), firstText(task, "host", "hostName", "hostId"));
      copyField(next.title, sizeof(next.title), task["title"] | "");
      copyField(next.model, sizeof(next.model), task["model"] | "");
      copyField(next.speed, sizeof(next.speed), task["speed"] | "");
      copyField(next.status, sizeof(next.status), task["status"] | "idle");
      next.pinned = task["pinned"] | false;
      const JsonObjectConst usage = task["usage"].as<JsonObjectConst>();
      next.totalTokens = usage["totalTokens"] | 0U;
      next.rateTokensPerMinute = usage["rateTokensPerMinute"] | 0U;
      next.valid = next.id[0] != '\0' && next.title[0] != '\0';
      if (next.valid) output.codex.taskCount++;
    }
    if (output.codex.taskCount > 0) {
      if (output.codex.selectedIndex >= output.codex.taskCount) output.codex.selectedIndex = 0;
      output.codex.valid = true;
    }
  }

  const JsonObjectConst library = document["library"].as<JsonObjectConst>();
  if (!library.isNull()) {
    copyField(output.library.catalogUrl, sizeof(output.library.catalogUrl), library["catalogUrl"] | "");
    copyField(output.library.revision, sizeof(output.library.revision), library["revision"] | "");
    output.library.bookCount = library["bookCount"] | 0U;
    output.library.changedCount = library["changedCount"] | 0U;
    output.library.valid = output.library.catalogUrl[0] != '\0' || output.library.bookCount > 0;
  }

  const JsonObjectConst card = document["card"].as<JsonObjectConst>();
  if (!card.isNull()) {
    copyField(output.card.title, sizeof(output.card.title), card["title"] | "");
    copyField(output.card.summary, sizeof(output.card.summary), card["summary"] | "");
    copyField(output.card.domain, sizeof(output.card.domain), card["domain"] | "");
    output.card.valid = output.card.title[0] != '\0';
  }

  const JsonObjectConst spotify = document["spotify"].as<JsonObjectConst>();
  if (!spotify.isNull()) {
    copyField(output.spotify.track, sizeof(output.spotify.track), spotify["track"] | "");
    copyField(output.spotify.artist, sizeof(output.spotify.artist), spotify["artist"] | "");
    copyField(output.spotify.device, sizeof(output.spotify.device), spotify["device"] | "");
    output.spotify.volume = spotify["volume"] | 0;
    output.spotify.connected = spotify["connected"] | false;
    output.spotify.playing = spotify["playing"] | false;
  }

  const JsonObjectConst lights = document["lights"].as<JsonObjectConst>();
  if (!lights.isNull()) {
    copyField(output.lights.room, sizeof(output.lights.room), lights["room"] | "");
    copyField(output.lights.scene, sizeof(output.lights.scene), lights["scene"] | "");
    output.lights.brightness = lights["brightness"] | 0;
    output.lights.available = lights["available"] | false;
    output.lights.on = lights["on"] | false;
  }

  const JsonObjectConst permissions = document["permissions"].as<JsonObjectConst>();
  output.permissions.calendar = permissions["calendar"] | true;
  output.permissions.astrology = permissions["astrology"] | true;
  output.permissions.fortune = permissions["fortune"] | true;
  output.permissions.cards = permissions["cards"] | true;
  output.permissions.spotifyControl = permissions["spotifyControl"] | false;
  output.permissions.lightControl = permissions["lightControl"] | false;
  output.permissions.codexControl = permissions["codexControl"] | false;
  output.permissions.administration = permissions["administration"] | false;

  output.valid = output.nextEvent.valid || output.weather.valid || output.dayAphorism.valid ||
                 output.selfWeather.valid || output.synastryWeather.valid || output.familyCount > 0 ||
                 output.fortune.valid || output.card.valid || output.news.valid || output.art.valid ||
                 output.quote.valid || output.mindfulness.valid || output.taskCount > 0 ||
                 output.library.valid || output.codex.valid || output.spotify.connected ||
                 output.lights.available;
  return output.valid;
}

bool ensureCacheDirectory() {
  return Storage.exists("/.einq") || Storage.mkdir("/.einq");
}

std::string configuredGateway() {
  JsonDocument config;
  if (deserializeJson(config, EinqConfigStore::load())) {
    return {};
  }
  std::string gateway = config["gatewayUrl"] | "";
  while (!gateway.empty() && gateway.back() == '/') {
    gateway.pop_back();
  }
  return gateway;
}

std::string configuredCalendar() {
  JsonDocument config;
  if (deserializeJson(config, EinqConfigStore::load())) return {};
  return config["calendarId"] | "";
}

std::string accessToken() {
  const String session = Storage.readFile(kSessionPath);
  if (session.isEmpty()) {
    return {};
  }
  JsonDocument document;
  if (deserializeJson(document, session)) {
    return {};
  }
  return document["access_token"] | "";
}

void applyLocalPolicy(EinqHomePayload& payload) {
  JsonDocument config;
  if (deserializeJson(config, EinqConfigStore::load())) {
    return;
  }

  const JsonObjectConst features = config["features"].as<JsonObjectConst>();
  payload.permissions.calendar &= features["calendar"] | true;
  payload.permissions.astrology &= features["astrology"] | true;
  payload.permissions.fortune &= features["fortune"] | true;
  payload.permissions.cards &= features["cards"] | true;

  const char* localProfile = config["profile"] | "parent";
  if (std::strcmp(localProfile, "kid") == 0) {
    copyField(payload.profile, sizeof(payload.profile), "kid");
    payload.permissions.administration = false;
  }
}

std::string encodeQuery(const char* value) {
  std::string encoded;
  if (value == nullptr) {
    return encoded;
  }
  constexpr char hex[] = "0123456789ABCDEF";
  for (const unsigned char byte : std::string(value)) {
    if (std::isalnum(byte) || byte == '-' || byte == '_' || byte == '.') {
      encoded.push_back(static_cast<char>(byte));
    } else {
      encoded.push_back('%');
      encoded.push_back(hex[byte >> 4]);
      encoded.push_back(hex[byte & 0x0f]);
    }
  }
  return encoded;
}

}  // namespace

namespace EinqHome {

bool loadCached(EinqHomePayload& out) {
  const String content = Storage.readFile(kCachePath);
  if (content.isEmpty() || !parsePayload(content.c_str(), content.length(), out)) {
    resetPayload(out);
    return false;
  }
  applyLocalPolicy(out);
  out.fromCache = true;
  return true;
}

bool fetchAndCache(const char* room, EinqHomePayload& out) {
  resetPayload(out);
  if (WiFi.status() != WL_CONNECTED) {
    return false;
  }

  const std::string gateway = configuredGateway();
  if (gateway.empty()) {
    return false;
  }
  std::string url = gateway + "/api/v1/device/daily";
  bool hasQuery = false;
  if (room != nullptr && room[0] != '\0') {
    url += "?room=" + encodeQuery(room);
    hasQuery = true;
  }
  const std::string calendar = configuredCalendar();
  if (!calendar.empty()) {
    url += hasQuery ? "&calendar=" : "?calendar=";
    url += encodeQuery(calendar.c_str());
  }

  NetworkClientSecure client;
  client.setInsecure();
  HTTPClient http;
  if (!http.begin(client, url.c_str())) {
    return false;
  }
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  http.setTimeout(15000);
  http.addHeader("Accept", "application/json");
  http.addHeader("User-Agent", "Mynah-eInq/1.4");
  const std::string token = accessToken();
  if (!token.empty()) {
    http.addHeader("Authorization", (std::string("Bearer ") + token).c_str());
  }

  const int status = http.GET();
  if (status != HTTP_CODE_OK) {
    http.end();
    return false;
  }
  const String body = http.getString();
  http.end();
  if (!parsePayload(body.c_str(), body.length(), out)) {
    return false;
  }
  applyLocalPolicy(out);

  if (ensureCacheDirectory()) {
    Storage.writeFile(kCachePath, body);
  }
  return true;
}

bool sync(const char* room, EinqHomePayload& out) {
  if (fetchAndCache(room, out)) {
    return true;
  }
  return loadCached(out);
}

bool sendAction(const char* action, const char* room) {
  return sendAction(action, room, nullptr);
}

bool sendAction(const char* action, const char* room, const char* taskId) {
  if (action == nullptr || action[0] == '\0' || WiFi.status() != WL_CONNECTED) {
    return false;
  }

  const std::string gateway = configuredGateway();
  if (gateway.empty()) {
    return false;
  }

  JsonDocument document;
  document["action"] = action;
  if (room != nullptr && room[0] != '\0') {
    document["room"] = room;
  }
  if (taskId != nullptr && taskId[0] != '\0') {
    document["taskId"] = taskId;
  }
  String body;
  serializeJson(document, body);

  NetworkClientSecure client;
  client.setInsecure();
  HTTPClient http;
  if (!http.begin(client, (gateway + "/api/v1/device/actions").c_str())) {
    return false;
  }
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  http.setTimeout(15000);
  http.addHeader("Accept", "application/json");
  http.addHeader("Content-Type", "application/json");
  http.addHeader("User-Agent", "Mynah-eInq/1.4");
  const std::string token = accessToken();
  if (!token.empty()) {
    http.addHeader("Authorization", (std::string("Bearer ") + token).c_str());
  }

  const int status = http.POST(body);
  http.end();
  return status >= 200 && status < 300;
}

}  // namespace EinqHome
