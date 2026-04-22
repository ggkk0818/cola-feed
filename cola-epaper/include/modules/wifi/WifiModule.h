#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <vector>

class WifiModule {
 public:
  void begin();
  std::vector<String> scanNetworks();
  bool connect(const String& ssid, const String& password, uint32_t timeoutMs = 15000);
  bool isConnected() const;
  String localIp() const;
  void disconnect(bool powerOffRadio = false);
};
