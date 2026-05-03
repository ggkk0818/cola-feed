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
  delay(500);
  lcd.clearScreen(ST77XX_BLACK);
  lcd.printText("System booting...", 0, 0, ST77XX_GREEN, 1);
  delay(500);

  if (i2c.begin()) {
    lcd.printText("I2C bus initialized.", 0, 10, ST77XX_GREEN, 1);
  } else {
    lcd.printText("I2C bus init failed.", 0, 10, ST77XX_RED, 1);
  }
  delay(1000);

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

  delay(10000);
  lcd.backlightOff();
  led.off();
}

void loop() {
  delay(1000);
}