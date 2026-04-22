#pragma once

#include <Arduino.h>
#include <vector>

#include "modules/drawing/DrawingModule.h"
#include "modules/feed/FeedRecord.h"

class FeedController {
 public:
  static constexpr size_t kMaxFeedRecords = 12;
  static constexpr uint32_t kFeedRenderIntervalMs = 60UL * 1000UL;

  FeedController();

  bool hasFeedData() const;
  bool pushFeedData(const FeedRecord& record);
  void clearFeedData();

  void calcFeedTime();

  bool setServerTime(const String& serverTimeStr);
  void updateServerTime();
  void requestRenderNow();

  const std::vector<FeedRecord>& feedRecords() const;
  long lastFeedDiffTimeSeconds() const;
  String lastFeedDiffTimeStr() const;
  bool renderFeedScreenIfNeeded(DrawingModule& drawingModule, bool wifiConnected,
                                const String& localIp);

 private:
  static bool parseDateTimeToEpoch(const String& dateTime, time_t& outEpochSeconds);
  static String formatDiffTime(long diffSeconds);
  String latestFeedEndTime() const;

  std::vector<FeedRecord> feedRecords_;
  long lastFeedDiffTimeSeconds_;
  String lastFeedDiffTimeStr_;

  bool hasRenderedFeedScreen_;
  uint32_t lastRenderTimestampMs_;
  uint32_t lastFeedRenderTickMs_;
  bool lastRenderedWifiConnected_;
  bool lastRenderedHasLatestFeedData_;
  String lastRenderedIp_;
  String lastRenderedDiffText_;
  String lastRenderedLatestEndTime_;

  bool hasServerTime_;
  time_t serverTimeEpochSeconds_;
  uint32_t lastBoardTimestampMs_;
};
