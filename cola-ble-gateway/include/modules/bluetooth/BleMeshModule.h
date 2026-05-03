#pragma once

#include <Arduino.h>

#include <vector>

#include "modules/display/LcdModule.h"
#include "modules/network/WifiProvisioningModule.h"

class BLEServer;
class BLEService;
class BLECharacteristic;

class BleMeshModule {
 public:
  explicit BleMeshModule(LcdModule& lcd);

  void begin();
  void loop();
  void handleWifiStateChange(WifiProvisioningModule::ConnectionState state);

  // Internal callback entry points used by BLE adapter callbacks.
  void onClientConnected();
  void onClientDisconnected();
  void onClientBroadcast(const String& payload);

 private:
  struct FeedRecord {
    String id;
    String startTime;
    String endTime;
    int duration = 0;
  };

  void startMesh();
  void stopMesh();

  void refreshMockFeedCache();
  String buildFeedPayloadJson() const;

  void updateScreen() const;

  LcdModule& lcd_;

  BLEServer* bleServer_ = nullptr;
  BLEService* feedService_ = nullptr;
  BLECharacteristic* feedDataCharacteristic_ = nullptr;
  BLECharacteristic* broadcastCharacteristic_ = nullptr;

  bool meshRunning_ = false;
  bool restartAdvertisingPending_ = false;
  uint16_t clientCount_ = 0;

  String serverTimeCache_;
  std::vector<FeedRecord> feedRecordsCache_;
};
