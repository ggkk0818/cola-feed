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

static void drawUtf8Scaled2x(
  GxEPD2_3C<GxEPD2_420c, GxEPD2_420c::HEIGHT>& epd,
  const char* text,
  int16_t x,
  int16_t y,
  const uint8_t* font
) {
  U8G2_FOR_ADAFRUIT_GFX metrics;
  metrics.begin(epd);
  metrics.setFont(font);

  const int16_t w = metrics.getUTF8Width(text);
  const int16_t h = metrics.getFontAscent() - metrics.getFontDescent();
  if (w <= 0 || h <= 0) return;

  GFXcanvas1 canvas(w, h);
  canvas.fillScreen(0);

  U8G2_FOR_ADAFRUIT_GFX canvasFonts;
  canvasFonts.begin(canvas);
  canvasFonts.setFontMode(1);
  canvasFonts.setForegroundColor(1);
  canvasFonts.setBackgroundColor(0);
  canvasFonts.setFont(font);
  canvasFonts.setCursor(0, metrics.getFontAscent());
  canvasFonts.print(text);

  // Scale each lit pixel into a 2x2 block to double visual font size.
  for (int16_t py = 0; py < h; py++) {
    for (int16_t px = 0; px < w; px++) {
      if (canvas.getPixel(px, py)) {
        epd.fillRect(x + px * 2, y + py * 2, 2, 2, GxEPD_BLACK);
      }
    }
  }
}

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
  u8g2Fonts.setFont(u8g2_font_wqy16_t_gb2312);

  const char* text = "喂奶";
  const int16_t textWidth = u8g2Fonts.getUTF8Width(text) * 2;
  const int16_t textHeight = (u8g2Fonts.getFontAscent() - u8g2Fonts.getFontDescent()) * 2;

  const int16_t x = (display.width() - textWidth) / 2;
  const int16_t y = (display.height() - textHeight) / 2;

  display.firstPage();
  do {
    display.fillScreen(GxEPD_WHITE);
    drawUtf8Scaled2x(display, text, x, y, u8g2_font_wqy16_t_gb2312);
  } while (display.nextPage());

  display.hibernate();
}

void loop() {
  delay(1000);
}