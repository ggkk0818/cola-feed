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

  FeedRecord mockFeedRecord;
  mockFeedRecord.id = "0123456789abcdef0123456789abcdef";
  mockFeedRecord.startTime = "2026-04-22 09:20:00";
  mockFeedRecord.endTime = "2026-04-22 09:40:00";
  mockFeedRecord.duration = 20L * 60L;
  feedController.pushFeedData(mockFeedRecord);
  feedController.setServerTime("2026-04-22 10:05:00");

  if (connected) {
    webServerModule.begin(networkModule, feedController, drawingModule, wifiModule);
    feedController.renderFeedScreenIfNeeded(drawingModule, true, wifiModule.localIp());
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
  feedController.renderFeedScreenIfNeeded(drawingModule, wifiConnected, localIp);

  delay(1000);
}