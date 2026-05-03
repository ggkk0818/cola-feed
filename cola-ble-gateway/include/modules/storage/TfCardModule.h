#pragma once

#include <Arduino.h>

class TfCardModule {
 public:
  bool begin();
  bool isMounted() const;
  bool exists(const char* path) const;
  String readTextFile(const char* path) const;
  bool writeTextFile(const char* path, const String& content) const;
  size_t readBinaryFile(const char* path, uint8_t* buffer, size_t bufferSize) const;

 private:
  bool mounted_ = false;
};
