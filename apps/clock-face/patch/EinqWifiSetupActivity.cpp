#include "EinqWifiSetupActivity.h"

#include <GfxRenderer.h>

#include <string>

#include "components/UITheme.h"
#include "einq-wifi/EinqWifiPortal.h"
#include "fontIds.h"
#include "util/QrUtils.h"

namespace {
constexpr int kQrSize = 150;
}

void EinqWifiSetupActivity::drawQrScreen() const {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int pageWidth = renderer.getScreenWidth();
  const int pageHeight = renderer.getScreenHeight();
  const int bodyFont = SMALL_FONT_ID;
  const int bodyH = renderer.getLineHeight(bodyFont);

  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, "WiFi Setup", nullptr);

  int y = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
  renderer.drawText(bodyFont, metrics.contentSidePadding, y, "1. Scan to join Einq WiFi", true);
  y += bodyH + 4;

  const Rect wifiQr(metrics.contentSidePadding, y, kQrSize, kQrSize);
  QrUtils::drawQrCode(renderer, wifiQr, wifiQrPayload);
  renderer.drawText(bodyFont, metrics.contentSidePadding + kQrSize + 8, y + 40, apSsid.c_str());
  y += kQrSize + metrics.verticalSpacing;

  renderer.drawText(bodyFont, metrics.contentSidePadding, y, "2. Scan to open settings", true);
  y += bodyH + 4;

  const Rect urlQr(metrics.contentSidePadding, y, kQrSize, kQrSize);
  QrUtils::drawQrCode(renderer, urlQr, settingsUrl);
  renderer.drawText(bodyFont, metrics.contentSidePadding + kQrSize + 8, y + 20, settingsUrl.c_str());
  y += kQrSize + 4;

  if (!apIp.empty()) {
    const std::string fallback = "Or: http://" + apIp + "/settings";
    renderer.drawText(bodyFont, metrics.contentSidePadding, y, fallback.c_str());
    y += bodyH;
  }

  const int hintY = pageHeight - bodyH - metrics.verticalSpacing;
  renderer.drawCenteredText(bodyFont, hintY, "Back to cancel", true);
}

void EinqWifiSetupActivity::onEnter() {
  Activity::onEnter();
  phase = Phase::Starting;
  apSsid.clear();
  settingsUrl.clear();
  wifiQrPayload.clear();
  apIp.clear();
  requestUpdate();
}

void EinqWifiSetupActivity::onExit() {
  EinqWifiPortal::stop();
  Activity::onExit();
}

void EinqWifiSetupActivity::loop() {
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    finish();
    return;
  }

  if (phase == Phase::Starting) {
    EinqWifiPortal::Info info {};
    if (!EinqWifiPortal::start(info)) {
      finish();
      return;
    }
    apSsid = info.apSsid;
    settingsUrl = info.settingsUrl;
    wifiQrPayload = info.wifiQrPayload;
    apIp = info.apIp;
    phase = Phase::Running;
    requestUpdate();
    return;
  }

  if (phase == Phase::Running) {
    EinqWifiPortal::loop();
    if (EinqWifiPortal::credentialsSaved()) {
      phase = Phase::Saved;
      EinqWifiPortal::stop();
      requestUpdate();
      delay(1200);
      finish();
    }
  }
}

void EinqWifiSetupActivity::render(RenderLock&&) {
  renderer.clearScreen();

  if (phase == Phase::Starting) {
    const auto& metrics = UITheme::getInstance().getMetrics();
    const int pageWidth = renderer.getScreenWidth();
    const int pageHeight = renderer.getScreenHeight();
    const int bodyH = renderer.getLineHeight(SMALL_FONT_ID);
    GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, "WiFi Setup", nullptr);
    renderer.drawCenteredText(SMALL_FONT_ID, (pageHeight - bodyH) / 2, "Starting hotspot...", true);
  } else if (phase == Phase::Saved) {
    const auto& metrics = UITheme::getInstance().getMetrics();
    const int pageWidth = renderer.getScreenWidth();
    const int pageHeight = renderer.getScreenHeight();
    const int bodyH = renderer.getLineHeight(NOTOSANS_14_FONT_ID);
    GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, "WiFi Setup", nullptr);
    renderer.drawCenteredText(NOTOSANS_14_FONT_ID, (pageHeight - bodyH) / 2, "WiFi saved", true);
  } else {
    drawQrScreen();
  }

  renderer.displayBuffer();
}
