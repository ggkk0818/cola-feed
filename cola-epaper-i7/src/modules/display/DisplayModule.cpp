#include "modules/display/DisplayModule.h"

#include <SPI.h>

namespace {
constexpr uint8_t EPD_SCK_PIN = 1;
constexpr uint8_t EPD_MOSI_PIN = 0;
constexpr uint8_t EPD_CS_PIN = 6;
constexpr uint8_t EPD_RST_PIN = 8;
constexpr uint8_t EPD_DC_PIN = 7;
constexpr uint8_t EPD_BUSY_PIN = 9;

constexpr uint16_t FAST_BW_WINDOW_X = 40;
constexpr uint16_t FAST_BW_WINDOW_Y = 420;
constexpr uint16_t FAST_BW_WINDOW_W = 720;
constexpr uint16_t FAST_BW_WINDOW_H = 50;
}  // namespace

DisplayModule::DisplayModule()
    : display_(GxEPD2_750c_GDEW075Z08(EPD_CS_PIN, EPD_DC_PIN, EPD_RST_PIN, EPD_BUSY_PIN)),
      fastBwReady_(false) {}

void DisplayModule::begin() {
  SPI.begin(EPD_SCK_PIN, -1, EPD_MOSI_PIN, EPD_CS_PIN);
  display_.epd2.selectSPI(SPI, SPISettings(4000000, MSBFIRST, SPI_MODE0));

  display_.init(115200);
  display_.setRotation(1);
  display_.setTextColor(GxEPD_BLACK);
  display_.setTextSize(2);
  display_.setFullWindow();
  fastBwReady_ = false;
}

void DisplayModule::renderLogo() {
  const int16_t centerX = display_.width() / 2;
  const int16_t centerY = display_.height() / 2;

  display_.setFullWindow();
  display_.firstPage();
  do {
    display_.fillScreen(GxEPD_WHITE);

    display_.drawRoundRect(centerX - 170, centerY - 90, 340, 180, 24, GxEPD_BLACK);
    display_.drawRoundRect(centerX - 174, centerY - 94, 348, 188, 28, GxEPD_BLACK);

    display_.setTextSize(5);
    display_.setCursor(centerX - 170, centerY - 6);
    display_.print("ColaFeed");

    display_.setTextSize(2);
    display_.setCursor(centerX - 85, centerY + 44);
    display_.print("EPD 7.5C i7");
  } while (display_.nextPage());

  // Prime a b/w-only partial window, then use fast b/w differential refresh.
  if (!fastBwReady_) {
    clearFastBwWindow_(FAST_BW_WINDOW_X, FAST_BW_WINDOW_Y, FAST_BW_WINDOW_W, FAST_BW_WINDOW_H);
    fastBwReady_ = true;
  }
  drawFastBwText_(FAST_BW_WINDOW_X, FAST_BW_WINDOW_Y, FAST_BW_WINDOW_W, FAST_BW_WINDOW_H,
                  "BW fast refresh ready");
}

void DisplayModule::hibernate() {
  display_.hibernate();
}

void DisplayModule::clearFastBwWindow_(uint16_t x, uint16_t y, uint16_t w, uint16_t h) {
  display_.setPartialWindow(x, y, w, h);
  display_.firstPage();
  do {
    display_.fillScreen(GxEPD_WHITE);
  } while (display_.nextPage());
}

void DisplayModule::drawFastBwText_(uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                                    const String& text) {
  display_.setPartialWindow(x, y, w, h);
  display_.setTextColor(GxEPD_BLACK);
  display_.setTextSize(2);

  display_.firstPage();
  do {
    display_.fillScreen(GxEPD_WHITE);
    display_.setCursor(static_cast<int16_t>(x + 18), static_cast<int16_t>(y + 32));
    display_.print(text);
  } while (display_.nextPageBW());
}
