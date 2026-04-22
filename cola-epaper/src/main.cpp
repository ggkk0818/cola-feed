#include <Arduino.h>
#include <vector>

#include "modules/drawing/DrawingModule.h"
#include "modules/wifi/WifiModule.h"
#include "utils/Utils.h"

DrawingModule drawingModule;
WifiModule wifiModule;

void setup() {
  Serial.begin(115200);

  drawingModule.begin();
  wifiModule.begin();

  const std::vector<String> scannedSsidList = wifiModule.scanNetworks();
  std::vector<String> displayLines = Utils::normalizeSsidList(scannedSsidList, 32);
  if (displayLines.empty()) {
    displayLines.push_back("No WiFi network found");
  }

  drawingModule.renderWifiList(displayLines);
  drawingModule.hibernate();
}

void loop() {
  delay(1000);
}