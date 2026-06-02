#pragma once

#include <Arduino.h>

#include "modules/display/BitmapFontTypes.h"
#include "modules/display/DisplayDriver.h"

class BitmapFontRenderer {
 public:
  BitmapFontRenderer(DisplayDriver& display, const BitmapFont::Face& face, uint8_t bytesPerRow,
                     BitmapFont::FindGlyphFn findGlyph);

  void setFont(const BitmapFont::Face* face);
  void setFontMode(uint8_t mode);
  void setForegroundColor(uint16_t color);
  void setBackgroundColor(uint16_t color);

  int16_t getFontAscent() const;
  int16_t getFontDescent() const;
  int16_t getUTF8Width(const char* text) const;

  void setCursor(int16_t x, int16_t y);
  int16_t drawUTF8(int16_t x, int16_t y, const char* text);
  size_t print(const String& text);
  size_t print(const char* text);

 private:
  // Decode UTF-8 once here so all bitmap font faces traverse text identically.
  static bool readNextCodePoint(const char* text, size_t textLength, size_t& index,
                                uint32_t& codePoint);

  const BitmapFont::Glyph* findGlyph(uint32_t codePoint) const;
  int16_t advanceForCodePoint(uint32_t codePoint) const;
  void drawCodePoint(uint32_t codePoint, int16_t x, int16_t baselineY);

  DisplayDriver* display_ = nullptr;
  const BitmapFont::Face* face_ = nullptr;
  uint8_t bytesPerRow_ = 0;
  BitmapFont::FindGlyphFn findGlyph_ = nullptr;
  uint16_t foregroundColor_ = GxEPD_BLACK;
  uint16_t backgroundColor_ = GxEPD_WHITE;
  int16_t cursorX_ = 0;
  int16_t cursorY_ = 0;
  int16_t lineStartX_ = 0;
  bool transparentBackground_ = true;
};

BitmapFontRenderer createFontCN16Renderer(DisplayDriver& display);
BitmapFontRenderer createFontCN12Renderer(DisplayDriver& display);
BitmapFontRenderer createFontCN24Renderer(DisplayDriver& display);
BitmapFontRenderer createFontCN32Renderer(DisplayDriver& display);
BitmapFontRenderer createFontCN64Renderer(DisplayDriver& display);
BitmapFontRenderer createFontCN96Renderer(DisplayDriver& display);
void drawLeftBitmapText(BitmapFontRenderer& fonts, const String& text, int16_t leftX,
                        int16_t centerY);
void drawRightBitmapText(BitmapFontRenderer& fonts, const String& text, int16_t rightX,
                         int16_t centerY);
void drawCenterBitmapText(BitmapFontRenderer& fonts, const String& text, int16_t centerX,
                          int16_t centerY);