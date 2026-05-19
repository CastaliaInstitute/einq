#include "EinqBle.h"

#include <ArduinoJson.h>
#include <NimBLEDevice.h>

#include <cstring>
#include <mutex>

namespace {

constexpr char kDeviceName[] = "Einq";
// Castalia Einq GATT (provisional; stable across firmware releases)
constexpr char kServiceUuid[] = "a1b2c3d4-e5f6-4789-a012-3456789abcde";
constexpr char kDisplayStateUuid[] = "a1b2c3d4-e5f6-4789-a012-3456789abc01";
constexpr char kDisplayCmdUuid[] = "a1b2c3d4-e5f6-4789-a012-3456789abc02";

std::mutex gMutex;
EinqDisplaySnapshot gSnapshot {};
bool gCommandPending = false;
EinqDisplayCommand gPendingCommand {};

NimBLECharacteristic* gStateChar = nullptr;

void copyField(char* dest, size_t destLen, const char* src) {
  if (!src) {
    dest[0] = '\0';
    return;
  }
  strncpy(dest, src, destLen - 1);
  dest[destLen - 1] = '\0';
}

bool parseDisplayCommand(const uint8_t* data, size_t len, EinqDisplayCommand& out) {
  JsonDocument doc;
  const DeserializationError err = deserializeJson(doc, data, len);
  if (err) {
    return false;
  }

  const char* mode = doc["mode"] | "clock";
  copyField(out.mode, sizeof(out.mode), mode);

  if (strcmp(out.mode, "message") == 0) {
    copyField(out.title, sizeof(out.title), doc["title"] | "Einq");
    copyField(out.line1, sizeof(out.line1), doc["line1"] | "");
    copyField(out.line2, sizeof(out.line2), doc["line2"] | "");
    copyField(out.line3, sizeof(out.line3), doc["line3"] | "");
    const JsonArrayConst lines = doc["lines"].as<JsonArrayConst>();
    if (!lines.isNull()) {
      if (lines.size() > 0 && out.line1[0] == '\0') {
        copyField(out.line1, sizeof(out.line1), lines[0] | "");
      }
      if (lines.size() > 1 && out.line2[0] == '\0') {
        copyField(out.line2, sizeof(out.line2), lines[1] | "");
      }
      if (lines.size() > 2 && out.line3[0] == '\0') {
        copyField(out.line3, sizeof(out.line3), lines[2] | "");
      }
    }
  } else if (strcmp(out.mode, "clock") != 0) {
    return false;
  }

  out.valid = true;
  return true;
}

size_t snapshotToJson(const EinqDisplaySnapshot& snap, char* buf, size_t bufLen) {
  JsonDocument doc;
  doc["mode"] = snap.mode;
  doc["title"] = snap.title;
  if (snap.line1[0] != '\0') {
    doc["line1"] = snap.line1;
  }
  if (snap.line2[0] != '\0') {
    doc["line2"] = snap.line2;
  }
  if (snap.line3[0] != '\0') {
    doc["line3"] = snap.line3;
  }
  if (snap.time[0] != '\0') {
    doc["time"] = snap.time;
  }
  if (snap.day[0] != '\0') {
    doc["day"] = snap.day;
  }
  if (snap.date[0] != '\0') {
    doc["date"] = snap.date;
  }
  return serializeJson(doc, buf, bufLen);
}

class DisplayStateCallbacks : public NimBLECharacteristicCallbacks {
  void onRead(NimBLECharacteristic* characteristic, NimBLEConnInfo& /*connInfo*/) override {
    char json[384];
    {
      const std::lock_guard<std::mutex> lock(gMutex);
      snapshotToJson(gSnapshot, json, sizeof(json));
    }
    characteristic->setValue(reinterpret_cast<const uint8_t*>(json), strlen(json));
  }

  void onSubscribe(NimBLECharacteristic* /*characteristic*/, NimBLEConnInfo& /*connInfo*/, uint16_t /*subVal*/) override {
    EinqBle::notifyDisplayChanged();
  }
};

class DisplayCmdCallbacks : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic* characteristic, NimBLEConnInfo& /*connInfo*/) override {
    const NimBLEAttValue& value = characteristic->getAttVal();
    EinqDisplayCommand cmd {};
    if (!parseDisplayCommand(value.data(), value.length(), cmd)) {
      return;
    }
    {
      const std::lock_guard<std::mutex> lock(gMutex);
      gPendingCommand = cmd;
      gCommandPending = true;
    }
    EinqBle::notifyDisplayChanged();
  }
};

}  // namespace

void EinqBle::begin() {
  NimBLEDevice::init(kDeviceName);
  NimBLEDevice::setPower(ESP_PWR_LVL_P9);

  NimBLEServer* server = NimBLEDevice::createServer();
  NimBLEService* service = server->createService(kServiceUuid);

  gStateChar = service->createCharacteristic(kDisplayStateUuid, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);
  gStateChar->setCallbacks(new DisplayStateCallbacks());

  NimBLECharacteristic* cmdChar =
      service->createCharacteristic(kDisplayCmdUuid, NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR);
  cmdChar->setCallbacks(new DisplayCmdCallbacks());

  service->start();

  NimBLEAdvertising* advertising = NimBLEDevice::getAdvertising();
  advertising->addServiceUUID(kServiceUuid);
  advertising->setScanResponse(true);
  advertising->start();
}

void EinqBle::setSnapshot(const EinqDisplaySnapshot& snapshot) {
  const std::lock_guard<std::mutex> lock(gMutex);
  gSnapshot = snapshot;
}

bool EinqBle::takeCommand(EinqDisplayCommand& out) {
  const std::lock_guard<std::mutex> lock(gMutex);
  if (!gCommandPending) {
    return false;
  }
  out = gPendingCommand;
  gCommandPending = false;
  return out.valid;
}

void EinqBle::notifyDisplayChanged() {
  if (gStateChar == nullptr) {
    return;
  }
  char json[384];
  {
    const std::lock_guard<std::mutex> lock(gMutex);
    snapshotToJson(gSnapshot, json, sizeof(json));
  }
  gStateChar->setValue(reinterpret_cast<const uint8_t*>(json), strlen(json));
  gStateChar->notify();
}
