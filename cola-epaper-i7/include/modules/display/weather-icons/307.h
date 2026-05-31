#pragma once

#include <Arduino.h>

namespace WeatherIcon307 {
constexpr uint16_t kWidth = 32;
constexpr uint16_t kHeight = 32;
constexpr size_t kByteSize = (kWidth * kHeight) / 8;

extern const uint8_t kBitmap[kByteSize];
}  // namespace WeatherIcon307
