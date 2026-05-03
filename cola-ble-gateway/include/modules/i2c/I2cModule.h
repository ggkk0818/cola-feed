#pragma once

#include <Arduino.h>

class I2cModule {
 public:
  bool begin(uint32_t frequency = 400000);
  bool probe(uint8_t address) const;
  bool readRegister(uint8_t address, uint8_t reg, uint8_t* data, size_t len) const;
  bool readBytes(uint8_t address, uint8_t* data, size_t len) const;
};
