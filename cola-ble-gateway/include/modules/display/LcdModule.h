#pragma once

#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <Arduino.h>
#include <U8g2_for_Adafruit_GFX.h>

class TfCardModule;

class LcdModule {
 public:
  LcdModule();

  bool begin();
  void backlightOn();
  void backlightOff();
  void setScreenEnabled(bool enabled);
  void toggleScreen();
  bool isScreenEnabled() const;
  void clearScreen(uint16_t color = ST77XX_BLACK);
  void printText(const String& text, int16_t x = 0, int16_t y = 0, uint16_t color = ST77XX_WHITE,
                 uint8_t textSize = 1);
  void printUtf8(const String& text, int16_t x = 0, int16_t y = 14,
                 uint16_t color = ST77XX_WHITE);
  void drawImage(int16_t x, int16_t y, const uint16_t* imageData, int16_t width, int16_t height);
  bool showBootLogoFromTfCard(TfCardModule& tfCard, const char* logoPath = "/images/logo.png",
                              const char* fallbackText = "System booting...",
                              uint16_t fallbackColor = ST77XX_GREEN, uint8_t fallbackTextSize = 1);
  int16_t width() const;
  int16_t height() const;

 private:
  int16_t centerTextX(const char* text, uint8_t textSize) const;
  void showCenteredText(const char* text, uint16_t color, uint8_t textSize);

  Adafruit_ST7735 tft_;
  U8G2_FOR_ADAFRUIT_GFX u8g2_;
  bool initialized_ = false;
  bool screenEnabled_ = true;
};
