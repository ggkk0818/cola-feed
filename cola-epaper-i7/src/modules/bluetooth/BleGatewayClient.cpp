#include "modules/bluetooth/BleGatewayClient.h"

#include <BLEAddress.h>
#include <BLEAdvertisedDevice.h>
#include <BLEClient.h>
#include <BLEDevice.h>
#include <BLERemoteCharacteristic.h>
#include <BLERemoteService.h>
#include <BLEScan.h>
#include <BLEUUID.h>

namespace {

constexpr const char* kGatewayRequestDeviceName = "Cola-ePaper";
constexpr const char* kFeedServiceUuid = "4e6a1000-5c4f-45af-8f1f-c41c01ab1000";
constexpr const char* kFeedDataCharacteristicUuid = "4e6a1001-5c4f-45af-8f1f-c41c01ab1001";
constexpr const char* kBroadcastCharacteristicUuid = "4e6a1002-5c4f-45af-8f1f-c41c01ab1002";

constexpr uint32_t kFeedPollIntervalMs = 60000UL;
constexpr uint32_t kRequestTimeoutMs = 10000UL;
constexpr uint32_t kScanStepIntervalMs = 1000UL;

BleGatewayClient* gBleGatewayClientInstance = nullptr;

void onGatewayNotify(BLERemoteCharacteristic* /*characteristic*/, uint8_t* data, size_t length,
                     bool /*isNotify*/) {
  if (gBleGatewayClientInstance != nullptr) {
    gBleGatewayClientInstance->handleNotification(data, length);
  }
}

}  // namespace

BleGatewayClient::BleGatewayClient() = default;

bool BleGatewayClient::begin() {
  BLEDevice::init("");
  scan_ = BLEDevice::getScan();
  if (scan_ == nullptr) {
    return false;
  }

  scan_->setActiveScan(true);
  scan_->setInterval(1349);
  scan_->setWindow(449);

  status_.initialized = true;
  nextRequestDueMs_ = 0;
  return true;
}

void BleGatewayClient::update() {
  if (!status_.initialized) {
    return;
  }

  const uint32_t nowMs = millis();

  switch (state_) {
    case State::kIdle:
      if (nowMs >= nextRequestDueMs_) {
        beginRequest();
      }
      return;

    case State::kScanning:
      if ((nowMs - status_.requestStartedMs) >= kRequestTimeoutMs) {
        completeRequestTimeout();
        return;
      }

      if (gatewayAddress_.isEmpty() &&
          (lastScanAttemptMs_ == 0 || (nowMs - lastScanAttemptMs_) >= kScanStepIntervalMs)) {
        lastScanAttemptMs_ = nowMs;
        scanForGateway();
      }

      if (!gatewayAddress_.isEmpty()) {
        state_ = State::kConnectPending;
      }
      return;

    case State::kConnectPending:
      if (!connectAndRequest()) {
        completeRequestTimeout();
        return;
      }

      state_ = State::kWaitingForResponse;
      return;

    case State::kWaitingForResponse:
      if (hasPendingPayload_) {
        completeRequestSuccess(pendingPayload_);
        return;
      }

      if ((nowMs - status_.requestStartedMs) >= kRequestTimeoutMs) {
        completeRequestTimeout();
      }
      return;
  }
}

const BleGatewayClient::StatusSnapshot& BleGatewayClient::getStatus() const { return status_; }

void BleGatewayClient::beginRequest() {
  status_.requestInFlight = true;
  status_.timedOut = false;
  status_.requestStartedMs = millis();
  gatewayAddress_.remove(0);
  responseBuffer_.remove(0);
  hasPendingPayload_ = false;
  lastScanAttemptMs_ = 0;
  disconnectClient();
  state_ = State::kScanning;
}

void BleGatewayClient::completeRequestSuccess(const FeedData::Payload& payload) {
  status_.payload = payload;
  status_.hasValidPayload = true;
  status_.requestInFlight = false;
  status_.timedOut = false;
  status_.lastSuccessLocalMs = millis();
  status_.requestStartedMs = 0;
  hasPendingPayload_ = false;
  nextRequestDueMs_ = millis() + kFeedPollIntervalMs;
  disconnectClient();
  state_ = State::kIdle;
}

void BleGatewayClient::completeRequestTimeout() {
  status_.hasValidPayload = false;
  status_.requestInFlight = false;
  status_.timedOut = true;
  status_.requestStartedMs = 0;
  responseBuffer_.remove(0);
  hasPendingPayload_ = false;
  nextRequestDueMs_ = millis() + kFeedPollIntervalMs;
  disconnectClient();
  state_ = State::kIdle;
}

bool BleGatewayClient::scanForGateway() {
  if (scan_ == nullptr) {
    return false;
  }

  BLEScanResults* scanResults = scan_->start(1, false);
  if (scanResults == nullptr) {
    return false;
  }

  const BLEUUID serviceUuid(kFeedServiceUuid);
  const int deviceCount = scanResults->getCount();

  for (int index = 0; index < deviceCount; ++index) {
    BLEAdvertisedDevice advertisedDevice = scanResults->getDevice(index);
    if (!advertisedDevice.haveServiceUUID() ||
        !advertisedDevice.isAdvertisingService(serviceUuid)) {
      continue;
    }

    gatewayAddress_ = String(advertisedDevice.getAddress().toString().c_str());
    break;
  }

  scan_->clearResults();
  return !gatewayAddress_.isEmpty();
}

bool BleGatewayClient::connectAndRequest() {
  disconnectClient();

  if (gatewayAddress_.isEmpty()) {
    return false;
  }

  client_ = BLEDevice::createClient();
  if (client_ == nullptr) {
    return false;
  }

  BLEAddress gatewayAddress(gatewayAddress_);
  if (!client_->connect(gatewayAddress)) {
    disconnectClient();
    return false;
  }

  client_->setMTU(517);

  BLERemoteService* feedService = client_->getService(BLEUUID(kFeedServiceUuid));
  if (feedService == nullptr) {
    disconnectClient();
    return false;
  }

  feedDataCharacteristic_ = feedService->getCharacteristic(BLEUUID(kFeedDataCharacteristicUuid));
  broadcastCharacteristic_ = feedService->getCharacteristic(BLEUUID(kBroadcastCharacteristicUuid));
  if (feedDataCharacteristic_ == nullptr || broadcastCharacteristic_ == nullptr) {
    disconnectClient();
    return false;
  }

  gBleGatewayClientInstance = this;
  feedDataCharacteristic_->registerForNotify(onGatewayNotify);

  const String requestPayload = String("{\"device_name\":\"") + kGatewayRequestDeviceName + "\"}";
  broadcastCharacteristic_->writeValue(requestPayload, false);
  return true;
}

void BleGatewayClient::disconnectClient() {
  feedDataCharacteristic_ = nullptr;
  broadcastCharacteristic_ = nullptr;
  gBleGatewayClientInstance = nullptr;

  if (client_ != nullptr) {
    if (client_->isConnected()) {
      client_->disconnect();
    }

    delete client_;
    client_ = nullptr;
  }
}

void BleGatewayClient::handleNotification(const uint8_t* data, size_t length) {
  if (data == nullptr || length == 0) {
    return;
  }

  for (size_t index = 0; index < length; ++index) {
    responseBuffer_ += static_cast<char>(data[index]);
  }

  FeedData::Payload payload;
  if (!FeedData::parsePayloadJson(responseBuffer_, &payload)) {
    return;
  }

  pendingPayload_ = payload;
  hasPendingPayload_ = true;
}