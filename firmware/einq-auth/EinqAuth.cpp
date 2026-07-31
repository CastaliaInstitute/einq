#include "EinqAuth.h"

#include <ArduinoJson.h>
#include <HalStorage.h>
#include <Logging.h>
#include <SecureHttpClient.h>
#include <WiFi.h>
#include <esp_heap_caps.h>

#include <cctype>
#include <ctime>
#include <cstring>
#include <string>

#include "einq-config/EinqConfigStore.h"

namespace {

constexpr char kPairingBase[] =
    "https://pilmscrodlitdrygabvo.supabase.co/functions/v1/mynah-castalia-link";
// Google OAuth is optional in Castalia's Supabase project. Use the regular
// sign-in page so its enabled email/password flow can complete the handoff.
constexpr char kSignInBase[] = "https://castalia.institute/auth/signin/?redirect=";
constexpr char kSessionPath[] = "/.einq/session.json";
constexpr uint64_t kRefreshLeadMs = 5 * 60 * 1000;

void copyField(char* destination, size_t length, const char* source) {
  if (length == 0) return;
  std::strncpy(destination, source == nullptr ? "" : source, length - 1);
  destination[length - 1] = '\0';
}

std::string encodeQuery(const char* value) {
  std::string encoded;
  constexpr char hex[] = "0123456789ABCDEF";
  if (value == nullptr) return encoded;
  for (const unsigned char byte : std::string(value)) {
    if (std::isalnum(byte) || byte == '-' || byte == '_' || byte == '.' || byte == '~') {
      encoded.push_back(static_cast<char>(byte));
    } else {
      encoded.push_back('%');
      encoded.push_back(hex[byte >> 4]);
      encoded.push_back(hex[byte & 0x0f]);
    }
  }
  return encoded;
}

bool readSession(JsonDocument& document) {
  const String session = Storage.readFile(kSessionPath);
  return !session.isEmpty() && !deserializeJson(document, session);
}

bool saveSession(const char* accessToken, const char* refreshToken, long expiresIn) {
  if (accessToken == nullptr || accessToken[0] == '\0' || refreshToken == nullptr || refreshToken[0] == '\0') {
    return false;
  }
  if (!Storage.exists("/.einq") && !Storage.mkdir("/.einq")) {
    return false;
  }
  if (expiresIn < 60) expiresIn = 3600;
  const time_t now = time(nullptr);
  const uint64_t expiresAt = now > 100000 ? (static_cast<uint64_t>(now) + expiresIn) * 1000ULL : 0;

  JsonDocument document;
  document["access_token"] = accessToken;
  document["refresh_token"] = refreshToken;
  document["expires_at_ms"] = expiresAt;
  String output;
  serializeJson(document, output);
  return Storage.writeFile(kSessionPath, output);
}

std::string configuredGateway() {
  JsonDocument config;
  if (deserializeJson(config, EinqConfigStore::load())) return {};
  std::string gateway = config["gatewayUrl"] | "";
  while (!gateway.empty() && gateway.back() == '/') gateway.pop_back();
  return gateway;
}

void appendCastaliaIdentity(std::string& redirectPath) {
  JsonDocument config;
  if (deserializeJson(config, EinqConfigStore::load())) return;
  const char* individual = config["individual"] | "dcmcshan";
  const char* repository = config["individualRepo"] | "CastaliaInstitute/castalia-dcmcshan";
  const char* settingsPath = config["settingsPath"] | "settings/faces.json";
  if (individual[0] != '\0') redirectPath += "&individual=" + encodeQuery(individual);
  if (repository[0] != '\0') redirectPath += "&repo=" + encodeQuery(repository);
  if (settingsPath[0] != '\0') redirectPath += "&settings=" + encodeQuery(settingsPath);
}

bool requestJson(const char* url, bool post, const char* body, JsonDocument& output) {
  LOG_INF("EINQ_AUTH", "request begin free=%u largest=%u",
          static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_INTERNAL)),
          static_cast<unsigned>(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL)));
  // The C3 has no PSRAM and cannot afford Arduino HTTPClient's mbedTLS peak
  // allocation here. FreeInk's wolfSSL client uses the same low-memory TLS
  // path as book/catalog downloads (including its compact X25519 key share).
  freeink::SecureHttpClient http;
  http.setInsecure();
  http.setTimeout(20000);
  http.setUserAgent("eInq-Castalia/1");
  if (!http.begin(url)) {
    LOG_ERR("EINQ_AUTH", "HTTP begin failed");
    return false;
  }
  http.addHeader("Accept", "application/json");
  int status;
  if (post) {
    http.addHeader("Content-Type", "application/json");
    status = http.POST(std::string(body == nullptr ? "{}" : body));
  } else {
    status = http.GET();
  }
  LOG_INF("EINQ_AUTH", "HTTP status=%d free=%u largest=%u", status,
          static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_INTERNAL)),
          static_cast<unsigned>(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL)));
  if (status < 200 || status >= 300) {
    LOG_ERR("EINQ_AUTH", "request failed: status=%d complete=%d", status,
            http.responseComplete());
    http.end();
    return false;
  }
  const std::string response = http.getString();
  const bool complete = http.responseComplete();
  http.end();
  if (!complete) {
    LOG_ERR("EINQ_AUTH", "incomplete response len=%u", static_cast<unsigned>(response.size()));
    return false;
  }
  const DeserializationError parseError = deserializeJson(output, response);
  if (parseError) {
    LOG_ERR("EINQ_AUTH", "JSON failed len=%u: %s", static_cast<unsigned>(response.size()),
            parseError.c_str());
    return false;
  }
  LOG_INF("EINQ_AUTH", "JSON ok len=%u", static_cast<unsigned>(response.size()));
  return true;
}

}  // namespace

