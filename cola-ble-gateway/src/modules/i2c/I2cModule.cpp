#include "modules/i2c/I2cModule.h"

#include <Wire.h>

#include "config/BoardPins.h"

bool I2cModule::begin(uint32_t frequency) {
  return Wire.begin(board::kQwiicSdaPin, board::kQwiicSclPin, frequency);
}

bool I2cModule::probe(uint8_t address) const {
  Wire.beginTransmission(address);
  return Wire.endTransmission() == 0;
}

bool I2cModule::readRegister(uint8_t address, uint8_t reg, uint8_t* data, size_t len) const {
  if (data == nullptr || len == 0) {
    return false;
  }

  Wire.beginTransmission(address);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) {
    return false;
  }

  if (Wire.requestFrom(static_cast<int>(address), static_cast<int>(len)) != len) {
    return false;
  }

  for (size_t i = 0; i < len; ++i) {
    data[i] = Wire.read();
  }
  return true;
}

bool I2cModule::readBytes(uint8_t address, uint8_t* data, size_t len) const {
  if (data == nullptr || len == 0) {
    return false;
  }

  if (Wire.requestFrom(static_cast<int>(address), static_cast<int>(len)) != len) {
    return false;
  }

  for (size_t i = 0; i < len; ++i) {
    data[i] = Wire.read();
  }
  return true;
}
