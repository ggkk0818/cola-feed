#include <Arduino.h>

#include "modules/bluetooth/BleMeshModule.h"
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
BleMeshModule bleMesh(lcd);
WifiProvisioningModule wifiProvisioning(lcd, tfCard);
}  // namespace

void setup() {
  Serial.begin(115200);
  delay(300);

  if (tfCard.begin()) {
    Serial.println("[BOOT] TF card initialized.");
  } else {
    Serial.println("[BOOT] TF card init failed.");
  }

  lcd.begin();
  lcd.showBootLogoFromTfCard(tfCard);
  delay(500);

  if (i2c.begin()) {
    Serial.println("[BOOT] I2C bus initialized.");
  } else {
    Serial.println("[BOOT] I2C bus init failed.");
  }

  led.begin();
  // led.setColor(0, 60, 0);
  // led.on();
  led.off();

  bleMesh.begin();
  wifiProvisioning.setStateChangedCallback(
      [](WifiProvisioningModule::ConnectionState state) { bleMesh.handleWifiStateChange(state); });
  wifiProvisioning.begin();
}

void loop() {
  wifiProvisioning.loop();
  bleMesh.loop();
  delay(10);
}
