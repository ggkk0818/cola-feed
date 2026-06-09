#include "modules/app/AppRuntime.h"

#include <driver/gpio.h>
#include <esp_sleep.h>

#include <cmath>

#include "modules/feed/FeedData.h"

namespace {

constexpr uint32_t kNoScheduledWorkMs = 0xFFFFFFFFUL;
constexpr uint32_t kShortWorkerDelayMs = 25UL;
constexpr uint32_t kLightSleepThresholdMs = 50UL;
constexpr uint32_t kBusyRequestPollMs = 100UL;
constexpr uint32_t kDataTaskStackSize = 8192U;
constexpr uint32_t kRenderTaskStackSize = 12288U;
constexpr UBaseType_t kDataTaskPriority = 1;
constexpr UBaseType_t kRenderTaskPriority = 2;

TickType_t toTicksAtLeastOne(uint32_t delayMs) {
  TickType_t ticks = pdMS_TO_TICKS(delayMs);
  return ticks == 0 ? 1 : ticks;
}

void delayCurrentTask(uint32_t delayMs) {
  if (delayMs == 0) {
    taskYIELD();
    return;
  }

  vTaskDelay(toTicksAtLeastOne(delayMs));
}

uint32_t earlierDueMs(uint32_t left, uint32_t right) {
  if (left == kNoScheduledWorkMs) {
    return right;
  }

  if (right == kNoScheduledWorkMs) {
    return left;
  }

  return left < right ? left : right;
}

WeatherData::OutdoorEnvironmentData createOutdoorFallback() {
  WeatherData::OutdoorEnvironmentData data;
  data.temp = "--";
  data.icon = FeedData::kUnknownWeatherIcon;
  data.text = "未知";
  return data;
}

String formatSensorValue(float value, uint8_t decimals) {
  if (!std::isfinite(value)) {
    return String("--");
  }

  char buffer[24] = {0};
  dtostrf(static_cast<double>(value), 0, decimals, buffer);
  return String(buffer);
}

uint8_t clampBatteryPercentage(float percentage) {
  if (!std::isfinite(percentage) || percentage <= 0.0f) {
    return 0;
  }

  if (percentage >= 100.0f) {
    return 100;
  }

  return static_cast<uint8_t>(percentage + 0.5f);
}

bool isOutdoorEnvironmentEqual(const WeatherData::OutdoorEnvironmentData& left,
                               const WeatherData::OutdoorEnvironmentData& right) {
  return left.temp == right.temp && left.icon == right.icon && left.text == right.text;
}

bool isIndoorEnvironmentEqual(const WeatherData::IndoorEnvironmentData& left,
                              const WeatherData::IndoorEnvironmentData& right) {
  return left.temp == right.temp && left.humidity == right.humidity;
}

bool isForecastEqual(const WeatherData::DailyForecastData& left,
                     const WeatherData::DailyForecastData& right) {
  return left.fxDate == right.fxDate && left.tempMax == right.tempMax &&
         left.tempMin == right.tempMin && left.iconDay == right.iconDay &&
         left.textDay == right.textDay;
}

uint32_t calculateNextMinuteBoundaryMs(const BleGatewayClient::StatusSnapshot& status,
                                       uint32_t nowMs) {
  if (!status.hasValidPayload || status.payload.serverTime.isEmpty()) {
    return kNoScheduledWorkMs;
  }

  const String currentServerTime =
      FeedData::buildCurrentServerTime(status.payload.serverTime, status.lastSuccessLocalMs, nowMs);

  FeedData::DateTimeParts currentParts;
  if (!FeedData::parseDateTime(currentServerTime, &currentParts)) {
    return kNoScheduledWorkMs;
  }

  const uint32_t elapsedMs = nowMs - status.lastSuccessLocalMs;
  const uint32_t msWithinSecond = elapsedMs % 1000UL;
  const uint32_t remainingMs = static_cast<uint32_t>(60 - currentParts.second) * 1000UL - msWithinSecond;
  return nowMs + (remainingMs == 0 ? 1000UL : remainingMs);
}

}  // namespace

