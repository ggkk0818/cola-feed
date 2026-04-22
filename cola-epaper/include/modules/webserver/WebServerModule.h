#pragma once

#include <Arduino.h>

#include "modules/drawing/DrawingModule.h"
#include "modules/feed/FeedController.h"
#include "modules/network/NetworkModule.h"
#include "modules/wifi/WifiModule.h"

class WebServerModule {
 public:
  WebServerModule();

  void begin(NetworkModule& networkModule, FeedController& feedController,
             DrawingModule& drawingModule, WifiModule& wifiModule);
  bool isStarted() const;

 private:
  void registerRoutes(NetworkModule& networkModule);
  void handleRootPage();
  void handleFeedDataPut();
  void sendJsonError(int statusCode, const char* message);
  String buildConfigPlaceholderHtml() const;
  static bool isDateTimeFormatValid(const String& dateTime);

  bool started_;
  NetworkModule* networkModule_;
  FeedController* feedController_;
  DrawingModule* drawingModule_;
  WifiModule* wifiModule_;
};
