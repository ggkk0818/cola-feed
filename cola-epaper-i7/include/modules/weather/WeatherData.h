#pragma once

#include <Arduino.h>

namespace WeatherData {

constexpr size_t kForecastDayCount = 4;

struct OutdoorEnvironmentData {
  String temp;
  uint16_t icon;
  String text;
};

struct IndoorEnvironmentData {
  String temp;
  String humidity;
};

struct DailyForecastData {
  String fxDate;
  String tempMax;
  String tempMin;
  uint16_t iconDay;
  String textDay;
};

struct SidebarWeatherData {
  OutdoorEnvironmentData outdoor;
  IndoorEnvironmentData indoor;
  DailyForecastData forecast[kForecastDayCount];
};

}  // namespace WeatherData