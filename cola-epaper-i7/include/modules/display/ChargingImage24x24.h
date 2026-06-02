#pragma once

#include <Arduino.h>

namespace ChargingImage24x24 {
constexpr uint16_t kWidth = 24;
constexpr uint16_t kHeight = 24;
constexpr size_t kByteSize = (kWidth * kHeight) / 8;

extern const uint8_t kBitmap[kByteSize];
}  // namespace ChargingImage24x24