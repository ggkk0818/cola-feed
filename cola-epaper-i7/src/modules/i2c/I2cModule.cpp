#include "modules/i2c/I2cModule.h"

#include <Wire.h>
#include <SHTC3.h>

#include <cmath>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

namespace {
constexpr uint8_t kI2cSclPin = 25;
constexpr uint8_t kI2cSdaPin = 26;
constexpr uint32_t kI2cFrequencyHz = 400000;

constexpr uint8_t kAdxl343Address = 0x53;
constexpr uint8_t kAdxl343ExpectedDeviceId = 0xE5;
constexpr uint8_t kAdxl343RegDeviceId = 0x00;
constexpr uint8_t kAdxl343RegThreshAct = 0x24;
constexpr uint8_t kAdxl343RegActInactCtl = 0x27;
constexpr uint8_t kAdxl343RegBwRate = 0x2C;
constexpr uint8_t kAdxl343RegPowerCtl = 0x2D;
constexpr uint8_t kAdxl343RegIntEnable = 0x2E;
constexpr uint8_t kAdxl343RegIntMap = 0x2F;
constexpr uint8_t kAdxl343RegIntSource = 0x30;
constexpr uint8_t kAdxl343RegDataFormat = 0x31;
constexpr uint8_t kAdxl343RegDataX0 = 0x32;
constexpr uint8_t kAdxl343MeasureMode = 0x08;
constexpr uint8_t kAdxl343FullResolutionRange2G = 0x08;
constexpr uint8_t kAdxl343Rate100Hz = 0x0A;
constexpr uint8_t kAdxl343ActivityThresholdCounts = 8;
constexpr uint8_t kAdxl343ActivityAxesDcCoupled = 0xF0;
constexpr uint8_t kAdxl343ActivityInterruptMask = 0x10;
constexpr float kAdxl343ScaleFactorG = 0.0039f;

constexpr uint8_t kShtc3Address = 0x70;
constexpr uint16_t kShtc3WakeupCommand = 0x3517;
constexpr uint16_t kShtc3SleepCommand = 0xB098;
constexpr uint16_t kShtc3SoftResetCommand = 0x805D;
constexpr uint16_t kShtc3MeasureNormalTFirstCommand = 0x7866;

constexpr uint8_t kMax17048Address = 0x36;
constexpr uint8_t kMax17048RegVcell = 0x02;
constexpr uint8_t kMax17048RegSoc = 0x04;

constexpr uint32_t kAccelerationPollIntervalMs = 2000;
constexpr uint32_t kEnvironmentPollIntervalMs = 60000;
constexpr uint32_t kBatteryPollIntervalMs = 5000;
constexpr uint32_t kShtc3MeasurementDelayMs = 15;
constexpr float kOrientationMinAxisMagnitudeG = 0.65f;
constexpr float kMax17048VoltageResolutionV = 0.000078125f;

// 趋势窗口：始终取最近 120s 内的样本。polling 抖动（5s light-sleep、BLE 30s+ 延迟）
// 只影响窗口内样本数，不改变窗口长度，避免之前用"缓冲区填满 + span 上限"在
// 抖动下被打穿的问题。
constexpr uint32_t kBatteryTrendWindowMs = 120000;
// 采样间隔超过 90s 视为异常（BLE 周期最大约 40s，留 50s 余量），丢弃历史重建。
constexpr uint32_t kBatteryHistoryResetGapMs = 90000;
// 趋势有效所需的最小时间跨度：60s 对应 1.5 %/h 阈值下 ~6 个 LSB 变化，
// 是可靠速率推断的下限。
constexpr uint32_t kBatteryTrendMinSpanMs = 60000;
// 状态切换防抖：连续 2 次原始推断一致才更新对外状态，避免阈值附近抖动让图标闪烁。
constexpr uint8_t kBatteryStateDebounceCount = 2;

// SOC 速率阈值（%/h）。5000mAh + 5V/400mA 输入峰值 ~9-10 %/h，80%+ 进入 CV 后
// 回落到 1-4 %/h。1.5 %/h 覆盖整个 CV 阶段；高于电机短脉冲在 120s 内的平均
// 放电（<1 %/h）与 SOC 量化噪声（~0.12 %/h）。
constexpr float kSocRateChargeThreshold = 1.5f;
constexpr float kSocRateDischargeThreshold = -0.5f;
constexpr float kSocChargeCompleteThreshold = 99.0f;
// 充满判定要求 SOC 速率基本平稳或上行。5000mAh 典型空闲放电 0.4-0.6 %/h，
// -0.1 %/h 确保任何超过量化噪声的下行都不会被误判为"仍在外接电源"。
constexpr float kSocRateChargeCompleteStableThreshold = -0.1f;

constexpr float kOrientationDominanceMarginG = 0.15f;

volatile bool gAdxl343InterruptRaised = false;

void IRAM_ATTR onAdxl343Interrupt() { gAdxl343InterruptRaised = true; }

bool isFiniteFloat(float value) { return std::isfinite(value); }

void sleepWithScheduler(uint32_t delayMs) {
  if (delayMs == 0) {
    return;
  }

  if (xTaskGetSchedulerState() == taskSCHEDULER_RUNNING) {
    TickType_t ticks = pdMS_TO_TICKS(delayMs);
    if (ticks == 0) {
      ticks = 1;
    }
    vTaskDelay(ticks);
    return;
  }

  delay(delayMs);
}

float clampPercentage(float value) {
  if (value < 0.0f) {
    return 0.0f;
  }
  if (value > 100.0f) {
    return 100.0f;
  }
  return value;
}

uint8_t calculateShtc3Crc(const uint8_t* bytes, size_t length) {
  uint8_t crc = 0xFF;
  for (size_t index = 0; index < length; ++index) {
    crc ^= bytes[index];
    for (uint8_t bit = 0; bit < 8; ++bit) {
      crc = (crc & 0x80u) != 0 ? static_cast<uint8_t>((crc << 1) ^ 0x31u)
                               : static_cast<uint8_t>(crc << 1);
    }
  }
  return crc;
}
}  // namespace

