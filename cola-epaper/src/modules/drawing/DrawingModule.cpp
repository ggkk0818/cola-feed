#include "modules/drawing/DrawingModule.h"

#include <SPI.h>

namespace {
static constexpr uint8_t EPD_SCK_PIN = 12;
static constexpr uint8_t EPD_MOSI_PIN = 13;
static constexpr uint8_t EPD_CS_PIN = 10;
static constexpr uint8_t EPD_RST_PIN = 14;
static constexpr uint8_t EPD_DC_PIN = 9;
static constexpr uint8_t EPD_BUSY_PIN = 11;
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
}

void DrawingModule::hibernate() {
  display_.hibernate();
}
