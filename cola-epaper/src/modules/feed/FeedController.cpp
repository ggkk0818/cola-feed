#include "modules/feed/FeedController.h"

#include <time.h>

FeedController::FeedController()
    : lastFeedDiffTimeSeconds_(0),
      lastFeedDiffTimeStr_(""),
      hasRenderedFeedScreen_(false),
      lastRenderTimestampMs_(0),
  lastFeedRenderTickMs_(0),
      lastRenderedWifiConnected_(false),
      lastRenderedHasLatestFeedData_(false),
      lastRenderedIp_(""),
      lastRenderedDiffText_(""),
      lastRenderedLatestEndTime_(""),
      hasServerTime_(false),
      serverTimeEpochSeconds_(0),
      lastBoardTimestampMs_(0) {
  feedRecords_.reserve(kMaxFeedRecords);
}

bool FeedController::hasFeedData() const {
  return !feedRecords_.empty();
}

bool FeedController::pushFeedData(const FeedRecord& record) {
  if (record.id.length() != 32 || record.endTime.isEmpty()) {
    return false;
  }

  if (feedRecords_.size() >= kMaxFeedRecords) {
    feedRecords_.erase(feedRecords_.begin());
  }

  feedRecords_.push_back(record);
  return true;
}

void FeedController::clearFeedData() {
  feedRecords_.clear();
  lastFeedDiffTimeSeconds_ = 0;
  lastFeedDiffTimeStr_ = "";
}

void FeedController::calcFeedTime() {
  updateServerTime();

  if (!hasFeedData()) {
    lastFeedDiffTimeSeconds_ = 0;
    lastFeedDiffTimeStr_ = "";
    return;
  }

  const time_t currentEpochSeconds = hasServerTime_
                                         ? serverTimeEpochSeconds_
                                         : static_cast<time_t>(millis() / 1000UL);

  bool foundLatest = false;
  time_t latestEndEpochSeconds = 0;
  for (size_t index = 0; index < feedRecords_.size(); ++index) {
    time_t endEpochSeconds = 0;
    if (!parseDateTimeToEpoch(feedRecords_[index].endTime, endEpochSeconds)) {
      continue;
    }

    if (!foundLatest || endEpochSeconds > latestEndEpochSeconds) {
      latestEndEpochSeconds = endEpochSeconds;
      foundLatest = true;
    }
  }

  if (!foundLatest || currentEpochSeconds <= latestEndEpochSeconds) {
    lastFeedDiffTimeSeconds_ = 0;
    lastFeedDiffTimeStr_ = formatDiffTime(0);
    return;
  }

  const long diffSeconds = static_cast<long>(currentEpochSeconds - latestEndEpochSeconds);
  lastFeedDiffTimeSeconds_ = diffSeconds;
  lastFeedDiffTimeStr_ = formatDiffTime(diffSeconds);
}

bool FeedController::setServerTime(const String& serverTimeStr) {
  time_t parsedServerTime = 0;
  if (!parseDateTimeToEpoch(serverTimeStr, parsedServerTime)) {
    return false;
  }

  serverTimeEpochSeconds_ = parsedServerTime;
  lastBoardTimestampMs_ = millis();
  hasServerTime_ = true;
  return true;
}

void FeedController::updateServerTime() {
  if (!hasServerTime_) {
    return;
  }

  const uint32_t nowBoardTimestampMs = millis();
  const uint32_t elapsedMs = nowBoardTimestampMs - lastBoardTimestampMs_;
  const uint32_t elapsedSeconds = elapsedMs / 1000UL;
  if (elapsedSeconds == 0) {
    return;
  }

  serverTimeEpochSeconds_ += static_cast<time_t>(elapsedSeconds);
  lastBoardTimestampMs_ += elapsedSeconds * 1000UL;
}

void FeedController::requestRenderNow() {
  lastFeedRenderTickMs_ = 0;
}

const std::vector<FeedRecord>& FeedController::feedRecords() const {
  return feedRecords_;
}

long FeedController::lastFeedDiffTimeSeconds() const {
  return lastFeedDiffTimeSeconds_;
}

String FeedController::lastFeedDiffTimeStr() const {
  return lastFeedDiffTimeStr_;
}

