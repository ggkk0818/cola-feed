#pragma once

#include <Arduino.h>
#include <GxEPD2_3C.h>
#include <U8g2_for_Adafruit_GFX.h>

class DisplayModule {
 public:
  DisplayModule();

  void begin();
  void renderLogo();
  void hibernate();

 private:
  using DisplayDriver =
      GxEPD2_3C<GxEPD2_750c_GDEY075Z08, GxEPD2_750c_GDEY075Z08::HEIGHT / 2>;

  DisplayDriver display_;
  U8G2_FOR_ADAFRUIT_GFX fonts_;
};
