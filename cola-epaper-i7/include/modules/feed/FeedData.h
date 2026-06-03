#pragma once

#include <Arduino.h>

#include "modules/weather/WeatherData.h"

namespace FeedData {

constexpr uint16_t kUnknownWeatherIcon = 999;

struct Record {
  String id;
  String startTime;
  String endTime;
  uint32_t durationSeconds = 0;
};

struct Payload {
  String serverTime;
  bool hasLatestRecord = false;
  Record latestRecord;
  bool hasWeather = false;
  WeatherData::OutdoorEnvironmentData outdoor;
  WeatherData::DailyForecastData forecast[WeatherData::kForecastDayCount];
  size_t forecastCount = 0;
};

struct DateTimeParts {
  int year = 0;
  int month = 0;
  int day = 0;
  int hour = 0;
  int minute = 0;
  int second = 0;
};

bool parsePayloadJson(const String& json, Payload* payload);
bool parseDateTime(const String& value, DateTimeParts* dateTime);
String formatDateTime(const DateTimeParts& dateTime);
String buildCurrentServerTime(const String& sourceServerTime, uint32_t receivedAtMs,
                              uint32_t nowMs);
String formatClockTime(const String& dateTimeValue);
String formatElapsedDuration(const String& currentServerTime, const String& endTime);

}  // namespace FeedData