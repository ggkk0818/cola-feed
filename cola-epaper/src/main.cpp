#include <Arduino.h>

#include "modules/drawing/DrawingModule.h"
#include "modules/wifi/WifiModule.h"

DrawingModule drawingModule;
WifiModule wifiModule;

void setup() {
  Serial.begin(115200);

  drawingModule.begin();
  drawingModule.renderLogo();

  wifiModule.begin();

  const bool hasNetworkConfig = wifiModule.hasNetworkConfig();
  bool connected = false;
  if (hasNetworkConfig) {
    connected = wifiModule.connectConfigured(15000);
  }

  if (connected) {
    // TODO: implement post-connection workflow.
  } else {
    drawingModule.renderNoNetwork();
  }

  drawingModule.hibernate();
}

void loop() {
  delay(1000);
}