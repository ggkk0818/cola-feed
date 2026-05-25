#include <Arduino.h>

#include "modules/display/DisplayModule.h"

DisplayModule displayModule;

void setup() {
  displayModule.begin();
  displayModule.renderLogo();
  displayModule.hibernate();
}

void loop() {
  // Nothing else to run after startup logo render.
}
