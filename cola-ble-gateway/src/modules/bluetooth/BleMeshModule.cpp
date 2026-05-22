#include "modules/bluetooth/BleMeshModule.h"

#include <ArduinoJson.h>
#include <BLE2902.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>

namespace {
constexpr const char* kBleDeviceName = "Cola-gateway-mesh";

constexpr const char* kFeedServiceUuid = "4e6a1000-5c4f-45af-8f1f-c41c01ab1000";
constexpr const char* kFeedDataCharacteristicUuid = "4e6a1001-5c4f-45af-8f1f-c41c01ab1001";
constexpr const char* kBroadcastCharacteristicUuid = "4e6a1002-5c4f-45af-8f1f-c41c01ab1002";

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

bool BleMeshModule::replaceFeedCache(const String& serverTime, const std::vector<FeedRecord>& records) {
  serverTimeCache_ = serverTime;
  feedRecordsCache_ = records;
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
  serverTimeCache_ = "";
  feedRecordsCache_.clear();
}

String BleMeshModule::buildFeedPayloadJson() const {
  DynamicJsonDocument doc(2048);
  doc["serverTime"] = serverTimeCache_;

  JsonArray records = doc.createNestedArray("records");
  for (const FeedRecord& record : feedRecordsCache_) {
    JsonObject item = records.createNestedObject();
    item["id"] = record.id;
    item["startTime"] = record.startTime;
    item["endTime"] = record.endTime;
    item["duration"] = record.duration;
  }

  String payload;
  serializeJson(doc, payload);
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