I2cModule::I2cModule() = default;

bool I2cModule::begin() {
  busInitialized_ = Wire.begin(kI2cSdaPin, kI2cSclPin, kI2cFrequencyHz);
  if (!busInitialized_) {
    return false;
  }

  Wire.setTimeOut(50);

  pinMode(kActivityInterruptPin, INPUT);
  gAdxl343InterruptRaised = false;
  activityEventLatched_ = false;

  availability_.accelerometer = initializeAdxl343();
  availability_.environment = initializeShtc3();
  availability_.fuelGauge = initializeMax17048();

  // if (availability_.accelerometer) {
  //   attachInterrupt(digitalPinToInterrupt(kActivityInterruptPin), onAdxl343Interrupt, RISING);
  // }

  update();
  return availability_.accelerometer || availability_.environment || availability_.fuelGauge;
}

void I2cModule::update() {
  if (!busInitialized_) {
    return;
  }

  const uint32_t nowMs = millis();

  if (availability_.accelerometer) {
    const bool hadLatchedActivity = activityEventLatched_;
    handleActivityInterrupt();
    const bool shouldRefreshOnActivity = activityEventLatched_ && !hadLatchedActivity;
    if (shouldRefreshOnActivity || accelerationSample_.timestampMs == 0 ||
        (nowMs - lastAccelerationPollMs_) >= kAccelerationPollIntervalMs) {
      updateAcceleration(nowMs);
    }
  }

  if (availability_.environment &&
      (environmentSample_.timestampMs == 0 ||
       (nowMs - lastEnvironmentPollMs_) >= kEnvironmentPollIntervalMs)) {
    updateEnvironment(nowMs);
  }

  if (availability_.fuelGauge &&
      (batterySample_.timestampMs == 0 || (nowMs - lastBatteryPollMs_) >= kBatteryPollIntervalMs)) {
    updateBattery(nowMs);
  }
}

void I2cModule::updateBatteryOnly() {
  if (!busInitialized_ || !availability_.fuelGauge) {
    return;
  }

  const uint32_t nowMs = millis();
  if (batterySample_.timestampMs == 0 || (nowMs - lastBatteryPollMs_) >= kBatteryPollIntervalMs) {
    updateBattery(nowMs);
  }
}

