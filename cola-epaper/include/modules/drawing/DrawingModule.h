#pragma once

#include <Arduino.h>
#include <GxEPD2_3C.h>
#include <U8g2_for_Adafruit_GFX.h>
#include <vector>

#include "modules/drawing/ImageData.h"

class DrawingModule {
 public:
  DrawingModule();

  void begin();
  void renderLogo();
  void renderNoNetwork();
  void renderWifiList(const std::vector<String>& ssidList);
  void renderFeedScreen(bool wifiConnected, const String& localIp,
                        const String& lastFeedDiffTimeStr, bool hasLatestFeedData,
                        const String& latestFeedEndTime, bool isNearFeedingTime);
  void renderImage(const ImageData& image, int16_t x = 0, int16_t y = 0);
  void hibernate();

 private:
  GxEPD2_3C<GxEPD2_420c, GxEPD2_420c::HEIGHT> display_;
  U8G2_FOR_ADAFRUIT_GFX fonts_;
};
