#pragma once

#include "activities/Activity.h"

/** Castalia Einq demo: large clock face; refreshes each minute. Back with Confirm. */
class EinqClockActivity final : public Activity {
  unsigned long lastDrawMs = 0;
  int lastMinute = -1;

  void drawFace();

 public:
  explicit EinqClockActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("EinqClock", renderer, mappedInput) {}

  void onEnter() override;
  void loop() override;
  bool preventAutoSleep() override { return true; }
  bool skipLoopDelay() override { return true; }
};
