#include "EinqAuth.h"

#include <ArduinoJson.h>
#include <HalStorage.h>
#include <HTTPClient.h>
#include <Logging.h>
#include <NetworkClientSecure.h>
#include <WiFi.h>
#include <esp_attr.h>
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
constexpr uint32_t kPendingPairingMagic = 0x45494E51;
RTC_NOINIT_ATTR uint32_t gPendingPairingMagic;
RTC_NOINIT_ATTR EinqPairing gPendingPairing;

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

bool requestJson(const char* url, bool post, const char* body, JsonDocument& output, int* statusOut = nullptr) {
  LOG_INF("EINQ_AUTH", "request begin free=%u largest=%u",
          static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_INTERNAL)),
          static_cast<unsigned>(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL)));
  NetworkClientSecure client;
  client.setInsecure();
  HTTPClient http;
  if (!http.begin(client, url)) {
    LOG_ERR("EINQ_AUTH", "HTTP begin failed");
    return false;
  }
  http.setTimeout(20000);
  http.addHeader("Accept", "application/json");
  http.addHeader("User-Agent", "eInq-Castalia/1");
  int status;
  if (post) {
    http.addHeader("Content-Type", "application/json");
    status = http.POST(String(body == nullptr ? "{}" : body));
  } else {
    status = http.GET();
  }
  if (statusOut != nullptr) *statusOut = status;
  LOG_INF("EINQ_AUTH", "HTTP status=%d free=%u largest=%u", status,
          static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_INTERNAL)),
          static_cast<unsigned>(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL)));
  if (status < 200 || status >= 300) {
    LOG_ERR("EINQ_AUTH", "request failed: status=%d", status);
    http.end();
    return false;
  }
  const String response = http.getString();
  http.end();
  const DeserializationError parseError = deserializeJson(output, response);
  if (parseError) {
    LOG_ERR("EINQ_AUTH", "JSON failed len=%u: %s", static_cast<unsigned>(response.length()),
            parseError.c_str());
    return false;
  }
  LOG_INF("EINQ_AUTH", "JSON ok len=%u", static_cast<unsigned>(response.length()));
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

  // Preserve a live, single-use pairing across a soft reset. This lets a USB
  // host inspect the QR URL without causing the device to abandon that pair.
  if (gPendingPairingMagic == kPendingPairingMagic && gPendingPairing.valid &&
      gPendingPairing.pairId[0] != '\0' && gPendingPairing.pairSecret[0] != '\0' &&
      gPendingPairing.signInUrl[0] != '\0') {
    out = gPendingPairing;
    return true;
  }

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
  // Make the same short-lived URL shown in the QR available to a physically
  // connected USB host for desktop pairing and diagnostics.
  LOG_ERR("EINQ_PAIR_URL", "%s", signInUrl.c_str());
  copyField(out.signInUrl, sizeof(out.signInUrl), signInUrl.c_str());
  out.valid = out.signInUrl[0] != '\0';
  if (out.valid) {
    gPendingPairing = out;
    gPendingPairingMagic = kPendingPairingMagic;
  }
  return out.valid;
}

bool adoptPairing(const char* pairId, const char* pairSecret) {
  if (pairId == nullptr || pairId[0] == '\0' || pairSecret == nullptr || pairSecret[0] == '\0') {
    return false;
  }
  EinqPairing adopted {};
  copyField(adopted.pairId, sizeof(adopted.pairId), pairId);
  copyField(adopted.pairSecret, sizeof(adopted.pairSecret), pairSecret);
  std::string redirectPath = std::string("/auth/mynah-device/?pair=") + pairId +
                             "&key=" + encodeQuery(pairSecret) + "&device=einq";
  appendCastaliaIdentity(redirectPath);
  const std::string signInUrl = std::string(kSignInBase) + encodeQuery(redirectPath.c_str());
  copyField(adopted.signInUrl, sizeof(adopted.signInUrl), signInUrl.c_str());
  adopted.valid = adopted.signInUrl[0] != '\0';
  if (!adopted.valid) return false;
  gPendingPairing = adopted;
  gPendingPairingMagic = kPendingPairingMagic;
  return true;
}

