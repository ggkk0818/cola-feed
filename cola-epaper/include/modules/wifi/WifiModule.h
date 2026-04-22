#pragma once

#include <Arduino.h>
#include <Preferences.h>
#include <WiFi.h>
#include <vector>

class WifiModule {
 public:
  void begin();
  bool hasNetworkConfig() const;
  bool saveNetworkConfig(const String& ssid, const String& password);
  bool connectConfigured(uint32_t timeoutMs = 15000);
  String configuredSsid() const;
  String configuredPassword() const;
  std::vector<String> scanNetworks();
  bool connect(const String& ssid, const String& password, uint32_t timeoutMs = 15000);
  bool isConnected() const;
  String localIp() const;
  void disconnect(bool powerOffRadio = false);

 private:
  String configuredSsid_;
  String configuredPassword_;
};
