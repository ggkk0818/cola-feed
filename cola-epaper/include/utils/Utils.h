#pragma once

#include <Arduino.h>
#include <vector>

class Utils {
 public:
  static String fallbackIfEmpty(const String& value, const String& fallback);
  static String truncateWithEllipsis(const String& value, size_t maxLength);
  static std::vector<String> normalizeSsidList(const std::vector<String>& ssidList, size_t maxLengthPerLine);
};
