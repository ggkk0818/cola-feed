#pragma once

#include <Arduino.h>
#include <pgmspace.h>

namespace FontCN32 {

constexpr uint8_t kGlyphWidth = 48;
constexpr uint8_t kGlyphHeight = 62;
constexpr uint8_t kBytesPerRow = kGlyphWidth / 8;
constexpr uint16_t kBytesPerGlyph = kBytesPerRow * kGlyphHeight;
constexpr int16_t kGlyphAscent = kGlyphHeight;
constexpr int16_t kGlyphDescent = 0;
constexpr int16_t kGlyphAdvance = kGlyphWidth;
constexpr int16_t kSpaceAdvance = kGlyphWidth / 2;

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
};

extern const Face kFace;
extern const Glyph kGlyphs[];
extern const size_t kGlyphCount;

const Glyph* findGlyph(uint32_t codePoint);

}  // namespace FontCN32
