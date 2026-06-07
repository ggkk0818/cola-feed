#include "modules/bluetooth/BleGatewayClient.h"

#include <BLEAddress.h>
#include <BLEAdvertisedDevice.h>
#include <BLEClient.h>
#include <BLEDevice.h>
#include <BLERemoteCharacteristic.h>
#include <BLERemoteService.h>
#include <BLEScan.h>
#include <BLEUUID.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

namespace {

constexpr const char* kGatewayRequestDeviceName = "Cola-ePaper";
constexpr const char* kFeedServiceUuid = "4e6a1000-5c4f-45af-8f1f-c41c01ab1000";
constexpr const char* kFeedDataCharacteristicUuid = "4e6a1001-5c4f-45af-8f1f-c41c01ab1001";
constexpr const char* kBroadcastCharacteristicUuid = "4e6a1002-5c4f-45af-8f1f-c41c01ab1002";
constexpr const char* kFeedChunkPrefix = "CF1|";

constexpr uint32_t kFeedPollIntervalMs = 60000UL;
constexpr uint32_t kRequestTimeoutMs = 10000UL;
constexpr uint32_t kScanStepIntervalMs = 1000UL;

BleGatewayClient* gBleGatewayClientInstance = nullptr;

void yieldToScheduler() {
  if (xTaskGetSchedulerState() == taskSCHEDULER_RUNNING) {
    vTaskDelay(1);
    return;
  }

  delay(1);
}

void onGatewayNotify(BLERemoteCharacteristic* /*characteristic*/, uint8_t* data, size_t length,
                     bool /*isNotify*/) {
  if (gBleGatewayClientInstance != nullptr) {
    gBleGatewayClientInstance->handleNotification(data, length);
  }
}

bool parsePositiveUint16(const String& value, uint16_t* result) {
  if (result == nullptr || value.isEmpty()) {
    return false;
  }

  unsigned long parsedValue = 0UL;
  for (size_t index = 0; index < static_cast<size_t>(value.length()); ++index) {
    const char currentChar = value[index];
    if (currentChar < '0' || currentChar > '9') {
      return false;
    }

    parsedValue = (parsedValue * 10UL) + static_cast<unsigned long>(currentChar - '0');
    if (parsedValue > 65535UL) {
      return false;
    }
  }

  if (parsedValue == 0UL) {
    return false;
  }

  *result = static_cast<uint16_t>(parsedValue);
  return true;
}

bool parseChunkPacket(const String& packet, uint16_t* chunkIndex, uint16_t* totalChunks, String* chunkPayload) {
  if (chunkIndex == nullptr || totalChunks == nullptr || chunkPayload == nullptr ||
      !packet.startsWith(kFeedChunkPrefix)) {
    return false;
  }

  const int firstSeparator = packet.indexOf('|', 4);
  if (firstSeparator < 0) {
    return false;
  }

  const int secondSeparator = packet.indexOf('|', firstSeparator + 1);
  if (secondSeparator < 0) {
    return false;
  }

  const String chunkIndexText = packet.substring(4, firstSeparator);
  const String totalChunksText = packet.substring(firstSeparator + 1, secondSeparator);
  if (!parsePositiveUint16(chunkIndexText, chunkIndex) || !parsePositiveUint16(totalChunksText, totalChunks) ||
      *chunkIndex > *totalChunks) {
    return false;
  }

  *chunkPayload = packet.substring(secondSeparator + 1);
  return true;
}

}  // namespace

BleGatewayClient::BleGatewayClient() = default;

bool BleGatewayClient::begin() {
  moduleReady_ = true;
  status_.initialized = false;
  nextRequestDueMs_ = 0;
  return true;
}

