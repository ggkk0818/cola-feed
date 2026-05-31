#include "modules/display/DisplayModule.h"

#include <Adafruit_GFX.h>
#include <SPI.h>

#include <cstring>

#include "modules/display/BatteryImage128x128.h"
#include "modules/display/FontCN32.h"
#include "modules/display/LogoImage64x64.h"

namespace {
constexpr uint8_t EPD_SCK_PIN = 14;
constexpr uint8_t EPD_MOSI_PIN = 13;
constexpr uint8_t EPD_CS_PIN = 4;
constexpr uint8_t EPD_RST_PIN = 23;
constexpr uint8_t EPD_DC_PIN = 5;
constexpr uint8_t EPD_BUSY_PIN = 24;

using DisplayDriver =
  GxEPD2_3C<GxEPD2_750c_GDEY075Z08, GxEPD2_750c_GDEY075Z08::HEIGHT / 2>;

class FontCN32Renderer {
 public:
  explicit FontCN32Renderer(DisplayDriver& display) : display_(&display), face_(&FontCN32::kFace) {}

  void setFont(const FontCN32::Face* face) {
    if (face != nullptr) {
      face_ = face;
    }
  }

  void setFontMode(uint8_t mode) { transparentBackground_ = (mode != 0); }
  void setForegroundColor(uint16_t color) { foregroundColor_ = color; }
  void setBackgroundColor(uint16_t color) { backgroundColor_ = color; }

  int16_t getFontAscent() const { return face_->ascent; }
  int16_t getFontDescent() const { return face_->descent; }

  int16_t getUTF8Width(const char* text) const {
    if (text == nullptr) {
      return 0;
    }

    const size_t textLength = std::strlen(text);
    size_t index = 0;
    int16_t width = 0;
    while (index < textLength) {
      uint32_t codePoint = 0;
      if (!readNextCodePoint(text, textLength, index, codePoint)) {
        break;
      }

      if (codePoint == '\r' || codePoint == '\n') {
        continue;
      }

      width += advanceForCodePoint(codePoint);
    }

    return width;
  }

  void setCursor(int16_t x, int16_t y) {
    cursorX_ = x;
    cursorY_ = y;
    lineStartX_ = x;
  }

  int16_t drawUTF8(int16_t x, int16_t y, const char* text) {
    setCursor(x, y);
    print(text);
    return cursorX_;
  }

  size_t print(const String& text) { return print(text.c_str()); }

  size_t print(const char* text) {
    if (display_ == nullptr || face_ == nullptr || text == nullptr) {
      return 0;
    }

    const size_t textLength = std::strlen(text);
    size_t index = 0;
    size_t renderedGlyphs = 0;
    while (index < textLength) {
      uint32_t codePoint = 0;
      if (!readNextCodePoint(text, textLength, index, codePoint)) {
        break;
      }

      if (codePoint == '\r') {
        continue;
      }

      if (codePoint == '\n') {
        cursorX_ = lineStartX_;
        cursorY_ += face_->glyphHeight;
        continue;
      }

      drawCodePoint(codePoint, cursorX_, cursorY_);
      cursorX_ += advanceForCodePoint(codePoint);
      ++renderedGlyphs;
    }

    return renderedGlyphs;
  }

