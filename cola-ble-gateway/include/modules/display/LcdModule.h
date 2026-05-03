#pragma once

#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <Arduino.h>

class LcdModule {
 public:
  LcdModule();

  bool begin();
  void clearScreen(uint16_t color = ST77XX_BLACK);
  void printText(const String& text, int16_t x = 0, int16_t y = 0, uint16_t color = ST77XX_WHITE,
                 uint8_t textSize = 1);
  void drawImage(int16_t x, int16_t y, const uint16_t* imageData, int16_t width, int16_t height);
  int16_t width() const;
  int16_t height() const;

 private:
  Adafruit_ST7735 tft_;
  bool initialized_ = false;
};
