#include "modules/wifi/WifiModule.h"

namespace {
constexpr const char* kWifiPrefsNamespace = "wifi_cfg";
constexpr const char* kSsidKey = "ssid";
constexpr const char* kPasswordKey = "pwd";
}  // namespace

void WifiModule::begin() {
  WiFi.mode(WIFI_STA);
  WiFi.disconnect(false, true);
  delay(100);

  Preferences preferences;
  if (!preferences.begin(kWifiPrefsNamespace, false)) {
    configuredSsid_ = "";
    configuredPassword_ = "";
    return;
  }

  const bool hasStoredSsid = preferences.isKey(kSsidKey);
  const bool hasStoredPassword = preferences.isKey(kPasswordKey);

  configuredSsid_ = preferences.getString(kSsidKey, "");
  configuredPassword_ = preferences.getString(kPasswordKey, "");

  if (!hasStoredSsid || !hasStoredPassword) {
    preferences.putString(kSsidKey, configuredSsid_);
    preferences.putString(kPasswordKey, configuredPassword_);
  }

  preferences.end();
}

bool WifiModule::hasNetworkConfig() const {
  return !configuredSsid_.isEmpty();
}

bool WifiModule::saveNetworkConfig(const String& ssid, const String& password) {
  if (ssid.isEmpty()) {
    return false;
  }

  Preferences preferences;
  if (!preferences.begin(kWifiPrefsNamespace, false)) {
    return false;
  }

  preferences.putString(kSsidKey, ssid);
  preferences.putString(kPasswordKey, password);
  preferences.end();

  configuredSsid_ = ssid;
  configuredPassword_ = password;
  return true;
}

bool WifiModule::connectConfigured(uint32_t timeoutMs) {
  if (!hasNetworkConfig()) {
    return false;
  }

  return connect(configuredSsid_, configuredPassword_, timeoutMs);
}

String WifiModule::configuredSsid() const {
  return configuredSsid_;
}

String WifiModule::configuredPassword() const {
  return configuredPassword_;
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
  if (ssid.isEmpty()) {
    return false;
  }

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
