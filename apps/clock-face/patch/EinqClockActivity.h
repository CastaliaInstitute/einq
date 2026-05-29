#pragma once

#include "activities/Activity.h"
#include "einq-ble/EinqDisplaySnapshot.h"
#include "einq-cotd/EinqCotd.h"
#include "einq-glyph/EinqGlyph.h"

/** Castalia Einq demo: inquiry glyph + clock; syncs Card of the Day from cards.castalia.institute. */
class EinqClockActivity final : public Activity {
  unsigned long lastDrawMs = 0;
  int lastMinute = -1;
  int lastDayOfYear = -1;
  EinqGlyph::Kind currentGlyph = EinqGlyph::Kind::Person;
  EinqCotdCard cotdCard {};
  char cotdDateLoaded[16] = {};
  char otaCheckedDate[16] = {};
  bool messageMode = false;
  EinqDisplaySnapshot messageSnapshot {};

  void drawClockFace(const struct tm& localTime);
  void drawMessageFace();
  void drawFace();
  void publishSnapshot(const struct tm* localTime);
  void syncCotdForDate(const struct tm& localTime);
  void maybeMidnightOta(const struct tm& localTime);
  bool ensureWifiForSync();

 public:
  explicit EinqClockActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("EinqClock", renderer, mappedInput) {}

  void onEnter() override;
  void loop() override;
  bool preventAutoSleep() override { return true; }
  bool skipLoopDelay() override { return true; }
};
