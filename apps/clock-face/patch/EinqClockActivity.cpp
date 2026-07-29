#include "EinqClockActivity.h"
#include "EinqAuthActivity.h"
#include "EinqWifiSetupActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>
#include <InputManager.h>
#include <Logging.h>
#include <WiFi.h>
#include <esp_sntp.h>

#include <ctime>
#include <string>

#include "components/UITheme.h"
#include "EinqCornerArt.h"
#include "einq-ble/EinqBle.h"
#include "einq-auth/EinqAuth.h"
#include "einq-cotd/EinqCotd.h"
#include "einq-ota/EinqOta.h"
#include "einq-room/EinqRoomScanner.h"
#include "einq-schedule/EinqSchedule.h"
#include "einq-wifi/EinqWifiPortal.h"
#include "einq-wifi/EinqWifiStore.h"
#include "fontIds.h"

namespace {
constexpr unsigned long WIFI_CONNECT_TIMEOUT_MS = 15000;
constexpr unsigned long DAILY_SYNC_INTERVAL_MS = 24UL * 60 * 60 * 1000;
constexpr unsigned long AUTH_REFRESH_INTERVAL_MS = 5 * 60 * 1000;
constexpr unsigned long SIDE_BUTTON_LONG_PRESS_MS = 1200;
constexpr int kGlyphSize = 180;
#ifndef EINQ_KEEP_AWAKE
#define EINQ_KEEP_AWAKE 1
#endif
constexpr bool kDevelopmentKeepAwake = EINQ_KEEP_AWAKE != 0;

bool wifiBootstrapStarted = false;
bool ntpSyncAttempted = false;
bool redrawAfterNtp = false;
bool backupAttempted = false;
unsigned long wifiBootstrapStartMs = 0;
std::string wifiSsid;
std::string wifiPassword;
std::string wifiBackupSsid;
std::string wifiBackupPassword;

Rect cornerSafeHeaderRect(const ThemeMetrics& metrics, const int pageWidth) {
  int left = 0;
  int right = 0;
  EinqCornerArt::contentInsets(true, left, right);
  return Rect{left, metrics.topPadding, pageWidth - left - right, metrics.headerHeight};
}

void drawCornerSafeFooter(GfxRenderer& renderer, const ThemeMetrics& metrics,
                          const int pageWidth, const int pageHeight,
                          const char* btn1, const char* btn2, const char* btn3, const char* btn4) {
  int left = 0;
  int right = 0;
  EinqCornerArt::contentInsets(false, left, right);
  const int width = pageWidth - left - right;
  const int y = pageHeight - metrics.buttonHintsHeight;
  const char* labels[] = {btn1, btn2, btn3, btn4};
  const int segmentWidth = width / 4;
  for (int i = 0; i < 4; ++i) {
    if (labels[i] == nullptr || labels[i][0] == '\0') continue;
    const int x = left + i * segmentWidth;
    const int itemWidth = i == 3 ? width - segmentWidth * 3 : segmentWidth;
    renderer.fillRect(x, y, itemWidth, metrics.buttonHintsHeight, false);
    renderer.drawRect(x, y, itemWidth, metrics.buttonHintsHeight);
    const int textWidth = renderer.getTextWidth(UI_10_FONT_ID, labels[i]);
    const int textHeight = renderer.getTextHeight(UI_10_FONT_ID);
    renderer.drawText(UI_10_FONT_ID, x + (itemWidth - textWidth) / 2,
                      y + (metrics.buttonHintsHeight - textHeight) / 2, labels[i]);
  }
}

void beginWifiAttempt(const std::string& ssid, const std::string& password) {
  wifiSsid = ssid;
  wifiPassword = password;
  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.disconnect(true, false);
  delay(100);
  WiFi.begin(wifiSsid.c_str(), wifiPassword.c_str());
  wifiBootstrapStarted = true;
  wifiBootstrapStartMs = millis();
}

void stopWifi() {
  if (EinqWifiPortal::isRunning()) {
    EinqWifiPortal::stop();
  }
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  wifiBootstrapStarted = false;
}

void startFallbackAccessPoint() {
  if (EinqWifiPortal::isRunning()) return;
  EinqWifiPortal::Info info {};
  EinqWifiPortal::start(info);
}

void syncTimeWithNTP() {
  setenv("TZ", "MST7MDT,M3.2.0,M11.1.0", 1);
  tzset();
  if (esp_sntp_enabled()) {
    esp_sntp_stop();
  }
  esp_sntp_setoperatingmode(ESP_SNTP_OPMODE_POLL);
  esp_sntp_setservername(0, "pool.ntp.org");
  esp_sntp_init();

  int retry = 0;
  constexpr int maxRetries = 50;
  while (sntp_get_sync_status() != SNTP_SYNC_STATUS_COMPLETED && retry < maxRetries) {
    delay(200);
    retry++;
  }
}

void tickWifiAndNtp() {
  if (EinqWifiPortal::isRunning()) {
    EinqWifiPortal::loop();
    if (EinqWifiPortal::isAccessPoint() && EinqWifiPortal::credentialsSaved()) {
      EinqWifiPortal::stop();
      wifiBootstrapStarted = false;
      backupAttempted = false;
      wifiSsid.clear();
      wifiPassword.clear();
      wifiBackupSsid.clear();
      wifiBackupPassword.clear();
    }
  }

  if (!wifiBootstrapStarted) {
    EinqWifiStore::Credentials primary {};
    EinqWifiStore::Credentials backup {};
    if (!EinqWifiStore::loadPrimary(primary)) {
      startFallbackAccessPoint();
      return;
    }
    EinqWifiStore::loadBackup(backup);
    wifiBackupSsid = backup.valid ? backup.ssid : "";
    wifiBackupPassword = backup.valid ? backup.password : "";
    backupAttempted = false;
    beginWifiAttempt(primary.ssid, primary.password);
    return;
  }

  if (WiFi.status() != WL_CONNECTED) {
    if (millis() - wifiBootstrapStartMs > WIFI_CONNECT_TIMEOUT_MS) {
      if (!backupAttempted && !wifiBackupSsid.empty()) {
        backupAttempted = true;
        beginWifiAttempt(wifiBackupSsid, wifiBackupPassword);
      } else {
        wifiBootstrapStarted = false;
        startFallbackAccessPoint();
      }
    }
    return;
  }

  if (!EinqWifiPortal::isRunning()) {
    EinqWifiPortal::startStation();
  }
  if (ntpSyncAttempted) return;

  syncTimeWithNTP();
  ntpSyncAttempted = true;
  redrawAfterNtp = true;
}

bool validWallClock(const struct tm& t) { return t.tm_year + 1900 >= 2024; }

void formatClock(const struct tm& t, char* timeBuf, size_t timeLen, char* dayBuf, size_t dayLen) {
  strftime(timeBuf, timeLen, "%H:%M", &t);
  strftime(dayBuf, dayLen, "%A", &t);
}

void copySnapshotField(char* dest, size_t destLen, const char* src) {
  strncpy(dest, src, destLen - 1);
  dest[destLen - 1] = '\0';
}

int drawWrappedCentered(const GfxRenderer& renderer, int font, int y, const char* text, int maxLines) {
  if (text == nullptr || text[0] == '\0') {
    return y;
  }
  const int width = renderer.getScreenWidth() - 48;
  const int lineHeight = renderer.getLineHeight(font);
  for (const std::string& line : renderer.wrappedText(font, text, width, maxLines)) {
    renderer.drawCenteredText(font, y, line.c_str(), true);
    y += lineHeight + 7;
  }
  return y;
}

const char* faceLabel(EinqClockActivity::Face face) {
  switch (face) {
    case EinqClockActivity::Face::Day:
      return "Today";
    case EinqClockActivity::Face::Calendar:
      return "Calendar";
    case EinqClockActivity::Face::SelfWeather:
      return "Self Weather";
    case EinqClockActivity::Face::Synastry:
      return "Synastry";
    case EinqClockActivity::Face::Family:
      return "Family";
    case EinqClockActivity::Face::Fortune:
      return "Fortune";
    case EinqClockActivity::Face::Card:
      return "eINQ Card";
    case EinqClockActivity::Face::Spotify:
      return "Spotify";
    case EinqClockActivity::Face::Lights:
      return "Lights";
    default:
      return "Mynah";
  }
}

const char* pressedRole(const MappedInputManager& input, bool released) {
  if (released) {
    if (input.wasReleased(MappedInputManager::Button::Back)) return "Back";
    if (input.wasReleased(MappedInputManager::Button::Confirm)) return "Confirm";
    if (input.wasReleased(MappedInputManager::Button::Left)) return "Left";
    if (input.wasReleased(MappedInputManager::Button::Right)) return "Right";
    if (input.wasReleased(MappedInputManager::Button::Up)) return "Up";
    if (input.wasReleased(MappedInputManager::Button::Down)) return "Down";
    if (input.wasReleased(MappedInputManager::Button::Power)) return "Power";
  } else {
    if (input.wasPressed(MappedInputManager::Button::Back)) return "Back";
    if (input.wasPressed(MappedInputManager::Button::Confirm)) return "Confirm";
    if (input.wasPressed(MappedInputManager::Button::Left)) return "Left";
    if (input.wasPressed(MappedInputManager::Button::Right)) return "Right";
    if (input.wasPressed(MappedInputManager::Button::Up)) return "Up";
    if (input.wasPressed(MappedInputManager::Button::Down)) return "Down";
    if (input.wasPressed(MappedInputManager::Button::Power)) return "Power";
  }
  return "Unknown";
}
}  // namespace