 private:
  static bool readNextCodePoint(const char* text, size_t textLength, size_t& index,
                                uint32_t& codePoint) {
    if (text == nullptr || index >= textLength) {
      return false;
    }

    const uint8_t firstByte = static_cast<uint8_t>(text[index]);
    if ((firstByte & 0x80u) == 0) {
      codePoint = firstByte;
      ++index;
      return true;
    }

    if ((firstByte & 0xE0u) == 0xC0u && (index + 1) < textLength) {
      const uint8_t secondByte = static_cast<uint8_t>(text[index + 1]);
      if ((secondByte & 0xC0u) == 0x80u) {
        codePoint = ((firstByte & 0x1Fu) << 6) | (secondByte & 0x3Fu);
        index += 2;
        return true;
      }
    }

    if ((firstByte & 0xF0u) == 0xE0u && (index + 2) < textLength) {
      const uint8_t secondByte = static_cast<uint8_t>(text[index + 1]);
      const uint8_t thirdByte = static_cast<uint8_t>(text[index + 2]);
      if ((secondByte & 0xC0u) == 0x80u && (thirdByte & 0xC0u) == 0x80u) {
        codePoint = ((firstByte & 0x0Fu) << 12) | ((secondByte & 0x3Fu) << 6) |
                    (thirdByte & 0x3Fu);
        index += 3;
        return true;
      }
    }

    if ((firstByte & 0xF8u) == 0xF0u && (index + 3) < textLength) {
      const uint8_t secondByte = static_cast<uint8_t>(text[index + 1]);
      const uint8_t thirdByte = static_cast<uint8_t>(text[index + 2]);
      const uint8_t fourthByte = static_cast<uint8_t>(text[index + 3]);
      if ((secondByte & 0xC0u) == 0x80u && (thirdByte & 0xC0u) == 0x80u &&
          (fourthByte & 0xC0u) == 0x80u) {
        codePoint = ((firstByte & 0x07u) << 18) | ((secondByte & 0x3Fu) << 12) |
                    ((thirdByte & 0x3Fu) << 6) | (fourthByte & 0x3Fu);
        index += 4;
        return true;
      }
    }

    codePoint = '?';
    ++index;
    return true;
  }

  int16_t advanceForCodePoint(uint32_t codePoint) const {
    if (codePoint == ' ') {
      return face_->spaceAdvance;
    }

    return face_->glyphAdvance;
  }

  void drawCodePoint(uint32_t codePoint, int16_t x, int16_t baselineY) {
    const int16_t glyphTopY = baselineY - face_->ascent;
    const int16_t glyphHeight = face_->glyphHeight;
    const int16_t glyphWidth = advanceForCodePoint(codePoint);

    if (!transparentBackground_) {
      display_->fillRect(x, glyphTopY, glyphWidth, glyphHeight, backgroundColor_);
    }

    if (codePoint == ' ') {
      return;
    }

    const FontCN32::Glyph* glyph = FontCN32::findGlyph(codePoint);
    if (glyph == nullptr) {
      return;
    }

    for (int16_t yIndex = 0; yIndex < face_->glyphHeight; ++yIndex) {
      for (int16_t byteIndex = 0; byteIndex < FontCN32::kBytesPerRow; ++byteIndex) {
        const uint8_t rowByte =
            pgm_read_byte(glyph->bitmap + (yIndex * FontCN32::kBytesPerRow) + byteIndex);
        for (uint8_t bitIndex = 0; bitIndex < 8; ++bitIndex) {
          if ((rowByte & (0x80u >> bitIndex)) != 0) {
            display_->drawPixel(x + (byteIndex * 8) + bitIndex, glyphTopY + yIndex,
                                foregroundColor_);
          }
        }
      }
    }
  }

  DisplayDriver* display_ = nullptr;
  const FontCN32::Face* face_ = nullptr;
  uint16_t foregroundColor_ = GxEPD_BLACK;
  uint16_t backgroundColor_ = GxEPD_WHITE;
  int16_t cursorX_ = 0;
  int16_t cursorY_ = 0;
  int16_t lineStartX_ = 0;
  bool transparentBackground_ = true;
};

FontCN32Renderer createFontCN32Renderer(DisplayDriver& display) {
  FontCN32Renderer fonts(display);
  fonts.setFont(&FontCN32::kFace);
  fonts.setFontMode(1);
  fonts.setForegroundColor(GxEPD_BLACK);
  fonts.setBackgroundColor(GxEPD_WHITE);
  return fonts;
}

void drawCenterBitmapText(FontCN32Renderer& fonts, const String& text, int16_t centerX,
                          int16_t centerY) {
  if (text.isEmpty()) {
    return;
  }

  const int16_t fontAscent = fonts.getFontAscent();
  const int16_t fontHeight = fontAscent - fonts.getFontDescent();
  const int16_t textWidth = fonts.getUTF8Width(text.c_str());
  if (fontHeight <= 0 || textWidth <= 0) {
    return;
  }

  const int16_t textX = centerX - (textWidth / 2);
  const int16_t textBaselineY = centerY + ((fontAscent + fonts.getFontDescent()) / 2);
  fonts.drawUTF8(textX, textBaselineY, text.c_str());
}
}  // namespace

