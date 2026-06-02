#include "modules/bluetooth/BleMeshModule.h"

#include <ArduinoJson.h>
#include <BLE2902.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <stdio.h>

namespace {
constexpr const char* kBleDeviceName = "Cola-gateway-mesh";

constexpr const char* kFeedServiceUuid = "4e6a1000-5c4f-45af-8f1f-c41c01ab1000";
constexpr const char* kFeedDataCharacteristicUuid = "4e6a1001-5c4f-45af-8f1f-c41c01ab1001";
constexpr const char* kBroadcastCharacteristicUuid = "4e6a1002-5c4f-45af-8f1f-c41c01ab1002";

struct DateTimeParts {
  int year = 0;
  int month = 0;
  int day = 0;
  int hour = 0;
  int minute = 0;
  int second = 0;
};

bool isLeapYear(int year) {
  return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
}

int daysInMonth(int year, int month) {
  static const int kDaysByMonth[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  if (month == 2) {
    return isLeapYear(year) ? 29 : 28;
  }

  if (month < 1 || month > 12) {
    return 31;
  }

  return kDaysByMonth[month - 1];
}

bool parseDateTime(const String& value, DateTimeParts* dateTime) {
  if (dateTime == nullptr) {
    return false;
  }

  const int matched = sscanf(value.c_str(),
                             "%d-%d-%d %d:%d:%d",
                             &dateTime->year,
                             &dateTime->month,
                             &dateTime->day,
                             &dateTime->hour,
                             &dateTime->minute,
                             &dateTime->second);
  if (matched != 6) {
    return false;
  }

  if (dateTime->year < 1970 || dateTime->month < 1 || dateTime->month > 12 || dateTime->day < 1 ||
      dateTime->hour < 0 || dateTime->hour > 23 || dateTime->minute < 0 || dateTime->minute > 59 ||
      dateTime->second < 0 || dateTime->second > 59) {
    return false;
  }

  return dateTime->day <= daysInMonth(dateTime->year, dateTime->month);
}

void addSeconds(DateTimeParts* dateTime, unsigned long secondsToAdd) {
  if (dateTime == nullptr) {
    return;
  }

  unsigned long totalSeconds = static_cast<unsigned long>(dateTime->second) + secondsToAdd;
  dateTime->second = static_cast<int>(totalSeconds % 60UL);

  unsigned long totalMinutes = static_cast<unsigned long>(dateTime->minute) + (totalSeconds / 60UL);
  dateTime->minute = static_cast<int>(totalMinutes % 60UL);

  unsigned long totalHours = static_cast<unsigned long>(dateTime->hour) + (totalMinutes / 60UL);
  dateTime->hour = static_cast<int>(totalHours % 24UL);

  unsigned long extraDays = totalHours / 24UL;
  while (extraDays > 0UL) {
    ++dateTime->day;
    if (dateTime->day > daysInMonth(dateTime->year, dateTime->month)) {
      dateTime->day = 1;
      ++dateTime->month;
      if (dateTime->month > 12) {
        dateTime->month = 1;
        ++dateTime->year;
      }
    }

    --extraDays;
  }
}

String formatDateTime(const DateTimeParts& dateTime) {
  char buffer[20] = {0};
  snprintf(buffer,
           sizeof(buffer),
           "%04d-%02d-%02d %02d:%02d:%02d",
           dateTime.year,
           dateTime.month,
           dateTime.day,
           dateTime.hour,
           dateTime.minute,
           dateTime.second);
  return String(buffer);
}

String buildCurrentServerTime(const String& sourceServerTime, uint32_t receivedAtMs) {
  if (sourceServerTime.isEmpty()) {
    return "";
  }

  DateTimeParts dateTime;
  if (!parseDateTime(sourceServerTime, &dateTime)) {
    return sourceServerTime;
  }

  const unsigned long elapsedSeconds = (millis() - receivedAtMs) / 1000UL;
  addSeconds(&dateTime, elapsedSeconds);
  return formatDateTime(dateTime);
}

class BleServerCallbacks : public BLEServerCallbacks {
 public:
  explicit BleServerCallbacks(BleMeshModule* owner) : owner_(owner) {}

  void onConnect(BLEServer* /*server*/) override {
    if (owner_ != nullptr) {
      owner_->onClientConnected();
    }
  }

  void onDisconnect(BLEServer* /*server*/) override {
    if (owner_ != nullptr) {
      owner_->onClientDisconnected();
    }
  }

 private:
  BleMeshModule* owner_;
};

class BroadcastWriteCallbacks : public BLECharacteristicCallbacks {
 public:
  explicit BroadcastWriteCallbacks(BleMeshModule* owner) : owner_(owner) {}

  void onWrite(BLECharacteristic* characteristic) override {
    if (owner_ == nullptr || characteristic == nullptr) {
      return;
    }

    const std::string value = characteristic->getValue();
    owner_->onClientBroadcast(String(value.c_str()));
  }

 private:
  BleMeshModule* owner_;
};

}  // namespace

BleMeshModule::BleMeshModule(LcdModule& lcd) : lcd_(lcd) {}

void BleMeshModule::begin() {
  refreshFeedCache();
}

void BleMeshModule::loop() {
  if (!meshRunning_ || !restartAdvertisingPending_ || bleServer_ == nullptr) {
    return;
  }

  // Restart advertising in the main loop instead of callback context to avoid
  // race conditions in some BLE stacks when a client disconnects.
  bleServer_->startAdvertising();
  restartAdvertisingPending_ = false;
}

void BleMeshModule::handleWifiStateChange(WifiProvisioningModule::ConnectionState state) {
  if (state == WifiProvisioningModule::ConnectionState::kConnected) {
    startMesh();
    return;
  }

  stopMesh();
}

bool BleMeshModule::replaceFeedCache(const String& serverTime,
                                     const std::vector<FeedRecord>& records,
                                     const String& weatherDataJson,
                                     bool weatherDataIsNull) {
  feedCache_.sourceServerTime = serverTime;
  feedCache_.receivedAtMs = millis();
  feedCache_.records = records;
  feedCache_.weatherDataJson = weatherDataJson;
  feedCache_.weatherDataIsNull = weatherDataIsNull;
  return true;
}

void BleMeshModule::startMesh() {
  if (meshRunning_) {
    updateScreen();
    return;
  }

  BLEDevice::init(kBleDeviceName);
  bleServer_ = BLEDevice::createServer();
  bleServer_->setCallbacks(new BleServerCallbacks(this));

  feedService_ = bleServer_->createService(kFeedServiceUuid);

  // Notify characteristic for cached feed payload delivery.
  feedDataCharacteristic_ =
      feedService_->createCharacteristic(kFeedDataCharacteristicUuid, BLECharacteristic::PROPERTY_NOTIFY);
  feedDataCharacteristic_->addDescriptor(new BLE2902());

  // Client writes a broadcast packet here, then gateway pushes feed payload back.
  broadcastCharacteristic_ = feedService_->createCharacteristic(
      kBroadcastCharacteristicUuid, BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_WRITE_NR);
  broadcastCharacteristic_->setCallbacks(new BroadcastWriteCallbacks(this));

  feedService_->start();
  bleServer_->getAdvertising()->addServiceUUID(kFeedServiceUuid);
  bleServer_->startAdvertising();

  meshRunning_ = true;
  clientCount_ = 0;
  restartAdvertisingPending_ = false;
  updateScreen();
}

void BleMeshModule::stopMesh() {
  if (!meshRunning_) {
    return;
  }

  BLEDevice::getAdvertising()->stop();
  BLEDevice::deinit(false);

  bleServer_ = nullptr;
  feedService_ = nullptr;
  feedDataCharacteristic_ = nullptr;
  broadcastCharacteristic_ = nullptr;

  meshRunning_ = false;
  restartAdvertisingPending_ = false;
  clientCount_ = 0;
}

void BleMeshModule::refreshFeedCache() {
  feedCache_.sourceServerTime = "";
  feedCache_.receivedAtMs = 0;
  feedCache_.records.clear();
  feedCache_.weatherDataJson = "";
  feedCache_.weatherDataIsNull = true;
}

String BleMeshModule::buildFeedPayloadJson() const {
  const size_t docCapacity = 2048U + (feedCache_.records.size() * 256U);
  DynamicJsonDocument doc(docCapacity);
  doc["serverTime"] = buildCurrentServerTime(feedCache_.sourceServerTime, feedCache_.receivedAtMs);

  JsonArray records = doc.createNestedArray("records");
  for (const FeedRecord& record : feedCache_.records) {
    JsonObject item = records.createNestedObject();
    item["id"] = record.id;
    item["startTime"] = record.startTime;
    item["endTime"] = record.endTime;
    item["duration"] = record.duration;
  }

  String payload;
  serializeJson(doc, payload);

  if (!payload.isEmpty() && payload[payload.length() - 1] == '}') {
    payload.remove(payload.length() - 1);
  }

  payload += ",\"weatherData\":";
  if (feedCache_.weatherDataIsNull || feedCache_.weatherDataJson.isEmpty()) {
    payload += "null";
  } else {
    payload += feedCache_.weatherDataJson;
  }
  payload += '}';

  return payload;
}

void BleMeshModule::onClientConnected() {
  ++clientCount_;
  updateScreen();
}

void BleMeshModule::onClientDisconnected() {
  if (clientCount_ > 0) {
    --clientCount_;
  }

  restartAdvertisingPending_ = true;
  updateScreen();
}

void BleMeshModule::onClientBroadcast(const String& payload) {
  if (!meshRunning_ || feedDataCharacteristic_ == nullptr) {
    return;
  }

  DynamicJsonDocument requestDoc(512);
  const DeserializationError parseError = deserializeJson(requestDoc, payload);
  if (!parseError) {
    const String deviceName = String(requestDoc["device_name"] | "");
    if (!deviceName.isEmpty() && deviceName != "Cola-ePaper") {
      return;
    }
  }

  const String feedPayload = buildFeedPayloadJson();
  feedDataCharacteristic_->setValue(feedPayload.c_str());
  feedDataCharacteristic_->notify();
}

void BleMeshModule::updateScreen() const {
  if (!meshRunning_) {
    return;
  }

  lcd_.clearScreen(ST77XX_BLACK);
  lcd_.printUtf8("蓝牙模式", 0, 18, ST77XX_CYAN);
  lcd_.printUtf8("客户端数量", 0, 42, ST77XX_WHITE);
  lcd_.printText(String(clientCount_), 96, 42, ST77XX_GREEN, 1);
}
