#pragma once

#include <Arduino.h>

namespace WeatherIcon457 {
constexpr uint16_t kWidth = 64;
constexpr uint16_t kHeight = 64;
constexpr size_t kByteSize = (kWidth * kHeight) / 8;

extern const uint8_t kBitmap[kByteSize];
}  // namespace WeatherIcon457
