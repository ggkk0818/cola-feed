#include "modules/led/RgbLedModule.h"

#include "config/BoardPins.h"

RgbLedModule::RgbLedModule()
    : led_(1, board::kLedDataPin, board::kLedClockPin, DOTSTAR_BGR) {}

void RgbLedModule::begin() {
  led_.begin();
  led_.show();
}

void RgbLedModule::setColor(uint8_t red, uint8_t green, uint8_t blue) {
  red_ = red;
  green_ = green;
  blue_ = blue;
  isOn_ = true;
  applyColor();
}

void RgbLedModule::on() {
  isOn_ = true;
  applyColor();
}

void RgbLedModule::off() {
  isOn_ = false;
  applyColor();
}

void RgbLedModule::applyColor() {
  if (isOn_) {
    led_.setPixelColor(0, red_, green_, blue_);
  } else {
    led_.setPixelColor(0, 0, 0, 0);
  }
  led_.show();
}
