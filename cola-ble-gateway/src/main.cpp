#include <Arduino.h>

#include "modules/display/LcdModule.h"
#include "modules/i2c/I2cModule.h"
#include "modules/led/RgbLedModule.h"
#include "modules/storage/TfCardModule.h"

namespace {
constexpr const char* kDisplayFilePath = "/display.txt";

LcdModule lcd;
TfCardModule tfCard;
RgbLedModule led;
I2cModule i2c;
}  // namespace

void setup() {
  Serial.begin(115200);
  delay(300);

  lcd.begin();
  lcd.clearScreen(ST77XX_BLACK);
  lcd.printText("System booting...", 0, 0, ST77XX_CYAN, 1);

  if (i2c.begin()) {
    Serial.println("I2C bus initialized.");
  } else {
    Serial.println("I2C bus init failed.");
  }

  String textToDisplay;
  if (!tfCard.begin()) {
    textToDisplay = "TF mount failed.";
  } else {
    if (!tfCard.exists(kDisplayFilePath)) {
      tfCard.writeTextFile(kDisplayFilePath,
                           "Hello from TF card!\n"
                           "Edit /display.txt to update this screen.");
    }

    textToDisplay = tfCard.readTextFile(kDisplayFilePath);
    if (textToDisplay.isEmpty()) {
      textToDisplay = "TF read failed or file is empty.";
    }
  }

  lcd.clearScreen(ST77XX_BLACK);
  lcd.printText(textToDisplay, 0, 0, ST77XX_WHITE, 1);

  led.begin();
  led.setColor(0, 0, 255);
  led.on();
}

void loop() {
  delay(1000);
}