const I2cModule::AccelerationSample& I2cModule::getAccelerationSample() const {
  return accelerationSample_;
}

I2cModule::DeviceOrientation I2cModule::getDeviceOrientation() const {
  return deviceOrientation_;
}

const I2cModule::EnvironmentSample& I2cModule::getEnvironmentSample() const {
  return environmentSample_;
}

const I2cModule::BatterySample& I2cModule::getBatterySample() const { return batterySample_; }

const I2cModule::SensorAvailability& I2cModule::getAvailability() const { return availability_; }

uint32_t I2cModule::getNextUpdateDueMs(uint32_t nowMs) const {
  if (!busInitialized_) {
    return nowMs;
  }

  bool hasAnySensor = false;
  uint32_t nextDueMs = nowMs;

  if (availability_.accelerometer) {
    hasAnySensor = true;
    const uint32_t accelerationDueMs =
        accelerationSample_.timestampMs == 0 ? nowMs : (lastAccelerationPollMs_ + kAccelerationPollIntervalMs);
    nextDueMs = accelerationDueMs;
  }

  if (availability_.environment) {
    const uint32_t environmentDueMs =
        environmentSample_.timestampMs == 0 ? nowMs : (lastEnvironmentPollMs_ + kEnvironmentPollIntervalMs);
    nextDueMs = !hasAnySensor || environmentDueMs < nextDueMs ? environmentDueMs : nextDueMs;
    hasAnySensor = true;
  }

  if (availability_.fuelGauge) {
    const uint32_t batteryDueMs =
        batterySample_.timestampMs == 0 ? nowMs : (lastBatteryPollMs_ + kBatteryPollIntervalMs);
    nextDueMs = !hasAnySensor || batteryDueMs < nextDueMs ? batteryDueMs : nextDueMs;
    hasAnySensor = true;
  }

  if (activityEventLatched_) {
    return nowMs;
  }

  return hasAnySensor ? nextDueMs : nowMs;
}

uint32_t I2cModule::getNextBatteryUpdateDueMs(uint32_t nowMs) const {
  if (!busInitialized_ || !availability_.fuelGauge || batterySample_.timestampMs == 0) {
    return nowMs;
  }

  return lastBatteryPollMs_ + kBatteryPollIntervalMs;
}

bool I2cModule::isBusInitialized() const { return busInitialized_; }

bool I2cModule::hasPendingActivityEvent() const { return activityEventLatched_; }

bool I2cModule::consumeActivityEvent() {
  const bool hadActivity = activityEventLatched_;
  activityEventLatched_ = false;
  return hadActivity;
}

bool I2cModule::initializeAdxl343() {
  uint8_t deviceId = 0;
  if (!readRegister(kAdxl343Address, kAdxl343RegDeviceId, deviceId) ||
      deviceId != kAdxl343ExpectedDeviceId) {
    return false;
  }

  if (!writeRegister(kAdxl343Address, kAdxl343RegPowerCtl, 0x00) ||
      !writeRegister(kAdxl343Address, kAdxl343RegDataFormat, kAdxl343FullResolutionRange2G) ||
      !writeRegister(kAdxl343Address, kAdxl343RegBwRate, kAdxl343Rate100Hz) ||
      !writeRegister(kAdxl343Address, kAdxl343RegThreshAct,
                     kAdxl343ActivityThresholdCounts) ||
      !writeRegister(kAdxl343Address, kAdxl343RegActInactCtl,
                     kAdxl343ActivityAxesDcCoupled) ||
      !writeRegister(kAdxl343Address, kAdxl343RegIntMap, 0x00) ||
      !writeRegister(kAdxl343Address, kAdxl343RegIntEnable,
                     kAdxl343ActivityInterruptMask) ||
      !writeRegister(kAdxl343Address, kAdxl343RegPowerCtl, kAdxl343MeasureMode)) {
    return false;
  }

  uint8_t ignored = 0;
  readRegister(kAdxl343Address, kAdxl343RegIntSource, ignored);
  return true;
}

bool I2cModule::initializeShtc3() {
  return shtc3_.begin(false);
}

