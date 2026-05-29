#pragma once

#include "EinqDisplaySnapshot.h"

/** NimBLE GATT surface: read/notify current display; write to show text or clock. */
class EinqBle {
 public:
  static void begin();
  static void setSnapshot(const EinqDisplaySnapshot& snapshot);
  /** Returns true once per received BLE write; clears the pending flag. */
  static bool takeCommand(EinqDisplayCommand& out);
  static void notifyDisplayChanged();
  static void suspendForSleep();
  static void resumeAfterSleep();
};
