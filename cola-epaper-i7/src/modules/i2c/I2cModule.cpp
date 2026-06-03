#include "modules/i2c/I2cModule.h"

#include <Wire.h>

#include <cmath>

namespace {
constexpr uint8_t kI2cSclPin = 25;
constexpr uint8_t kI2cSdaPin = 26;
constexpr uint32_t kI2cFrequencyHz = 400000;

constexpr uint8_t kAdxl343Address = 0x53;
constexpr uint8_t kAdxl343InterruptPin = 10;
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
constexpr uint8_t kAdxl343ActivityAxesDcCoupled = 0x70;
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

constexpr uint32_t kAccelerationPollIntervalMs = 200;
constexpr uint32_t kEnvironmentPollIntervalMs = 2000;
constexpr uint32_t kBatteryPollIntervalMs = 5000;
constexpr uint32_t kShtc3MeasurementDelayMs = 15;
constexpr float kMax17048VoltageResolutionV = 0.000078125f;
constexpr float kChargingRateThresholdPercentPerHour = 0.12f;
constexpr float kDischargingRateThresholdPercentPerHour = -0.08f;
constexpr float kChargeCompleteVoltageThresholdV = 4.12f;
constexpr float kChargeCompletePercentageThreshold = 99.0f;

volatile bool gAdxl343InterruptRaised = false;

void IRAM_ATTR onAdxl343Interrupt() { gAdxl343InterruptRaised = true; }

bool isFiniteFloat(float value) { return std::isfinite(value); }

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

  pinMode(kAdxl343InterruptPin, INPUT);
  gAdxl343InterruptRaised = false;
  activityEventLatched_ = false;

  availability_.accelerometer = initializeAdxl343();
  availability_.environment = initializeShtc3();
  availability_.fuelGauge = initializeMax17048();

  if (availability_.accelerometer) {
    attachInterrupt(digitalPinToInterrupt(kAdxl343InterruptPin), onAdxl343Interrupt, RISING);
  }

  update();
  return availability_.accelerometer || availability_.environment || availability_.fuelGauge;
}

void I2cModule::update() {
  if (!busInitialized_) {
    return;
  }

  const uint32_t nowMs = millis();

  if (availability_.accelerometer) {
    handleActivityInterrupt();
    if (accelerationSample_.timestampMs == 0 ||
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

const I2cModule::AccelerationSample& I2cModule::getAccelerationSample() const {
  return accelerationSample_;
}

const I2cModule::EnvironmentSample& I2cModule::getEnvironmentSample() const {
  return environmentSample_;
}

const I2cModule::BatterySample& I2cModule::getBatterySample() const { return batterySample_; }

const I2cModule::SensorAvailability& I2cModule::getAvailability() const { return availability_; }

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
  if (!writeCommand(kShtc3Address, kShtc3SoftResetCommand)) {
    return false;
  }

  delay(1);
  writeCommand(kShtc3Address, kShtc3SleepCommand);
  return true;
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

bool I2cModule::readShtc3Environment(EnvironmentSample& sample, uint32_t nowMs) {
  if (!writeCommand(kShtc3Address, kShtc3WakeupCommand)) {
    return false;
  }

  delay(1);

  if (!writeCommand(kShtc3Address, kShtc3MeasureNormalTFirstCommand)) {
    writeCommand(kShtc3Address, kShtc3SleepCommand);
    return false;
  }

  delay(kShtc3MeasurementDelayMs);

  uint8_t bytes[6] = {0};
  const bool readOk = readBytes(kShtc3Address, bytes, sizeof(bytes));
  writeCommand(kShtc3Address, kShtc3SleepCommand);
  if (!readOk) {
    return false;
  }

  if (calculateShtc3Crc(bytes, 2) != bytes[2] || calculateShtc3Crc(bytes + 3, 2) != bytes[5]) {
    return false;
  }

  const uint16_t rawTemperature = static_cast<uint16_t>((bytes[0] << 8) | bytes[1]);
  const uint16_t rawHumidity = static_cast<uint16_t>((bytes[3] << 8) | bytes[4]);

  sample.temperatureC = -45.0f + (175.0f * static_cast<float>(rawTemperature) / 65535.0f);
  sample.humidityPercent = 100.0f * static_cast<float>(rawHumidity) / 65535.0f;
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

  recordBatteryHistory(percentage, voltageV, nowMs);

  const float percentageRatePerHour = computeBatteryPercentageRatePerHour();
  const BatteryPowerState powerState =
      inferBatteryPowerState(percentage, voltageV, percentageRatePerHour);

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

void I2cModule::recordBatteryHistory(float percentage, float voltageV, uint32_t timestampMs) {
  batteryHistory_[batteryHistoryNextIndex_].percentage = percentage;
  batteryHistory_[batteryHistoryNextIndex_].voltageV = voltageV;
  batteryHistory_[batteryHistoryNextIndex_].timestampMs = timestampMs;
  batteryHistoryNextIndex_ = (batteryHistoryNextIndex_ + 1u) % kBatteryHistorySize;
  if (batteryHistoryCount_ < kBatteryHistorySize) {
    ++batteryHistoryCount_;
  }
}

float I2cModule::computeBatteryPercentageRatePerHour() const {
  if (batteryHistoryCount_ < 2) {
    return 0.0f;
  }

  const size_t oldestIndex = batteryHistoryCount_ == kBatteryHistorySize ? batteryHistoryNextIndex_ : 0;
  const size_t newestIndex =
      (batteryHistoryNextIndex_ + kBatteryHistorySize - 1u) % kBatteryHistorySize;
  const BatteryHistoryEntry& oldest = batteryHistory_[oldestIndex];
  const BatteryHistoryEntry& newest = batteryHistory_[newestIndex];
  if (!isFiniteFloat(oldest.percentage) || !isFiniteFloat(newest.percentage) ||
      newest.timestampMs <= oldest.timestampMs) {
    return 0.0f;
  }

  const float elapsedHours =
      static_cast<float>(newest.timestampMs - oldest.timestampMs) / 3600000.0f;
  if (elapsedHours <= 0.0f) {
    return 0.0f;
  }

  return (newest.percentage - oldest.percentage) / elapsedHours;
}

I2cModule::BatteryPowerState I2cModule::inferBatteryPowerState(
    float percentage, float voltageV, float percentageRatePerHour) const {
  if (!isFiniteFloat(percentage) || !isFiniteFloat(voltageV)) {
    return BatteryPowerState::kUnknown;
  }

  if (percentageRatePerHour >= kChargingRateThresholdPercentPerHour) {
    return BatteryPowerState::kCharging;
  }

  const bool nearFull = percentage >= kChargeCompletePercentageThreshold &&
                        voltageV >= kChargeCompleteVoltageThresholdV;
  if (nearFull && percentageRatePerHour >= -0.03f) {
    return BatteryPowerState::kChargeCompletePowerConnected;
  }

  if (percentageRatePerHour <= kDischargingRateThresholdPercentPerHour) {
    return BatteryPowerState::kDischarging;
  }

  return nearFull ? BatteryPowerState::kChargeCompletePowerConnected
                  : BatteryPowerState::kUnknown;
}