#include "EinqRoomScanner.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <NimBLEDevice.h>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>

#include "einq-config/EinqConfigStore.h"

namespace {

constexpr uint32_t kScanDurationMs = 2000;
constexpr uint32_t kLearningIntervalMs = 3000;
constexpr uint32_t kTrackingIntervalMs = 60000;
constexpr uint16_t kScanIntervalMs = 160;
constexpr uint16_t kScanWindowMs = 80;

std::mutex scannerMutex;
EinqRoom::Resolver resolver;
EinqRoom::Result currentResult {};
NimBLEScan* scanner = nullptr;
bool enabled = false;
bool scanCompleted = false;
uint32_t lastScanStartMs = 0;

std::string lowerText(std::string text) {
  std::transform(text.begin(), text.end(), text.begin(), [](unsigned char value) {
    return static_cast<char>(std::tolower(value));
  });
  return text;
}

void observeKey(const std::string& key, int rssi, uint32_t nowMs) {
  if (!key.empty()) {
    resolver.observe(lowerText(key).c_str(), rssi, nowMs);
  }
}

std::string iBeaconKey(const NimBLEAdvertisedDevice* device) {
  if (!device->haveManufacturerData()) {
    return {};
  }
  const std::string data = device->getManufacturerData();
  if (data.size() < 25 || static_cast<uint8_t>(data[0]) != 0x4c || static_cast<uint8_t>(data[1]) != 0x00 ||
      static_cast<uint8_t>(data[2]) != 0x02 || static_cast<uint8_t>(data[3]) != 0x15) {
    return {};
  }

  char key[64];
  int written = std::snprintf(key, sizeof(key), "ibeacon:");
  for (size_t index = 4; index < 20 && written > 0 && static_cast<size_t>(written) < sizeof(key); ++index) {
    written += std::snprintf(key + written, sizeof(key) - static_cast<size_t>(written), "%02x",
                             static_cast<uint8_t>(data[index]));
  }
  const uint16_t major = static_cast<uint16_t>((static_cast<uint8_t>(data[20]) << 8) | static_cast<uint8_t>(data[21]));
  const uint16_t minor = static_cast<uint16_t>((static_cast<uint8_t>(data[22]) << 8) | static_cast<uint8_t>(data[23]));
  std::snprintf(key + written, sizeof(key) - static_cast<size_t>(written), ":%u:%u", major, minor);
  return key;
}

class RoomScanCallbacks final : public NimBLEScanCallbacks {
  void onResult(const NimBLEAdvertisedDevice* device) override {
    const std::lock_guard<std::mutex> lock(scannerMutex);
    const uint32_t nowMs = millis();
    const int rssi = device->getRSSI();
    observeKey(device->getAddress().toString(), rssi, nowMs);
    if (device->haveName()) {
      observeKey(device->getName(), rssi, nowMs);
    }
    observeKey(iBeaconKey(device), rssi, nowMs);
  }

  void onScanEnd(const NimBLEScanResults& /*results*/, int /*reason*/) override {
    const std::lock_guard<std::mutex> lock(scannerMutex);
    scanCompleted = true;
  }
};

RoomScanCallbacks scanCallbacks;

void loadConfiguration() {
  JsonDocument document;
  if (deserializeJson(document, EinqConfigStore::load())) {
    return;
  }

  EinqRoom::Config config;
  config.minimumLead = document["roomMinimumLead"] | 5;
  resolver = EinqRoom::Resolver(config);

  const JsonArrayConst rooms = document["rooms"].as<JsonArrayConst>();
  for (const JsonObjectConst room : rooms) {
    const char* id = room["beaconId"] | "";
    const char* name = room["name"] | "";
    const int offset = room["calibrationOffset"] | 0;
    const std::string normalizedId = lowerText(id);
    resolver.addBeacon(normalizedId.c_str(), name, offset);
  }
  enabled = !rooms.isNull() && rooms.size() > 0;
}

}  // namespace

namespace EinqRoomScanner {

void begin() {
  const std::lock_guard<std::mutex> lock(scannerMutex);
  loadConfiguration();
  currentResult = {};
  scanCompleted = false;
  lastScanStartMs = 0;
  if (!enabled) {
    return;
  }

  scanner = NimBLEDevice::getScan();
  scanner->setScanCallbacks(&scanCallbacks, false);
  scanner->setActiveScan(true);
  scanner->setInterval(kScanIntervalMs);
  scanner->setWindow(kScanWindowMs);
  scanner->setMaxResults(0);
}

void poll() {
  const std::lock_guard<std::mutex> lock(scannerMutex);
  if (!enabled || scanner == nullptr) {
    return;
  }

  const uint32_t nowMs = millis();
  if (scanCompleted) {
    scanCompleted = false;
    currentResult = resolver.resolve(nowMs);
    scanner->clearResults();
  }

  if (scanner->isScanning()) {
    return;
  }

  const uint32_t interval = currentResult.known ? kTrackingIntervalMs : kLearningIntervalMs;
  if (lastScanStartMs != 0 && static_cast<uint32_t>(nowMs - lastScanStartMs) < interval) {
    return;
  }
  if (scanner->start(kScanDurationMs, false, true)) {
    lastScanStartMs = nowMs;
  }
}

void suspend() {
  const std::lock_guard<std::mutex> lock(scannerMutex);
  if (scanner != nullptr && scanner->isScanning()) {
    scanner->stop();
  }
}

void resume() {
  const std::lock_guard<std::mutex> lock(scannerMutex);
  lastScanStartMs = 0;
}

bool isScanning() {
  const std::lock_guard<std::mutex> lock(scannerMutex);
  return scanner != nullptr && scanner->isScanning();
}

EinqRoom::Result current() {
  const std::lock_guard<std::mutex> lock(scannerMutex);
  return currentResult;
}

}  // namespace EinqRoomScanner
