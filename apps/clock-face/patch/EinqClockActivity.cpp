#include "EinqClockActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>
#include <WiFi.h>
#include <esp_sntp.h>

#include <ctime>
#include <string>

#include "WifiCredentialStore.h"
#include "components/UITheme.h"
#include "einq-ble/EinqBle.h"
#include "einq-cotd/EinqCotd.h"
#include "einq-ota/EinqOta.h"
#include "einq-schedule/EinqSchedule.h"
#include "fontIds.h"

namespace {
constexpr unsigned long WIFI_CONNECT_TIMEOUT_MS = 30000;
constexpr int kGlyphSize = 180;

bool wifiBootstrapStarted = false;
bool ntpSyncAttempted = false;
bool redrawAfterNtp = false;
unsigned long wifiBootstrapStartMs = 0;
std::string wifiSsid;
std::string wifiPassword;

void syncTimeWithNTP() {
  if (esp_sntp_enabled()) {
    esp_sntp_stop();
  }
  esp_sntp_setoperatingmode(ESP_SNTP_OPMODE_POLL);
  esp_sntp_setservername(0, "pool.ntp.org");
  esp_sntp_init();

  int retry = 0;
  constexpr int maxRetries = 15;
  while (sntp_get_sync_status() != SNTP_SYNC_STATUS_COMPLETED && retry < maxRetries) {
    delay(200);
    retry++;
  }
}

void tickWifiAndNtp() {
  if (!wifiBootstrapStarted) {
    WIFI_STORE.loadFromFile();
    std::string ssid = WIFI_STORE.getLastConnectedSsid();
    const WifiCredential* cred = nullptr;
    if (!ssid.empty()) {
      cred = WIFI_STORE.findCredential(ssid);
    }
    if (cred == nullptr && !WIFI_STORE.getCredentials().empty()) {
      cred = &WIFI_STORE.getCredentials().front();
      ssid = cred->ssid;
    }
    if (cred == nullptr || ssid.empty()) {
      return;
    }

    wifiSsid = ssid;
    wifiPassword = cred->password;
    WiFi.persistent(false);
    WiFi.mode(WIFI_STA);
    WiFi.disconnect(true, true);
    delay(100);
    WiFi.begin(wifiSsid.c_str(), wifiPassword.c_str());
    wifiBootstrapStarted = true;
    wifiBootstrapStartMs = millis();
    return;
  }

  if (ntpSyncAttempted) {
    return;
  }

  if (WiFi.status() != WL_CONNECTED) {
    if (millis() - wifiBootstrapStartMs > WIFI_CONNECT_TIMEOUT_MS) {
      ntpSyncAttempted = true;
    }
    return;
  }

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
}  // namespace

bool EinqClockActivity::ensureWifiForSync() {
  if (WiFi.status() == WL_CONNECTED) {
    return true;
  }
  if (wifiSsid.empty()) {
    tickWifiAndNtp();
    if (wifiSsid.empty()) {
      return false;
    }
  }

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
  if (ensureWifiForSync()) {
    EinqCotd::syncForDate(dateStr, next);
  } else {
    EinqCotd::loadCached(dateStr, next);
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
    return;
  }

  copySnapshotField(otaCheckedDate, sizeof(otaCheckedDate), dateStr);

  if (!EinqOta::isNewerThanRunning(version)) {
    return;
  }

  const auto& metrics = UITheme::getInstance().getMetrics();
  const int pageWidth = renderer.getScreenWidth();
  const int pageHeight = renderer.getScreenHeight();
  renderer.clearScreen();
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, "Einq");
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

  if (!cotdCard.valid) {
    currentGlyph = EinqGlyph::kindForDayOfYear(localTime.tm_yday);
  }

