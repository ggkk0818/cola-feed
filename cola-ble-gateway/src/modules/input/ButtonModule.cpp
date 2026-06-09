#include "modules/input/ButtonModule.h"

bool ButtonModule::begin(uint8_t pin, uint8_t mode, uint8_t activeLevel, uint32_t debounceMs) {
  pin_ = pin;
  activeLevel_ = activeLevel;
  debounceMs_ = debounceMs;

  pinMode(pin_, mode);

  const bool pressed = digitalRead(pin_) == activeLevel_;
  stablePressed_ = pressed;
  lastRawPressed_ = pressed;
  lastRawChangeMs_ = millis();
  pressedEvent_ = false;
  initialized_ = true;
  return true;
}

void ButtonModule::poll() {
  if (!initialized_) {
    return;
  }

  const bool rawPressed = digitalRead(pin_) == activeLevel_;
  const uint32_t now = millis();

  if (rawPressed != lastRawPressed_) {
    lastRawPressed_ = rawPressed;
    lastRawChangeMs_ = now;
  }

  if (now - lastRawChangeMs_ < debounceMs_) {
    return;
  }

  if (rawPressed != stablePressed_) {
    stablePressed_ = rawPressed;
    if (stablePressed_) {
      pressedEvent_ = true;
    }
  }
}

bool ButtonModule::wasPressed() {
  if (!pressedEvent_) {
    return false;
  }

  pressedEvent_ = false;
  return true;
}
