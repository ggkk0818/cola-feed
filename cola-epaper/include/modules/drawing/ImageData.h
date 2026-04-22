#pragma once

#include <Arduino.h>

class ImageData {
 public:
  ImageData(uint16_t width, uint16_t height, const uint8_t* bitmapData, size_t byteSize);

  uint16_t width() const;
  uint16_t height() const;
  size_t byteSize() const;
  const uint8_t* bitmapData() const;
  bool isValid() const;

  static const ImageData& logoImage64x64();

 private:
  uint16_t width_;
  uint16_t height_;
  size_t byteSize_;
  const uint8_t* bitmapData_;
};
