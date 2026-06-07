#include <Arduino.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "modules/app/AppRuntime.h"

namespace {
AppRuntime appRuntime;
}

void setup() {
  appRuntime.begin();
}

void loop() {
  appRuntime.loopForever();
}
