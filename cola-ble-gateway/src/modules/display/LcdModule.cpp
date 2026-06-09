#include "modules/display/LcdModule.h"

#include <FS.h>
#include <PNGdec.h>
#include <SD_MMC.h>

#include <cstring>

#include "config/BoardPins.h"
#include "modules/storage/TfCardModule.h"

namespace {
constexpr uint8_t kBacklightOnLevel = LOW;
constexpr uint8_t kBacklightOffLevel = HIGH;
constexpr int16_t kMaxPngLinePixels = 320;

PNG gPngDecoder;
File gPngFile;
LcdModule* gActiveLcd = nullptr;
int16_t gDrawOffsetX = 0;
int16_t gDrawOffsetY = 0;

void* pngOpen(const char* filename, int32_t* size) {
  if (filename == nullptr || size == nullptr) {
    return nullptr;
  }

  gPngFile = SD_MMC.open(filename, FILE_READ);
  if (!gPngFile) {
    *size = 0;
    return nullptr;
  }

  *size = static_cast<int32_t>(gPngFile.size());
  return &gPngFile;
}

void pngClose(void* /* handle */) {
  if (gPngFile) {
    gPngFile.close();
  }
}

int32_t pngRead(PNGFILE* /* handle */, uint8_t* buffer, int32_t length) {
  if (!gPngFile || buffer == nullptr || length <= 0) {
    return 0;
  }

  return gPngFile.read(buffer, length);
}

int32_t pngSeek(PNGFILE* /* handle */, int32_t position) {
  if (!gPngFile || position < 0) {
    return 0;
  }

  return gPngFile.seek(position);
}

int pngDraw(PNGDRAW* draw) {
  if (gActiveLcd == nullptr || draw == nullptr || draw->iWidth <= 0 ||
      draw->iWidth > kMaxPngLinePixels) {
    return 0;
  }

  uint16_t lineBuffer[kMaxPngLinePixels];
  gPngDecoder.getLineAsRGB565(draw, lineBuffer, PNG_RGB565_LITTLE_ENDIAN, 0x000000);
  gActiveLcd->drawImage(gDrawOffsetX, static_cast<int16_t>(gDrawOffsetY + draw->y), lineBuffer,
                        draw->iWidth, 1);
  return 1;
}
}

LcdModule::LcdModule()
    : tft_(board::kLcdCsPin, board::kLcdDcPin, board::kLcdMosiPin, board::kLcdSclkPin,
           board::kLcdResetPin) {}

bool LcdModule::begin() {
  pinMode(board::kLcdBacklightPin, OUTPUT);
  screenEnabled_ = true;
  backlightOn();

  tft_.initR(INITR_MINI160x80_PLUGIN);

  tft_.setRotation(1);
//   tft_.invertDisplay(true);
  tft_.setTextSize(1);
  tft_.fillScreen(ST77XX_BLACK);
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
  screenEnabled_ = true;
}

void LcdModule::backlightOff() {
  pinMode(board::kLcdBacklightPin, OUTPUT);
  digitalWrite(board::kLcdBacklightPin, kBacklightOffLevel);
  screenEnabled_ = false;
}

void LcdModule::setScreenEnabled(bool enabled) {
  if (enabled == screenEnabled_) {
    return;
  }

  if (enabled) {
    backlightOn();
    return;
  }

  backlightOff();
}

void LcdModule::toggleScreen() {
  setScreenEnabled(!screenEnabled_);
}

bool LcdModule::isScreenEnabled() const {
  return screenEnabled_;
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

bool LcdModule::showBootLogoFromTfCard(TfCardModule& tfCard, const char* logoPath,
                                       const char* fallbackText, uint16_t fallbackColor,
                                       uint8_t fallbackTextSize) {
  const char* safeLogoPath = (logoPath == nullptr) ? "/images/logo.png" : logoPath;
  const char* safeFallbackText = (fallbackText == nullptr) ? "System booting..." : fallbackText;

  if (!tfCard.isMounted() && !tfCard.begin()) {
    Serial.println("[BOOT] TF card mount failed.");
    showCenteredText(safeFallbackText, fallbackColor, fallbackTextSize);
    return false;
  }

  if (!tfCard.exists(safeLogoPath)) {
    Serial.printf("[BOOT] Logo not found: %s\n", safeLogoPath);
    showCenteredText(safeFallbackText, fallbackColor, fallbackTextSize);
    return false;
  }

  const int openResult = gPngDecoder.open(safeLogoPath, pngOpen, pngClose, pngRead, pngSeek, pngDraw);
  if (openResult != PNG_SUCCESS) {
    Serial.printf("[BOOT] PNG open failed: %d\n", openResult);
    showCenteredText(safeFallbackText, fallbackColor, fallbackTextSize);
    return false;
  }

  const int16_t logoWidth = static_cast<int16_t>(gPngDecoder.getWidth());
  const int16_t logoHeight = static_cast<int16_t>(gPngDecoder.getHeight());
  gDrawOffsetX = static_cast<int16_t>((width() - logoWidth) / 2);
  gDrawOffsetY = static_cast<int16_t>((height() - logoHeight) / 2);
  if (gDrawOffsetX < 0) {
    gDrawOffsetX = 0;
  }
  if (gDrawOffsetY < 0) {
    gDrawOffsetY = 0;
  }

  clearScreen(ST77XX_BLACK);
  gActiveLcd = this;
  const int decodeResult = gPngDecoder.decode(nullptr, 0);
  gPngDecoder.close();
  gActiveLcd = nullptr;

  if (decodeResult != PNG_SUCCESS) {
    Serial.printf("[BOOT] PNG decode failed: %d\n", decodeResult);
    showCenteredText(safeFallbackText, fallbackColor, fallbackTextSize);
    return false;
  }

  Serial.printf("[BOOT] Displayed logo: %s (%dx%d)\n", safeLogoPath, logoWidth, logoHeight);
  return true;
}

int16_t LcdModule::centerTextX(const char* text, uint8_t textSize) const {
  if (text == nullptr || textSize == 0) {
    return 0;
  }

  const int16_t textWidth = static_cast<int16_t>(std::strlen(text)) * 6 * textSize;
  if (textWidth >= width()) {
    return 0;
  }

  return static_cast<int16_t>((width() - textWidth) / 2);
}

void LcdModule::showCenteredText(const char* text, uint16_t color, uint8_t textSize) {
  if (text == nullptr || textSize == 0) {
    return;
  }

  const int16_t x = centerTextX(text, textSize);
  int16_t y = static_cast<int16_t>((height() - (8 * textSize)) / 2);
  if (y < 0) {
    y = 0;
  }

  clearScreen(ST77XX_BLACK);
  printText(text, x, y, color, textSize);
}

int16_t LcdModule::width() const {
  return tft_.width();
}

int16_t LcdModule::height() const {
  return tft_.height();
}
