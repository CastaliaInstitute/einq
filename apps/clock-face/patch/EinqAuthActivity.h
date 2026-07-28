#pragma once

#include "activities/Activity.h"
#include "einq-auth/EinqAuth.h"

class EinqAuthActivity final : public Activity {
  enum class Phase { Starting, Pairing, SignedIn, Error };

  Phase phase = Phase::Starting;
  EinqPairing pairing {};
  unsigned long lastPollMs = 0;
  int pollErrors = 0;
  char status[64] = {};

  bool connectWifi();
  void setError(const char* message);

 public:
  explicit EinqAuthActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("EinqAuth", renderer, mappedInput) {}

  void onEnter() override;
  void loop() override;
  void render(RenderLock&& renderLock) override;
  bool preventAutoSleep() override { return true; }
};
