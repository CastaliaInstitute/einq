#pragma once

#include "activities/Activity.h"
#include "einq-ble/EinqDisplaySnapshot.h"
#include "einq-cotd/EinqCotd.h"
#include "einq-glyph/EinqGlyph.h"
#include "einq-home/EinqHome.h"

/** Castalia Einq demo: inquiry glyph + clock; syncs Card of the Day from cards.castalia.institute. */
class EinqClockActivity final : public Activity {
 public:
  enum class Face : unsigned char {
    Day,
    Calendar,
    SelfWeather,
    Synastry,
    Family,
    Fortune,
    Card,
    Spotify,
    Lights
  };

 private:
  unsigned long lastDrawMs = 0;
  unsigned long lastHomeSyncMs = 0;
  unsigned long lastAuthRefreshMs = 0;
  int lastMinute = -1;
  int lastDayOfYear = -1;
  EinqGlyph::Kind currentGlyph = EinqGlyph::Kind::Person;
  EinqCotdCard cotdCard {};
  char cotdDateLoaded[16] = {};
  char otaCheckedDate[16] = {};
  bool messageMode = false;
  EinqDisplaySnapshot messageSnapshot {};
  char currentRoom[32] = {};
  EinqHomePayload home {};
  Face face = Face::Day;
  bool actionPending = false;

  void drawClockFace(const struct tm& localTime);
  void drawHomeFace();
  void drawMessageFace();
  void drawFace();
  void publishSnapshot(const struct tm* localTime);
  void syncCotdForDate(const struct tm& localTime);
  void maybeMidnightOta(const struct tm& localTime);
  void syncHome(bool allowNetwork);
  bool faceAvailable(Face candidate) const;
  void moveFace(int direction);
  void performFaceAction(const char* requestedAction = nullptr);
  void openWifiSetup();
  void openCastaliaPairing();
  bool ensureWifiForSync();

 public:
  explicit EinqClockActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("EinqClock", renderer, mappedInput) {}

  void onEnter() override;
  void loop() override;
  bool preventAutoSleep() override { return true; }
  bool skipLoopDelay() override { return true; }
};
