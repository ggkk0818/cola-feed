#include "modules/display/LcdModule.h"

#include <SPI.h>

#include "config/BoardPins.h"

LcdModule::LcdModule() : tft_(board::kLcdCsPin, board::kLcdDcPin, board::kLcdResetPin) {}

bool LcdModule::begin() {
  pinMode(board::kLcdBacklightPin, OUTPUT);
  digitalWrite(board::kLcdBacklightPin, HIGH);

  SPI.begin(board::kLcdSclkPin, -1, board::kLcdMosiPin, board::kLcdCsPin);

  tft_.initR(INITR_BLACKTAB);
  tft_.setRotation(1);
  tft_.fillScreen(ST77XX_BLACK);
  tft_.setTextWrap(true);
  initialized_ = true;
  return true;
}

void LcdModule::clearScreen(uint16_t color) {
  if (!initialized_) {
    return;
  }
  tft_.fillScreen(color);
}

void LcdModule::printText(const String& text, int16_t x, int16_t y, uint16_t color, uint8_t textSize) {
  if (!initialized_) {
    return;
  }
  tft_.setCursor(x, y);
  tft_.setTextColor(color);
  tft_.setTextSize(textSize);
  tft_.print(text);
}

void LcdModule::drawImage(int16_t x, int16_t y, const uint16_t* imageData, int16_t width,
                          int16_t height) {
  if (!initialized_ || imageData == nullptr || width <= 0 || height <= 0) {
    return;
  }
  tft_.drawRGBBitmap(x, y, imageData, width, height);
}

int16_t LcdModule::width() const {
  return tft_.width();
}

int16_t LcdModule::height() const {
  return tft_.height();
}
