#pragma once

#include <Arduino.h>

namespace BatteryImage128x128 {
constexpr uint16_t kWidth = 128;
constexpr uint16_t kHeight = 128;
constexpr size_t kByteSize = (kWidth * kHeight) / 8;

extern const uint8_t kBitmap[kByteSize];
}  // namespace BatteryImage128x128