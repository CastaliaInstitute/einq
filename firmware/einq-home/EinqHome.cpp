#include "EinqHome.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <HalStorage.h>
#include <Logging.h>
#include <NetworkClientSecure.h>
#include <WiFi.h>
#include <esp_heap_caps.h>

#include <cctype>
#include <cstring>
#include <string>
#include <type_traits>

#include "einq-config/EinqConfigStore.h"

namespace {

constexpr char kLegacySupabaseGateway[] =
    "https://pilmscrodlitdrygabvo.supabase.co/functions/v1/mynah-gateway";
constexpr char kSupabaseGatewayHost[] = "pilmscrodlitdrygabvo.supabase.co";

constexpr char kCachePath[] = "/.einq/home.json";
constexpr char kCacheTempPath[] = "/.einq/home.tmp.json";
constexpr const char* kSegmentNames[] = {
    "core", "astrology-self", "astrology-synastry", "daily-1", "daily-2", "daily-3", "daily-4",
    "scriptorium"};
constexpr const char* kSegmentCachePaths[] = {
    kCachePath, "/.einq/home-astrology-self.json", "/.einq/home-astrology-synastry.json",
    "/.einq/home-daily-1.json", "/.einq/home-daily-2.json", "/.einq/home-daily-3.json",
    "/.einq/home-daily-4.json", "/.einq/home-scriptorium.json"};
constexpr const char* kSegmentTempPaths[] = {
    kCacheTempPath, "/.einq/home-astrology-self.tmp.json", "/.einq/home-astrology-synastry.tmp.json",
    "/.einq/home-daily-1.tmp.json", "/.einq/home-daily-2.tmp.json",
    "/.einq/home-daily-3.tmp.json", "/.einq/home-daily-4.tmp.json",
    "/.einq/home-scriptorium.tmp.json"};
constexpr size_t kSegmentCount = sizeof(kSegmentNames) / sizeof(kSegmentNames[0]);
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

bool parseDetailedReading(JsonObjectConst object, EinqHomeDetailedReading& output) {
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
  copyField(output.source, sizeof(output.source), firstText(object, "source", "detail"));
  output.valid = output.title[0] != '\0' || output.summary[0] != '\0';
  return output.valid;
}

bool parseDailyItem(JsonObjectConst object, EinqHomeDailyItem& output) {
  if (object.isNull()) return false;
  copyField(output.title, sizeof(output.title), object["title"] | "");
  copyField(output.summary, sizeof(output.summary), object["summary"] | "");
  copyField(output.byline, sizeof(output.byline), object["byline"] | "");
  copyField(output.detail, sizeof(output.detail), object["detail"] | "");
  output.valid = output.title[0] != '\0' || output.summary[0] != '\0';
  return output.valid;
}

bool parsePayload(const char* json, size_t length, EinqHomePayload& output, bool merge = false) {
  if (json == nullptr || length == 0 || length > kMaximumPayloadBytes) {
    return false;
  }

  JsonDocument document;
  if (deserializeJson(document, json, length)) {
    return false;
  }
  const bool recognizedSchema =
      std::strcmp(document["schema"] | "", "castalia.device.daily.v1") == 0;

  if (!merge) resetPayload(output);
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
  if (!parseDetailedReading(document["selfWeather"].as<JsonObjectConst>(), output.selfWeather)) {
    parseDetailedReading(document["astrology"].as<JsonObjectConst>(), output.selfWeather);
  }
  parseDetailedReading(document["synastryWeather"].as<JsonObjectConst>(), output.synastryWeather);
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
  const JsonObjectConst gazetteer = document["gazetteer"].as<JsonObjectConst>();
  if (!gazetteer.isNull()) {
    copyField(output.gazetteer.season, sizeof(output.gazetteer.season), gazetteer["season"] | "");
    copyField(output.gazetteer.theme, sizeof(output.gazetteer.theme), gazetteer["theme"] | "");
    const bool hasBook = parseDailyItem(gazetteer["book"].as<JsonObjectConst>(), output.gazetteer.book);
    const bool hasPoem = parseDailyItem(gazetteer["poem"].as<JsonObjectConst>(), output.gazetteer.poem);
    const bool hasFaculty = parseDailyItem(gazetteer["faculty"].as<JsonObjectConst>(), output.gazetteer.faculty);
    const bool hasHistory = parseDailyItem(gazetteer["history"].as<JsonObjectConst>(), output.gazetteer.history);
    const bool hasCountry = parseDailyItem(gazetteer["country"].as<JsonObjectConst>(), output.gazetteer.country);
    const bool hasBible = parseDailyItem(gazetteer["bible"].as<JsonObjectConst>(), output.gazetteer.bible);
    output.gazetteer.valid = hasBook || hasPoem || hasFaculty || hasHistory || hasCountry || hasBible;
  }
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
      if (output.codex.taskCount >= 8) break;
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
    copyField(output.library.repository, sizeof(output.library.repository), library["repository"] | "");
    copyField(output.library.revision, sizeof(output.library.revision), library["revision"] | "");
    output.library.bookCount = library["bookCount"] | 0U;
    output.library.changedCount = library["changedCount"] | 0U;
    const JsonArrayConst books = library["books"].as<JsonArrayConst>();
    for (const JsonObjectConst book : books) {
      if (output.library.listedCount >= 8) break;
      EinqHomeLibrary::Book& next = output.library.books[output.library.listedCount];
      copyField(next.id, sizeof(next.id), book["id"] | "");
      copyField(next.title, sizeof(next.title), book["title"] | "Untitled");
      copyField(next.authors, sizeof(next.authors), book["authors"] | "");
      next.valid = next.title[0] != '\0';
      if (next.valid) output.library.listedCount++;
    }
    output.library.valid = true;
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
  if (!permissions.isNull()) {
    output.permissions.calendar = permissions["calendar"] | true;
    output.permissions.astrology = permissions["astrology"] | true;
    output.permissions.fortune = permissions["fortune"] | true;
    output.permissions.cards = permissions["cards"] | true;
    output.permissions.spotifyControl = permissions["spotifyControl"] | false;
    output.permissions.lightControl = permissions["lightControl"] | false;
    output.permissions.codexControl = permissions["codexControl"] | false;
    output.permissions.administration = permissions["administration"] | false;
  }

  output.valid = output.nextEvent.valid || output.weather.valid || output.dayAphorism.valid ||
                 output.selfWeather.valid || output.synastryWeather.valid || output.familyCount > 0 ||
                 output.fortune.valid || output.card.valid || output.news.valid || output.art.valid ||
                 output.quote.valid || output.gazetteer.valid || output.mindfulness.valid || output.taskCount > 0 ||
                 output.library.valid || output.codex.valid || output.spotify.connected ||
                 output.lights.available;
  return output.valid || recognizedSchema;
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

void reset(EinqHomePayload& out) {
  resetPayload(out);
}

bool loadCached(EinqHomePayload& out) {
  const String content = Storage.readFile(kCachePath);
  if (content.isEmpty() || !parsePayload(content.c_str(), content.length(), out)) {
    resetPayload(out);
    return false;
  }
  for (size_t index = 1; index < kSegmentCount; ++index) {
    const String segment = Storage.readFile(kSegmentCachePaths[index]);
    if (!segment.isEmpty()) parsePayload(segment.c_str(), segment.length(), out, true);
  }
  applyLocalPolicy(out);
  out.fromCache = true;
  return true;
}

bool fetchAndCache(const char* room, EinqHomePayload& out) {
  resetPayload(out);
  if (WiFi.status() != WL_CONNECTED) {
    LOG_ERR("EINQ_HOME", "sync skipped: WiFi disconnected");
    return false;
  }

  const std::string gateway = configuredGateway();
  if (gateway.empty()) {
    LOG_ERR("EINQ_HOME", "sync skipped: gateway missing");
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

  const std::string token = accessToken();
  if (token.empty()) {
    LOG_ERR("EINQ_HOME", "sync request has no session token");
  }

  LOG_INF("EINQ_HOME", "request begin free=%u largest=%u",
          static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_INTERNAL)),
          static_cast<unsigned>(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL)));
  if (!ensureCacheDirectory()) {
    LOG_ERR("EINQ_HOME", "payload staging directory unavailable");
    return false;
  }
  size_t totalBytes = 0;
  for (size_t index = 0; index < kSegmentCount; ++index) {
    const std::string segmentUrl = url + (url.find('?') == std::string::npos ? "?" : "&") +
                                   "segment=" + kSegmentNames[index];

    // Supabase's edge may close keep-alive between bounded segments. Reusing
    // HTTPClient in that state can block indefinitely on ESP32-C3, so give
    // every segment an independent connection. Preconnect before HTTPClient
    // and the bearer header consume the X3's last large contiguous heap block.
    NetworkClientSecure client;
    client.setInsecure();
    client.setHandshakeTimeout(15);
    if (gateway == kLegacySupabaseGateway && !client.connect(kSupabaseGatewayHost, 443)) {
      char error[96] {};
      client.lastError(error, sizeof(error));
      LOG_ERR("EINQ_HOME", "TLS preconnect failed: segment=%s error=%s", kSegmentNames[index],
              error);
      return false;
    }
    HTTPClient http;
    const bool began = gateway == kLegacySupabaseGateway
                           ? http.begin(client, kSupabaseGatewayHost, 443,
                                        segmentUrl.c_str() +
                                            std::strlen("https://pilmscrodlitdrygabvo.supabase.co"),
                                        true)
                           : http.begin(client, segmentUrl.c_str());
    if (!began) {
      LOG_ERR("EINQ_HOME", "HTTP begin failed: segment=%s", kSegmentNames[index]);
      return false;
    }
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    // Personalized astrology can cold-start the Edge Function and perform
    // ephemeris/database work before returning its bounded response.
    http.setTimeout(45000);
    http.addHeader("Accept", "application/json");
    http.addHeader("Accept-Encoding", "identity");
    http.addHeader("User-Agent", "Mynah-eInq/1.4");
    if (!token.empty()) {
      const std::string authorization = std::string("Bearer ") + token;
      http.addHeader("Authorization", authorization.c_str());
    }

    LOG_INF("EINQ_HOME", "segment begin: name=%s free=%u largest=%u", kSegmentNames[index],
            static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_INTERNAL)),
            static_cast<unsigned>(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL)));

    const int status = http.GET();
    if (status != HTTP_CODE_OK) {
      LOG_ERR("EINQ_HOME", "sync request failed: segment=%s status=%d", kSegmentNames[index], status);
      http.end();
      return false;
    }
    const String body = http.getString();
    if (body.isEmpty() || body.length() > kMaximumPayloadBytes) {
      LOG_ERR("EINQ_HOME", "payload download failed: segment=%s bytes=%u", kSegmentNames[index],
              static_cast<unsigned>(body.length()));
      http.end();
      return false;
    }
    // Release the TLS buffers before ArduinoJson allocates its parse tree.
    // The body is already owned by String, and X3 otherwise has too little
    // contiguous heap to parse even a valid two-kilobyte core response.
    http.end();
    if (!parsePayload(body.c_str(), body.length(), out, index > 0)) {
      LOG_ERR("EINQ_HOME", "payload parse failed: segment=%s bytes=%u", kSegmentNames[index],
              static_cast<unsigned>(body.length()));
      return false;
    }
    const char* tempPath = kSegmentTempPaths[index];
    if (!Storage.writeFile(tempPath, body)) {
      LOG_ERR("EINQ_HOME", "payload staging write failed: segment=%s", kSegmentNames[index]);
      return false;
    }
    LOG_INF("EINQ_HOME", "segment ok: name=%s bytes=%u", kSegmentNames[index],
            static_cast<unsigned>(body.length()));
    totalBytes += body.length();
  }
  LOG_INF("EINQ_HOME", "payload ok: bytes=%u gazetteer=%s", static_cast<unsigned>(totalBytes),
          out.gazetteer.valid ? "yes" : "no");
  applyLocalPolicy(out);

  for (size_t index = 0; index < kSegmentCount; ++index) {
    if (Storage.exists(kSegmentCachePaths[index])) Storage.remove(kSegmentCachePaths[index]);
    if (!Storage.rename(kSegmentTempPaths[index], kSegmentCachePaths[index])) {
      LOG_ERR("EINQ_HOME", "payload cache promotion failed: segment=%s", kSegmentNames[index]);
      Storage.remove(kSegmentTempPaths[index]);
    }
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

bool sendAlResponse(const char* message, const char* room, EinqHomeAlReply& out) {
  out = EinqHomeAlReply {};
  if (message == nullptr || message[0] == '\0' || WiFi.status() != WL_CONNECTED) return false;
  const std::string gateway = configuredGateway();
  if (gateway.empty()) return false;

  JsonDocument document;
  document["action"] = "al.respond";
  document["message"] = message;
  if (room != nullptr && room[0] != '\0') document["room"] = room;
  String body;
  serializeJson(document, body);

  NetworkClientSecure client;
  client.setInsecure();
  HTTPClient http;
  if (!http.begin(client, (gateway + "/api/v1/device/actions").c_str())) return false;
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  http.setTimeout(20000);
  http.addHeader("Accept", "application/json");
  http.addHeader("Content-Type", "application/json");
  http.addHeader("User-Agent", "Mynah-eInq/1.4");
  const std::string token = accessToken();
  if (!token.empty()) http.addHeader("Authorization", (std::string("Bearer ") + token).c_str());

  const int status = http.POST(body);
  const String responseBody = status >= 200 && status < 300 ? http.getString() : String();
  http.end();
  if (status < 200 || status >= 300 || responseBody.isEmpty()) return false;

  JsonDocument response;
  if (deserializeJson(response, responseBody) != DeserializationError::Ok || !response["ok"].as<bool>()) {
    return false;
  }
  copyField(out.transcript, sizeof(out.transcript), response["transcript"] | "");
  copyField(out.reply, sizeof(out.reply), response["reply"] | "");
  out.valid = out.reply[0] != '\0';
  return out.valid;
}

}  // namespace EinqHome
