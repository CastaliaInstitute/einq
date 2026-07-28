#pragma once

#include "EinqRoomResolver.h"

namespace EinqRoomScanner {

/**
 * Load configured beacons and attach a short-window NimBLE scanner.
 * EinqBle::begin() must be called first so the NimBLE host already exists.
 */
void begin();

/** Start due scans and publish completed resolver results. */
void poll();

/** Stop scanning before radio sleep or WiFi-intensive work. */
void suspend();

/** Allow scanning again after wake. */
void resume();

bool isScanning();
EinqRoom::Result current();

}  // namespace EinqRoomScanner
