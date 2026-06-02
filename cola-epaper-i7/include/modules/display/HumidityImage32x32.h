#pragma once

#include <Arduino.h>

namespace HumidityImage32x32 {
constexpr uint16_t kWidth = 32;
constexpr uint16_t kHeight = 32;
constexpr size_t kByteSize = (kWidth * kHeight) / 8;

extern const uint8_t kBitmap[kByteSize];
}  // namespace HumidityImage32x32
