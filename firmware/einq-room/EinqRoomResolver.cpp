#include "EinqRoomResolver.h"

#include <cmath>
#include <cstring>

namespace {

void copyText(char* destination, size_t length, const char* source) {
  if (length == 0) {
    return;
  }
  if (source == nullptr) {
    destination[0] = '\0';
    return;
  }
  std::strncpy(destination, source, length - 1);
  destination[length - 1] = '\0';
}

bool sameText(const char* left, const char* right) {
  return std::strncmp(left, right, EinqRoom::kRoomLength) == 0;
}

}  // namespace

namespace EinqRoom {

Resolver::Resolver(Config config) : config_(config) {
  if (config_.smoothing <= 0.0F || config_.smoothing > 1.0F) {
    config_.smoothing = 0.35F;
  }
  if (config_.switchWins == 0) {
    config_.switchWins = 1;
  }
}

bool Resolver::addBeacon(const char* beaconId, const char* room, int calibrationOffset) {
  if (beaconId == nullptr || beaconId[0] == '\0' || room == nullptr || room[0] == '\0') {
    return false;
  }

  for (Beacon& beacon : beacons_) {
    if (beacon.configured && std::strncmp(beacon.id, beaconId, kIdLength) == 0) {
      copyText(beacon.room, sizeof(beacon.room), room);
      beacon.calibrationOffset = calibrationOffset;
      return true;
    }
  }

  for (Beacon& beacon : beacons_) {
    if (!beacon.configured) {
      copyText(beacon.id, sizeof(beacon.id), beaconId);
      copyText(beacon.room, sizeof(beacon.room), room);
      beacon.calibrationOffset = calibrationOffset;
      beacon.configured = true;
      return true;
    }
  }
  return false;
}

bool Resolver::observe(const char* beaconId, int rssi, uint32_t nowMs) {
  for (Beacon& beacon : beacons_) {
    if (!beacon.configured || std::strncmp(beacon.id, beaconId, kIdLength) != 0) {
      continue;
    }
    const float corrected = static_cast<float>(rssi + beacon.calibrationOffset);
    beacon.smoothedRssi = beacon.seen
        ? config_.smoothing * corrected + (1.0F - config_.smoothing) * beacon.smoothedRssi
        : corrected;
    beacon.lastSeenMs = nowMs;
    beacon.seen = true;
    return true;
  }
  return false;
}

Result Resolver::resolve(uint32_t nowMs) {
  Result result {};
  const Beacon* best = nullptr;
  float bestScore = -128.0F;
  float runnerUp = -128.0F;

  for (const Beacon& beacon : beacons_) {
    if (!beacon.configured || !beacon.seen ||
        static_cast<uint32_t>(nowMs - beacon.lastSeenMs) > config_.sampleTtlMs ||
        beacon.smoothedRssi < static_cast<float>(config_.minimumRssi)) {
      continue;
    }
    if (beacon.smoothedRssi > bestScore) {
      runnerUp = bestScore;
      bestScore = beacon.smoothedRssi;
      best = &beacon;
    } else if (beacon.smoothedRssi > runnerUp) {
      runnerUp = beacon.smoothedRssi;
    }
  }

  if (best == nullptr) {
    pendingRoom_[0] = '\0';
    pendingWins_ = 0;
    return result;
  }

  const int lead = runnerUp <= -128.0F
      ? 127
      : static_cast<int>(std::lround(bestScore - runnerUp));
  if (lead < config_.minimumLead) {
    copyText(result.room, sizeof(result.room), currentRoom_);
    result.known = currentRoom_[0] != '\0';
    result.score = static_cast<int>(std::lround(bestScore));
    result.lead = lead;
    return result;
  }

  if (sameText(best->room, currentRoom_)) {
    pendingRoom_[0] = '\0';
    pendingWins_ = 0;
  } else if (sameText(best->room, pendingRoom_)) {
    if (pendingWins_ < UINT8_MAX) {
      ++pendingWins_;
    }
  } else {
    copyText(pendingRoom_, sizeof(pendingRoom_), best->room);
    pendingWins_ = 1;
  }

  if (pendingWins_ >= config_.switchWins) {
    copyText(currentRoom_, sizeof(currentRoom_), pendingRoom_);
    pendingRoom_[0] = '\0';
    pendingWins_ = 0;
    result.changed = true;
  }

  copyText(result.room, sizeof(result.room), currentRoom_);
  result.known = currentRoom_[0] != '\0';
  result.score = static_cast<int>(std::lround(bestScore));
  result.lead = lead;
  return result;
}

void Resolver::clear() {
  for (Beacon& beacon : beacons_) {
    beacon = {};
  }
  currentRoom_[0] = '\0';
  pendingRoom_[0] = '\0';
  pendingWins_ = 0;
}

}  // namespace EinqRoom