bool I2cModule::initializeMax17048() {
  BatterySample sample;
  return readMax17048Battery(sample, millis());
}

bool I2cModule::writeRegister(uint8_t address, uint8_t reg, uint8_t value) {
  Wire.beginTransmission(address);
  Wire.write(reg);
  Wire.write(value);
  return Wire.endTransmission() == 0;
}

bool I2cModule::readRegister(uint8_t address, uint8_t reg, uint8_t& value) {
  return readRegisters(address, reg, &value, 1);
}

bool I2cModule::readRegisters(uint8_t address, uint8_t reg, uint8_t* buffer, size_t length) {
  Wire.beginTransmission(address);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) {
    return false;
  }

  if (Wire.requestFrom(static_cast<int>(address), static_cast<int>(length)) != length) {
    return false;
  }

  for (size_t index = 0; index < length; ++index) {
    if (!Wire.available()) {
      return false;
    }
    buffer[index] = static_cast<uint8_t>(Wire.read());
  }

  return true;
}

bool I2cModule::writeCommand(uint8_t address, uint16_t command) {
  Wire.beginTransmission(address);
  Wire.write(static_cast<uint8_t>(command >> 8));
  Wire.write(static_cast<uint8_t>(command & 0xFFu));
  return Wire.endTransmission() == 0;
}

bool I2cModule::readBytes(uint8_t address, uint8_t* buffer, size_t length) {
  if (Wire.requestFrom(static_cast<int>(address), static_cast<int>(length)) != length) {
    return false;
  }

  for (size_t index = 0; index < length; ++index) {
    if (!Wire.available()) {
      return false;
    }
    buffer[index] = static_cast<uint8_t>(Wire.read());
  }

  return true;
}

void I2cModule::updateAcceleration(uint32_t nowMs) {
  AccelerationSample sample;
  if (readAdxl343Acceleration(sample, nowMs)) {
    accelerationSample_ = sample;
    deviceOrientation_ = inferDeviceOrientation(sample);
  }
  lastAccelerationPollMs_ = nowMs;
}

void I2cModule::updateEnvironment(uint32_t nowMs) {
  EnvironmentSample sample;
  if (readShtc3Environment(sample, nowMs)) {
    environmentSample_ = sample;
  }
  lastEnvironmentPollMs_ = nowMs;
}

void I2cModule::updateBattery(uint32_t nowMs) {
  BatterySample sample;
  if (readMax17048Battery(sample, nowMs)) {
    batterySample_ = sample;
  }
  lastBatteryPollMs_ = nowMs;
}

void I2cModule::handleActivityInterrupt() {
  if (!gAdxl343InterruptRaised) {
    return;
  }

  gAdxl343InterruptRaised = false;

  uint8_t interruptSource = 0;
  if (!readRegister(kAdxl343Address, kAdxl343RegIntSource, interruptSource)) {
    return;
  }

  if ((interruptSource & kAdxl343ActivityInterruptMask) != 0u) {
    activityEventLatched_ = true;
  }
}

bool I2cModule::readAdxl343Acceleration(AccelerationSample& sample, uint32_t nowMs) {
  uint8_t bytes[6] = {0};
  if (!readRegisters(kAdxl343Address, kAdxl343RegDataX0, bytes, sizeof(bytes))) {
    return false;
  }

  const int16_t rawX = static_cast<int16_t>((static_cast<uint16_t>(bytes[1]) << 8) | bytes[0]);
  const int16_t rawY = static_cast<int16_t>((static_cast<uint16_t>(bytes[3]) << 8) | bytes[2]);
  const int16_t rawZ = static_cast<int16_t>((static_cast<uint16_t>(bytes[5]) << 8) | bytes[4]);

  sample.xG = static_cast<float>(rawX) * kAdxl343ScaleFactorG;
  sample.yG = static_cast<float>(rawY) * kAdxl343ScaleFactorG;
  sample.zG = static_cast<float>(rawZ) * kAdxl343ScaleFactorG;
  sample.timestampMs = nowMs;
  return true;
}