bool EinqClockActivity::ensureWifiForSync() {
  if (WiFi.status() == WL_CONNECTED) {
    return true;
  }
  if (wifiSsid.empty()) {
    EinqWifiStore::Credentials primary {};
    if (!EinqWifiStore::loadPrimary(primary)) {
      return false;
    }
    wifiSsid = primary.ssid;
    wifiPassword = primary.password;
  }

  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.begin(wifiSsid.c_str(), wifiPassword.c_str());
  const unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < WIFI_CONNECT_TIMEOUT_MS) {
    delay(200);
  }
  return WiFi.status() == WL_CONNECTED;
}

void EinqClockActivity::syncCotdForDate(const struct tm& localTime) {
  char dateStr[16];
  strftime(dateStr, sizeof(dateStr), "%Y-%m-%d", &localTime);
  if (strcmp(cotdDateLoaded, dateStr) == 0 && cotdCard.valid) {
    return;
  }

  EinqCotdCard next {};
  const bool online = ensureWifiForSync();
  if (online) {
    EinqCotd::syncForDate(dateStr, next);
  } else {
    EinqCotd::loadCached(dateStr, next);
  }
  if (online) {
    stopWifi();
  }

  if (next.valid) {
    cotdCard = next;
    copySnapshotField(cotdDateLoaded, sizeof(cotdDateLoaded), dateStr);
    currentGlyph = EinqGlyph::kindForDomain(cotdCard.domain);
  }
}

