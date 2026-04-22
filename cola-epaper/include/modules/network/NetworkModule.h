#pragma once

#include <Arduino.h>
#include <HTTPClient.h>
#include <WebServer.h>

class NetworkModule {
 public:
  NetworkModule();

  bool httpGet(const String& url, String& responseBody, int& statusCode, uint32_t timeoutMs = 8000);

  void beginWebServer();
  void on(const String& uri, HTTPMethod method, WebServer::THandlerFunction handler);
  void onNotFound(WebServer::THandlerFunction handler);
  bool hasArg(const String& argName);
  String arg(const String& argName);
  void send(int code, const String& contentType, const String& content);
  void handleClient();
  bool isWebServerStarted() const;

 private:
  WebServer webServer_;
  bool webServerStarted_;
};
