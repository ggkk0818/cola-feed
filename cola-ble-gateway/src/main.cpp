#include <Arduino.h>

#include "modules/display/LcdModule.h"
#include "modules/i2c/I2cModule.h"
#include "modules/led/RgbLedModule.h"
#include "modules/network/WifiProvisioningModule.h"
#include "modules/storage/TfCardModule.h"

namespace {
LcdModule lcd;
TfCardModule tfCard;
RgbLedModule led;
I2cModule i2c;
WifiProvisioningModule wifiProvisioning(lcd, tfCard);
}  // namespace

void setup() {
  Serial.begin(115200);
  delay(300);

  lcd.begin();
  delay(500);
  lcd.clearScreen(ST77XX_BLACK);
  lcd.printText("System booting...", 0, 0, ST77XX_GREEN, 1);
  delay(500);

  if (i2c.begin()) {
    lcd.printText("I2C bus initialized.", 0, 10, ST77XX_GREEN, 1);
  } else {
    lcd.printText("I2C bus init failed.", 0, 10, ST77XX_RED, 1);
  }

  led.begin();
  led.setColor(0, 60, 0);
  led.on();

  wifiProvisioning.begin();
}

void loop() {
  wifiProvisioning.loop();
  delay(10);
}
