#pragma once

#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <Arduino.h>
#include <U8g2_for_Adafruit_GFX.h>

class LcdModule {
 public:
  LcdModule();

  bool begin();
  void backlightOn();
  void backlightOff();
  void clearScreen(uint16_t color = ST77XX_BLACK);
  void printText(const String& text, int16_t x = 0, int16_t y = 0, uint16_t color = ST77XX_WHITE,
                 uint8_t textSize = 1);
  void printUtf8(const String& text, int16_t x = 0, int16_t y = 14,
                 uint16_t color = ST77XX_WHITE);
  void drawImage(int16_t x, int16_t y, const uint16_t* imageData, int16_t width, int16_t height);
  int16_t width() const;
  int16_t height() const;

 private:
  Adafruit_ST7735 tft_;
  U8G2_FOR_ADAFRUIT_GFX u8g2_;
  bool initialized_ = false;
};
