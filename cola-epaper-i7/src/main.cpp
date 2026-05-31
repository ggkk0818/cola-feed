#include <Arduino.h>
#include <Adafruit_NeoPixel.h>

#include "modules/display/DisplayModule.h"

namespace {
constexpr uint8_t RGB_LED_PIN = 27;
constexpr uint8_t RGB_LED_COUNT = 1;
}

DisplayModule displayModule;
Adafruit_NeoPixel rgbLed(RGB_LED_COUNT, RGB_LED_PIN, NEO_RGB + NEO_KHZ800);

void setup() {
  displayModule.begin();
  displayModule.renderLogo();
  displayModule.renderFontCN32Test();

  rgbLed.begin();
  rgbLed.clear();
  rgbLed.setPixelColor(0, rgbLed.Color(55, 255, 55));
  rgbLed.show();
  delay(1000);
  rgbLed.clear();
  rgbLed.show();
}

void loop() {
  // Nothing else to run after startup logo render.
}
