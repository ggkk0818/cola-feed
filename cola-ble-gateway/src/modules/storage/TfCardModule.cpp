#include "modules/storage/TfCardModule.h"

#include <FS.h>
#include <SD_MMC.h>

#include "config/BoardPins.h"

bool TfCardModule::begin() {
  if (!SD_MMC.setPins(board::kTfClkPin, board::kTfCmdPin, board::kTfD0Pin, board::kTfD1Pin,
                      board::kTfD2Pin, board::kTfD3Pin)) {
    mounted_ = false;
    return false;
  }

  mounted_ = SD_MMC.begin("/sdcard", false, false);
  return mounted_;
}

bool TfCardModule::isMounted() const {
  return mounted_;
}

bool TfCardModule::exists(const char* path) const {
  if (!mounted_ || path == nullptr) {
    return false;
  }
  return SD_MMC.exists(path);
}

String TfCardModule::readTextFile(const char* path) const {
  if (!mounted_ || path == nullptr) {
    return "";
  }

  File file = SD_MMC.open(path, FILE_READ);
  if (!file) {
    return "";
  }

  String content = file.readString();
  file.close();
  return content;
}

bool TfCardModule::writeTextFile(const char* path, const String& content) const {
  if (!mounted_ || path == nullptr) {
    return false;
  }

  File file = SD_MMC.open(path, FILE_WRITE);
  if (!file) {
    return false;
  }

  const bool ok = file.print(content);
  file.close();
  return ok;
}

size_t TfCardModule::readBinaryFile(const char* path, uint8_t* buffer, size_t bufferSize) const {
  if (!mounted_ || path == nullptr || buffer == nullptr || bufferSize == 0) {
    return 0;
  }

  File file = SD_MMC.open(path, FILE_READ);
  if (!file) {
    return 0;
  }

  const size_t bytesRead = file.read(buffer, bufferSize);
  file.close();
  return bytesRead;
}
