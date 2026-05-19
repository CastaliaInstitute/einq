#pragma once

#include "activities/Activity.h"
#include "einq-ble/EinqDisplaySnapshot.h"

/** Castalia Einq demo: clock face or BLE-driven message; refreshes each minute in clock mode. */
class EinqClockActivity final : public Activity {
  unsigned long lastDrawMs = 0;
  int lastMinute = -1;
  bool messageMode = false;
  EinqDisplaySnapshot messageSnapshot {};

  void drawClockFace(const struct tm& localTime);
  void drawMessageFace();
  void drawFace();
  void publishSnapshot(const struct tm* localTime);

 public:
  explicit EinqClockActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("EinqClock", renderer, mappedInput) {}

  void onEnter() override;
  void loop() override;
  bool preventAutoSleep() override { return true; }
  bool skipLoopDelay() override { return true; }
};
