#pragma once

#include <Arduino.h>

namespace BitmapFont {

struct Face {
  uint8_t glyphWidth;
  uint8_t glyphHeight;
  int16_t ascent;
  int16_t descent;
  int16_t glyphAdvance;
  int16_t spaceAdvance;
};

struct Glyph {
  uint32_t codePoint;
  const uint8_t* bitmap;
  uint8_t xOffset;
  uint8_t width;
  uint8_t advance;
};

using FindGlyphFn = const Glyph* (*)(uint32_t codePoint);

}  // namespace BitmapFont