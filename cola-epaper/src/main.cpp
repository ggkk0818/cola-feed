#include <Arduino.h>

#include "modules/drawing/DrawingModule.h"
#include "modules/feed/FeedController.h"
#include "modules/network/NetworkModule.h"
#include "modules/webserver/WebServerModule.h"
#include "modules/wifi/WifiModule.h"

DrawingModule drawingModule;
FeedController feedController;
NetworkModule networkModule;
WebServerModule webServerModule;
WifiModule wifiModule;
unsigned long startupTimeMs = 0;

void setup() {
  Serial.begin(115200);
  startupTimeMs = millis();

  drawingModule.begin();
  drawingModule.renderLogo();

  wifiModule.begin();

  const bool hasNetworkConfig = wifiModule.hasNetworkConfig();
  bool connected = false;
  if (hasNetworkConfig) {
    connected = wifiModule.connectConfigured(15000);
  }

  if (connected) {
    webServerModule.begin(networkModule, feedController, drawingModule, wifiModule);
  } else {
    drawingModule.renderNoNetwork();
  }
}

void loop() {
  const bool wifiConnected = wifiModule.isConnected();
  if (wifiConnected && !webServerModule.isStarted()) {
    webServerModule.begin(networkModule, feedController, drawingModule, wifiModule);
  }

  if (webServerModule.isStarted()) {
    networkModule.handleClient();
  }

  const String localIp = wifiConnected ? wifiModule.localIp() : String("");
  networkModule.handleDiscoveryBroadcast(wifiConnected, localIp);

  if (millis() - startupTimeMs > 25000UL) {
    // Only attempt to render the feed screen after 25 seconds have passed since startup, 
    // to give the web server some time to start and potentially receive feed data from the client.
    feedController.renderFeedScreenIfNeeded(drawingModule, wifiConnected, localIp);
  }

  delay(1000);
}