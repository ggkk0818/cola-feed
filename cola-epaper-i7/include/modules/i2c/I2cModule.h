#pragma once

#include <Arduino.h>
#include <SHTC3.h>

class I2cModule {
 public:
  static constexpr uint8_t kActivityInterruptPin = 10;

  struct AccelerationSample {
    float xG = NAN;
    float yG = NAN;
    float zG = NAN;
    uint32_t timestampMs = 0;
  };

  struct EnvironmentSample {
    float temperatureC = NAN;
    float humidityPercent = NAN;
    uint32_t timestampMs = 0;
  };

  enum class DeviceOrientation : uint8_t {
    kUnknown,
    kBottomEdgeDown,
    kTopEdgeDown,
  };

  enum class BatteryPowerState : uint8_t {
    kUnknown,
    kDischarging,
    kCharging,
    kChargeCompletePowerConnected,
  };

  struct BatterySample {
    float percentage = NAN;
    float voltageV = NAN;
    float percentageRatePerHour = 0.0f;
    BatteryPowerState powerState = BatteryPowerState::kUnknown;
    bool externalPowerLikely = false;
    uint32_t timestampMs = 0;
  };

  struct SensorAvailability {
    bool accelerometer = false;
    bool environment = false;
    bool fuelGauge = false;
  };

  I2cModule();

  bool begin();
  void update();
  void updateBatteryOnly();

  const AccelerationSample& getAccelerationSample() const;
  DeviceOrientation getDeviceOrientation() const;
  const EnvironmentSample& getEnvironmentSample() const;
  const BatterySample& getBatterySample() const;
  const SensorAvailability& getAvailability() const;
  uint32_t getNextUpdateDueMs(uint32_t nowMs) const;
  uint32_t getNextBatteryUpdateDueMs(uint32_t nowMs) const;

  bool isBusInitialized() const;
  bool hasPendingActivityEvent() const;
  bool consumeActivityEvent();

 private:
  static constexpr size_t kBatteryHistorySize = 25;

  struct BatteryHistoryEntry {
    float percentage = NAN;
    float voltageV = NAN;
    uint32_t timestampMs = 0;
  };

  bool initializeAdxl343();
  bool initializeShtc3();
  bool initializeMax17048();

  bool writeRegister(uint8_t address, uint8_t reg, uint8_t value);
  bool readRegister(uint8_t address, uint8_t reg, uint8_t& value);
  bool readRegisters(uint8_t address, uint8_t reg, uint8_t* buffer, size_t length);
  bool writeCommand(uint8_t address, uint16_t command);
  bool readBytes(uint8_t address, uint8_t* buffer, size_t length);

  void updateAcceleration(uint32_t nowMs);
  void updateEnvironment(uint32_t nowMs);
  void updateBattery(uint32_t nowMs);
  void handleActivityInterrupt();

  bool readAdxl343Acceleration(AccelerationSample& sample, uint32_t nowMs);
  bool readShtc3Environment(EnvironmentSample& sample, uint32_t nowMs);
  bool readMax17048Battery(BatterySample& sample, uint32_t nowMs);
  DeviceOrientation inferDeviceOrientation(const AccelerationSample& sample) const;

  void resetBatteryHistory();
  void recordBatteryHistory(float percentage, float voltageV, uint32_t timestampMs);
  bool getBatteryTrendWindowBounds(size_t* oldestIdx, size_t* newestIdx) const;
  float computeBatteryPercentageRatePerHour() const;
  bool hasBatteryTrendWindow() const;
  bool shouldResetBatteryHistory(uint32_t timestampMs) const;
  BatteryPowerState inferBatteryPowerState(float percentage, float percentageRatePerHour) const;
  BatteryPowerState debouncePowerState(BatteryPowerState rawState);

  bool busInitialized_ = false;
  bool activityEventLatched_ = false;
  SensorAvailability availability_{};
  AccelerationSample accelerationSample_{};
  DeviceOrientation deviceOrientation_ = DeviceOrientation::kUnknown;
  EnvironmentSample environmentSample_{};
  BatterySample batterySample_{};
  BatteryHistoryEntry batteryHistory_[kBatteryHistorySize]{};
  size_t batteryHistoryCount_ = 0;
  size_t batteryHistoryNextIndex_ = 0;
  BatteryPowerState lastRawPowerState_ = BatteryPowerState::kUnknown;
  uint8_t rawPowerStateRepeatCount_ = 0;
  BatteryPowerState debouncedPowerState_ = BatteryPowerState::kUnknown;
  uint32_t lastAccelerationPollMs_ = 0;
  uint32_t lastEnvironmentPollMs_ = 0;
  uint32_t lastBatteryPollMs_ = 0;
  SHTC3 shtc3_{Wire};
};
