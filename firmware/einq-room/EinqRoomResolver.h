#pragma once

#include <cstddef>
#include <cstdint>

namespace EinqRoom {

constexpr size_t kMaxBeacons = 16;
constexpr size_t kIdLength = 64;
constexpr size_t kRoomLength = 32;

struct Config {
  float smoothing = 0.35F;
  int minimumRssi = -88;
  int minimumLead = 5;
  uint8_t switchWins = 3;
  uint32_t sampleTtlMs = 12000;
};

struct Result {
  char room[kRoomLength] {};
  int score = -127;
  int lead = 0;
  bool known = false;
  bool changed = false;
};

class Resolver {
 public:
  explicit Resolver(Config config = {});

  bool addBeacon(const char* beaconId, const char* room, int calibrationOffset = 0);
  bool observe(const char* beaconId, int rssi, uint32_t nowMs);
  Result resolve(uint32_t nowMs);
  void clear();

 private:
  struct Beacon {
    char id[kIdLength] {};
    char room[kRoomLength] {};
    float smoothedRssi = -127.0F;
    int calibrationOffset = 0;
    uint32_t lastSeenMs = 0;
    bool configured = false;
    bool seen = false;
  };

  Config config_;
  Beacon beacons_[kMaxBeacons] {};
  char currentRoom_[kRoomLength] {};
  char pendingRoom_[kRoomLength] {};
  uint8_t pendingWins_ = 0;
};

}  // namespace EinqRoom
