#include "modules/display/DisplayModule.h"

#include <Adafruit_GFX.h>
#include <SPI.h>

#include "modules/display/BatteryImage128x128.h"
#include "modules/display/BitmapFontRenderer.h"
#include "modules/display/LogoImage64x64.h"

namespace {
constexpr uint8_t EPD_SCK_PIN = 14;
constexpr uint8_t EPD_MOSI_PIN = 13;
constexpr uint8_t EPD_CS_PIN = 4;
constexpr uint8_t EPD_RST_PIN = 23;
constexpr uint8_t EPD_DC_PIN = 5;
constexpr uint8_t EPD_BUSY_PIN = 24;
}  // namespace

DisplayModule::DisplayModule()
    : display_(GxEPD2_750c_GDEY075Z08(EPD_CS_PIN, EPD_DC_PIN, EPD_RST_PIN, EPD_BUSY_PIN)) {}

void DisplayModule::begin() {
  SPI.begin(EPD_SCK_PIN, -1, EPD_MOSI_PIN, EPD_CS_PIN);
  display_.epd2.selectSPI(SPI, SPISettings(4000000, MSBFIRST, SPI_MODE0));

  display_.init(0);
  display_.setRotation(0);
  display_.setFullWindow();
}

void DisplayModule::renderLowBattery() {
  const int16_t imageX =
      (display_.width() - static_cast<int16_t>(BatteryImage128x128::kWidth)) / 2;
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

    drawCenterBitmapText(bitmapFonts, logoTextStr, centerX, logoTextCenterY);
  } while (display_.nextPage());

  hibernate();
}

void DisplayModule::renderFontCN16Test() {
  static constexpr char kTestText[] = "这也是中国人有时";
  display_.setFullWindow();

  auto bitmapFonts = createFontCN16Renderer(display_);
  const int16_t centerX = display_.width() / 2;
  const int16_t centerY = display_.height() / 2;

  display_.firstPage();
  do {
    display_.fillScreen(GxEPD_WHITE);
    drawCenterBitmapText(bitmapFonts, String(kTestText), centerX, centerY);
  } while (display_.nextPage());

  hibernate();
}

void DisplayModule::renderFontCN32Test() {
  static constexpr char kTestText[] = "温！湿度23晴AB吃cd奶时!间";
  display_.setFullWindow();

  auto bitmapFonts = createFontCN32Renderer(display_);
  const int16_t centerX = display_.width() / 2;
  const int16_t centerY = display_.height() / 2;

  display_.firstPage();
  do {
    display_.fillScreen(GxEPD_WHITE);
    drawCenterBitmapText(bitmapFonts, String(kTestText), centerX, centerY);
  } while (display_.nextPage());

  hibernate();
}

void DisplayModule::hibernate() {
  display_.hibernate();
}
