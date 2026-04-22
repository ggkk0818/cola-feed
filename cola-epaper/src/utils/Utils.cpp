#include "utils/Utils.h"

String Utils::fallbackIfEmpty(const String& value, const String& fallback) {
  if (value.isEmpty()) {
    return fallback;
  }
  return value;
}

String Utils::truncateWithEllipsis(const String& value, size_t maxLength) {
  if (maxLength == 0) {
    return "";
  }

  if (value.length() <= maxLength) {
    return value;
  }

  if (maxLength <= 3) {
    return value.substring(0, static_cast<unsigned int>(maxLength));
  }

  return value.substring(0, static_cast<unsigned int>(maxLength - 3)) + "...";
}

std::vector<String> Utils::normalizeSsidList(const std::vector<String>& ssidList, size_t maxLengthPerLine) {
  std::vector<String> normalized;
  normalized.reserve(ssidList.size());

  for (size_t index = 0; index < ssidList.size(); ++index) {
    const String safeSsid = fallbackIfEmpty(ssidList[index], "<hidden ssid>");
    normalized.push_back(truncateWithEllipsis(safeSsid, maxLengthPerLine));
  }

  return normalized;
}
