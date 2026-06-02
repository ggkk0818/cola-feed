#pragma once

#include <Arduino.h>

namespace ChargingImage16x16 {
constexpr uint16_t kWidth = 16;
constexpr uint16_t kHeight = 16;
constexpr size_t kByteSize = (kWidth * kHeight) / 8;

extern const uint8_t kBitmap[kByteSize];
}  // namespace ChargingImage16x16