#pragma once

#include <Arduino.h>
#include <U8g2_for_Adafruit_GFX.h>

#include "modules/display/DisplayDriver.h"

class DisplayModule {
 public:
  DisplayModule();

  void begin();
  void renderLowBattery();
  void renderLogo();
  void renderFontCN16Test();
  void renderFontCN32Test();
  void hibernate();

 private:
  DisplayDriver display_;
  U8G2_FOR_ADAFRUIT_GFX fonts_;
};
