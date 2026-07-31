#include "EinqAuthActivity.h"

#include <GfxRenderer.h>
#include <WiFi.h>

#include <cstring>
#include <string>

#include "components/UITheme.h"
#include "einq-wifi/EinqWifiStore.h"
#include "fontIds.h"
#include "util/QrUtils.h"

namespace {
constexpr unsigned long kConnectTimeoutMs = 15000;
constexpr unsigned long kPollIntervalMs = 2500;
// Version 11 plus its quiet zone renders at five pixels per module.
constexpr int kQrSize = 360;
}

bool EinqAuthActivity::connectWifi() {
  if (WiFi.status() == WL_CONNECTED) return true;
  EinqWifiStore::Credentials primary {};
  EinqWifiStore::Credentials backup {};
  if (!EinqWifiStore::loadPrimary(primary)) return false;
  EinqWifiStore::loadBackup(backup);

  auto tryNetwork = [](const EinqWifiStore::Credentials& network) {
    if (!network.valid) return false;
    WiFi.persistent(false);
    WiFi.mode(WIFI_STA);
    WiFi.disconnect(true, false);
    delay(100);
    WiFi.begin(network.ssid, network.password);
    const unsigned long started = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - started < kConnectTimeoutMs) {
      delay(200);
    }
    return WiFi.status() == WL_CONNECTED;
  };

  return tryNetwork(primary) || tryNetwork(backup);
}

void EinqAuthActivity::setError(const char* message) {
  phase = Phase::Error;
  std::strncpy(status, message, sizeof(status) - 1);
  status[sizeof(status) - 1] = '\0';
  requestUpdate();
}

void EinqAuthActivity::onEnter() {
  Activity::onEnter();
  phase = Phase::Starting;
  pairing = {};
  lastPollMs = 0;
  pollErrors = 0;
  status[0] = '\0';
  requestUpdate();
}

void EinqAuthActivity::loop() {
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    finish();
    return;
  }

  if (phase == Phase::Starting) {
    if (EinqAuth::hasSession()) {
      phase = Phase::SignedIn;
      requestUpdate();
      return;
    }
    if (!connectWifi()) {
      setError("Set up WiFi first");
      return;
    }
    if (!EinqAuth::startPairing(pairing)) {
      setError("Could not start pairing");
      return;
    }
    phase = Phase::Pairing;
    lastPollMs = millis();
    requestUpdate();
    return;
  }

  if (phase != Phase::Pairing || millis() - lastPollMs < kPollIntervalMs) return;
  lastPollMs = millis();
  switch (EinqAuth::pollPairing(pairing)) {
    case EinqAuth::PollResult::Ready:
      phase = Phase::SignedIn;
      requestUpdate();
      break;
    case EinqAuth::PollResult::Expired:
      setError("Pairing expired");
      break;
    case EinqAuth::PollResult::Error:
      if (++pollErrors >= 3) setError("Pairing connection failed");
      break;
    case EinqAuth::PollResult::Pending:
      pollErrors = 0;
      break;
  }
}

void EinqAuthActivity::render(RenderLock&&) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int pageWidth = renderer.getScreenWidth();
  const int pageHeight = renderer.getScreenHeight();
  renderer.clearScreen();
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, "Castalia");

  if (phase == Phase::Pairing && pairing.valid) {
    const int qrX = (pageWidth - kQrSize) / 2;
    const int qrY = metrics.topPadding + metrics.headerHeight + 55;
    renderer.drawCenteredText(NOTOSANS_14_FONT_ID, qrY - 30, "Scan with a parent's phone", true);
    QrUtils::drawQrCode(renderer, Rect{qrX, qrY, kQrSize, kQrSize}, pairing.signInUrl);
    renderer.drawCenteredText(SMALL_FONT_ID, qrY + kQrSize + 24, "Waiting for sign-in...", true);
  } else if (phase == Phase::SignedIn) {
    renderer.drawCenteredText(NOTOSANS_18_FONT_ID, pageHeight / 2 - 20, "Mynah is connected", true);
    renderer.drawCenteredText(SMALL_FONT_ID, pageHeight / 2 + 24, "Back to return", true);
  } else if (phase == Phase::Error) {
    renderer.drawCenteredText(NOTOSANS_14_FONT_ID, pageHeight / 2 - 20, status, true);
    renderer.drawCenteredText(SMALL_FONT_ID, pageHeight / 2 + 24, "Back to return", true);
  } else {
    renderer.drawCenteredText(NOTOSANS_14_FONT_ID, pageHeight / 2, "Starting secure pairing...", true);
  }
  renderer.displayBuffer();
}
