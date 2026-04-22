#include "modules/drawing/DrawingModule.h"

#include <Adafruit_GFX.h>
#include <SPI.h>

namespace {
using DisplayType = GxEPD2_3C<GxEPD2_420c, GxEPD2_420c::HEIGHT>;

static constexpr uint8_t EPD_SCK_PIN = 12;
static constexpr uint8_t EPD_MOSI_PIN = 13;
static constexpr uint8_t EPD_CS_PIN = 10;
static constexpr uint8_t EPD_RST_PIN = 14;
static constexpr uint8_t EPD_DC_PIN = 9;
static constexpr uint8_t EPD_BUSY_PIN = 11;

static void drawWifiIcon16x16(DisplayType& display, int16_t x, int16_t y) {
  const ImageData& wifiImage = ImageData::wifiImage32x32();
  if (!wifiImage.isValid()) {
    return;
  }

  const uint8_t* bitmap = wifiImage.bitmapData();
  const uint16_t sourceWidth = wifiImage.width();
  const uint16_t sourceHeight = wifiImage.height();
  const uint16_t sourceBytesPerRow = static_cast<uint16_t>((sourceWidth + 7U) / 8U);

  for (uint16_t dy = 0; dy < 16; ++dy) {
    const uint16_t sourceY = static_cast<uint16_t>(dy * 2U);
    if (sourceY >= sourceHeight) {
      break;
    }

    for (uint16_t dx = 0; dx < 16; ++dx) {
      const uint16_t sourceX = static_cast<uint16_t>(dx * 2U);
      if (sourceX >= sourceWidth) {
        break;
      }

      const size_t byteIndex = static_cast<size_t>(sourceY) * sourceBytesPerRow + (sourceX / 8U);
      const uint8_t bitMask = static_cast<uint8_t>(0x80U >> (sourceX % 8U));
      if ((bitmap[byteIndex] & bitMask) != 0U) {
        display.drawPixel(x + static_cast<int16_t>(dx), y + static_cast<int16_t>(dy), GxEPD_BLACK);
      }
    }
  }
}

static void drawCenterText3x(DisplayType& display, U8G2_FOR_ADAFRUIT_GFX& fonts, const String& text,
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

DrawingModule::DrawingModule()
    : display_(GxEPD2_420c(EPD_CS_PIN, EPD_DC_PIN, EPD_RST_PIN, EPD_BUSY_PIN)) {}

void DrawingModule::begin() {
  SPI.begin(EPD_SCK_PIN, -1, EPD_MOSI_PIN, EPD_CS_PIN);
  display_.epd2.selectSPI(SPI, SPISettings(4000000, MSBFIRST, SPI_MODE0));

  display_.init(115200);
  display_.setRotation(0);
  display_.setFullWindow();

  fonts_.begin(display_);
  fonts_.setFontMode(1);
  fonts_.setForegroundColor(GxEPD_BLACK);
  fonts_.setBackgroundColor(GxEPD_WHITE);
  fonts_.setFont(u8g2_font_wqy16_t_gb2312);
}

void DrawingModule::renderLogo() {
  const int16_t centerX = display_.width() / 2;
  const int16_t centerY = display_.height() / 2;
  const int16_t outerRadius = 56;
  const int16_t innerRadius = 50;
  const char* logoText = "ColaFeed";
  const String logoTextStr(logoText);
  const int16_t logoTextCenterY = centerY + 86;

  const ImageData& logoImage = ImageData::logoImage64x64();
  const int16_t imageX = centerX - static_cast<int16_t>(logoImage.width() / 2);
  const int16_t imageY = centerY - static_cast<int16_t>(logoImage.height() / 2);

  display_.firstPage();
  do {
    display_.fillScreen(GxEPD_WHITE);
    display_.fillCircle(centerX, centerY, outerRadius, GxEPD_RED);
    display_.fillCircle(centerX, centerY, innerRadius, GxEPD_WHITE);
    display_.drawBitmap(imageX, imageY, logoImage.bitmapData(), logoImage.width(), logoImage.height(),
                        GxEPD_RED);

    fonts_.setFont(u8g2_font_logisoso32_tf);
    const int16_t largeFontAscent = fonts_.getFontAscent();
    const int16_t largeFontDescent = fonts_.getFontDescent();
    const int16_t largeTextWidth = fonts_.getUTF8Width(logoText);
    const int16_t largeTextHeight = largeFontAscent - largeFontDescent;
    const bool canUseLargeFont =
        (largeTextWidth > 0) && (largeTextHeight > 0) && (largeTextWidth <= display_.width());

    if (canUseLargeFont) {
      const int16_t textX = centerX - (largeTextWidth / 2);
      const int16_t textBaselineY = logoTextCenterY + ((largeFontAscent + largeFontDescent) / 2);
      fonts_.setCursor(textX, textBaselineY);
      fonts_.print(logoText);
    } else {
      fonts_.setFont(u8g2_font_wqy16_t_gb2312);
      drawCenterText3x(display_, fonts_, logoTextStr, centerX, logoTextCenterY);
    }
  } while (display_.nextPage());

  fonts_.setFont(u8g2_font_wqy16_t_gb2312);

  hibernate();
}

void DrawingModule::renderNoNetwork() {
  const int16_t centerX = display_.width() / 2;
  const int16_t centerY = display_.height() / 2;

  const ImageData& wifiImage = ImageData::wifiImage32x32();
  const int16_t iconCenterY = centerY - 32;
  const int16_t iconX = centerX - static_cast<int16_t>(wifiImage.width() / 2);
  const int16_t iconY = iconCenterY - static_cast<int16_t>(wifiImage.height() / 2);

  const char* noNetworkText = "无网络";
  const int16_t textWidth = fonts_.getUTF8Width(noNetworkText);
  const int16_t textBaselineY = centerY + 20;
  const int16_t textX = centerX - (textWidth / 2);

  display_.firstPage();
  do {
    display_.fillScreen(GxEPD_WHITE);
    display_.drawBitmap(iconX, iconY, wifiImage.bitmapData(), wifiImage.width(), wifiImage.height(),
                        GxEPD_BLACK);
    fonts_.setCursor(textX, textBaselineY);
    fonts_.print(noNetworkText);
  } while (display_.nextPage());

  hibernate();
}

void DrawingModule::renderWifiList(const std::vector<String>& ssidList) {
  const int16_t fontAscent = fonts_.getFontAscent();
  const int16_t lineHeight = fontAscent - fonts_.getFontDescent() + 4;
  const int16_t leftPadding = 8;
  const int16_t topPadding = 12;

  display_.firstPage();
  do {
    display_.fillScreen(GxEPD_WHITE);

    int16_t y = topPadding + fontAscent;
    for (size_t index = 0; index < ssidList.size(); ++index) {
      if (y > display_.height() - 2) {
        break;
      }

      fonts_.setCursor(leftPadding, y);
      fonts_.print(ssidList[index]);
      y += lineHeight;
    }
  } while (display_.nextPage());

  hibernate();
}

void DrawingModule::renderFeedScreen(bool wifiConnected, const String& localIp,
                                     const String& lastFeedDiffTimeStr, bool hasLatestFeedData,
                                     const String& latestFeedEndTime) {
  fonts_.setFont(u8g2_font_wqy16_t_gb2312);

  const int16_t centerX = display_.width() / 2;
  const int16_t centerY = display_.height() / 2;

  const char* titleText = "距离上次喂奶";
  const String mainText = lastFeedDiffTimeStr.isEmpty() ? String("无数据") : lastFeedDiffTimeStr;

  const int16_t fontAscent = fonts_.getFontAscent();
  const int16_t fontHeight = fontAscent - fonts_.getFontDescent();

  const int16_t titleWidth = fonts_.getUTF8Width(titleText);
  const int16_t titleX = centerX - (titleWidth / 2);
  const int16_t titleBaselineY = centerY - 50;

  const int16_t latestWidth = hasLatestFeedData ? fonts_.getUTF8Width(latestFeedEndTime.c_str()) : 0;
  const int16_t latestX = centerX - (latestWidth / 2);
  const int16_t latestBaselineY = centerY + 50;

  const int16_t topPadding = 6;
  const int16_t rightPadding = 6;
  const int16_t iconSize = 16;
  const int16_t iconSpacing = 4;

  display_.firstPage();
  do {
    display_.fillScreen(GxEPD_WHITE);

    if (wifiConnected) {
      const int16_t ipWidth = localIp.isEmpty() ? 0 : fonts_.getUTF8Width(localIp.c_str());
      const int16_t ipBaselineY = topPadding + fontAscent;
      const int16_t ipX = display_.width() - rightPadding - ipWidth;
      const int16_t iconX = ipX - iconSpacing - iconSize;
      const int16_t iconY = topPadding + ((fontHeight - iconSize) / 2);

      drawWifiIcon16x16(display_, iconX, iconY);
      if (!localIp.isEmpty()) {
        fonts_.setCursor(ipX, ipBaselineY);
        fonts_.print(localIp);
      }
    }

    fonts_.setCursor(titleX, titleBaselineY);
    fonts_.print(titleText);

    if (hasLatestFeedData && !lastFeedDiffTimeStr.isEmpty()) {
      fonts_.setFont(u8g2_font_logisoso58_tr);
      const int16_t mainWidth = fonts_.getUTF8Width(lastFeedDiffTimeStr.c_str());
      const int16_t mainAscent = fonts_.getFontAscent();
      const int16_t mainDescent = fonts_.getFontDescent();
      const int16_t mainX = centerX - (mainWidth / 2);
      const int16_t mainBaselineY = centerY + ((mainAscent + mainDescent) / 2);
      fonts_.setCursor(mainX, mainBaselineY);
      fonts_.print(lastFeedDiffTimeStr);
      fonts_.setFont(u8g2_font_wqy16_t_gb2312);
    } else {
      drawCenterText3x(display_, fonts_, mainText, centerX, centerY);
    }

    if (hasLatestFeedData && !latestFeedEndTime.isEmpty()) {
      fonts_.setCursor(latestX, latestBaselineY);
      fonts_.print(latestFeedEndTime);
    }
  } while (display_.nextPage());

  hibernate();
}

void DrawingModule::renderImage(const ImageData& image, int16_t x, int16_t y) {
  if (!image.isValid()) {
    return;
  }

  display_.firstPage();
  do {
    display_.fillScreen(GxEPD_WHITE);
    display_.drawBitmap(x, y, image.bitmapData(), image.width(), image.height(), GxEPD_BLACK);
  } while (display_.nextPage());

  hibernate();
}

void DrawingModule::hibernate() {
  display_.hibernate();
}
