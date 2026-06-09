#pragma once

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include <vector>

#include "modules/display/DisplayService.h"
#include "modules/feed/FeedRecord.h"
#include "modules/network/WifiProvisioningModule.h"

class BLEServer;
class BLEService;
class BLECharacteristic;

class BleMeshModule {
 public:
  explicit BleMeshModule(DisplayService& display);

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
  static constexpr size_t kPendingBroadcastPayloadMaxBytes = 128U;

  struct FeedSyncCache {
    String sourceServerTime;
    uint32_t receivedAtMs = 0;
    std::vector<FeedRecord> records;
    String weatherDataJson;
    bool weatherDataIsNull = true;
  };

  struct PendingBroadcastRequest {
    char payload[kPendingBroadcastPayloadMaxBytes] = {0};
    size_t length = 0;
    bool pending = false;
  };

  struct PendingFeedTransfer {
    String payload;
    size_t totalChunks = 0;
    size_t nextChunkIndex = 0;
    uint32_t lastChunkSentAtMs = 0;
    bool active = false;
  };

  void startMesh();
  void stopMesh();

  void refreshFeedCache();
  String buildFeedPayloadJson() const;
  void processPendingBroadcast();

  void updateScreen();
  bool lock(TickType_t waitTicks = portMAX_DELAY) const;
  void unlock() const;

  DisplayService& display_;
  mutable SemaphoreHandle_t mutex_ = nullptr;

  BLEServer* bleServer_ = nullptr;
  BLEService* feedService_ = nullptr;
  BLECharacteristic* feedDataCharacteristic_ = nullptr;
  BLECharacteristic* broadcastCharacteristic_ = nullptr;

  bool meshRunning_ = false;
  bool restartAdvertisingPending_ = false;
  uint16_t clientCount_ = 0;

  FeedSyncCache feedCache_;
  PendingBroadcastRequest pendingBroadcastRequest_;
  PendingFeedTransfer pendingFeedTransfer_;
};
