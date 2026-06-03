#include <Arduino.h>
#include <Adafruit_NeoPixel.h>

#include <cmath>

#include "modules/bluetooth/BleGatewayClient.h"
#include "modules/display/DisplayModule.h"
#include "modules/feed/FeedData.h"
#include "modules/i2c/I2cModule.h"

namespace {
constexpr uint8_t RGB_LED_PIN = 27;
constexpr uint8_t RGB_LED_COUNT = 1;

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

WeatherData::OutdoorEnvironmentData createOutdoorFallback() {
  WeatherData::OutdoorEnvironmentData data;
  data.temp = "--";
  data.icon = FeedData::kUnknownWeatherIcon;
  data.text = "未知";
  return data;
}

void bindBatteryState(DisplayModule& displayModule, const I2cModule& i2cModule) {
  const I2cModule::BatterySample& batterySample = i2cModule.getBatterySample();
  const bool isCharging = batterySample.powerState == I2cModule::BatteryPowerState::kCharging;
  const bool isPowerConnected = batterySample.externalPowerLikely ||
                                batterySample.powerState ==
                                    I2cModule::BatteryPowerState::kChargeCompletePowerConnected;
  displayModule.setMainPageBatteryStatus(clampBatteryPercentage(batterySample.percentage),
                                         isCharging, isPowerConnected);
}

void bindDeviceOrientation(DisplayModule& displayModule, const I2cModule& i2cModule) {
  switch (i2cModule.getDeviceOrientation()) {
    case I2cModule::DeviceOrientation::kBottomEdgeDown:
      displayModule.setDeviceOrientation(DisplayModule::DeviceOrientation::kBottomEdgeDown);
      return;
    case I2cModule::DeviceOrientation::kTopEdgeDown:
      displayModule.setDeviceOrientation(DisplayModule::DeviceOrientation::kTopEdgeDown);
      return;
    case I2cModule::DeviceOrientation::kUnknown:
      return;
  }
}

void bindIndoorEnvironment(DisplayModule& displayModule, const I2cModule& i2cModule) {
  const I2cModule::EnvironmentSample& environment = i2cModule.getEnvironmentSample();
  WeatherData::IndoorEnvironmentData indoor;
  indoor.temp = formatSensorValue(environment.temperatureC, 1);
  indoor.humidity = formatSensorValue(environment.humidityPercent, 1);
  displayModule.setMainPageIndoorEnvironmentData(indoor);
}

void bindFeedState(DisplayModule& displayModule, const BleGatewayClient& bleGatewayClient) {
  const BleGatewayClient::StatusSnapshot& status = bleGatewayClient.getStatus();
  if (!status.hasValidPayload) {
    displayModule.setMainPageTopTime("--:--");
    displayModule.setMainPageOutdoorEnvironmentData(createOutdoorFallback());
    displayModule.setMainPageForecastData(nullptr, 0);
    displayModule.setMainPageContentData("--", String(), false);
    return;
  }

  const String currentServerTime =
      FeedData::buildCurrentServerTime(status.payload.serverTime, status.lastSuccessLocalMs, millis());
  displayModule.setMainPageTopTime(FeedData::formatClockTime(currentServerTime));

  if (status.payload.hasWeather) {
    displayModule.setMainPageOutdoorEnvironmentData(status.payload.outdoor);
    displayModule.setMainPageForecastData(status.payload.forecast, status.payload.forecastCount);
  } else {
    displayModule.setMainPageOutdoorEnvironmentData(createOutdoorFallback());
    displayModule.setMainPageForecastData(nullptr, 0);
  }

  if (status.payload.hasLatestRecord) {
    displayModule.setMainPageContentData(
        FeedData::formatElapsedDuration(currentServerTime, status.payload.latestRecord.endTime),
        status.payload.latestRecord.endTime, true);
    return;
  }

  displayModule.setMainPageContentData("--", String(), false);
}
}

DisplayModule displayModule;
I2cModule i2cModule;
BleGatewayClient bleGatewayClient;
Adafruit_NeoPixel rgbLed(RGB_LED_COUNT, RGB_LED_PIN, NEO_RGB + NEO_KHZ800);

void setup() {
  i2cModule.begin();
  bleGatewayClient.begin();
  displayModule.begin();
  displayModule.renderLogo();
  bindDeviceOrientation(displayModule, i2cModule);
  displayModule.renderMainPage();

  rgbLed.begin();
  rgbLed.clear();
  rgbLed.setPixelColor(0, rgbLed.Color(55, 255, 55));
  rgbLed.show();
  delay(1000);
  rgbLed.clear();
  rgbLed.show();
}

void loop() {
  i2cModule.update();
  bleGatewayClient.update();
  bindDeviceOrientation(displayModule, i2cModule);
  bindBatteryState(displayModule, i2cModule);
  bindIndoorEnvironment(displayModule, i2cModule);
  bindFeedState(displayModule, bleGatewayClient);
  displayModule.renderMainPage();
  delay(10);
}
