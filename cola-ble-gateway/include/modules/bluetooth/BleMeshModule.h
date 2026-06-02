#pragma once

#include <Arduino.h>

#include <vector>

#include "modules/display/LcdModule.h"
#include "modules/feed/FeedRecord.h"
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
  bool replaceFeedCache(const String& serverTime,
                        const std::vector<FeedRecord>& records,
                        const String& weatherDataJson,
                        bool weatherDataIsNull);

  // Internal callback entry points used by BLE adapter callbacks.
  void onClientConnected();
  void onClientDisconnected();
  void onClientBroadcast(const String& payload);

 private:
  struct FeedSyncCache {
    String sourceServerTime;
    uint32_t receivedAtMs = 0;
    std::vector<FeedRecord> records;
    String weatherDataJson;
    bool weatherDataIsNull = true;
  };

  void startMesh();
  void stopMesh();

  void refreshFeedCache();
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

  FeedSyncCache feedCache_;
};
