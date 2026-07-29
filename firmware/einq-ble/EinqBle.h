#pragma once

#include "EinqDisplaySnapshot.h"

/** NimBLE GATT surface: read/notify current display; write to show text or clock. */
class EinqBle {
 public:
  static void begin();
  /** Advertise the active iNQ Card title in the BLE scan-response name. */
  static void setAdvertisedCard(const char* title);
  static void setSnapshot(const EinqDisplaySnapshot& snapshot);
  /** Returns true once per received BLE write; clears the pending flag. */
  static bool takeCommand(EinqDisplayCommand& out);
  static void notifyDisplayChanged();
  /** Fully release the BLE stack before memory-heavy WiFi/TLS work. */
  static void shutdown();
  static void suspendForSleep();
  static void resumeAfterSleep();
};
