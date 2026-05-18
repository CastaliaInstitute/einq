#include "EinqClockActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>

#include <ctime>

#include "components/UITheme.h"
#include "fontIds.h"

namespace {
bool validWallClock(const struct tm& t) { return t.tm_year + 1900 >= 2024; }

void formatClock(const struct tm& t, char* timeBuf, size_t timeLen, char* dayBuf, size_t dayLen) {
  strftime(timeBuf, timeLen, "%H:%M", &t);
  strftime(dayBuf, dayLen, "%A", &t);
}
}  // namespace

void EinqClockActivity::drawFace() {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int pageWidth = renderer.getScreenWidth();
  const int pageHeight = renderer.getScreenHeight();

  time_t now = time(nullptr);
  struct tm localTime {};
  localtime_r(&now, &localTime);

  char timeStr[16];
  char dayStr[32];
  formatClock(localTime, timeStr, sizeof(timeStr), dayStr, sizeof(dayStr));

  renderer.clearScreen();
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, "Einq");

  const int timeFont = NOTOSANS_18_FONT_ID;
  const int dayFont = NOTOSANS_14_FONT_ID;
  const int timeH = renderer.getLineHeight(timeFont);
  const int dayH = renderer.getLineHeight(dayFont);
  const int blockH = timeH + dayH + 12;
  int y = (pageHeight - blockH) / 2;

  renderer.drawCenteredText(timeFont, y, timeStr, true);
  y += timeH + 12;
  renderer.drawCenteredText(dayFont, y, dayStr, true);

  if (!validWallClock(localTime)) {
    y += dayH + 16;
    renderer.drawCenteredText(UI_10_FONT_ID, y, "Connect WiFi to sync time", true);
  } else {
    y += dayH + 16;
    char dateStr[48];
    strftime(dateStr, sizeof(dateStr), "%Y-%m-%d", &localTime);
    renderer.drawCenteredText(SMALL_FONT_ID, y, dateStr, true);
  }

  const auto labels = mappedInput.mapLabels("CrossPoint", "", "", "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}

void EinqClockActivity::onEnter() {
  Activity::onEnter();
  lastDrawMs = 0;
  lastMinute = -1;
  drawFace();
}

void EinqClockActivity::loop() {
  if (mappedInput.wasReleased(MappedInputManager::Button::Back) ||
      mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    onGoHome();
    return;
  }

  const unsigned long nowMs = millis();
  time_t now = time(nullptr);
  struct tm localTime {};
  localtime_r(&now, &localTime);

  const bool minuteTick = localTime.tm_min != lastMinute;
  const bool periodic = nowMs - lastDrawMs >= 30000;

  if (minuteTick || periodic) {
    lastMinute = localTime.tm_min;
    lastDrawMs = nowMs;
    drawFace();
  }

  delay(200);
}
