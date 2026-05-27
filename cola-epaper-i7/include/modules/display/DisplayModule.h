#pragma once

#include <Arduino.h>
#include <GxEPD2_3C.h>

class DisplayModule {
 public:
  DisplayModule();

  void begin();
  void renderLogo();
  void hibernate();

 private:
  using DisplayDriver =
      GxEPD2_3C<GxEPD2_750c_GDEY075Z08, GxEPD2_750c_GDEY075Z08::HEIGHT / 2>;

  void clearFastBwWindow_(uint16_t x, uint16_t y, uint16_t w, uint16_t h);
  void drawFastBwText_(uint16_t x, uint16_t y, uint16_t w, uint16_t h, const String& text);

  DisplayDriver display_;
  bool fastBwReady_;
};