String FeedController::latestFeedEndTime() const {
  if (feedRecords_.empty()) {
    return "";
  }

  bool foundLatest = false;
  time_t latestEndEpochSeconds = 0;
  String latestEndTime = "";
  for (size_t index = 0; index < feedRecords_.size(); ++index) {
    if (feedRecords_[index].endTime.isEmpty()) {
      continue;
    }

    time_t endEpochSeconds = 0;
    if (parseDateTimeToEpoch(feedRecords_[index].endTime, endEpochSeconds)) {
      if (!foundLatest || endEpochSeconds > latestEndEpochSeconds) {
        latestEndEpochSeconds = endEpochSeconds;
        latestEndTime = feedRecords_[index].endTime;
        foundLatest = true;
      }
      continue;
    }

    if (!foundLatest && latestEndTime.isEmpty()) {
      latestEndTime = feedRecords_[index].endTime;
    }
  }

  return latestEndTime;
}

bool FeedController::renderFeedScreenIfNeeded(DrawingModule& drawingModule, bool wifiConnected,
                                              const String& localIp) {
  if (!wifiConnected) {
    lastFeedRenderTickMs_ = 0;
    return false;
  }

  const uint32_t nowMs = millis();
  const bool shouldRender =
      (lastFeedRenderTickMs_ == 0) ||
      (static_cast<uint32_t>(nowMs - lastFeedRenderTickMs_) >= kFeedRenderIntervalMs);
  if (!shouldRender) {
    return false;
  }

  lastFeedRenderTickMs_ = nowMs;
  calcFeedTime();

  const bool hasLatestFeedData = hasFeedData();
  const String latestEndTime = hasLatestFeedData ? latestFeedEndTime() : String("");
  const String diffText = lastFeedDiffTimeStr_.isEmpty() ? String("无数据") : lastFeedDiffTimeStr_;

  const bool shouldRefresh =
      !hasRenderedFeedScreen_ || (diffText != lastRenderedDiffText_) ||
      (hasLatestFeedData != lastRenderedHasLatestFeedData_) ||
      (latestEndTime != lastRenderedLatestEndTime_) ||
      (wifiConnected != lastRenderedWifiConnected_) ||
      (wifiConnected && (localIp != lastRenderedIp_));

  if (!shouldRefresh) {
    return false;
  }

  drawingModule.renderFeedScreen(wifiConnected, localIp, diffText, hasLatestFeedData, latestEndTime);

  hasRenderedFeedScreen_ = true;
  lastRenderTimestampMs_ = millis();
  lastRenderedWifiConnected_ = wifiConnected;
  lastRenderedHasLatestFeedData_ = hasLatestFeedData;
  lastRenderedIp_ = localIp;
  lastRenderedDiffText_ = diffText;
  lastRenderedLatestEndTime_ = latestEndTime;

  return true;
}

bool FeedController::parseDateTimeToEpoch(const String& dateTime, time_t& outEpochSeconds) {
  int year = 0;
  int month = 0;
  int day = 0;
  int hour = 0;
  int minute = 0;
  int second = 0;

  const int matched =
      sscanf(dateTime.c_str(), "%d-%d-%d %d:%d:%d", &year, &month, &day, &hour, &minute, &second);
  if (matched != 6) {
    return false;
  }

  struct tm timeInfo;
  memset(&timeInfo, 0, sizeof(timeInfo));
  timeInfo.tm_year = year - 1900;
  timeInfo.tm_mon = month - 1;
  timeInfo.tm_mday = day;
  timeInfo.tm_hour = hour;
  timeInfo.tm_min = minute;
  timeInfo.tm_sec = second;
  timeInfo.tm_isdst = -1;

  const time_t epoch = mktime(&timeInfo);
  if (epoch == static_cast<time_t>(-1)) {
    return false;
  }

  outEpochSeconds = epoch;
  return true;
}

String FeedController::formatDiffTime(long diffSeconds) {
  if (diffSeconds < 5L * 60L) {
    return "刚刚";
  }

  if (diffSeconds < 60L * 60L) {
    const long minutes = diffSeconds / 60L;
    return String(minutes) + "分钟";
  }

  const long hours = diffSeconds / (60L * 60L);
  const long minutes = (diffSeconds % (60L * 60L)) / 60L;
  if (minutes == 0) {
    return String(hours) + "小时";
  }

  return String(hours) + "小时" + String(minutes) + "分钟";
}
