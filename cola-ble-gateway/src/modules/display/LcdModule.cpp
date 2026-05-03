#include "modules/display/LcdModule.h"

#include "config/BoardPins.h"

namespace {
constexpr uint8_t kBacklightOnLevel = LOW;
constexpr uint8_t kBacklightOffLevel = HIGH;
}

LcdModule::LcdModule()
    : tft_(board::kLcdCsPin, board::kLcdDcPin, board::kLcdMosiPin, board::kLcdSclkPin,
           board::kLcdResetPin) {}

bool LcdModule::begin() {
  pinMode(board::kLcdBacklightPin, OUTPUT);
  backlightOn();

  tft_.initR(INITR_MINI160x80_PLUGIN);

  tft_.setRotation(1);
//   tft_.invertDisplay(true);
  tft_.setTextSize(1);
  tft_.fillScreen(ST77XX_BLACK);
  tft_.drawRect(0, 0, tft_.width(), tft_.height(), ST77XX_GREEN);
  tft_.setCursor(4, 4);
  tft_.setTextColor(ST77XX_WHITE);
  tft_.print("LCD OK");
  tft_.setTextWrap(true);

  u8g2_.begin(tft_);
  u8g2_.setFontMode(1);
  u8g2_.setForegroundColor(ST77XX_WHITE);
  u8g2_.setBackgroundColor(ST77XX_BLACK);
  u8g2_.setFontDirection(0);
  u8g2_.setFont(u8g2_font_wqy16_t_gb2312);

  initialized_ = true;
  return true;
}

void LcdModule::backlightOn() {
  pinMode(board::kLcdBacklightPin, OUTPUT);
  digitalWrite(board::kLcdBacklightPin, kBacklightOnLevel);
}

void LcdModule::backlightOff() {
  pinMode(board::kLcdBacklightPin, OUTPUT);
  digitalWrite(board::kLcdBacklightPin, kBacklightOffLevel);
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

void LcdModule::printUtf8(const String& text, int16_t x, int16_t y, uint16_t color) {
  if (!initialized_) {
    return;
  }

  u8g2_.setForegroundColor(color);
  u8g2_.setCursor(x, y);
  u8g2_.print(text);
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