bool AppDisplayState::operator==(const AppDisplayState& other) const {
  if (orientation != other.orientation || batteryPercentage != other.batteryPercentage ||
      charging != other.charging || powerConnected != other.powerConnected ||
      topTime != other.topTime || !isOutdoorEnvironmentEqual(outdoor, other.outdoor) ||
      !isIndoorEnvironmentEqual(indoor, other.indoor) || forecastCount != other.forecastCount ||
      contentDuration != other.contentDuration || contentTimestamp != other.contentTimestamp ||
      contentTimestampVisible != other.contentTimestampVisible) {
    return false;
  }

  for (size_t index = 0; index < WeatherData::kForecastDayCount; ++index) {
    if (!isForecastEqual(forecast[index], other.forecast[index])) {
      return false;
    }
  }

  return true;
}

AppDisplayState AppDisplayUtils::buildDisplayState(const I2cModule& i2cModule,
                                                   const BleGatewayClient& bleGatewayClient,
                                                   uint32_t nowMs,
                                                   const AppDisplayState* previousState) {
  AppDisplayState state;
  state.outdoor = createOutdoorFallback();

  switch (i2cModule.getDeviceOrientation()) {
    case I2cModule::DeviceOrientation::kBottomEdgeDown:
      state.orientation = DisplayModule::DeviceOrientation::kBottomEdgeDown;
      break;
    case I2cModule::DeviceOrientation::kTopEdgeDown:
      state.orientation = DisplayModule::DeviceOrientation::kTopEdgeDown;
      break;
    case I2cModule::DeviceOrientation::kUnknown:
      state.orientation = previousState != nullptr
                              ? previousState->orientation
                              : DisplayModule::DeviceOrientation::kBottomEdgeDown;
      break;
  }

  const I2cModule::BatterySample& batterySample = i2cModule.getBatterySample();
  state.batteryPercentage = clampBatteryPercentage(batterySample.percentage);
  state.charging = batterySample.powerState == I2cModule::BatteryPowerState::kCharging;
  state.powerConnected = batterySample.externalPowerLikely ||
                         batterySample.powerState ==
                             I2cModule::BatteryPowerState::kChargeCompletePowerConnected;

  const I2cModule::EnvironmentSample& environment = i2cModule.getEnvironmentSample();
  state.indoor.temp = formatSensorValue(environment.temperatureC, 1);
  state.indoor.humidity = formatSensorValue(environment.humidityPercent, 1);

  const BleGatewayClient::StatusSnapshot& status = bleGatewayClient.getStatus();
  if (!status.hasValidPayload) {
    return state;
  }

  const String currentServerTime =
      FeedData::buildCurrentServerTime(status.payload.serverTime, status.lastSuccessLocalMs, nowMs);
  state.topTime = FeedData::formatClockTime(currentServerTime);

  if (status.payload.hasWeather) {
    state.outdoor = status.payload.outdoor;
    state.forecastCount = status.payload.forecastCount < WeatherData::kForecastDayCount
                              ? status.payload.forecastCount
                              : WeatherData::kForecastDayCount;
    for (size_t index = 0; index < WeatherData::kForecastDayCount; ++index) {
      if (index < state.forecastCount) {
        state.forecast[index] = status.payload.forecast[index];
      }
    }
  }

  if (status.payload.hasLatestRecord) {
    state.contentDuration =
        FeedData::formatElapsedDuration(currentServerTime, status.payload.latestRecord.endTime);
    state.contentTimestamp = status.payload.latestRecord.endTime;
    state.contentTimestampVisible = true;
  }

  return state;
}