void EinqClockActivity::maybeMidnightOta(const struct tm& localTime) {
  char dateStr[16];
  strftime(dateStr, sizeof(dateStr), "%Y-%m-%d", &localTime);
  if (strcmp(otaCheckedDate, dateStr) == 0) {
    return;
  }

  if (!ensureWifiForSync()) {
    return;
  }

  char version[24];
  char url[sizeof(EinqOta::kDefaultFirmwareUrl) + 8];
  if (EinqOta::fetchManifest(version, sizeof(version), url, sizeof(url), nullptr) != EinqOta::Result::Ok) {
    stopWifi();
    return;
  }

  copySnapshotField(otaCheckedDate, sizeof(otaCheckedDate), dateStr);

  if (!EinqOta::isNewerThanRunning(version)) {
    stopWifi();
    return;
  }

  const auto& metrics = UITheme::getInstance().getMetrics();
  const int pageWidth = renderer.getScreenWidth();
  const int pageHeight = renderer.getScreenHeight();
  renderer.clearScreen();
  EinqCornerArt::drawFourCorners(renderer, pageWidth, pageHeight);
  GUI.drawHeader(renderer, cornerSafeHeaderRect(metrics, pageWidth), "Einq");
  const int bodyFont = NOTOSANS_14_FONT_ID;
  const int bodyH = renderer.getLineHeight(bodyFont);
  int y = (pageHeight - bodyH * 2) / 2;
  renderer.drawCenteredText(bodyFont, y, "Updating firmware", true);
  y += bodyH + 8;
  renderer.drawCenteredText(SMALL_FONT_ID, y, version, true);
  renderer.displayBuffer(HalDisplay::HALF_REFRESH);

  EinqOta::installFromUrl(url);
}

