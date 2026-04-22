#pragma once

#include <Arduino.h>
#include <GxEPD2_3C.h>
#include <U8g2_for_Adafruit_GFX.h>
#include <vector>

class DrawingModule {
 public:
  DrawingModule();

  void begin();
  void renderWifiList(const std::vector<String>& ssidList);
  void hibernate();

 private:
  GxEPD2_3C<GxEPD2_420c, GxEPD2_420c::HEIGHT> display_;
  U8G2_FOR_ADAFRUIT_GFX fonts_;
};
