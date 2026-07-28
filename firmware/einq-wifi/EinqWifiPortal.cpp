#include "EinqWifiPortal.h"

#include "EinqSetupAssets.h"
#include "EinqWifiStore.h"
#include "einq-config/EinqConfigStore.h"

#include <ArduinoJson.h>
#include <DNSServer.h>
#include <ESPmDNS.h>
#include <WebServer.h>
#include <WiFi.h>

#include <memory>
#include <cstdio>

namespace {

constexpr char kHostname[] = "x3";
constexpr uint8_t kApChannel = 1;
constexpr uint8_t kApMaxClients = 4;
constexpr uint16_t kDnsPort = 53;

constexpr char kSuccessHtml[] PROGMEM = R"HTML(<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Saved</title>
<style>
  body { font-family: system-ui, sans-serif; max-width: 28rem; margin: 3rem auto; padding: 0 1rem; text-align: center; }
  h1 { font-size: 1.35rem; }
</style>
</head>
<body>
<h1>WiFi saved</h1>
<p>Einq will join your network. You can disconnect from the Einq hotspot and close this page.</p>
</body>
</html>)HTML";

std::unique_ptr<WebServer> server;
std::unique_ptr<DNSServer> dnsServer;
bool running = false;
bool saved = false;
bool accessPoint = false;

bool requireSetupMode() {
  if (accessPoint) return true;
  server->send(403, "text/plain", "Open WiFi setup on the Mynah to make changes");
  return false;
}

void sendSettingsPage() {
  server->send_P(200, EinqSetupAssets::kIndexMime, EinqSetupAssets::kIndex);
}

void sendCss() {
  server->send_P(200, EinqSetupAssets::kCssMime, EinqSetupAssets::kCss);
}

void sendJs() {
  server->send_P(200, EinqSetupAssets::kJsMime, EinqSetupAssets::kJs);
}

void sendManifest() {
  server->send_P(200, EinqSetupAssets::kManifestMime, EinqSetupAssets::kManifest);
}

void sendServiceWorker() {
  server->sendHeader("Service-Worker-Allowed", "/");
  server->send_P(200, EinqSetupAssets::kServiceWorkerMime, EinqSetupAssets::kServiceWorker);
}

void redirectSettings() {
  server->sendHeader("Location", "/", true);
  server->send(302, "text/plain", "");
}

void handleWifiPost() {
  if (!requireSetupMode()) return;
  const String primarySsid = server->hasArg("primarySsid") ? server->arg("primarySsid") : server->arg("ssid");
  const String primaryPassword =
      server->hasArg("primaryPassword") ? server->arg("primaryPassword") : server->arg("password");
  const String backupSsid = server->arg("backupSsid");
  const String backupPassword = server->arg("backupPassword");
  if (primarySsid.isEmpty()) {
    server->send(400, "text/plain", "primary SSID required");
    return;
  }

  if (EinqWifiStore::saveNetworks(primarySsid.c_str(), primaryPassword.c_str(), backupSsid.c_str(),
                                  backupPassword.c_str())) {
    saved = true;
    server->send_P(200, "text/html", kSuccessHtml);
  } else {
    server->send(500, "text/plain", "save failed");
  }
}

void sendConfiguration() {
  const std::string json = EinqConfigStore::load();
  server->send(200, "application/json", json.c_str());
}

void handleConfigurationPut() {
  if (!requireSetupMode()) return;
  const String body = server->arg("plain");
  std::string canonical;
  std::string error;
  if (!EinqConfigStore::save(body.c_str(), body.length(), canonical, error)) {
    server->send(400, "text/plain", error.c_str());
    return;
  }
  server->send(200, "application/json", canonical.c_str());
}

void sendDeviceStatus() {
  JsonDocument status;
  status["hostname"] = std::string(kHostname) + ".local";
  status["mode"] = accessPoint ? "setup" : "station";
  status["canWrite"] = accessPoint;
  status["network"] = accessPoint ? WiFi.softAPSSID() : WiFi.SSID();
  String json;
  serializeJson(status, json);
  server->send(200, "application/json", json);
}