PollResult pollPairing(const EinqPairing& pairing) {
  if (!pairing.valid || WiFi.status() != WL_CONNECTED) return PollResult::Error;
  // Repeat the short-lived desktop pairing URL while pending. Logging uses a
  // 256-byte ring entry, so split it without losing query parameters.
  constexpr int kUrlChunk = 180;
  const size_t urlLength = std::strlen(pairing.signInUrl);
  LOG_ERR("EINQ_PAIR_URL", "1:%.*s", kUrlChunk, pairing.signInUrl);
  LOG_ERR("EINQ_PAIR_URL", "2:%s", pairing.signInUrl + (urlLength < kUrlChunk ? urlLength : kUrlChunk));
  const std::string url = std::string(kPairingBase) + "/poll?pair_id=" + encodeQuery(pairing.pairId) +
                          "&pair_secret=" + encodeQuery(pairing.pairSecret);
  JsonDocument response;
  if (!requestJson(url.c_str(), false, nullptr, response)) return PollResult::Error;

  const char* status = response["status"] | "";
  if (std::strcmp(status, "pending") == 0) return PollResult::Pending;
  if (std::strcmp(status, "consumed") == 0 || std::strcmp(status, "unknown") == 0) {
    gPendingPairingMagic = 0;
    return PollResult::Expired;
  }
  if (std::strcmp(status, "ready") != 0) return PollResult::Error;

  const char* accessToken = response["access_token"] | "";
  const char* refreshToken = response["refresh_token"] | "";
  const long expiresIn = response["expires_in"] | 3600;
  if (!saveSession(accessToken, refreshToken, expiresIn)) return PollResult::Error;
  gPendingPairingMagic = 0;
  return PollResult::Ready;
}

bool installSession(const char* accessToken, const char* refreshToken, long expiresIn) {
  const bool saved = saveSession(accessToken, refreshToken, expiresIn);
  if (saved) gPendingPairingMagic = 0;
  return saved;
}

bool refreshIfNeeded(bool* refreshed) {
  if (refreshed != nullptr) *refreshed = false;
  JsonDocument session;
  if (!readSession(session)) return false;
  const char* accessToken = session["access_token"] | "";
  const char* refreshToken = session["refresh_token"] | "";
  if (accessToken[0] == '\0' || refreshToken[0] == '\0') return false;

  const uint64_t expiresAt = session["expires_at_ms"] | 0ULL;
  const time_t now = time(nullptr);
  // A session can be paired before NTP is available, in which case saveSession
  // intentionally records an unknown expiry (0). Once wall-clock time is
  // valid, refresh that session instead of treating the access token as valid
  // forever.
  // Only skip refresh when both the clock and the stored expiry are usable.
  // Pairing commonly finishes before NTP on X3, which stores expires_at_ms=0;
  // treating that state as perpetually valid eventually leaves the device
  // presenting an expired access token on every boot.
  if (now > 100000 && expiresAt != 0 &&
      static_cast<uint64_t>(now) * 1000ULL + kRefreshLeadMs < expiresAt) {
    return true;
  }
  const std::string gateway = configuredGateway();
  if (gateway.empty() || WiFi.status() != WL_CONNECTED) return false;

  JsonDocument request;
  request["refresh_token"] = refreshToken;
  String body;
  serializeJson(request, body);
  JsonDocument response;
  int refreshStatus = 0;
  const std::string url = gateway + "/api/v1/device/session/refresh";
  if (!requestJson(url.c_str(), true, body.c_str(), response, &refreshStatus)) {
    // An explicitly rejected refresh token cannot recover. Remove only that
    // invalid device session so the activity can reopen pairing; retain it for
    // transport and server errors where a later retry may succeed.
    if (refreshStatus == 401 || refreshStatus == 403) clearSession();
    return false;
  }
  const bool saved = saveSession(response["access_token"] | "", response["refresh_token"] | "",
                                 response["expires_in"] | 3600);
  if (saved && refreshed != nullptr) *refreshed = true;
  return saved;
}

bool clearSession() {
  if (!Storage.exists(kSessionPath)) return true;
  return Storage.remove(kSessionPath);
}

}  // namespace EinqAuth