void BleGatewayClient::update() {
  if (!moduleReady_) {
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

uint32_t BleGatewayClient::getNextWorkDueMs(uint32_t nowMs) const {
  if (!moduleReady_) {
    return nowMs;
  }

  switch (state_) {
    case State::kIdle:
      return nowMs >= nextRequestDueMs_ ? nowMs : nextRequestDueMs_;

    case State::kScanning: {
      const uint32_t timeoutDueMs = status_.requestStartedMs + kRequestTimeoutMs;
      if (!gatewayAddress_.isEmpty()) {
        return nowMs;
      }

      if (lastScanAttemptMs_ == 0 || (nowMs - lastScanAttemptMs_) >= kScanStepIntervalMs) {
        return nowMs;
      }

      const uint32_t nextScanDueMs = lastScanAttemptMs_ + kScanStepIntervalMs;
      return nextScanDueMs < timeoutDueMs ? nextScanDueMs : timeoutDueMs;
    }

    case State::kConnectPending:
      return nowMs;

    case State::kWaitingForResponse: {
      if (hasPendingPayload_) {
        return nowMs;
      }

      const uint32_t timeoutDueMs = status_.requestStartedMs + kRequestTimeoutMs;
      const uint32_t nextPollDueMs = nowMs + 100UL;
      return nextPollDueMs < timeoutDueMs ? nextPollDueMs : timeoutDueMs;
    }
  }

  return nowMs;
}

void BleGatewayClient::beginRequest() {
  status_.requestInFlight = true;
  status_.timedOut = false;
  status_.requestStartedMs = millis();
  gatewayAddress_.remove(0);
  resetResponseAssembly();
  hasPendingPayload_ = false;
  lastScanAttemptMs_ = 0;
  disconnectClient();

  if (!initializeBleStack()) {
    completeRequestTimeout();
    return;
  }

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
  deinitializeBleStack();
  state_ = State::kIdle;
}

void BleGatewayClient::completeRequestTimeout() {
  status_.hasValidPayload = false;
  status_.requestInFlight = false;
  status_.timedOut = true;
  status_.requestStartedMs = 0;
  resetResponseAssembly();
  hasPendingPayload_ = false;
  nextRequestDueMs_ = millis() + kFeedPollIntervalMs;
  deinitializeBleStack();
  state_ = State::kIdle;
}

bool BleGatewayClient::initializeBleStack() {
  if (status_.initialized) {
    return scan_ != nullptr;
  }

  BLEDevice::init("");
  scan_ = BLEDevice::getScan();
  if (scan_ == nullptr) {
    BLEDevice::deinit(false);
    status_.initialized = false;
    return false;
  }

  scan_->setActiveScan(true);
  scan_->setInterval(1349);
  scan_->setWindow(449);
  status_.initialized = true;
  return true;
}

void BleGatewayClient::deinitializeBleStack() {
  disconnectClient();
  scan_ = nullptr;

  if (!status_.initialized) {
    return;
  }

  BLEDevice::deinit(false);
  status_.initialized = false;
}

bool BleGatewayClient::scanForGateway() {
  if (scan_ == nullptr) {
    return false;
  }

  yieldToScheduler();
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
  yieldToScheduler();
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

  yieldToScheduler();
  BLEAddress gatewayAddress(gatewayAddress_);
  if (!client_->connect(gatewayAddress)) {
    disconnectClient();
    return false;
  }

  client_->setMTU(517);

  yieldToScheduler();
  BLERemoteService* feedService = client_->getService(BLEUUID(kFeedServiceUuid));
  if (feedService == nullptr) {
    disconnectClient();
    return false;
  }

  yieldToScheduler();
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
  yieldToScheduler();
  return true;
}

void BleGatewayClient::resetResponseAssembly() {
  responseBuffer_.remove(0);
  expectedNextChunkIndex_ = 1;
  expectedTotalChunks_ = 0;
}

void BleGatewayClient::disconnectClient() {
  feedDataCharacteristic_ = nullptr;
  broadcastCharacteristic_ = nullptr;
  gBleGatewayClientInstance = nullptr;
  resetResponseAssembly();

  if (client_ != nullptr) {
    if (client_->isConnected()) {
      client_->disconnect();
    }
    client_ = nullptr;
  }
}

void BleGatewayClient::handleNotification(const uint8_t* data, size_t length) {
  if (data == nullptr || length == 0) {
    return;
  }

  String packet;
  packet.reserve(length);
  for (size_t index = 0; index < length; ++index) {
    packet += static_cast<char>(data[index]);
  }

  uint16_t chunkIndex = 0;
  uint16_t totalChunks = 0;
  String chunkPayload;
  if (parseChunkPacket(packet, &chunkIndex, &totalChunks, &chunkPayload)) {
    if (chunkIndex == 1) {
      resetResponseAssembly();
      expectedTotalChunks_ = totalChunks;
    }

    if (expectedTotalChunks_ == 0 || totalChunks != expectedTotalChunks_ || chunkIndex != expectedNextChunkIndex_) {
      resetResponseAssembly();
      return;
    }

    responseBuffer_ += chunkPayload;
    ++expectedNextChunkIndex_;

    if (chunkIndex < totalChunks) {
      return;
    }
  } else {
    responseBuffer_ += packet;
  }

  FeedData::Payload payload;
  if (!FeedData::parsePayloadJson(responseBuffer_, &payload)) {
    if (packet.startsWith(kFeedChunkPrefix)) {
      resetResponseAssembly();
    }
    return;
  }

  pendingPayload_ = payload;
  hasPendingPayload_ = true;
}