#pragma once

#include <Arduino.h>

#include "modules/feed/FeedData.h"

class BLEClient;
class BLEScan;
class BLERemoteCharacteristic;

class BleGatewayClient {
 public:
  struct StatusSnapshot {
    // True only while the BLE stack is active for the current request cycle.
    bool initialized = false;
    bool requestInFlight = false;
    bool timedOut = false;
    bool hasValidPayload = false;
    uint32_t requestStartedMs = 0;
    uint32_t lastSuccessLocalMs = 0;
    FeedData::Payload payload;
  };

  BleGatewayClient();

  bool begin();
  void setEnabled(bool enabled, bool requestImmediately = false);
  void update();
  void handleNotification(const uint8_t* data, size_t length);
  uint32_t getNextWorkDueMs(uint32_t nowMs) const;

  const StatusSnapshot& getStatus() const;

 private:
  enum class State : uint8_t {
    kIdle,
    kScanning,
    kConnectPending,
    kWaitingForResponse,
  };

  void beginRequest();
  void completeRequestSuccess(const FeedData::Payload& payload);
  void completeRequestTimeout();
  bool initializeBleStack();
  void deinitializeBleStack();
  bool scanForGateway();
  bool connectAndRequest();
  void disconnectClient();
  void resetResponseAssembly();

  bool moduleReady_ = false;
  bool enabled_ = true;
  StatusSnapshot status_{};
  State state_ = State::kIdle;
  BLEScan* scan_ = nullptr;
  BLEClient* client_ = nullptr;
  BLERemoteCharacteristic* feedDataCharacteristic_ = nullptr;
  BLERemoteCharacteristic* broadcastCharacteristic_ = nullptr;
  String gatewayAddress_;
  String responseBuffer_;
  uint16_t expectedNextChunkIndex_ = 1;
  uint16_t expectedTotalChunks_ = 0;
  FeedData::Payload pendingPayload_;
  bool hasPendingPayload_ = false;
  uint32_t nextRequestDueMs_ = 0;
  uint32_t lastScanAttemptMs_ = 0;
};