I2cModule::DeviceOrientation I2cModule::inferDeviceOrientation(
    const AccelerationSample& sample) const {
  if (!isFiniteFloat(sample.xG) || !isFiniteFloat(sample.yG) || !isFiniteFloat(sample.zG)) {
    return deviceOrientation_;
  }

  const float absX = std::fabs(sample.xG);
  const float absY = std::fabs(sample.yG);
  const float absZ = std::fabs(sample.zG);
  const bool xIsDominant = absX >= kOrientationMinAxisMagnitudeG &&
                           absX >= (absY + kOrientationDominanceMarginG) &&
                           absX >= (absZ + kOrientationDominanceMarginG);
  if (!xIsDominant) {
    return deviceOrientation_;
  }

  if (sample.xG > 0.0f) {
    return DeviceOrientation::kTopEdgeDown;
  }

  return DeviceOrientation::kBottomEdgeDown;
}

bool I2cModule::readShtc3Environment(EnvironmentSample& sample, uint32_t nowMs) {
  if (!shtc3_.sample()) {
    return false;
  }

  sample.temperatureC = shtc3_.readTempC();
  sample.humidityPercent = shtc3_.readHumidity();
  sample.timestampMs = nowMs;
  return true;
}

bool I2cModule::readMax17048Battery(BatterySample& sample, uint32_t nowMs) {
  uint8_t vcellBytes[2] = {0};
  uint8_t socBytes[2] = {0};
  if (!readRegisters(kMax17048Address, kMax17048RegVcell, vcellBytes, sizeof(vcellBytes)) ||
      !readRegisters(kMax17048Address, kMax17048RegSoc, socBytes, sizeof(socBytes))) {
    return false;
  }

  const uint16_t rawVcell = static_cast<uint16_t>((static_cast<uint16_t>(vcellBytes[0]) << 8) |
                                                  vcellBytes[1]);
  const uint16_t rawSoc = static_cast<uint16_t>((static_cast<uint16_t>(socBytes[0]) << 8) |
                                                socBytes[1]);

  const float percentage = clampPercentage(static_cast<float>(rawSoc >> 8) +
                                           static_cast<float>(rawSoc & 0xFFu) / 256.0f);
  const float voltageV = static_cast<float>(rawVcell) * kMax17048VoltageResolutionV;

  if (shouldResetBatteryHistory(nowMs)) {
    resetBatteryHistory();
  }

  recordBatteryHistory(percentage, voltageV, nowMs);

  const float percentageRatePerHour = computeBatteryPercentageRatePerHour();
  const BatteryPowerState rawPowerState =
      inferBatteryPowerState(percentage, percentageRatePerHour);
  const BatteryPowerState powerState = debouncePowerState(rawPowerState);

  sample.percentage = percentage;
  sample.voltageV = voltageV;
  sample.percentageRatePerHour = percentageRatePerHour;
  sample.powerState = powerState;
  sample.externalPowerLikely =
      powerState == BatteryPowerState::kCharging ||
      powerState == BatteryPowerState::kChargeCompletePowerConnected;
  sample.timestampMs = nowMs;
  return true;
}

void I2cModule::resetBatteryHistory() {
  batteryHistoryCount_ = 0;
  batteryHistoryNextIndex_ = 0;
  for (BatteryHistoryEntry& entry : batteryHistory_) {
    entry = BatteryHistoryEntry{};
  }
  lastRawPowerState_ = BatteryPowerState::kUnknown;
  rawPowerStateRepeatCount_ = 0;
  debouncedPowerState_ = BatteryPowerState::kUnknown;
}

void I2cModule::recordBatteryHistory(float percentage, float voltageV, uint32_t timestampMs) {
  batteryHistory_[batteryHistoryNextIndex_].percentage = percentage;
  batteryHistory_[batteryHistoryNextIndex_].voltageV = voltageV;
  batteryHistory_[batteryHistoryNextIndex_].timestampMs = timestampMs;
  batteryHistoryNextIndex_ = (batteryHistoryNextIndex_ + 1u) % kBatteryHistorySize;
  if (batteryHistoryCount_ < kBatteryHistorySize) {
    ++batteryHistoryCount_;
  }
}

