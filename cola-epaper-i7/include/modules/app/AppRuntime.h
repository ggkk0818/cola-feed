#pragma once

#include <Arduino.h>

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

#include "modules/bluetooth/BleGatewayClient.h"
#include "modules/display/DisplayModule.h"
#include "modules/i2c/I2cModule.h"
#include "modules/weather/WeatherData.h"

struct AppDisplayState {
  DisplayModule::DeviceOrientation orientation = DisplayModule::DeviceOrientation::kBottomEdgeDown;
  uint8_t batteryPercentage = 0;
  bool charging = false;
  bool powerConnected = false;
  String topTime = "--:--";
  WeatherData::OutdoorEnvironmentData outdoor;
  WeatherData::IndoorEnvironmentData indoor;
  WeatherData::DailyForecastData forecast[WeatherData::kForecastDayCount];
  size_t forecastCount = 0;
  String contentDuration = "--";
  String contentTimestamp;
  bool contentTimestampVisible = false;

  bool operator==(const AppDisplayState& other) const;
};

class AppDisplayUtils {
 public:
  // Builds the next screen snapshot from the latest sensor and BLE state.
  static AppDisplayState buildDisplayState(const I2cModule& i2cModule,
                                           const BleGatewayClient& bleGatewayClient,
                                           uint32_t nowMs,
                                           const AppDisplayState* previousState);

  // Applies a snapshot to DisplayModule without leaking formatting/binding logic into main.cpp.
  static void applyDisplayState(DisplayModule& displayModule, const AppDisplayState& state);

  // Returns the next minute boundary when time-dependent UI text may change again.
  static uint32_t getNextUiUpdateDueMs(const BleGatewayClient& bleGatewayClient, uint32_t nowMs);
};

class AppRuntime {
 public:
  AppRuntime();

  bool begin();
  void loopForever();

 private:
  enum class RenderCommand : uint8_t {
    kNone,
    kMainPage,
    kLowBattery,
  };

  // SharedState is the only cross-task handoff surface: the data task publishes
  // snapshots here and the render task consumes them.
  struct SharedState {
    AppDisplayState pendingDisplayState;
    RenderCommand pendingRenderCommand = RenderCommand::kNone;
    bool forceMainPageRefresh = false;
    bool displayIdle = false;
  };

  static void dataTaskEntry(void* parameter);
  static void renderTaskEntry(void* parameter);

  void runDataTask();
  void runRenderTask();
  void publishDisplayState(const AppDisplayState& state, bool forceMainPageRefresh = false);
  void publishLowBatteryRender();
  RenderCommand consumePendingRenderCommand(AppDisplayState* state, bool* forceMainPageRefresh);
  void setDisplayIdle(bool isIdle);
  void readSharedState(bool* renderRequested, bool* displayIdle);
  bool isBatteryDetected() const;
  bool isBatteryChargingOrPowered() const;
  bool shouldEnterLowBatteryMode() const;
  bool shouldExitLowBatteryMode() const;
  void enterLowBatteryMode();
  void exitLowBatteryMode();
  uint32_t computeNextWakeDueMs(uint32_t nowMs);
  // The low-priority data task uses timer wait or light sleep here once fetch and render work are idle.
  void idleDataTaskUntil(uint32_t nowMs, uint32_t nextWakeDueMs);
  void configureWakeSources();

  I2cModule i2cModule_;
  BleGatewayClient bleGatewayClient_;
  DisplayModule displayModule_;
  TaskHandle_t dataTaskHandle_ = nullptr;
  TaskHandle_t renderTaskHandle_ = nullptr;
  SemaphoreHandle_t sharedStateMutex_ = nullptr;
  SharedState sharedState_{};
  bool lowBatteryMode_ = false;
};
