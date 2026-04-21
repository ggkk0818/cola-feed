#include <Arduino.h>
#include <SPI.h>
#include <GxEPD2_3C.h>
#include <U8g2_for_Adafruit_GFX.h>

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

U8G2_FOR_ADAFRUIT_GFX u8g2Fonts;

void setup() {
  Serial.begin(115200);

  // SPI uses the pin mapping defined above.
  SPI.begin(EPD_SCK_PIN, -1, EPD_MOSI_PIN, EPD_CS_PIN);
  display.epd2.selectSPI(SPI, SPISettings(4000000, MSBFIRST, SPI_MODE0));

  display.init(115200);
  display.setRotation(0);
  display.setFullWindow();

  u8g2Fonts.begin(display);
  u8g2Fonts.setFontMode(1);
  u8g2Fonts.setForegroundColor(GxEPD_BLACK);
  u8g2Fonts.setBackgroundColor(GxEPD_WHITE);
  u8g2Fonts.setFont(u8g2_font_unifont_t_chinese2);

  const char* text = "愚蠢的小秦";
  const int16_t textWidth = u8g2Fonts.getUTF8Width(text);
  const int16_t textHeight = u8g2Fonts.getFontAscent() - u8g2Fonts.getFontDescent();

  const int16_t x = (display.width() - textWidth) / 2;
  const int16_t y = (display.height() + textHeight) / 2;

  display.firstPage();
  do {
    display.fillScreen(GxEPD_WHITE);
    u8g2Fonts.setCursor(x, y);
    u8g2Fonts.print(text);
  } while (display.nextPage());

  display.hibernate();
}

void loop() {
  delay(1000);
}