bool I2cModule::getBatteryTrendWindowBounds(size_t* oldestIdx, size_t* newestIdx) const {
  if (batteryHistoryCount_ < 2) {
    return false;
  }

  const size_t newest = (batteryHistoryNextIndex_ + kBatteryHistorySize - 1u) % kBatteryHistorySize;
  const uint32_t newestMs = batteryHistory_[newest].timestampMs;
  const uint32_t windowStartMs =
      (newestMs >= kBatteryTrendWindowMs) ? (newestMs - kBatteryTrendWindowMs) : 0u;

  size_t oldest = newest;
  for (size_t step = 1; step < batteryHistoryCount_; ++step) {
    const size_t idx = (newest + kBatteryHistorySize - step) % kBatteryHistorySize;
    if (batteryHistory_[idx].timestampMs < windowStartMs) {
      break;
    }
    oldest = idx;
  }

  if (oldest == newest) {
    return false;
  }

  const uint32_t oldestMs = batteryHistory_[oldest].timestampMs;
  if (newestMs <= oldestMs || newestMs - oldestMs < kBatteryTrendMinSpanMs) {
    return false;
  }

  *oldestIdx = oldest;
  *newestIdx = newest;
  return true;
}

float I2cModule::computeBatteryPercentageRatePerHour() const {
  size_t oldestIdx = 0;
  size_t newestIdx = 0;
  if (!getBatteryTrendWindowBounds(&oldestIdx, &newestIdx)) {
    return 0.0f;
  }

  const BatteryHistoryEntry& oldest = batteryHistory_[oldestIdx];
  const BatteryHistoryEntry& newest = batteryHistory_[newestIdx];
  if (!isFiniteFloat(oldest.percentage) || !isFiniteFloat(newest.percentage)) {
    return 0.0f;
  }

  const float elapsedHours =
      static_cast<float>(newest.timestampMs - oldest.timestampMs) / 3600000.0f;
  if (elapsedHours <= 0.0f) {
    return 0.0f;
  }

  return (newest.percentage - oldest.percentage) / elapsedHours;
}

bool I2cModule::hasBatteryTrendWindow() const {
  size_t oldestIdx = 0;
  size_t newestIdx = 0;
  return getBatteryTrendWindowBounds(&oldestIdx, &newestIdx);
}

bool I2cModule::shouldResetBatteryHistory(uint32_t timestampMs) const {
  return batterySample_.timestampMs != 0 && timestampMs > batterySample_.timestampMs &&
         (timestampMs - batterySample_.timestampMs) > kBatteryHistoryResetGapMs;
}

I2cModule::BatteryPowerState I2cModule::inferBatteryPowerState(
    float percentage, float percentageRatePerHour) const {
  if (!isFiniteFloat(percentage)) {
    return BatteryPowerState::kUnknown;
  }

  const bool nearFull = percentage >= kSocChargeCompleteThreshold;
  const bool hasTrend = hasBatteryTrendWindow();

  // 明确充电中（rate 超过充电阈值）优先返回 kCharging，即使 SOC 已达 99%+。
  // 否则 CV 阶段的高速充电会被 nearFull 短路成"充满/外接电源"，丢失充电图标。
  if (hasTrend && percentageRatePerHour > kSocRateChargeThreshold) {
    return BatteryPowerState::kCharging;
  }

  // 充满且未观察到下行 -> 视为仍连接电源
  if (nearFull && percentageRatePerHour >= kSocRateChargeCompleteStableThreshold) {
    return BatteryPowerState::kChargeCompletePowerConnected;
  }

  if (!hasTrend) {
    return BatteryPowerState::kUnknown;
  }

  if (percentageRatePerHour < kSocRateDischargeThreshold) {
    return BatteryPowerState::kDischarging;
  }

  return BatteryPowerState::kUnknown;
}

I2cModule::BatteryPowerState I2cModule::debouncePowerState(BatteryPowerState rawState) {
  if (rawState != lastRawPowerState_) {
    lastRawPowerState_ = rawState;
    rawPowerStateRepeatCount_ = 0;
  }
  if (rawPowerStateRepeatCount_ < 255u) {
    ++rawPowerStateRepeatCount_;
  }

  if (rawPowerStateRepeatCount_ >= kBatteryStateDebounceCount) {
    debouncedPowerState_ = rawState;
  }
  return debouncedPowerState_;
}
