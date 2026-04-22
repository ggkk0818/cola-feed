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

  GFXcanvas1 textCanvas(textWidth, fontHeight);
  textCanvas.fillScreen(0);

  U8G2_FOR_ADAFRUIT_GFX canvasFonts;
  canvasFonts.begin(textCanvas);
  canvasFonts.setFontMode(1);
  canvasFonts.setForegroundColor(1);
  canvasFonts.setBackgroundColor(0);
  canvasFonts.setFont(u8g2_font_wqy16_t_gb2312);
  canvasFonts.setCursor(0, fontAscent);
  canvasFonts.print(text);

  const int16_t scale = 3;
  const int16_t scaledWidth = textWidth * scale;
  const int16_t scaledHeight = fontHeight * scale;
  const int16_t drawX = centerX - (scaledWidth / 2);
  const int16_t drawY = centerY - (scaledHeight / 2);

  for (int16_t yIndex = 0; yIndex < fontHeight; ++yIndex) {
    for (int16_t xIndex = 0; xIndex < textWidth; ++xIndex) {
      if (textCanvas.getPixel(xIndex, yIndex) != 0) {
        display.fillRect(drawX + xIndex * scale, drawY + yIndex * scale, scale, scale, GxEPD_BLACK);
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
  } while (display_.nextPage());

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
  const int16_t centerX = display_.width() / 2;
  const int16_t centerY = display_.height() / 2;

  const char* titleText = "上次喂奶";
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

    drawCenterText3x(display_, fonts_, mainText, centerX, centerY);

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