void EinqClockActivity::drawClockFace(const struct tm& localTime) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int pageWidth = renderer.getScreenWidth();
  const int pageHeight = renderer.getScreenHeight();

  char timeStr[16];
  char dayStr[32];
  formatClock(localTime, timeStr, sizeof(timeStr), dayStr, sizeof(dayStr));

  renderer.clearScreen();
  EinqCornerArt::drawFourCorners(renderer, pageWidth, pageHeight);
  GUI.drawHeader(renderer, cornerSafeHeaderRect(metrics, pageWidth), "Today",
                 strcmp(home.profile, "kid") == 0 ? "Kid" : nullptr);

  const int timeFont = NOTOSANS_18_FONT_ID;
  const int dayFont = NOTOSANS_14_FONT_ID;
  const int timeH = renderer.getLineHeight(timeFont);
  const int dayH = renderer.getLineHeight(dayFont);
  int y = metrics.topPadding + metrics.headerHeight + 30;
  renderer.drawCenteredText(timeFont, y, timeStr, true);
  y += timeH + 8;
  renderer.drawCenteredText(dayFont, y, dayStr, true);
  y += dayH + 6;

  if (!validWallClock(localTime)) {
    renderer.drawCenteredText(UI_10_FONT_ID, y, "Connect WiFi to sync time", true);
  } else {
    char dateStr[48];
    strftime(dateStr, sizeof(dateStr), "%B %d, %Y", &localTime);
    renderer.drawCenteredText(SMALL_FONT_ID, y, dateStr, true);
  }

  y += renderer.getLineHeight(SMALL_FONT_ID) + 22;
  if (home.weather.valid) {
    char weatherLine[80];
    snprintf(weatherLine, sizeof(weatherLine), "%s  %s", home.weather.temperature, home.weather.condition);
    renderer.drawCenteredText(NOTOSANS_16_FONT_ID, y, weatherLine, true);
    y += renderer.getLineHeight(NOTOSANS_16_FONT_ID) + 8;
    y = drawWrappedCentered(renderer, SMALL_FONT_ID, y, home.weather.summary, 2);
  }

  if (home.dayAphorism.valid) {
    y += 22;
    if (home.dayAphorism.title[0] != '\0') {
      renderer.drawCenteredText(NOTOSANS_14_FONT_ID, y, home.dayAphorism.title, true);
      y += renderer.getLineHeight(NOTOSANS_14_FONT_ID) + 8;
    }
    drawWrappedCentered(renderer, NOTOSANS_14_FONT_ID, y, home.dayAphorism.summary, 4);
  } else if (!home.valid) {
    y += 24;
    renderer.drawCenteredText(SMALL_FONT_ID, y,
                              authSessionAvailable ? "Waiting for today's weather" : "Pair with Castalia for your day",
                              true);
  }

  const auto labels = mappedInput.mapLabels("Home", authSessionAvailable ? "" : "Pair", "<", ">");
  drawCornerSafeFooter(renderer, metrics, pageWidth, pageHeight,
                       labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  GUI.drawSideButtonHints(renderer, "Prev", "Next");
  renderer.displayBuffer();
}

bool EinqClockActivity::faceAvailable(Face candidate) const {
  switch (candidate) {
    case Face::Day:
      return true;
    case Face::Calendar:
      return home.valid && home.permissions.calendar && home.nextEvent.valid;
    case Face::SelfWeather:
      return home.valid && home.permissions.astrology && home.selfWeather.valid;
    case Face::Synastry:
      return home.valid && home.permissions.astrology && home.synastryWeather.valid;
    case Face::Family:
      return home.valid && home.permissions.astrology && home.familyCount > 0;
    case Face::Fortune:
      return home.valid && home.permissions.fortune && home.fortune.valid;
    case Face::Card:
      return home.valid && home.permissions.cards && home.card.valid;
    case Face::Spotify:
      return home.valid && home.spotify.connected;
    case Face::Lights:
      return home.valid && home.lights.available;
  }
  return false;
}

void EinqClockActivity::moveFace(int direction) {
  constexpr int faceCount = 9;
  int candidate = static_cast<int>(face);
  for (int attempt = 0; attempt < faceCount; attempt++) {
    candidate = (candidate + direction + faceCount) % faceCount;
    const Face next = static_cast<Face>(candidate);
    if (faceAvailable(next)) {
      face = next;
      drawFace();
      return;
    }
  }
}

void EinqClockActivity::syncHome(bool allowNetwork) {
  EinqHomePayload next {};
  const bool loaded = allowNetwork ? EinqHome::sync(currentRoom, next) : EinqHome::loadCached(next);
  if (!loaded) {
    return;
  }
  home = next;
  lastHomeSyncMs = millis();
  if (!faceAvailable(face)) {
    face = Face::Day;
  }
}

void EinqClockActivity::performFaceAction(const char* requestedAction) {
  const char* action = requestedAction;
  if (action == nullptr && face == Face::Spotify && home.permissions.spotifyControl && home.spotify.connected) {
    action = "spotify.toggle";
  } else if (action == nullptr && face == Face::Lights && home.permissions.lightControl && home.lights.available) {
    action = "lights.toggle";
  }
  if (action == nullptr) {
    return;
  }

  actionPending = true;
  drawFace();
  if (ensureWifiForSync() && EinqHome::sendAction(action, currentRoom)) {
    syncHome(true);
  }
  stopWifi();
  actionPending = false;
  drawFace();
}

