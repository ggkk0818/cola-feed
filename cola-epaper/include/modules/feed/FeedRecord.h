#pragma once

#include <Arduino.h>

struct FeedRecord {
  String id;
  String startTime;
  String endTime;
  long duration;
};
