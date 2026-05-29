#pragma once

#include <cstdint>

struct tm;

/** Wall-clock scheduling helpers for Einq faces (light sleep between refreshes). */
namespace EinqSchedule {

/** Seconds until the next local minute boundary (1..60). */
uint32_t secondsUntilNextMinute(const struct tm& localTime);

/** Seconds until the next local midnight (1..86400). */
uint32_t secondsUntilMidnight(const struct tm& localTime);

/** Next wake: sooner of minute tick or midnight glyph rotation. */
uint32_t nextWakeSeconds(const struct tm& localTime);

/** Light sleep until timer or power-button GPIO wake. Returns ESP wake cause. */
int lightSleepForSeconds(uint32_t seconds);

}  // namespace EinqSchedule