void AppDisplayUtils::applyDisplayState(DisplayModule& displayModule,
                                        const AppDisplayState& state) {
  displayModule.setDeviceOrientation(state.orientation);
  displayModule.setMainPageBatteryStatus(state.batteryPercentage, state.charging,
                                         state.powerConnected);
  displayModule.setMainPageTopTime(state.topTime);
  displayModule.setMainPageOutdoorEnvironmentData(state.outdoor);
  displayModule.setMainPageIndoorEnvironmentData(state.indoor);
  displayModule.setMainPageForecastData(state.forecast, state.forecastCount);
  displayModule.setMainPageContentData(state.contentDuration, state.contentTimestamp,
                                       state.contentTimestampVisible);
}

uint32_t AppDisplayUtils::getNextUiUpdateDueMs(const BleGatewayClient& bleGatewayClient,
                                               uint32_t nowMs) {
  return calculateNextMinuteBoundaryMs(bleGatewayClient.getStatus(), nowMs);
}

AppRuntime::AppRuntime() = default;

bool AppRuntime::begin() {
  i2cModule_.begin();
  bleGatewayClient_.begin();
  displayModule_.begin();
  displayModule_.renderLogo();

  sharedStateMutex_ = xSemaphoreCreateMutex();
  if (sharedStateMutex_ == nullptr) {
    return false;
  }

  configureWakeSources();

  if (xTaskCreate(&AppRuntime::renderTaskEntry, "render_task", kRenderTaskStackSize, this,
                  kRenderTaskPriority, &renderTaskHandle_) != pdPASS) {
    return false;
  }

  if (xTaskCreate(&AppRuntime::dataTaskEntry, "data_task", kDataTaskStackSize, this,
                  kDataTaskPriority, &dataTaskHandle_) != pdPASS) {
    return false;
  }

  return true;
}

void AppRuntime::loopForever() {
  for (;;) {
    vTaskDelay(portMAX_DELAY);
  }
}

void AppRuntime::dataTaskEntry(void* parameter) {
  static_cast<AppRuntime*>(parameter)->runDataTask();
}

void AppRuntime::renderTaskEntry(void* parameter) {
  static_cast<AppRuntime*>(parameter)->runRenderTask();
}

void AppRuntime::runDataTask() {
  AppDisplayState lastPublishedState;
  bool hasPublishedState = false;

  for (;;) {
    i2cModule_.update();

    uint32_t nowMs = millis();
    AppDisplayState nextState =
        AppDisplayUtils::buildDisplayState(i2cModule_, bleGatewayClient_, nowMs,
                                           hasPublishedState ? &lastPublishedState : nullptr);
    bool publishedState = false;

    if (!hasPublishedState || !(nextState == lastPublishedState)) {
      lastPublishedState = nextState;
      hasPublishedState = true;
      publishDisplayState(nextState);
      publishedState = true;
    }

    bleGatewayClient_.update();

    nowMs = millis();
    nextState = AppDisplayUtils::buildDisplayState(
        i2cModule_, bleGatewayClient_, nowMs,
        hasPublishedState ? &lastPublishedState : nullptr);

    if (!hasPublishedState || !(nextState == lastPublishedState)) {
      lastPublishedState = nextState;
      hasPublishedState = true;
      publishDisplayState(nextState);
      publishedState = true;
    }

    if (publishedState) {
      continue;
    }

    idleDataTaskUntil(nowMs, computeNextWakeDueMs(nowMs));
  }
}

void AppRuntime::runRenderTask() {
  for (;;) {
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

    AppDisplayState state;
    if (!consumePendingDisplayState(&state)) {
      continue;
    }

    AppDisplayUtils::applyDisplayState(displayModule_, state);
    if (displayModule_.hasPendingMainPageRender()) {
      displayModule_.renderMainPage();
    }

    setDisplayIdle(!displayModule_.hasPendingMainPageRender());
  }
}

void AppRuntime::publishDisplayState(const AppDisplayState& state) {
  if (sharedStateMutex_ == nullptr) {
    return;
  }

  xSemaphoreTake(sharedStateMutex_, portMAX_DELAY);
  sharedState_.pendingDisplayState = state;
  sharedState_.renderRequested = true;
  sharedState_.displayIdle = false;
  xSemaphoreGive(sharedStateMutex_);

  if (renderTaskHandle_ != nullptr) {
    xTaskNotifyGive(renderTaskHandle_);
  }
}