  renderer.clearScreen();
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, "Einq");

  const int glyphCenterX = pageWidth / 2;
  const int glyphCenterY = pageHeight / 2 - 56;
  EinqGlyph::draw(renderer, glyphCenterX, glyphCenterY, kGlyphSize, currentGlyph);

  const int titleFont = cotdCard.valid ? NOTOSANS_16_FONT_ID : NOTOSANS_18_FONT_ID;
  const int timeFont = NOTOSANS_18_FONT_ID;
  const int dayFont = NOTOSANS_14_FONT_ID;
  const int titleH = renderer.getLineHeight(titleFont);
  const int timeH = renderer.getLineHeight(timeFont);
  const int dayH = renderer.getLineHeight(dayFont);
  int y = glyphCenterY + kGlyphSize / 2 + 16;

  if (cotdCard.valid) {
    renderer.drawCenteredText(titleFont, y, cotdCard.title, true);
    y += titleH + 6;
    if (cotdCard.topic[0] != '\0') {
      renderer.drawCenteredText(SMALL_FONT_ID, y, cotdCard.topic, true);
      y += renderer.getLineHeight(SMALL_FONT_ID) + 8;
    }
  }

  renderer.drawCenteredText(timeFont, y, timeStr, true);
  y += timeH + 8;
  renderer.drawCenteredText(dayFont, y, dayStr, true);

  if (!validWallClock(localTime)) {
    y += dayH + 16;
    renderer.drawCenteredText(UI_10_FONT_ID, y, "Connect WiFi to sync time", true);
  } else {
    y += dayH + 12;
    char dateStr[48];
    strftime(dateStr, sizeof(dateStr), "%Y-%m-%d", &localTime);
    renderer.drawCenteredText(SMALL_FONT_ID, y, dateStr, true);
  }

  const auto labels = mappedInput.mapLabels("CrossPoint", "", "", "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}

void EinqClockActivity::drawMessageFace() {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int pageWidth = renderer.getScreenWidth();
  const int pageHeight = renderer.getScreenHeight();

  renderer.clearScreen();
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, messageSnapshot.title);

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
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}

void EinqClockActivity::publishSnapshot(const struct tm* localTime) {
  EinqDisplaySnapshot snap {};
  if (messageMode) {
    copySnapshotField(snap.mode, sizeof(snap.mode), "message");
    copySnapshotField(snap.title, sizeof(snap.title), messageSnapshot.title);
    copySnapshotField(snap.line1, sizeof(snap.line1), messageSnapshot.line1);
    copySnapshotField(snap.line2, sizeof(snap.line2), messageSnapshot.line2);
    copySnapshotField(snap.line3, sizeof(snap.line3), messageSnapshot.line3);
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
    drawMessageFace();
    publishSnapshot(nullptr);
    return;
  }

  drawClockFace(localTime);
  publishSnapshot(&localTime);
}

void EinqClockActivity::onEnter() {
  Activity::onEnter();
  lastDrawMs = 0;
  lastMinute = -1;
  lastDayOfYear = -1;
  cotdCard = {};
  cotdDateLoaded[0] = '\0';
  otaCheckedDate[0] = '\0';
  messageMode = false;
  wifiBootstrapStarted = false;
  ntpSyncAttempted = false;
  redrawAfterNtp = false;
  EinqBle::begin();
  drawFace();

  time_t now = time(nullptr);
  struct tm localTime {};
  localtime_r(&now, &localTime);
  if (validWallClock(localTime)) {
    syncCotdForDate(localTime);
    drawFace();
  }
}

void EinqClockActivity::loop() {
  if (mappedInput.wasReleased(MappedInputManager::Button::Back) ||
      mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    onGoHome();
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

  tickWifiAndNtp();

  const unsigned long nowMs = millis();
  time_t now = time(nullptr);
  struct tm localTime {};
  localtime_r(&now, &localTime);

  const bool minuteTick = localTime.tm_min != lastMinute;
  const bool dayTick = localTime.tm_yday != lastDayOfYear;
  const bool periodic = nowMs - lastDrawMs >= 30000;

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

  if (!validWallClock(localTime)) {
    delay(200);
    return;
  }

  const uint32_t wakeSeconds = EinqSchedule::nextWakeSeconds(localTime);
  if (wakeSeconds <= 1) {
    return;
  }

  EinqBle::suspendForSleep();
  if (WiFi.status() == WL_CONNECTED) {
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
  }

  EinqSchedule::lightSleepForSeconds(wakeSeconds);

  if (wifiBootstrapStarted) {
    WiFi.mode(WIFI_STA);
  }
  EinqBle::resumeAfterSleep();
}