DisplayModule::DisplayModule()
    : display_(GxEPD2_750c_GDEY075Z08(EPD_CS_PIN, EPD_DC_PIN, EPD_RST_PIN, EPD_BUSY_PIN)) {}

void DisplayModule::begin() {
  SPI.begin(EPD_SCK_PIN, -1, EPD_MOSI_PIN, EPD_CS_PIN);
  display_.epd2.selectSPI(SPI, SPISettings(4000000, MSBFIRST, SPI_MODE0));

  display_.init(0);
  display_.setRotation(0);
  display_.setTextColor(GxEPD_BLACK);
  display_.setTextSize(2);
  display_.setFullWindow();

  fonts_.begin(display_);
  fonts_.setFontMode(1);
  fonts_.setForegroundColor(GxEPD_BLACK);
  fonts_.setBackgroundColor(GxEPD_WHITE);
  fonts_.setFont(u8g2_font_wqy16_t_gb2312);
}

void DisplayModule::renderLowBattery() {
    const int16_t imageX = (display_.width() - static_cast<int16_t>(BatteryImage128x128::kWidth)) / 2;
  const int16_t imageY =
      (display_.height() - static_cast<int16_t>(BatteryImage128x128::kHeight)) / 2;

  display_.setFullWindow();
  display_.firstPage();
  do {
    display_.fillScreen(GxEPD_WHITE);
    display_.drawBitmap(imageX, imageY, BatteryImage128x128::kBitmap,
              BatteryImage128x128::kWidth, BatteryImage128x128::kHeight,
              GxEPD_BLACK);
  } while (display_.nextPage());

  hibernate();
}

void DisplayModule::renderLogo() {
  const int16_t centerX = display_.width() / 2;
  const int16_t centerY = display_.height() / 2;
  const int16_t outerRadius = 56;
  const int16_t innerRadius = 50;
  const char* logoText = "ColaFeed";
  const String logoTextStr(logoText);
  const int16_t logoGraphicCenterY = centerY;
  const int16_t logoTextCenterY = centerY + 110;
  const int16_t imageX = centerX - static_cast<int16_t>(LogoImage64x64::kWidth / 2);
  const int16_t imageY = logoGraphicCenterY - static_cast<int16_t>(LogoImage64x64::kHeight / 2);
  auto bitmapFonts = createFontCN32Renderer(display_);

  display_.setFullWindow();
  display_.firstPage();
  do {
    display_.fillScreen(GxEPD_WHITE);
    display_.fillCircle(centerX, logoGraphicCenterY, outerRadius, GxEPD_RED);
    display_.fillCircle(centerX, logoGraphicCenterY, innerRadius, GxEPD_WHITE);
    display_.drawBitmap(imageX, imageY, LogoImage64x64::kBitmap, LogoImage64x64::kWidth,
                        LogoImage64x64::kHeight, GxEPD_RED);

    fonts_.setFont(u8g2_font_logisoso32_tf);
    const int16_t largeFontAscent = fonts_.getFontAscent();
    const int16_t largeFontDescent = fonts_.getFontDescent();
    const int16_t largeTextWidth = fonts_.getUTF8Width(logoText);
    const int16_t largeTextHeight = largeFontAscent - largeFontDescent;
    const bool canUseLargeFont =
        (largeTextWidth > 0) && (largeTextHeight > 0) && (largeTextWidth <= display_.width());

    if (canUseLargeFont) {
      const int16_t textX = centerX - (largeTextWidth / 2);
      const int16_t textBaselineY =
          logoTextCenterY + ((largeFontAscent + largeFontDescent) / 2);
      fonts_.setCursor(textX, textBaselineY);
      fonts_.print(logoText);
    } else {
      drawCenterBitmapText(bitmapFonts, logoTextStr, centerX, logoTextCenterY);
    }
  } while (display_.nextPage());

  fonts_.setFont(u8g2_font_wqy16_t_gb2312);

  hibernate();
}

void DisplayModule::hibernate() {
  display_.hibernate();
}