void EinqClockActivity::drawHomeFace() {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int pageWidth = renderer.getScreenWidth();
  const int pageHeight = renderer.getScreenHeight();
  const int contentTop = metrics.topPadding + metrics.headerHeight + 28;
  const int titleFont = NOTOSANS_18_FONT_ID;
  const int bodyFont = NOTOSANS_14_FONT_ID;
  const int smallFont = SMALL_FONT_ID;

  const char* title = "";
  const char* summary = "";
  char status[96] = {};

  switch (face) {
    case Face::Calendar:
      title = home.nextEvent.title;
      if (home.nextEvent.allDay) {
        snprintf(status, sizeof(status), "All day");
      } else if (strlen(home.nextEvent.start) >= 16) {
        snprintf(status, sizeof(status), "Next at %.5s", home.nextEvent.start + 11);
      }
      break;
    case Face::SelfWeather:
      title = home.selfWeather.title;
      summary = home.selfWeather.summary;
      break;
    case Face::Synastry:
      title = home.synastryWeather.title;
      summary = home.synastryWeather.summary;
      break;
    case Face::Fortune:
      title = home.fortune.title;
      summary = home.fortune.summary;
      break;
    case Face::Card:
      title = home.card.title;
      summary = home.card.summary;
      if (home.card.domain[0] != '\0') {
        snprintf(status, sizeof(status), "Domain: %s", home.card.domain);
      }
      break;
    case Face::Spotify:
      title = home.spotify.track;
      summary = home.spotify.artist;
      snprintf(status, sizeof(status), "%s on %s", home.spotify.playing ? "Playing" : "Paused",
               home.spotify.device[0] == '\0' ? "Spotify" : home.spotify.device);
      break;
    case Face::Lights:
      title = home.lights.room[0] == '\0' ? "Room lights" : home.lights.room;
      summary = home.lights.scene;
      snprintf(status, sizeof(status), "%s  %d%%", home.lights.on ? "On" : "Off", home.lights.brightness);
      break;
    default:
      break;
  }

  renderer.clearScreen();
  EinqCornerArt::drawFourCorners(renderer, pageWidth, pageHeight);
  GUI.drawHeader(renderer, cornerSafeHeaderRect(metrics, pageWidth), faceLabel(face),
                 strcmp(home.profile, "kid") == 0 ? "Kid" : nullptr);

  int y = contentTop;
  if (face == Face::Family) {
    for (size_t i = 0; i < home.familyCount && i < 6; i++) {
      if (!home.family[i].valid) continue;
      renderer.drawCenteredText(bodyFont, y, home.family[i].name, true);
      y += renderer.getLineHeight(bodyFont) + 4;
      y = drawWrappedCentered(renderer, smallFont, y, home.family[i].status, 2);
      y += 12;
    }
  } else {
    y = drawWrappedCentered(renderer, titleFont, y, title, 3);
    y += 12;
    y = drawWrappedCentered(renderer, bodyFont, y, summary, 6);
  }
  if (status[0] != '\0') {
    y += 16;
    drawWrappedCentered(renderer, smallFont, y, status, 2);
  }

  if (currentRoom[0] != '\0') {
    char roomLine[48];
    snprintf(roomLine, sizeof(roomLine), "Room: %s", currentRoom);
    renderer.drawCenteredText(UI_10_FONT_ID, pageHeight - 80, roomLine, true);
  }
  if (actionPending) {
    renderer.drawCenteredText(smallFont, pageHeight - 112, "Sending...", true);
  } else if ((face == Face::Spotify && home.permissions.spotifyControl) ||
             (face == Face::Lights && home.permissions.lightControl)) {
    renderer.drawCenteredText(smallFont, pageHeight - 112, "Press OK to toggle", true);
  }

  const bool actionable = (face == Face::Spotify && home.permissions.spotifyControl) ||
                          (face == Face::Lights && home.permissions.lightControl);
  const auto labels = mappedInput.mapLabels("Home", actionable ? "OK" : "", "<", ">");
  drawCornerSafeFooter(renderer, metrics, pageWidth, pageHeight,
                       labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  if (face == Face::Spotify && home.permissions.spotifyControl) {
    GUI.drawSideButtonHints(renderer, "<< hold", "hold >>");
  } else if (face == Face::Lights && home.permissions.lightControl) {
    GUI.drawSideButtonHints(renderer, "- hold", "hold +");
  } else {
    GUI.drawSideButtonHints(renderer, "Prev", "Next");
  }
  renderer.displayBuffer();
}

void EinqClockActivity::drawMessageFace() {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int pageWidth = renderer.getScreenWidth();
  const int pageHeight = renderer.getScreenHeight();

  renderer.clearScreen();
  EinqCornerArt::drawFourCorners(renderer, pageWidth, pageHeight);
  GUI.drawHeader(renderer, cornerSafeHeaderRect(metrics, pageWidth), messageSnapshot.title);

  const int bodyFont = NOTOSANS_14_FONT_ID;
  const int bodyH = renderer.getLineHeight(bodyFont);

  int y = (pageHeight - bodyH) / 2;
  if (messageSnapshot.line1[0] != '\0') {
    renderer.drawCenteredText(bodyFont, y, messageSnapshot.line1, true);
    y += bodyH + 8;
  }
  if (messageSnapshot.line2[0] != '\0') {
    renderer.drawCenteredText(bodyFont, y, messageSnapshot.line2, true);
    y += bodyH + 8;
  }
  if (messageSnapshot.line3[0] != '\0') {
    renderer.drawCenteredText(bodyFont, y, messageSnapshot.line3, true);
  }

  const auto labels = mappedInput.mapLabels("CrossPoint", "", "", "");
  drawCornerSafeFooter(renderer, metrics, pageWidth, pageHeight,
                       labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}

void EinqClockActivity::publishSnapshot(const struct tm* localTime) {
  EinqDisplaySnapshot snap {};
  copySnapshotField(snap.theme, sizeof(snap.theme), EinqCornerArt::currentThemeId());
  if (messageMode) {
    copySnapshotField(snap.mode, sizeof(snap.mode), "message");
    copySnapshotField(snap.title, sizeof(snap.title), messageSnapshot.title);
    copySnapshotField(snap.line1, sizeof(snap.line1), messageSnapshot.line1);
    copySnapshotField(snap.line2, sizeof(snap.line2), messageSnapshot.line2);
    copySnapshotField(snap.line3, sizeof(snap.line3), messageSnapshot.line3);
  } else if (localTime != nullptr && face != Face::Day) {
    copySnapshotField(snap.mode, sizeof(snap.mode), "home");
    copySnapshotField(snap.title, sizeof(snap.title), faceLabel(face));
    switch (face) {
      case Face::Calendar:
        copySnapshotField(snap.line1, sizeof(snap.line1), home.nextEvent.title);
        copySnapshotField(snap.line2, sizeof(snap.line2), home.nextEvent.start);
        break;
      case Face::SelfWeather:
        copySnapshotField(snap.line1, sizeof(snap.line1), home.selfWeather.title);
        copySnapshotField(snap.line2, sizeof(snap.line2), home.selfWeather.summary);
        break;
      case Face::Synastry:
        copySnapshotField(snap.line1, sizeof(snap.line1), home.synastryWeather.title);
        copySnapshotField(snap.line2, sizeof(snap.line2), home.synastryWeather.summary);
        break;
      case Face::Family:
        if (home.familyCount > 0) {
          copySnapshotField(snap.line1, sizeof(snap.line1), home.family[0].name);
          copySnapshotField(snap.line2, sizeof(snap.line2), home.family[0].status);
        }
        break;
      case Face::Fortune:
        copySnapshotField(snap.line1, sizeof(snap.line1), home.fortune.title);
        copySnapshotField(snap.line2, sizeof(snap.line2), home.fortune.summary);
        break;
      case Face::Card:
        copySnapshotField(snap.line1, sizeof(snap.line1), home.card.title);
        copySnapshotField(snap.line2, sizeof(snap.line2), home.card.summary);
        copySnapshotField(snap.glyph, sizeof(snap.glyph), home.card.domain);
        break;
      case Face::Spotify:
        copySnapshotField(snap.line1, sizeof(snap.line1), home.spotify.track);
        copySnapshotField(snap.line2, sizeof(snap.line2), home.spotify.artist);
        copySnapshotField(snap.line3, sizeof(snap.line3), home.spotify.playing ? "Playing" : "Paused");
        break;
      case Face::Lights:
        copySnapshotField(snap.line1, sizeof(snap.line1), home.lights.room);
        copySnapshotField(snap.line2, sizeof(snap.line2), home.lights.scene);
        copySnapshotField(snap.line3, sizeof(snap.line3), home.lights.on ? "On" : "Off");
        break;
      default:
        break;
    }
    formatClock(*localTime, snap.time, sizeof(snap.time), snap.day, sizeof(snap.day));
    if (validWallClock(*localTime)) {
      strftime(snap.date, sizeof(snap.date), "%Y-%m-%d", localTime);
    }
  } else if (localTime != nullptr) {
    copySnapshotField(snap.mode, sizeof(snap.mode), "clock");
    if (cotdCard.valid) {
      copySnapshotField(snap.title, sizeof(snap.title), cotdCard.title);
      copySnapshotField(snap.line1, sizeof(snap.line1), cotdCard.topic);
    } else {
      copySnapshotField(snap.title, sizeof(snap.title), "Einq");
    }
    formatClock(*localTime, snap.time, sizeof(snap.time), snap.day, sizeof(snap.day));
    if (validWallClock(*localTime)) {
      strftime(snap.date, sizeof(snap.date), "%Y-%m-%d", localTime);
    }
    copySnapshotField(snap.glyph, sizeof(snap.glyph), EinqGlyph::kindLabel(currentGlyph));
    if (!validWallClock(*localTime)) {
      copySnapshotField(snap.line1, sizeof(snap.line1), "Connect WiFi to sync time");
    }
  }
  EinqBle::setSnapshot(snap);
  EinqBle::notifyDisplayChanged();
}

void EinqClockActivity::drawFace() {
  time_t now = time(nullptr);
  struct tm localTime {};
  localtime_r(&now, &localTime);

  if (messageMode) {
    EinqBle::setAdvertisedCard(nullptr);
    drawMessageFace();
    publishSnapshot(nullptr);
    return;
  }

  EinqBle::setAdvertisedCard(face == Face::Card && home.card.valid ? home.card.title : nullptr);
  if (face == Face::Day) {
    drawClockFace(localTime);
  } else {
    drawHomeFace();
  }
  publishSnapshot(&localTime);
}

void EinqClockActivity::onEnter() {
  Activity::onEnter();
  LOG_INF("EINQ", "development keep-awake=%s", kDevelopmentKeepAwake ? "on" : "off");
  lastDrawMs = 0;
  lastHomeSyncMs = 0;
  lastAuthRefreshMs = 0;
  lastMinute = -1;
  lastDayOfYear = -1;
  cotdCard = {};
  cotdDateLoaded[0] = '\0';
  otaCheckedDate[0] = '\0';
  messageMode = false;
  currentRoom[0] = '\0';
  home = {};
  face = Face::Day;
  actionPending = false;
  authSessionAvailable = EinqAuth::hasSession();
  stopWifi();
  wifiBootstrapStarted = false;
  ntpSyncAttempted = false;
  redrawAfterNtp = false;
  bleStarted = false;
  syncHome(false);
  drawFace();
  // BLE is the normal control and monitoring transport. Wi-Fi remains off
  // until a sync, NTP, OTA, setup, or user action explicitly requests it.
  EinqBle::begin();
  EinqRoomScanner::begin();
  bleStarted = true;
  drawFace();
  lastHomeSyncMs = millis();
}

void EinqClockActivity::openWifiSetup() {
  startActivityForResult(std::make_unique<EinqWifiSetupActivity>(renderer, mappedInput), [this](const ActivityResult&) {
    wifiBootstrapStarted = false;
    ntpSyncAttempted = false;
    redrawAfterNtp = false;
    lastHomeSyncMs = 0;
    drawFace();
  });
}

void EinqClockActivity::openCastaliaPairing() {
  startActivityForResult(std::make_unique<EinqAuthActivity>(renderer, mappedInput), [this](const ActivityResult&) {
    authSessionAvailable = EinqAuth::hasSession();
    if (authSessionAvailable && ensureWifiForSync()) {
      EinqAuth::refreshIfNeeded();
      syncHome(true);
      stopWifi();
    }
    drawFace();
  });
}

void EinqClockActivity::loop() {
  if (mappedInput.wasAnyPressed()) {
    LOG_INF("EINQ_BTN", "press role=%s front=%d adc1=%d adc2=%d power=%d",
            pressedRole(mappedInput, false), mappedInput.getPressedFrontButton(),
            analogRead(InputManager::BUTTON_ADC_PIN_1), analogRead(InputManager::BUTTON_ADC_PIN_2),
            digitalRead(InputManager::POWER_BUTTON_PIN));
  }
  if (mappedInput.wasAnyReleased()) {
    LOG_INF("EINQ_BTN", "release role=%s held_ms=%lu adc1=%d adc2=%d power=%d",
            pressedRole(mappedInput, true), mappedInput.getHeldTime(), analogRead(InputManager::BUTTON_ADC_PIN_1),
            analogRead(InputManager::BUTTON_ADC_PIN_2), digitalRead(InputManager::POWER_BUTTON_PIN));
  }

  if (mappedInput.wasReleased(MappedInputManager::Button::PageBack)) {
    if (mappedInput.getHeldTime() >= SIDE_BUTTON_LONG_PRESS_MS) {
      if (face == Face::Spotify && home.permissions.spotifyControl) {
        performFaceAction("spotify.previous");
      } else if (face == Face::Lights && home.permissions.lightControl) {
        performFaceAction("lights.dimmer");
      } else {
        openCastaliaPairing();
      }
    } else {
      moveFace(-1);
    }
    return;
  }

  if (mappedInput.wasReleased(MappedInputManager::Button::PageForward)) {
    if (mappedInput.getHeldTime() >= SIDE_BUTTON_LONG_PRESS_MS) {
      if (face == Face::Spotify && home.permissions.spotifyControl) {
        performFaceAction("spotify.next");
      } else if (face == Face::Lights && home.permissions.lightControl) {
        performFaceAction("lights.brighter");
      } else {
        openWifiSetup();
      }
    } else {
      moveFace(1);
    }
    return;
  }

  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    onGoHome();
    return;
  }

  if (mappedInput.wasReleased(MappedInputManager::Button::Left)) {
    moveFace(-1);
    return;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Right)) {
    moveFace(1);
    return;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    if (face == Face::Day && !authSessionAvailable) {
      openCastaliaPairing();
    } else {
      performFaceAction();
    }
    return;
  }

  EinqDisplayCommand cmd {};
  if (EinqBle::takeCommand(cmd)) {
    if (strcmp(cmd.mode, "clock") == 0) {
      messageMode = false;
    } else if (strcmp(cmd.mode, "message") == 0) {
      messageMode = true;
      copySnapshotField(messageSnapshot.mode, sizeof(messageSnapshot.mode), "message");
      copySnapshotField(messageSnapshot.title, sizeof(messageSnapshot.title), cmd.title);
      copySnapshotField(messageSnapshot.line1, sizeof(messageSnapshot.line1), cmd.line1);
      copySnapshotField(messageSnapshot.line2, sizeof(messageSnapshot.line2), cmd.line2);
      copySnapshotField(messageSnapshot.line3, sizeof(messageSnapshot.line3), cmd.line3);
    }
    drawFace();
  }

  if (messageMode) {
    delay(200);
    return;
  }

  const unsigned long loopMs = millis();
  if (bleStarted) {
    EinqRoomScanner::poll();
  }

  const EinqRoom::Result roomResult = EinqRoomScanner::current();
  if (roomResult.known && (roomResult.changed || strcmp(currentRoom, roomResult.room) != 0)) {
    copySnapshotField(currentRoom, sizeof(currentRoom), roomResult.room);
    if (authSessionAvailable && ensureWifiForSync()) {
      syncHome(true);
      stopWifi();
    }
    drawFace();
  }

  const unsigned long nowMs = millis();
  time_t now = time(nullptr);
  struct tm localTime {};
  localtime_r(&now, &localTime);

  const bool minuteTick = localTime.tm_min != lastMinute;
  const bool dayTick = localTime.tm_yday != lastDayOfYear;
  const bool periodic = nowMs - lastDrawMs >= 30000;
  if (authSessionAvailable &&
      (lastHomeSyncMs == 0 || nowMs - lastHomeSyncMs >= DAILY_SYNC_INTERVAL_MS)) {
    // Timestamp the attempt as well as a success so a missing network does not
    // turn the main loop into a continuous series of blocking reconnects.
    lastHomeSyncMs = nowMs;
    if (ensureWifiForSync()) {
      if (!ntpSyncAttempted) {
        syncTimeWithNTP();
        ntpSyncAttempted = true;
        redrawAfterNtp = true;
      }
      if (lastAuthRefreshMs == 0 || loopMs - lastAuthRefreshMs >= AUTH_REFRESH_INTERVAL_MS) {
        lastAuthRefreshMs = loopMs;
        EinqAuth::refreshIfNeeded();
      }
      syncHome(true);
      stopWifi();
    }
  } else if (!validWallClock(localTime) && !ntpSyncAttempted) {
    ntpSyncAttempted = true;
    if (ensureWifiForSync()) {
      syncTimeWithNTP();
      redrawAfterNtp = true;
      stopWifi();
    }
  }

  if (redrawAfterNtp || minuteTick || dayTick || periodic) {
    if (validWallClock(localTime) && (dayTick || redrawAfterNtp)) {
      syncCotdForDate(localTime);
    }
    if (validWallClock(localTime) && dayTick) {
      maybeMidnightOta(localTime);
    }
    redrawAfterNtp = false;
    lastMinute = localTime.tm_min;
    lastDayOfYear = localTime.tm_yday;
    lastDrawMs = nowMs;
    drawFace();
  }

  if (kDevelopmentKeepAwake) {
    delay(50);
    return;
  }
  if (!validWallClock(localTime)) {
    delay(200);
    return;
  }
  if (EinqWifiPortal::isAccessPoint()) {
    delay(100);
    return;
  }

  const uint32_t wakeSeconds = EinqSchedule::nextWakeSeconds(localTime);
  if (wakeSeconds <= 1) {
    return;
  }
  if (EinqRoomScanner::isScanning()) {
    delay(100);
    return;
  }

  EinqRoomScanner::suspend();
  EinqBle::suspendForSleep();
  EinqWifiPortal::stop();
  if (WiFi.status() == WL_CONNECTED) {
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
  }

  EinqSchedule::lightSleepForSeconds(wakeSeconds);

  wifiBootstrapStarted = false;
  backupAttempted = false;
  EinqBle::resumeAfterSleep();
  EinqRoomScanner::resume();
}