namespace EinqAuth {

bool hasSession() {
  JsonDocument document;
  return readSession(document) && std::strlen(document["access_token"] | "") > 0 &&
         std::strlen(document["refresh_token"] | "") > 0;
}

bool startPairing(EinqPairing& out) {
  out = {};
  if (WiFi.status() != WL_CONNECTED) return false;

  JsonDocument response;
  const std::string url = std::string(kPairingBase) + "/start";
  if (!requestJson(url.c_str(), true, "{}", response)) return false;
  const char* pairId = response["pair_id"] | "";
  const char* pairSecret = response["pair_secret"] | "";
  if (pairId[0] == '\0' || pairSecret[0] == '\0') return false;

  copyField(out.pairId, sizeof(out.pairId), pairId);
  copyField(out.pairSecret, sizeof(out.pairSecret), pairSecret);
  std::string redirectPath =
      std::string("/auth/mynah-device/?pair=") + pairId + "&key=" + encodeQuery(pairSecret) + "&device=einq";
  appendCastaliaIdentity(redirectPath);
  const std::string signInUrl = std::string(kSignInBase) + encodeQuery(redirectPath.c_str());
  copyField(out.signInUrl, sizeof(out.signInUrl), signInUrl.c_str());
  out.valid = out.signInUrl[0] != '\0';
  return out.valid;
}

PollResult pollPairing(const EinqPairing& pairing) {
  if (!pairing.valid || WiFi.status() != WL_CONNECTED) return PollResult::Error;
  const std::string url = std::string(kPairingBase) + "/poll?pair_id=" + encodeQuery(pairing.pairId) +
                          "&pair_secret=" + encodeQuery(pairing.pairSecret);
  JsonDocument response;
  if (!requestJson(url.c_str(), false, nullptr, response)) return PollResult::Error;

  const char* status = response["status"] | "";
  if (std::strcmp(status, "pending") == 0) return PollResult::Pending;
  if (std::strcmp(status, "consumed") == 0 || std::strcmp(status, "unknown") == 0) {
    return PollResult::Expired;
  }
  if (std::strcmp(status, "ready") != 0) return PollResult::Error;

  const char* accessToken = response["access_token"] | "";
  const char* refreshToken = response["refresh_token"] | "";
  const long expiresIn = response["expires_in"] | 3600;
  return saveSession(accessToken, refreshToken, expiresIn) ? PollResult::Ready : PollResult::Error;
}

bool refreshIfNeeded() {
  JsonDocument session;
  if (!readSession(session)) return false;
  const char* accessToken = session["access_token"] | "";
  const char* refreshToken = session["refresh_token"] | "";
  if (accessToken[0] == '\0' || refreshToken[0] == '\0') return false;

  const uint64_t expiresAt = session["expires_at_ms"] | 0ULL;
  const time_t now = time(nullptr);
  if (expiresAt == 0 || now <= 100000 || static_cast<uint64_t>(now) * 1000ULL + kRefreshLeadMs < expiresAt) {
    return true;
  }
  const std::string gateway = configuredGateway();
  if (gateway.empty() || WiFi.status() != WL_CONNECTED) return false;

  JsonDocument request;
  request["refresh_token"] = refreshToken;
  String body;
  serializeJson(request, body);
  JsonDocument response;
  const std::string url = gateway + "/api/v1/device/session/refresh";
  if (!requestJson(url.c_str(), true, body.c_str(), response)) return false;
  return saveSession(response["access_token"] | "", response["refresh_token"] | "",
                     response["expires_in"] | 3600);
}

bool clearSession() {
  if (!Storage.exists(kSessionPath)) return true;
  return Storage.remove(kSessionPath);
}

}  // namespace EinqAuth