bool AppRuntime::consumePendingDisplayState(AppDisplayState* state) {
  if (sharedStateMutex_ == nullptr || state == nullptr) {
    return false;
  }

  xSemaphoreTake(sharedStateMutex_, portMAX_DELAY);
  if (!sharedState_.renderRequested) {
    xSemaphoreGive(sharedStateMutex_);
    return false;
  }

  *state = sharedState_.pendingDisplayState;
  sharedState_.renderRequested = false;
  xSemaphoreGive(sharedStateMutex_);
  return true;
}

void AppRuntime::setDisplayIdle(bool isIdle) {
  if (sharedStateMutex_ == nullptr) {
    return;
  }

  xSemaphoreTake(sharedStateMutex_, portMAX_DELAY);
  sharedState_.displayIdle = isIdle;
  xSemaphoreGive(sharedStateMutex_);
}

void AppRuntime::readSharedState(bool* renderRequested, bool* displayIdle) {
  if (renderRequested == nullptr || displayIdle == nullptr || sharedStateMutex_ == nullptr) {
    return;
  }

  xSemaphoreTake(sharedStateMutex_, portMAX_DELAY);
  *renderRequested = sharedState_.renderRequested;
  *displayIdle = sharedState_.displayIdle;
  xSemaphoreGive(sharedStateMutex_);
}

uint32_t AppRuntime::computeNextWakeDueMs(uint32_t nowMs) {
  const uint32_t sensorDueMs = i2cModule_.getNextUpdateDueMs(nowMs);
  const uint32_t bleDueMs = bleGatewayClient_.getNextWorkDueMs(nowMs);
  const uint32_t uiDueMs = AppDisplayUtils::getNextUiUpdateDueMs(bleGatewayClient_, nowMs);
  uint32_t nextWakeDueMs = earlierDueMs(sensorDueMs, bleDueMs);
  nextWakeDueMs = earlierDueMs(nextWakeDueMs, uiDueMs);
  return nextWakeDueMs == kNoScheduledWorkMs ? nowMs + kShortWorkerDelayMs : nextWakeDueMs;
}

void AppRuntime::idleDataTaskUntil(uint32_t nowMs, uint32_t nextWakeDueMs) {
  bool renderRequested = false;
  bool displayIdle = false;
  readSharedState(&renderRequested, &displayIdle);
  const BleGatewayClient::StatusSnapshot& bleStatus = bleGatewayClient_.getStatus();

  if (renderRequested || !displayIdle) {
    delayCurrentTask(1);
    return;
  }

  if (nextWakeDueMs <= nowMs) {
    delayCurrentTask(1);
    return;
  }

  const uint32_t remainingMs = nextWakeDueMs - nowMs;
  if (bleStatus.requestInFlight) {
    delayCurrentTask(remainingMs < kBusyRequestPollMs ? remainingMs : kBusyRequestPollMs);
    return;
  }

  // BLE is initialized only for an active request cycle. Keep the data task awake
  // with RTOS delays until that cycle deinitializes the stack again.
  if (bleStatus.initialized) {
    delayCurrentTask(remainingMs);
    return;
  }

  if (remainingMs < kLightSleepThresholdMs) {
    delayCurrentTask(remainingMs < kShortWorkerDelayMs ? remainingMs : kShortWorkerDelayMs);
    return;
  }

  esp_sleep_enable_timer_wakeup(static_cast<uint64_t>(remainingMs) * 1000ULL);
  esp_light_sleep_start();
}

void AppRuntime::configureWakeSources() {
  if (!i2cModule_.getAvailability().accelerometer) {
    return;
  }

  gpio_wakeup_enable(static_cast<gpio_num_t>(I2cModule::kActivityInterruptPin),
                     GPIO_INTR_HIGH_LEVEL);
  esp_sleep_enable_gpio_wakeup();
}
