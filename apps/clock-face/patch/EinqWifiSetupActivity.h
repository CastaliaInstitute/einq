#pragma once

#include <cstdint>

#include "activities/Activity.h"

/** QR-guided WiFi setup: join Einq AP, then open captive /settings page. */
class EinqWifiSetupActivity final : public Activity {
  enum class Phase : uint8_t { Starting, Running, Saved };

  Phase phase = Phase::Starting;
  std::string apSsid;
  std::string settingsUrl;
  std::string wifiQrPayload;
  std::string apIp;

  void drawQrScreen() const;

 public:
  explicit EinqWifiSetupActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("EinqWifiSetup", renderer, mappedInput) {}

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;
  bool skipLoopDelay() override { return phase == Phase::Running; }
  bool preventAutoSleep() override { return phase == Phase::Running; }
};