void registerRoutes() {
  server->on("/settings", HTTP_GET, sendSettingsPage);
  server->on("/app.css", HTTP_GET, sendCss);
  server->on("/app.js", HTTP_GET, sendJs);
  server->on("/manifest.webmanifest", HTTP_GET, sendManifest);
  server->on("/sw.js", HTTP_GET, sendServiceWorker);
  server->on("/api/v1/config", HTTP_GET, sendConfiguration);
  server->on("/api/v1/config", HTTP_PUT, handleConfigurationPut);
  server->on("/api/v1/device", HTTP_GET, sendDeviceStatus);
  server->on("/wifi", HTTP_POST, handleWifiPost);
  server->on("/", HTTP_GET, sendSettingsPage);
  server->on("/generate_204", HTTP_GET, redirectSettings);
  server->on("/hotspot-detect.html", HTTP_GET, redirectSettings);
  server->on("/connecttest.txt", HTTP_GET, redirectSettings);
  server->on("/canonical.html", HTTP_GET, redirectSettings);
  server->on("/success.txt", HTTP_GET, redirectSettings);
  server->onNotFound(redirectSettings);
}

std::string formatIp(const IPAddress& ip) {
  char buf[16];
  snprintf(buf, sizeof(buf), "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
  return buf;
}

std::string makeApSsid() {
  const uint64_t mac = ESP.getEfuseMac();
  char buf[20];
  snprintf(buf, sizeof(buf), "x3-%04x", static_cast<unsigned>(mac & 0xFFFF));
  return buf;
}

bool startHttpServer() {
  if (!MDNS.begin(kHostname)) return false;
  MDNS.addService("http", "tcp", 80);
  server = std::make_unique<WebServer>(80);
  registerRoutes();
  server->begin();
  running = true;
  saved = false;
  return true;
}

}  // namespace

namespace EinqWifiPortal {

bool start(Info& out) {
  stop();

  out.apSsid = makeApSsid();
  out.settingsUrl = std::string("http://") + kHostname + ".local/settings";
  out.wifiQrPayload = std::string("WIFI:S:") + out.apSsid + ";;";

  WiFi.persistent(false);
  WiFi.mode(WIFI_AP);
  delay(100);

  if (!WiFi.softAP(out.apSsid.c_str(), nullptr, kApChannel, false, kApMaxClients)) {
    stop();
    return false;
  }

  delay(100);
  const IPAddress apIp = WiFi.softAPIP();
  out.apIp = formatIp(apIp);

  dnsServer = std::make_unique<DNSServer>();
  dnsServer->setErrorReplyCode(DNSReplyCode::NoError);
  dnsServer->start(kDnsPort, "*", apIp);

  accessPoint = true;
  if (!startHttpServer()) {
    stop();
    return false;
  }
  return true;
}

bool startStation() {
  if (running && !accessPoint) return true;
  stop();
  if (WiFi.status() != WL_CONNECTED) return false;
  accessPoint = false;
  return startHttpServer();
}

void loop() {
  if (!running) {
    return;
  }
  if (dnsServer) {
    dnsServer->processNextRequest();
  }
  if (server) {
    server->handleClient();
  }
}

bool isRunning() { return running; }

bool isAccessPoint() { return running && accessPoint; }

bool credentialsSaved() { return saved; }

void clearCredentialsSaved() { saved = false; }

void stop() {
  if (server) {
    server->stop();
    server.reset();
  }
  if (dnsServer) {
    dnsServer->stop();
    dnsServer.reset();
  }
  if (accessPoint || WiFi.getMode() & WIFI_MODE_AP) {
    WiFi.softAPdisconnect(true);
  }
  MDNS.end();
  if (accessPoint) {
    WiFi.mode(WIFI_OFF);
  }
  running = false;
  saved = false;
  accessPoint = false;
}

}  // namespace EinqWifiPortal
