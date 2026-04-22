#include "modules/drawing/ImageData.h"
#include "modules/drawing/LogoImage64x64.h"
#include "modules/drawing/WifiImage32x32.h"

namespace {
const ImageData kLogoImage64x64(LogoImage64x64::kWidth, LogoImage64x64::kHeight,
                                LogoImage64x64::kBitmap, LogoImage64x64::kByteSize);
const ImageData kWifiImage32x32(WifiImage32x32::kWidth, WifiImage32x32::kHeight,
                                WifiImage32x32::kBitmap, WifiImage32x32::kByteSize);
}  // namespace

ImageData::ImageData(uint16_t width, uint16_t height, const uint8_t* bitmapData, size_t byteSize)
    : width_(width), height_(height), byteSize_(byteSize), bitmapData_(bitmapData) {}

uint16_t ImageData::width() const {
  return width_;
}

uint16_t ImageData::height() const {
  return height_;
}

size_t ImageData::byteSize() const {
  return byteSize_;
}

const uint8_t* ImageData::bitmapData() const {
  return bitmapData_;
}

bool ImageData::isValid() const {
  return bitmapData_ != nullptr && width_ > 0 && height_ > 0 && byteSize_ > 0;
}

const ImageData& ImageData::logoImage64x64() {
  return kLogoImage64x64;
}

const ImageData& ImageData::wifiImage32x32() {
  return kWifiImage32x32;
}
