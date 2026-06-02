#pragma once

#include <Arduino.h>
#include <pgmspace.h>

#include "modules/display/BitmapFontTypes.h"

namespace FontCN64 {

constexpr uint8_t kGlyphWidth = 48;
constexpr uint8_t kGlyphHeight = 84;
constexpr uint8_t kBytesPerRow = kGlyphWidth / 8;
constexpr uint16_t kBytesPerGlyph = kBytesPerRow * kGlyphHeight;
constexpr int16_t kGlyphAscent = kGlyphHeight;
constexpr int16_t kGlyphDescent = 0;
constexpr int16_t kGlyphAdvance = kGlyphWidth;
constexpr int16_t kSpaceAdvance = kGlyphWidth / 2;

using Face = BitmapFont::Face;
using Glyph = BitmapFont::Glyph;

extern const Face kFace;
extern const Glyph kGlyphs[];
extern const size_t kGlyphCount;

const Glyph* findGlyph(uint32_t codePoint);

}  // namespace FontCN64
