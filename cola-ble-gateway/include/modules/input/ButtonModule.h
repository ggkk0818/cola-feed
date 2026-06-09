#pragma once

#include <Arduino.h>

class ButtonModule {
 public:
  bool begin(uint8_t pin, uint8_t mode = INPUT_PULLUP, uint8_t activeLevel = LOW,
             uint32_t debounceMs = 50);
  void poll();
  bool wasPressed();

 private:
  uint8_t pin_ = 0;
  uint8_t activeLevel_ = LOW;
  uint32_t debounceMs_ = 50;
  bool initialized_ = false;
  bool stablePressed_ = false;
  bool lastRawPressed_ = false;
  uint32_t lastRawChangeMs_ = 0;
  bool pressedEvent_ = false;
};
