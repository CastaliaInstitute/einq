#include "EinqConfigStore.h"

#include <ArduinoJson.h>
#include <Preferences.h>

#include <cstring>

namespace {

constexpr char kNamespace[] = "einqcfg";
constexpr char kConfigKey[] = "config";
constexpr int kSchemaVersion = 1;
constexpr size_t kMaximumRooms = 12;
constexpr char kCastaliaGateway[] =
    "https://pilmscrodlitdrygabvo.supabase.co/functions/v1/mynah-gateway";

constexpr char kDefaultConfig[] =
    R"json({"schemaVersion":1,"deviceName":"Mynah eInq","board":"x3","profile":"parent","ageBand":"child","individual":"dcmcshan","individualRepo":"CastaliaInstitute/castalia-dcmcshan","settingsPath":"settings/faces.json","gatewayUrl":"https://pilmscrodlitdrygabvo.supabase.co/functions/v1/mynah-gateway","calendarId":"ca7a560e76044c59bbb72a70b98a21a774b99c2f5195eb7357ecd1a1cdf74344@group.calendar.google.com","features":{"calendar":true,"astrology":true,"fortune":true,"cards":true},"rooms":[],"roomMinimumLead":5})json";

bool isOneOf(const char* value, const char* first, const char* second) {
  return value != nullptr && (std::strcmp(value, first) == 0 || std::strcmp(value, second) == 0);
}

bool hasTextWithin(JsonVariantConst value, size_t maximumLength) {
  const char* text = value.as<const char*>();
  return text != nullptr && text[0] != '\0' && std::strlen(text) <= maximumLength;
}

bool normalize(const char* json, size_t length, std::string& output, std::string& error) {
  if (json == nullptr || length == 0 || length > EinqConfigStore::kMaximumJsonBytes) {
    error = "configuration must be 1-4096 bytes";
    return false;
  }

  JsonDocument input;
  const DeserializationError parseError = deserializeJson(input, json, length);
  if (parseError) {
    error = "invalid JSON";
    return false;
  }

  if ((input["schemaVersion"] | 0) != kSchemaVersion) {
    error = "unsupported schemaVersion";
    return false;
  }
  if (!hasTextWithin(input["deviceName"], 32)) {
    error = "deviceName must be 1-32 characters";
    return false;
  }

  const char* board = input["board"] | "";
  const char* profile = input["profile"] | "";
  const char* ageBand = input["ageBand"] | "child";
  if (!isOneOf(board, "x3", "x4")) {
    error = "board must be x3 or x4";
    return false;
  }
  if (!isOneOf(profile, "parent", "kid")) {
    error = "profile must be parent or kid";
    return false;
  }
  if (std::strcmp(ageBand, "child") != 0 && std::strcmp(ageBand, "tween") != 0 &&
      std::strcmp(ageBand, "teen") != 0) {
    error = "invalid ageBand";
    return false;
  }

  const int minimumLead = input["roomMinimumLead"] | 5;
  if (minimumLead < 2 || minimumLead > 15) {
    error = "roomMinimumLead must be 2-15";
    return false;
  }

  const JsonArrayConst rooms = input["rooms"].as<JsonArrayConst>();
  if (!rooms.isNull() && rooms.size() > kMaximumRooms) {
    error = "at most 12 rooms are supported";
    return false;
  }

  JsonDocument canonical;
  canonical["schemaVersion"] = kSchemaVersion;
  canonical["deviceName"] = input["deviceName"].as<const char*>();
  canonical["board"] = board;
  canonical["profile"] = profile;
  canonical["ageBand"] = ageBand;
  canonical["individual"] = input["individual"] | "dcmcshan";
  canonical["individualRepo"] = input["individualRepo"] | "CastaliaInstitute/castalia-dcmcshan";
  canonical["settingsPath"] = input["settingsPath"] | "settings/faces.json";
  const char* gateway = input["gatewayUrl"] | kCastaliaGateway;
  canonical["gatewayUrl"] =
      std::strcmp(gateway, "https://mynah.castalia.institute") == 0 ? kCastaliaGateway : gateway;
  const char* calendarId = input["calendarId"] | "";
  if (std::strlen(calendarId) > 192) {
    error = "calendarId must be at most 192 characters";
    return false;
  }
  canonical["calendarId"] = calendarId;

  JsonObject features = canonical["features"].to<JsonObject>();
  const JsonObjectConst inputFeatures = input["features"].as<JsonObjectConst>();
  features["calendar"] = inputFeatures["calendar"] | true;
  features["astrology"] = inputFeatures["astrology"] | true;
  features["fortune"] = inputFeatures["fortune"] | true;
  features["cards"] = inputFeatures["cards"] | true;

  JsonArray canonicalRooms = canonical["rooms"].to<JsonArray>();
  if (!rooms.isNull()) {
    for (const JsonObjectConst room : rooms) {
      if (!hasTextWithin(room["name"], 31) || !hasTextWithin(room["beaconId"], 63)) {
        error = "each room requires a name and beaconId";
        return false;
      }
      JsonObject next = canonicalRooms.add<JsonObject>();
      next["name"] = room["name"].as<const char*>();
      next["beaconId"] = room["beaconId"].as<const char*>();
      const int offset = room["calibrationOffset"] | 0;
      next["calibrationOffset"] = offset < -30 ? -30 : (offset > 30 ? 30 : offset);
    }
  }
  canonical["roomMinimumLead"] = minimumLead;

  output.clear();
  serializeJson(canonical, output);
  if (output.size() > EinqConfigStore::kMaximumJsonBytes) {
    error = "normalized configuration is too large";
    return false;
  }
  return true;
}

}  // namespace

namespace EinqConfigStore {

std::string load() {
  Preferences preferences;
  if (!preferences.begin(kNamespace, true)) {
    return kDefaultConfig;
  }
  const String stored = preferences.getString(kConfigKey, "");
  preferences.end();
  if (stored.isEmpty()) {
    return kDefaultConfig;
  }

  std::string canonical;
  std::string error;
  if (!normalize(stored.c_str(), stored.length(), canonical, error)) {
    return kDefaultConfig;
  }
  return canonical;
}

bool save(const char* json, size_t length, std::string& canonicalJson, std::string& error) {
  if (!normalize(json, length, canonicalJson, error)) {
    return false;
  }

  Preferences preferences;
  if (!preferences.begin(kNamespace, false)) {
    error = "configuration storage unavailable";
    return false;
  }
  const size_t written = preferences.putString(kConfigKey, canonicalJson.c_str());
  preferences.end();
  if (written != canonicalJson.size()) {
    error = "configuration write failed";
    return false;
  }
  return true;
}

void clear() {
  Preferences preferences;
  if (!preferences.begin(kNamespace, false)) {
    return;
  }
  preferences.remove(kConfigKey);
  preferences.end();
}

}  // namespace EinqConfigStore
