#include "modules/wifi/WifiModule.h"

void WifiModule::begin() {
  WiFi.mode(WIFI_STA);
  WiFi.disconnect(false, true);
  delay(100);
}

std::vector<String> WifiModule::scanNetworks() {
  std::vector<String> ssidList;

  const int16_t foundCount = WiFi.scanNetworks(false, true);
  if (foundCount <= 0) {
    return ssidList;
  }

  ssidList.reserve(foundCount);
  for (int16_t index = 0; index < foundCount; ++index) {
    String ssid = WiFi.SSID(index);
    if (ssid.isEmpty()) {
      ssid = "<hidden ssid>";
    }
    ssidList.push_back(ssid);
  }

  WiFi.scanDelete();
  return ssidList;
}

bool WifiModule::connect(const String& ssid, const String& password, uint32_t timeoutMs) {
  WiFi.begin(ssid.c_str(), password.c_str());

  const unsigned long startMillis = millis();
  while (WiFi.status() != WL_CONNECTED && (millis() - startMillis) < timeoutMs) {
    delay(250);
  }

  return WiFi.status() == WL_CONNECTED;
}

bool WifiModule::isConnected() const {
  return WiFi.status() == WL_CONNECTED;
}

String WifiModule::localIp() const {
  return WiFi.localIP().toString();
}

void WifiModule::disconnect(bool powerOffRadio) {
  WiFi.disconnect(powerOffRadio, true);
}
