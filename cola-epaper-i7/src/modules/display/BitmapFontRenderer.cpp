#include "modules/display/BitmapFontRenderer.h"

#include <cstring>

#include <pgmspace.h>

#include "modules/display/FontCN12.h"
#include "modules/display/FontCN16.h"
#include "modules/display/FontCN32.h"
#include "modules/display/FontCN64.h"
#include "modules/display/FontCN96.h"

namespace {

bool isAsciiVisibleCodePoint(uint32_t codePoint) {
  return codePoint >= 0x21u && codePoint <= 0x7Eu;
}

int16_t extraSpacingForFace(const BitmapFont::Face* face) {
  if (face == &FontCN12::kFace) {
    return 1;
  }

  if (face == &FontCN16::kFace) {
    return 2;
  }

  if (face == &FontCN32::kFace) {
    return 4;
  }

  return 0;
}

int16_t extraSpacingForCodePoint(const BitmapFont::Face* face,
                                const BitmapFont::Glyph* glyph,
                                uint32_t codePoint) {
  if (face == nullptr || glyph == nullptr || !isAsciiVisibleCodePoint(codePoint)) {
    return 0;
  }

  return extraSpacingForFace(face);
}

int16_t leadingSpacingForCodePoint(const BitmapFont::Face* face,
                                   const BitmapFont::Glyph* glyph,
                                   uint32_t codePoint) {
  return extraSpacingForCodePoint(face, glyph, codePoint) / 2;
}

BitmapFontRenderer createRenderer(DisplayDriver& display, const BitmapFont::Face& face,
                                  uint8_t bytesPerRow, BitmapFont::FindGlyphFn findGlyph) {
  BitmapFontRenderer fonts(display, face, bytesPerRow, findGlyph);
  fonts.setFontMode(1);
  fonts.setForegroundColor(GxEPD_BLACK);
  fonts.setBackgroundColor(GxEPD_WHITE);
  return fonts;
}

}  // namespace

BitmapFontRenderer::BitmapFontRenderer(DisplayDriver& display, const BitmapFont::Face& face,
                                       uint8_t bytesPerRow,
                                       BitmapFont::FindGlyphFn findGlyph)
    : display_(&display), face_(&face), bytesPerRow_(bytesPerRow), findGlyph_(findGlyph) {}

void BitmapFontRenderer::setFont(const BitmapFont::Face* face) {
  if (face != nullptr) {
    face_ = face;
  }
}

void BitmapFontRenderer::setFontMode(uint8_t mode) { transparentBackground_ = (mode != 0); }

void BitmapFontRenderer::setForegroundColor(uint16_t color) { foregroundColor_ = color; }

void BitmapFontRenderer::setBackgroundColor(uint16_t color) { backgroundColor_ = color; }

int16_t BitmapFontRenderer::getFontAscent() const { return face_ != nullptr ? face_->ascent : 0; }

int16_t BitmapFontRenderer::getFontDescent() const {
  return face_ != nullptr ? face_->descent : 0;
}

int16_t BitmapFontRenderer::getUTF8Width(const char* text) const {
  if (face_ == nullptr || text == nullptr) {
    return 0;
  }

  const size_t textLength = std::strlen(text);
  size_t index = 0;
  int16_t width = 0;
  while (index < textLength) {
    uint32_t codePoint = 0;
    if (!readNextCodePoint(text, textLength, index, codePoint)) {
      break;
    }

    if (codePoint == '\r' || codePoint == '\n') {
      continue;
    }

    width += advanceForCodePoint(codePoint);
  }

  return width;
}

void BitmapFontRenderer::setCursor(int16_t x, int16_t y) {
  cursorX_ = x;
  cursorY_ = y;
  lineStartX_ = x;
}

int16_t BitmapFontRenderer::drawUTF8(int16_t x, int16_t y, const char* text) {
  setCursor(x, y);
  print(text);
  return cursorX_;
}

size_t BitmapFontRenderer::print(const String& text) { return print(text.c_str()); }

size_t BitmapFontRenderer::print(const char* text) {
  if (display_ == nullptr || face_ == nullptr || findGlyph_ == nullptr || text == nullptr) {
    return 0;
  }

  const size_t textLength = std::strlen(text);
  size_t index = 0;
  size_t renderedGlyphs = 0;
  while (index < textLength) {
    uint32_t codePoint = 0;
    if (!readNextCodePoint(text, textLength, index, codePoint)) {
      break;
    }

    if (codePoint == '\r') {
      continue;
    }

    if (codePoint == '\n') {
      cursorX_ = lineStartX_;
      cursorY_ += face_->glyphHeight;
      continue;
    }

    drawCodePoint(codePoint, cursorX_, cursorY_);
    cursorX_ += advanceForCodePoint(codePoint);
    ++renderedGlyphs;
  }

  return renderedGlyphs;
}

