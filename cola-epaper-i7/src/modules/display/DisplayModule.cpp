#include "modules/display/DisplayModule.h"

#include <Adafruit_GFX.h>
#include <SPI.h>

#include "modules/display/LogoImage64x64.h"

namespace {
constexpr uint8_t EPD_SCK_PIN = 1;
constexpr uint8_t EPD_MOSI_PIN = 0;
constexpr uint8_t EPD_CS_PIN = 7;
constexpr uint8_t EPD_RST_PIN = 9;
constexpr uint8_t EPD_DC_PIN = 8;
constexpr uint8_t EPD_BUSY_PIN = 10;

using DisplayDriver =
  GxEPD2_3C<GxEPD2_750c_GDEY075Z08, GxEPD2_750c_GDEY075Z08::HEIGHT / 2>;

void drawCenterText3x(DisplayDriver& display, const String& text, U8G2_FOR_ADAFRUIT_GFX& fonts,
            int16_t centerX, int16_t centerY) {
  if (text.isEmpty()) {
    return;
  }

  const int16_t fontAscent = fonts.getFontAscent();
  const int16_t fontHeight = fontAscent - fonts.getFontDescent();
  const int16_t textWidth = fonts.getUTF8Width(text.c_str());
  if (fontHeight <= 0 || textWidth <= 0) {
    return;
  }

  const int16_t horizontalPadding = 2;
  const int16_t verticalPadding = 8;
  const int16_t canvasWidth = textWidth + (horizontalPadding * 2);
  const int16_t canvasHeight = fontHeight + (verticalPadding * 2);
  if (canvasWidth <= 0 || canvasHeight <= 0) {
    return;
  }

  GFXcanvas1 textCanvas(static_cast<uint16_t>(canvasWidth), static_cast<uint16_t>(canvasHeight));
  textCanvas.fillScreen(0);

  U8G2_FOR_ADAFRUIT_GFX canvasFonts;
  canvasFonts.begin(textCanvas);
  canvasFonts.setFontMode(1);
  canvasFonts.setForegroundColor(1);
  canvasFonts.setBackgroundColor(0);
  canvasFonts.setFont(u8g2_font_wqy16_t_gb2312);
  canvasFonts.setCursor(horizontalPadding, verticalPadding + fontAscent);
  canvasFonts.print(text);

  int16_t minX = canvasWidth;
  int16_t minY = canvasHeight;
  int16_t maxX = -1;
  int16_t maxY = -1;
  for (int16_t yIndex = 0; yIndex < canvasHeight; ++yIndex) {
    for (int16_t xIndex = 0; xIndex < canvasWidth; ++xIndex) {
      if (textCanvas.getPixel(xIndex, yIndex) != 0) {
        if (xIndex < minX) {
          minX = xIndex;
        }
        if (yIndex < minY) {
          minY = yIndex;
        }
        if (xIndex > maxX) {
          maxX = xIndex;
        }
        if (yIndex > maxY) {
          maxY = yIndex;
        }
      }
    }
  }

  if (maxX < minX || maxY < minY) {
    return;
  }

  const int16_t scale = 3;
  const int16_t glyphWidth = (maxX - minX) + 1;
  const int16_t glyphHeight = (maxY - minY) + 1;
  const int16_t scaledWidth = glyphWidth * scale;
  const int16_t scaledHeight = glyphHeight * scale;
  const int16_t drawX = centerX - (scaledWidth / 2);
  const int16_t drawY = centerY - (scaledHeight / 2);

  for (int16_t yIndex = minY; yIndex <= maxY; ++yIndex) {
    for (int16_t xIndex = minX; xIndex <= maxX; ++xIndex) {
      if (textCanvas.getPixel(xIndex, yIndex) != 0) {
        display.fillRect(drawX + (xIndex - minX) * scale, drawY + (yIndex - minY) * scale, scale,
                         scale, GxEPD_BLACK);
      }
    }
  }
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

void DisplayModule::renderLogo() {
  const int16_t centerX = display_.width() / 2;
  const int16_t centerY = display_.height() / 2;
  const int16_t outerRadius = 56;
  const int16_t innerRadius = 50;
  const char* logoText = "ColaFeed";
  const String logoTextStr(logoText);
  const int16_t logoGraphicCenterY = centerY - 44;
  const int16_t logoTextCenterY = centerY + 110;
  const int16_t imageX = centerX - static_cast<int16_t>(LogoImage64x64::kWidth / 2);
  const int16_t imageY = logoGraphicCenterY - static_cast<int16_t>(LogoImage64x64::kHeight / 2);

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
      fonts_.setFont(u8g2_font_wqy16_t_gb2312);
      drawCenterText3x(display_, logoTextStr, fonts_, centerX, logoTextCenterY);
    }
  } while (display_.nextPage());

  fonts_.setFont(u8g2_font_wqy16_t_gb2312);

  hibernate();
}

void DisplayModule::hibernate() {
  display_.hibernate();
}
