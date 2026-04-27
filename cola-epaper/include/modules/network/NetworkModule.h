#pragma once

#include <Arduino.h>
#include <HTTPClient.h>
#include <WiFiUdp.h>
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
  void handleDiscoveryBroadcast(bool wifiConnected, const String& localIp);

 private:
  static constexpr uint16_t kDiscoveryBroadcastPort = 6113;
  // 每 5 分钟广播一次，告知局域网内的客户端当前设备的 IP 地址，方便客户端发现设备并连接
  static constexpr unsigned long kDiscoveryBroadcastIntervalMs = 5 * 60000UL;

  WiFiUDP udp_;
  WebServer webServer_;
  bool webServerStarted_;
  bool discoveryUdpStarted_;
  bool lastWifiConnectedForDiscovery_;
  unsigned long lastDiscoveryBroadcastMs_;
};
