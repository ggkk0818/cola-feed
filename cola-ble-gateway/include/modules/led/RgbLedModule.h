#pragma once

#include <Adafruit_DotStar.h>
#include <Arduino.h>

class RgbLedModule {
 public:
  RgbLedModule();

  void begin();
  void setColor(uint8_t red, uint8_t green, uint8_t blue);
  void on();
  void off();

 private:
  void applyColor();

  Adafruit_DotStar led_;
  uint8_t red_ = 0;
  uint8_t green_ = 0;
  uint8_t blue_ = 0;
  bool isOn_ = false;
};
