#pragma once

#include <Arduino.h>

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
};
