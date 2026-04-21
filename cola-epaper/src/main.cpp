#include <Arduino.h>
#include <SPI.h>
#include <GxEPD2_3C.h>
#include <Fonts/FreeMonoBold24pt7b.h>

static constexpr uint8_t EPD_SCK_PIN = 12;
static constexpr uint8_t EPD_MOSI_PIN = 13;
static constexpr uint8_t EPD_CS_PIN = 10;
static constexpr uint8_t EPD_RST_PIN = 14;
static constexpr uint8_t EPD_DC_PIN = 9;
static constexpr uint8_t EPD_BUSY_PIN = 11;

// Waveshare 4.2" 3-color (B/C) panel: 400x300
GxEPD2_3C<GxEPD2_420c, GxEPD2_420c::HEIGHT> display(
  GxEPD2_420c(EPD_CS_PIN, EPD_DC_PIN, EPD_RST_PIN, EPD_BUSY_PIN)
);

void setup() {
  Serial.begin(115200);

  // Match SPI pins from DEV_Config.h: SCK=12, MOSI=13, CS=10
  SPI.begin(EPD_SCK_PIN, -1, EPD_MOSI_PIN, EPD_CS_PIN);
  display.epd2.selectSPI(SPI, SPISettings(4000000, MSBFIRST, SPI_MODE0));

  display.init(115200);
  display.setRotation(0);
  display.setFullWindow();

  const char* text = "test";
  int16_t tbx, tby;
  uint16_t tbw, tbh;

  display.setFont(&FreeMonoBold24pt7b);
  display.getTextBounds(text, 0, 0, &tbx, &tby, &tbw, &tbh);

  const int16_t x = (display.width() - tbw) / 2 - tbx;
  const int16_t y = (display.height() - tbh) / 2 - tby;

  display.firstPage();
  do {
    display.fillScreen(GxEPD_WHITE);
    display.setTextColor(GxEPD_BLACK);
    display.setCursor(x, y);
    display.print(text);
  } while (display.nextPage());

  display.hibernate();
}

void loop() {
  delay(1000);
}