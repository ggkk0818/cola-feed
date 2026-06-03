#include "modules/feed/FeedData.h"

#include <ArduinoJson.h>

namespace {

bool isLeapYear(int year) {
  return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
}

int daysInMonth(int year, int month) {
  static const int kDaysByMonth[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  if (month == 2) {
    return isLeapYear(year) ? 29 : 28;
  }

  if (month < 1 || month > 12) {
    return 31;
  }

  return kDaysByMonth[month - 1];
}

void addSeconds(FeedData::DateTimeParts* dateTime, uint32_t secondsToAdd) {
  if (dateTime == nullptr) {
    return;
  }

  uint32_t totalSeconds = static_cast<uint32_t>(dateTime->second) + secondsToAdd;
  dateTime->second = static_cast<int>(totalSeconds % 60UL);

  uint32_t totalMinutes = static_cast<uint32_t>(dateTime->minute) + (totalSeconds / 60UL);
  dateTime->minute = static_cast<int>(totalMinutes % 60UL);

  uint32_t totalHours = static_cast<uint32_t>(dateTime->hour) + (totalMinutes / 60UL);
  dateTime->hour = static_cast<int>(totalHours % 24UL);

  uint32_t extraDays = totalHours / 24UL;
  while (extraDays > 0UL) {
    ++dateTime->day;
    if (dateTime->day > daysInMonth(dateTime->year, dateTime->month)) {
      dateTime->day = 1;
      ++dateTime->month;
      if (dateTime->month > 12) {
        dateTime->month = 1;
        ++dateTime->year;
      }
    }

    --extraDays;
  }
}

bool toUnixSeconds(const FeedData::DateTimeParts& dateTime, uint32_t* unixSeconds) {
  if (unixSeconds == nullptr) {
    return false;
  }

  uint32_t days = 0;
  for (int year = 1970; year < dateTime.year; ++year) {
    days += isLeapYear(year) ? 366UL : 365UL;
  }

  for (int month = 1; month < dateTime.month; ++month) {
    days += static_cast<uint32_t>(daysInMonth(dateTime.year, month));
  }

  days += static_cast<uint32_t>(dateTime.day - 1);

  *unixSeconds = days * 86400UL + static_cast<uint32_t>(dateTime.hour) * 3600UL +
                 static_cast<uint32_t>(dateTime.minute) * 60UL +
                 static_cast<uint32_t>(dateTime.second);
  return true;
}

bool isEndTimeNewer(const String& candidate, const String& current) {
  if (candidate.isEmpty()) {
    return false;
  }

  if (current.isEmpty()) {
    return true;
  }

  return candidate.compareTo(current) > 0;
}

uint16_t parseIconCode(JsonVariantConst value, uint16_t fallback) {
  if (value.isNull()) {
    return fallback;
  }

  if (value.is<const char*>()) {
    return static_cast<uint16_t>(String(value.as<const char*>()).toInt());
  }

  return static_cast<uint16_t>(value.as<int>());
}

}  // namespace

namespace FeedData {

bool parseDateTime(const String& value, DateTimeParts* dateTime) {
  if (dateTime == nullptr) {
    return false;
  }

  const int matched = sscanf(value.c_str(), "%d-%d-%d %d:%d:%d", &dateTime->year, &dateTime->month,
                             &dateTime->day, &dateTime->hour, &dateTime->minute,
                             &dateTime->second);
  if (matched != 6) {
    return false;
  }

  if (dateTime->year < 1970 || dateTime->month < 1 || dateTime->month > 12 || dateTime->day < 1 ||
      dateTime->hour < 0 || dateTime->hour > 23 || dateTime->minute < 0 ||
      dateTime->minute > 59 || dateTime->second < 0 || dateTime->second > 59) {
    return false;
  }

  return dateTime->day <= daysInMonth(dateTime->year, dateTime->month);
}

String formatDateTime(const DateTimeParts& dateTime) {
  char buffer[20] = {0};
  snprintf(buffer, sizeof(buffer), "%04d-%02d-%02d %02d:%02d:%02d", dateTime.year,
           dateTime.month, dateTime.day, dateTime.hour, dateTime.minute, dateTime.second);
  return String(buffer);
}

String buildCurrentServerTime(const String& sourceServerTime, uint32_t receivedAtMs,
                              uint32_t nowMs) {
  if (sourceServerTime.isEmpty()) {
    return String();
  }

  DateTimeParts dateTime;
  if (!parseDateTime(sourceServerTime, &dateTime)) {
    return String();
  }

  addSeconds(&dateTime, (nowMs - receivedAtMs) / 1000UL);
  return formatDateTime(dateTime);
}

String formatClockTime(const String& dateTimeValue) {
  DateTimeParts dateTime;
  if (!parseDateTime(dateTimeValue, &dateTime)) {
    return String("--:--");
  }

  char buffer[6] = {0};
  snprintf(buffer, sizeof(buffer), "%02d:%02d", dateTime.hour, dateTime.minute);
  return String(buffer);
}

String formatElapsedDuration(const String& currentServerTime, const String& endTime) {
  DateTimeParts currentDateTime;
  DateTimeParts endDateTime;
  if (!parseDateTime(currentServerTime, &currentDateTime) || !parseDateTime(endTime, &endDateTime)) {
    return String("--");
  }

  uint32_t currentSeconds = 0;
  uint32_t endSeconds = 0;
  if (!toUnixSeconds(currentDateTime, &currentSeconds) || !toUnixSeconds(endDateTime, &endSeconds) ||
      currentSeconds < endSeconds) {
    return String("--");
  }

  const uint32_t elapsedSeconds = currentSeconds - endSeconds;
  const uint32_t elapsedMinutes = elapsedSeconds / 60UL;
  const uint32_t elapsedHours = elapsedMinutes / 60UL;
  const uint32_t remainingMinutes = elapsedMinutes % 60UL;

  if (elapsedHours == 0) {
    return String(elapsedMinutes) + "M";
  }

  return String(elapsedHours) + "H " + String(remainingMinutes) + "M";
}

bool parsePayloadJson(const String& json, Payload* payload) {
  if (payload == nullptr || json.isEmpty()) {
    return false;
  }

  const size_t docCapacity = 2048U + (static_cast<size_t>(json.length()) * 2U);
  DynamicJsonDocument doc(docCapacity);
  const DeserializationError error = deserializeJson(doc, json);
  if (error) {
    return false;
  }

  Payload nextPayload;
  nextPayload.serverTime = String(doc["serverTime"] | "");
  if (nextPayload.serverTime.isEmpty()) {
    return false;
  }

  const JsonArrayConst records = doc["records"].as<JsonArrayConst>();
  String latestEndTime;
  for (JsonObjectConst item : records) {
    const String endTime = String(item["endTime"] | "");
    if (!isEndTimeNewer(endTime, latestEndTime)) {
      continue;
    }

    latestEndTime = endTime;
    nextPayload.hasLatestRecord = true;
    nextPayload.latestRecord.id = String(item["id"] | "");
    nextPayload.latestRecord.startTime = String(item["startTime"] | "");
    nextPayload.latestRecord.endTime = endTime;
    nextPayload.latestRecord.durationSeconds = item["duration"] | 0UL;
  }

  JsonVariantConst weatherData = doc["weatherData"];
  if (!weatherData.isNull()) {
    nextPayload.hasWeather = true;

    JsonObjectConst now = weatherData["now"].as<JsonObjectConst>();
    nextPayload.outdoor.temp = String(now["temp"] | "--");
    nextPayload.outdoor.icon = parseIconCode(now["icon"], kUnknownWeatherIcon);
    nextPayload.outdoor.text = String(now["text"] | "未知");

    const JsonArrayConst daily3d = weatherData["daily3d"].as<JsonArrayConst>();
    size_t forecastIndex = 0;
    for (JsonObjectConst day : daily3d) {
      if (forecastIndex >= WeatherData::kForecastDayCount) {
        break;
      }

      WeatherData::DailyForecastData& forecast = nextPayload.forecast[forecastIndex];
      forecast.fxDate = String(day["fxDate"] | "");
      forecast.tempMax = String(day["tempMax"] | "");
      forecast.tempMin = String(day["tempMin"] | "");
      forecast.iconDay = parseIconCode(day["iconDay"], 0);
      forecast.textDay = String(day["textDay"] | "");
      ++forecastIndex;
    }
    nextPayload.forecastCount = forecastIndex;
  }

  *payload = nextPayload;
  return true;
}

}  // namespace FeedData