bool BitmapFontRenderer::readNextCodePoint(const char* text, size_t textLength, size_t& index,
                                           uint32_t& codePoint) {
  if (text == nullptr || index >= textLength) {
    return false;
  }

  const uint8_t firstByte = static_cast<uint8_t>(text[index]);
  if ((firstByte & 0x80u) == 0) {
    codePoint = firstByte;
    ++index;
    return true;
  }

  if ((firstByte & 0xE0u) == 0xC0u && (index + 1) < textLength) {
    const uint8_t secondByte = static_cast<uint8_t>(text[index + 1]);
    if ((secondByte & 0xC0u) == 0x80u) {
      codePoint = ((firstByte & 0x1Fu) << 6) | (secondByte & 0x3Fu);
      index += 2;
      return true;
    }
  }

  if ((firstByte & 0xF0u) == 0xE0u && (index + 2) < textLength) {
    const uint8_t secondByte = static_cast<uint8_t>(text[index + 1]);
    const uint8_t thirdByte = static_cast<uint8_t>(text[index + 2]);
    if ((secondByte & 0xC0u) == 0x80u && (thirdByte & 0xC0u) == 0x80u) {
      codePoint = ((firstByte & 0x0Fu) << 12) | ((secondByte & 0x3Fu) << 6) |
                  (thirdByte & 0x3Fu);
      index += 3;
      return true;
    }
  }

  if ((firstByte & 0xF8u) == 0xF0u && (index + 3) < textLength) {
    const uint8_t secondByte = static_cast<uint8_t>(text[index + 1]);
    const uint8_t thirdByte = static_cast<uint8_t>(text[index + 2]);
    const uint8_t fourthByte = static_cast<uint8_t>(text[index + 3]);
    if ((secondByte & 0xC0u) == 0x80u && (thirdByte & 0xC0u) == 0x80u &&
        (fourthByte & 0xC0u) == 0x80u) {
      codePoint = ((firstByte & 0x07u) << 18) | ((secondByte & 0x3Fu) << 12) |
                  ((thirdByte & 0x3Fu) << 6) | (fourthByte & 0x3Fu);
      index += 4;
      return true;
    }
  }

  codePoint = '?';
  ++index;
  return true;
}

const BitmapFont::Glyph* BitmapFontRenderer::findGlyph(uint32_t codePoint) const {
  return findGlyph_ != nullptr ? findGlyph_(codePoint) : nullptr;
}

int16_t BitmapFontRenderer::advanceForCodePoint(uint32_t codePoint) const {
  if (face_ == nullptr) {
    return 0;
  }

  if (codePoint == ' ') {
    return face_->spaceAdvance;
  }

  const BitmapFont::Glyph* glyph = findGlyph(codePoint);
  if (glyph != nullptr && glyph->advance > 0) {
    return glyph->advance + extraSpacingForCodePoint(face_, glyph, codePoint);
  }

  return face_->glyphAdvance;
}

void BitmapFontRenderer::drawCodePoint(uint32_t codePoint, int16_t x, int16_t baselineY) {
  if (display_ == nullptr || face_ == nullptr) {
    return;
  }

  const int16_t glyphTopY = baselineY - face_->ascent;
  const BitmapFont::Glyph* glyph = findGlyph(codePoint);
  const int16_t glyphWidth = advanceForCodePoint(codePoint);
  const int16_t glyphX = x + leadingSpacingForCodePoint(face_, glyph, codePoint);

  if (!transparentBackground_) {
    display_->fillRect(x, glyphTopY, glyphWidth, face_->glyphHeight, backgroundColor_);
  }

  if (codePoint == ' ' || glyph == nullptr) {
    return;
  }

  // Glyph rows are stored in PROGMEM as MSB-first packed bytes.
  for (int16_t yIndex = 0; yIndex < face_->glyphHeight; ++yIndex) {
    for (uint8_t byteIndex = 0; byteIndex < bytesPerRow_; ++byteIndex) {
      const uint8_t rowByte =
          pgm_read_byte(glyph->bitmap + (yIndex * bytesPerRow_) + byteIndex);
      for (uint8_t bitIndex = 0; bitIndex < 8; ++bitIndex) {
        if ((rowByte & (0x80u >> bitIndex)) == 0) {
          continue;
        }

        const int16_t sourceX = (byteIndex * 8) + bitIndex;
        if (sourceX < glyph->xOffset || sourceX >= (glyph->xOffset + glyph->width)) {
          continue;
        }

        display_->drawPixel(glyphX + sourceX - glyph->xOffset, glyphTopY + yIndex,
                            foregroundColor_);
      }
    }
  }
}

BitmapFontRenderer createFontCN16Renderer(DisplayDriver& display) {
  return createRenderer(display, FontCN16::kFace, FontCN16::kBytesPerRow, FontCN16::findGlyph);
}

BitmapFontRenderer createFontCN12Renderer(DisplayDriver& display) {
  return createRenderer(display, FontCN12::kFace, FontCN12::kBytesPerRow, FontCN12::findGlyph);
}

BitmapFontRenderer createFontCN32Renderer(DisplayDriver& display) {
  return createRenderer(display, FontCN32::kFace, FontCN32::kBytesPerRow, FontCN32::findGlyph);
}

BitmapFontRenderer createFontCN64Renderer(DisplayDriver& display) {
  return createRenderer(display, FontCN64::kFace, FontCN64::kBytesPerRow, FontCN64::findGlyph);
}

BitmapFontRenderer createFontCN96Renderer(DisplayDriver& display) {
  return createRenderer(display, FontCN96::kFace, FontCN96::kBytesPerRow, FontCN96::findGlyph);
}

void drawCenterBitmapText(BitmapFontRenderer& fonts, const String& text, int16_t centerX,
                          int16_t centerY) {
  if (text.isEmpty()) {
    return;
  }

  const int16_t fontAscent = fonts.getFontAscent();
  const int16_t fontHeight = fontAscent - fonts.getFontDescent();
  const int16_t textWidth = fonts.getUTF8Width(text.c_str());
  if (fontHeight <= 0 || textWidth <= 0) {
    return;
  }

  const int16_t textX = centerX - (textWidth / 2);
  const int16_t textBaselineY = centerY + ((fontAscent + fonts.getFontDescent()) / 2);
  fonts.drawUTF8(textX, textBaselineY, text.c_str());
}