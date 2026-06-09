#include "modules/network/WifiProvisioningModule.h"

#include <ArduinoJson.h>
#include <ESP.h>
#include <FS.h>
#include <SD_MMC.h>
#include <WiFi.h>
#include <mbedtls/base64.h>
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/entropy.h>
#include <mbedtls/md.h>
#include <mbedtls/pk.h>
#include <mbedtls/rsa.h>

#include <cstring>
#include <memory>
#include <vector>

#include "modules/bluetooth/BleMeshModule.h"

namespace {
constexpr const char* kPrefsNamespace = "wifi_cfg";
constexpr const char* kPrefsSsidKey = "ssid";
constexpr const char* kPrefsPasswordKey = "password";

constexpr const char* kApPassword = "12345678";
constexpr uint8_t kDnsPort = 53;
constexpr uint32_t kConnectTimeoutMs = 15000;
constexpr uint8_t kBackgroundReconnectMaxAttempts = 5;
constexpr uint32_t kBackgroundReconnectIntervalMs = 60000;

constexpr const char* kHtmlRootPath = "/html";

constexpr const char* kRsaPublicKeyPem =
    "-----BEGIN PUBLIC KEY-----\n"
    "MIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEA8cX7+asMYNEhAM9F3iTp\n"
    "woTeBWzhnQhibR1Taf9XwsqUQJXDHa/uNyoKRMPd49WKijvwtrOQuKxoVNlaSzUX\n"
    "X9y/4KB0cU0Zditdq5iGaN1kdt/tBrzUiG1kmJHCZfa2+G+n837tqYjbS8tLXRAX\n"
    "aqNEYf4NsNYoOD002/lmtBJPEMVqcCkGi0NBP03X5q/Otvh3uDm23Kann0yRp4dz\n"
    "0hPxzlid5f64L18sDMqZAvIewnh2O/baWNxvmgdwANX0iVFhBKIEIvioI2YMXZcy\n"
    "9m0HtZLCeH3p2UxMfbGWquAS7B41k/9mLBlq+JfJWJIVSdW2L/NVj3tN59oQ//Ij\n"
    "CQIDAQAB\n"
    "-----END PUBLIC KEY-----";

constexpr const char* kRsaPrivateKeyPem =
    "-----BEGIN RSA PRIVATE KEY-----\n"
    "Proc-Type: 4,ENCRYPTED\n"
    "DEK-Info: AES-128-CBC,9E39C7C1E892FDB24825F7549AB7B1EC\n"
    "\n"
    "zR4262DS6G5ka28Q38c3HhYudj8PKIh+dpCBENUeHYaEkIZyByutW43LZS3PqI+N\n"
    "hOqGjuG2SII9JjDiT/FAo/Vi9ahFeo1BjqL8t5jKAaxiCDtfsBNf/7yk4IQKdEw7\n"
    "tJndUfQtvV5JV4qM1gvJ3efNPEWEZg/OSgxnRucJrc2fVi538azdpQJUqPuNTkVr\n"
    "INbnlQ6rYg2nQlvCmeMKmhYN4lI/v3u0DL2FBytqGjabj3jm/MDj95QqxMNsLUJ+\n"
    "sAytNMckdDFEL4BTxttJ0f7F8rTzuL+xA6+XWKfKwIcvfWJvO/hCJO6ycQrPg315\n"
    "U5H/ZqGV7MnplNkD8/N+dKmtK839T022aDb2IvpKj3yNSwoXDqDLAuoC5SQEuGbe\n"
    "nAQ+wMeRBp0NPQxq3dE80xEG0wl08tMDmWQiaOoy3kbKacNdwlxeFl8/9u1f2XMD\n"
    "KB3hmf035JGagEhHZRBA6c1GMqggbGwceNI/XUQ3Hc/6/Juh/jM3bwNLwmQ0BZJR\n"
    "aDQIliQtJ/sIo66D9Ftz+5gJl6+Jv75Lr7OKlTNUvbV0xgT7EJsodj3mE5TxxSQF\n"
    "hFC61Z8XYXuS5e5nsySmO/Yt5J42PqKpd39o8EZ87LyVHkxkGsJMtDH5N5XWUh/K\n"
    "ZBNyUseK/zjosYA6T4sh6+yuiTuRp5KtRkTgVSecnsMDwW9Hdyuq/8tjO1jCnBOa\n"
    "6GeKcWXtUhI3p7IdMhvZYpBTAtflGya4vHbwQ0D5bY7gyuPsv3hePnr4iuGy5jX2\n"
    "fGBQo3B5RKKmBPz/PtXf6yWQxodrtlbfo2FCp1aN5Qx5WV+PtUhd/8fcZjK0459X\n"
    "T1K4SshOJ3IDF+bmhdOdZoJOa4w7oIKsAoCwF+JE/zhIY8+bh/A6s8HO5jZKXRr2\n"
    "rIrnH3LvN+0S5UpTPSfKBND3tTnV0g0D3KgfV0VDWo81qIUliVrk6Vi564WvvD01\n"
    "dyDJbiyv4sX0tkiJ32JbuoDYZcdfgNe52krNn1toyoBrHEhKuf7lBbYByEFss2fw\n"
    "a5H2dD/dFeHD8e03+qZ1uIJcTc0bgPgGDPjTutGC8mhOX5TXx3+m+p4FaZCsa3CA\n"
    "r/HQJT3t9LDAkELosaPhnijtlRA+7POBYvADc1nNQINUj0gbWrXEtqpJGgcBAfIf\n"
    "1VNLDrJbQs+wEpakwWL/KcI4dOcI4hy+JD3lMJEiN7M6hhuALfE4q9E05AzJiTaZ\n"
    "VfrnBjghxb0Sp043YTXc6ryjinADJy/UiYNMp7Xu5Fxu413bGruZyZrfxlvhSO+z\n"
    "wWQaM/mIsNvuu/Jhfo6TvEgTV0s5bPP8t3s6WHxiAj94yN58oZ0rzudiIPLsFnBD\n"
    "W/fHml3Cowii/f33aVNLRXq1Ok1QIb/4d7ZgFt3CQT2ItKcD31ls1uiG2HKFxi9V\n"
    "wABqIKChaTUBiQZmTjpcsUY8BJxeQB3J6VEfBWD3ym5zgAVqpmpZ6Cg20BBJtkPw\n"
    "3ggqGri+z1jGubnEtwJ7TIXD44qes62EIv2Gcis5K+sNTuXw3TNS2eOlNxp4HKjj\n"
    "232YqGW6vk3ojwQEoY+wGdfCM2UDeaM0qmub4UBZE/ITewr2qtYS3nwLfI9cKKP+\n"
    "-----END RSA PRIVATE KEY-----\n";

constexpr const char* kRsaPrivateKeyPassword = "abc123";

}  // namespace

WifiProvisioningModule::WifiProvisioningModule(DisplayService& display, TfCardModule& tfCard)
    : display_(display), tfCard_(tfCard), webServer_(80) {}

void WifiProvisioningModule::attachBleMeshModule(BleMeshModule& bleMesh) {
  bleMesh_ = &bleMesh;
}

void WifiProvisioningModule::setStateChangedCallback(StateChangedCallback callback) {
  stateChangedCallback_ = callback;
}

bool WifiProvisioningModule::isConnected() const {
  return state_ == ConnectionState::kConnected;
}

bool WifiProvisioningModule::isProvisioningMode() const {
  return provisioningModeActive_;
}

WifiProvisioningModule::ConnectionState WifiProvisioningModule::currentState() const {
  return state_;
}

void WifiProvisioningModule::setState(ConnectionState state) {
  if (state_ == state) {
    return;
  }

  state_ = state;
  if (stateChangedCallback_) {
    stateChangedCallback_(state_);
  }
}

void WifiProvisioningModule::begin() {
  preferences_.begin(kPrefsNamespace, false);

  if (!tfCard_.isMounted()) {
    sdMounted_ = tfCard_.begin();
  } else {
    sdMounted_ = true;
  }

  WifiCredentials savedCredentials;
  if (loadSavedCredentials(&savedCredentials) && savedCredentials.isValid()) {
    if (tryConnectStation(savedCredentials, false)) {
      return;
    }
  }

  enterProvisioningMode();
}

void WifiProvisioningModule::loop() {
  if (provisioningModeActive_) {
    dnsServer_.processNextRequest();
  }

  if (provisioningModeActive_ || isConnected()) {
    webServer_.handleClient();
  }

  if (backgroundReconnectActive_) {
    handleBackgroundReconnect();
  } else if (!provisioningModeActive_ && !pendingConnect_ && isConnected() &&
             WiFi.status() != WL_CONNECTED) {
    startBackgroundReconnect();
  }

  const bool wifiConnectedForDiscovery =
      !provisioningModeActive_ && !pendingConnect_ && isConnected() && !backgroundReconnectActive_ &&
      WiFi.status() == WL_CONNECTED;
  handleDiscoveryBroadcast(
      wifiConnectedForDiscovery, wifiConnectedForDiscovery ? WiFi.localIP().toString() : String());

  if (!pendingConnect_) {
    return;
  }

  clearBackgroundReconnect();
  pendingConnect_ = false;
  provisioningModeActive_ = false;

  dnsServer_.stop();
  webServer_.stop();

  if (!tryConnectStation(pendingCredentials_, true)) {
    enterProvisioningMode();
  }
}

void WifiProvisioningModule::handleDiscoveryBroadcast(bool wifiConnected, const String& localIp) {
  if (!wifiConnected) {
    lastWifiConnectedForDiscovery_ = false;
    lastDiscoveryBroadcastMs_ = 0;
    return;
  }

  const unsigned long nowMs = millis();
  const bool firstBroadcastAfterConnected = !lastWifiConnectedForDiscovery_;
  const bool intervalReached = (nowMs - lastDiscoveryBroadcastMs_) >= kDiscoveryBroadcastIntervalMs;
  if (!firstBroadcastAfterConnected && !intervalReached) {
    return;
  }

  DynamicJsonDocument doc(256);
  doc["type"] = "discover";
  doc["chip_id"] = String(static_cast<uint32_t>(ESP.getEfuseMac()), HEX);
  doc["device_name"] = "Cola-gateway";
  doc["ip"] = localIp;

  String payload;
  serializeJson(doc, payload);

  if (!discoveryUdpStarted_) {
    discoveryUdpStarted_ = discoveryUdp_.begin(kDiscoveryBroadcastPort) == 1;
  }
  if (!discoveryUdpStarted_) {
    return;
  }

  discoveryUdp_.beginPacket(IPAddress(255, 255, 255, 255), kDiscoveryBroadcastPort);
  discoveryUdp_.write(reinterpret_cast<const uint8_t*>(payload.c_str()), payload.length());
  discoveryUdp_.endPacket();

  lastWifiConnectedForDiscovery_ = true;
  lastDiscoveryBroadcastMs_ = nowMs;
}

bool WifiProvisioningModule::loadSavedCredentials(WifiCredentials* credentials) {
  if (credentials == nullptr) {
    return false;
  }

  credentials->ssid = preferences_.getString(kPrefsSsidKey, "");
  credentials->password = preferences_.getString(kPrefsPasswordKey, "");
  credentials->ssid.trim();
  credentials->password.trim();
  return credentials->isValid();
}

void WifiProvisioningModule::saveCredentials(const WifiCredentials& credentials) {
  preferences_.putString(kPrefsSsidKey, credentials.ssid);
  preferences_.putString(kPrefsPasswordKey, credentials.password);
}

void WifiProvisioningModule::startBackgroundReconnect() {
  if (backgroundReconnectActive_) {
    return;
  }

  WifiCredentials savedCredentials;
  if (!loadSavedCredentials(&savedCredentials) || !savedCredentials.isValid()) {
    Serial.println("[WiFi] Background reconnect skipped: saved credentials unavailable.");
    enterProvisioningMode();
    return;
  }

  Serial.printf("[WiFi] Connection lost; keeping BLE active and starting background reconnect to SSID=%s\n",
                savedCredentials.ssid.c_str());

  backgroundReconnectCredentials_ = savedCredentials;
  backgroundReconnectActive_ = true;
  backgroundReconnectAttemptInProgress_ = false;
  backgroundReconnectAttempts_ = 0;
  backgroundReconnectAttemptStartedMs_ = 0;
  backgroundReconnectNextAttemptMs_ = millis();

  beginBackgroundReconnectAttempt(backgroundReconnectNextAttemptMs_);
}

void WifiProvisioningModule::handleBackgroundReconnect() {
  if (!backgroundReconnectActive_) {
    return;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("[WiFi] Background reconnect succeeded: ip=%s, attempts=%u\n",
                  WiFi.localIP().toString().c_str(),
                  static_cast<unsigned>(backgroundReconnectAttempts_));
    clearBackgroundReconnect();
    provisioningModeActive_ = false;
    setupWebServer();
    return;
  }

  const unsigned long nowMs = millis();
  if (backgroundReconnectAttemptInProgress_) {
    if ((nowMs - backgroundReconnectAttemptStartedMs_) < kConnectTimeoutMs) {
      return;
    }

    backgroundReconnectAttemptInProgress_ = false;
    backgroundReconnectNextAttemptMs_ = backgroundReconnectAttemptStartedMs_ + kBackgroundReconnectIntervalMs;
    Serial.printf("[WiFi] Background reconnect attempt %u failed.\n",
                  static_cast<unsigned>(backgroundReconnectAttempts_));

    if (backgroundReconnectAttempts_ >= kBackgroundReconnectMaxAttempts) {
      Serial.println("[WiFi] Background reconnect exhausted; entering provisioning mode.");
      clearBackgroundReconnect();
      enterProvisioningMode();
    }
    return;
  }

  if (backgroundReconnectAttempts_ < kBackgroundReconnectMaxAttempts &&
      (nowMs - backgroundReconnectNextAttemptMs_) < 0x80000000UL) {
    beginBackgroundReconnectAttempt(nowMs);
  }
}

void WifiProvisioningModule::beginBackgroundReconnectAttempt(unsigned long nowMs) {
  if (!backgroundReconnectActive_ || !backgroundReconnectCredentials_.isValid()) {
    return;
  }

  ++backgroundReconnectAttempts_;
  backgroundReconnectAttemptInProgress_ = true;
  backgroundReconnectAttemptStartedMs_ = nowMs;
  Serial.printf("[WiFi] Background reconnect attempt %u/%u to SSID=%s\n",
                static_cast<unsigned>(backgroundReconnectAttempts_),
                static_cast<unsigned>(kBackgroundReconnectMaxAttempts),
                backgroundReconnectCredentials_.ssid.c_str());

  WiFi.mode(WIFI_STA);
  WiFi.disconnect(false, false);
  WiFi.begin(backgroundReconnectCredentials_.ssid.c_str(), backgroundReconnectCredentials_.password.c_str());
}

void WifiProvisioningModule::clearBackgroundReconnect() {
  backgroundReconnectActive_ = false;
  backgroundReconnectAttemptInProgress_ = false;
  backgroundReconnectAttempts_ = 0;
  backgroundReconnectAttemptStartedMs_ = 0;
  backgroundReconnectNextAttemptMs_ = 0;
  backgroundReconnectCredentials_.ssid = "";
  backgroundReconnectCredentials_.password = "";
}

bool WifiProvisioningModule::tryConnectStation(const WifiCredentials& credentials, bool saveOnSuccess) {
  clearBackgroundReconnect();
  showConnectingScreen(credentials.ssid);
  setState(ConnectionState::kConnecting);

  WiFi.mode(WIFI_STA);
  WiFi.disconnect(true, true);
  delay(200);

  WiFi.begin(credentials.ssid.c_str(), credentials.password.c_str());

  const uint32_t startMs = millis();
  while (millis() - startMs < kConnectTimeoutMs) {
    if (WiFi.status() == WL_CONNECTED) {
      break;
    }
    delay(300);
  }

  if (WiFi.status() == WL_CONNECTED) {
    if (saveOnSuccess) {
      saveCredentials(credentials);
    }

    provisioningModeActive_ = false;
    setupWebServer();

    showConnectResult(true, WiFi.localIP().toString());

    delay(3000);

    // Notify external modules (such as BLE) after connection result prompt finishes.
    setState(ConnectionState::kConnected);

    return true;
  }

  setState(ConnectionState::kDisconnected);
  showConnectResult(false, "请检查账号或密码");
  delay(10000);
  return false;
}

void WifiProvisioningModule::enterProvisioningMode() {
  clearBackgroundReconnect();
  WiFi.disconnect(true, true);
  delay(200);

  WiFi.mode(WIFI_AP_STA);
  apSsid_ = makeApSsid();
  WiFi.softAP(apSsid_.c_str(), kApPassword);

  if (!tfCard_.isMounted()) {
    sdMounted_ = tfCard_.begin();
  } else {
    sdMounted_ = true;
  }

  setupWebServer();

  dnsServer_.stop();
  dnsServer_.start(kDnsPort, "*", WiFi.softAPIP());

  provisioningModeActive_ = true;
  setState(ConnectionState::kProvisioning);
  showProvisioningScreen();
}

void WifiProvisioningModule::setupWebServer() {
  if (!webServerConfigured_) {
    webServer_.on("/", HTTP_GET, [this]() { handleRootPage(); });

    webServer_.on("/api/wifi/list", HTTP_GET, [this]() { handleWifiList(); });
    webServer_.on("/api/rsa/public_key", HTTP_GET, [this]() { handleRsaPublicKey(); });
    webServer_.on("/api/wifi/connect", HTTP_POST, [this]() { handleWifiConnect(); });
    webServer_.on("/api/feedData", HTTP_PUT, [this]() { handleFeedDataPut(); });

    webServer_.on("/generate_204", HTTP_GET, [this]() {
      webServer_.sendHeader("Location", "http://" + WiFi.softAPIP().toString() + "/", true);
      webServer_.send(302, "text/plain", "");
    });
    webServer_.on("/hotspot-detect.html", HTTP_GET, [this]() {
      webServer_.sendHeader("Location", "http://" + WiFi.softAPIP().toString() + "/", true);
      webServer_.send(302, "text/plain", "");
    });
    webServer_.on("/connecttest.txt", HTTP_GET, [this]() {
      webServer_.sendHeader("Location", "http://" + WiFi.softAPIP().toString() + "/", true);
      webServer_.send(302, "text/plain", "");
    });
    webServer_.on("/ncsi.txt", HTTP_GET, [this]() {
      webServer_.sendHeader("Location", "http://" + WiFi.softAPIP().toString() + "/", true);
      webServer_.send(302, "text/plain", "");
    });
    webServer_.on("/fwlink", HTTP_GET, [this]() {
      webServer_.sendHeader("Location", "http://" + WiFi.softAPIP().toString() + "/", true);
      webServer_.send(302, "text/plain", "");
    });

    webServer_.onNotFound([this]() { handleNotFound(); });
    webServerConfigured_ = true;
  }

  webServer_.begin();
}

void WifiProvisioningModule::handleRootPage() {
  if (!serveFileFromSd("/")) {
    webServer_.send(404, "text/plain;charset=utf-8", "404 Not Found");
  }
}

void WifiProvisioningModule::handleWifiList() {
  if (!provisioningModeActive_) {
    sendJsonError(403, "endpoint available in provisioning mode only");
    return;
  }

  DynamicJsonDocument responseDoc(6144);
  responseDoc["code"] = 200;
  JsonArray data = responseDoc.createNestedArray("data");

  const int16_t count = WiFi.scanNetworks(false, true);
  if (count > 0) {
    for (int16_t i = 0; i < count; ++i) {
      const String ssid = WiFi.SSID(i);
      if (ssid.isEmpty()) {
        continue;
      }
      JsonObject item = data.createNestedObject();
      item["ssid"] = ssid;
    }
  }
  WiFi.scanDelete();

  String body;
  serializeJson(responseDoc, body);
  webServer_.send(200, "application/json", body);
}

void WifiProvisioningModule::handleRsaPublicKey() {
  if (!provisioningModeActive_) {
    sendJsonError(403, "endpoint available in provisioning mode only");
    return;
  }

  DynamicJsonDocument responseDoc(2048);
  responseDoc["code"] = 200;
  JsonObject data = responseDoc.createNestedObject("data");
  data["public_key"] = kRsaPublicKeyPem;

  String body;
  serializeJson(responseDoc, body);
  webServer_.send(200, "application/json", body);
}

void WifiProvisioningModule::handleWifiConnect() {
  if (!provisioningModeActive_) {
    sendJsonError(403, "endpoint available in provisioning mode only");
    return;
  }

  DynamicJsonDocument requestDoc(2048);
  DeserializationError parseError = deserializeJson(requestDoc, webServer_.arg("plain"));
  if (parseError) {
    DynamicJsonDocument errorDoc(256);
    errorDoc["code"] = 400;
    errorDoc["message"] = "请求体格式错误";

    String body;
    serializeJson(errorDoc, body);
    webServer_.send(400, "application/json", body);
    return;
  }

  const String ssid = String(requestDoc["ssid"] | "");
  const String incomingPassword = String(requestDoc["password"] | "");

  if (ssid.isEmpty() || incomingPassword.isEmpty()) {
    DynamicJsonDocument errorDoc(256);
    errorDoc["code"] = 400;
    errorDoc["message"] = "ssid 或 password 不能为空";

    String body;
    serializeJson(errorDoc, body);
    webServer_.send(400, "application/json", body);
    return;
  }

  String plainPassword;
  if (!decryptPassword(incomingPassword, &plainPassword)) {
    if (incomingPassword.length() > 63) {
      DynamicJsonDocument errorDoc(256);
      errorDoc["code"] = 400;
      errorDoc["message"] = "密码解密失败";

      String body;
      serializeJson(errorDoc, body);
      webServer_.send(400, "application/json", body);
      return;
    }

    plainPassword = incomingPassword;
  }

  pendingCredentials_.ssid = ssid;
  pendingCredentials_.password = plainPassword;
  pendingConnect_ = true;

  DynamicJsonDocument responseDoc(256);
  responseDoc["code"] = 200;
  responseDoc["message"] = "连接请求已发送";

  String body;
  serializeJson(responseDoc, body);
  webServer_.send(200, "application/json", body);
}

void WifiProvisioningModule::handleFeedDataPut() {
  Serial.println("[HTTP] PUT /api/feedData received.");

  if (provisioningModeActive_ || !isConnected()) {
    Serial.printf("[HTTP] feedData rejected: provisioningModeActive=%d, connected=%d\n",
                  provisioningModeActive_ ? 1 : 0,
                  isConnected() ? 1 : 0);
    sendJsonError(503, "feedData endpoint unavailable while not connected");
    return;
  }

  if (bleMesh_ == nullptr) {
    Serial.println("[HTTP] feedData rejected: ble module is not ready.");
    sendJsonError(500, "ble module is not ready");
    return;
  }

  if (!webServer_.hasArg("plain")) {
    Serial.println("[HTTP] feedData rejected: missing request body.");
    sendJsonError(400, "missing request body");
    return;
  }

  const String requestBody = webServer_.arg("plain");
  if (requestBody.isEmpty()) {
    Serial.println("[HTTP] feedData rejected: empty request body.");
    sendJsonError(400, "empty request body");
    return;
  }

  Serial.printf("[HTTP] feedData body length=%u\n", static_cast<unsigned>(requestBody.length()));

  DynamicJsonDocument requestDoc(8192);
  const DeserializationError parseError = deserializeJson(requestDoc, requestBody);
  if (parseError) {
    Serial.printf("[HTTP] feedData JSON parse failed: %s\n", parseError.c_str());
    sendJsonError(400, "invalid json body");
    return;
  }

  JsonVariant serverTimeVar = requestDoc["serverTime"];
  JsonVariant recordsVar = requestDoc["records"];
  JsonVariant weatherDataVar = requestDoc["weatherData"];
  if (!serverTimeVar.is<const char*>() || !recordsVar.is<JsonArray>()) {
    Serial.println("[HTTP] feedData rejected: serverTime/records types invalid.");
    sendJsonError(400, "serverTime must be string and records must be array");
    return;
  }

  if (!weatherDataVar.isNull() && !weatherDataVar.is<JsonObject>()) {
    Serial.println("[HTTP] feedData rejected: weatherData type invalid.");
    sendJsonError(400, "weatherData must be object or null");
    return;
  }

  const String serverTime = String(serverTimeVar.as<const char*>());
  if (!isDateTimeFormatValid(serverTime)) {
    Serial.printf("[HTTP] feedData rejected: invalid serverTime=%s\n", serverTime.c_str());
    sendJsonError(400, "invalid serverTime format");
    return;
  }

  String weatherDataJson;
  const bool weatherDataIsNull = weatherDataVar.isNull();
  if (!weatherDataIsNull) {
    serializeJson(weatherDataVar, weatherDataJson);
  }

  Serial.printf("[HTTP] feedData parsed: serverTime=%s, weatherDataIsNull=%d\n",
                serverTime.c_str(),
                weatherDataIsNull ? 1 : 0);

  JsonArray records = recordsVar.as<JsonArray>();
  std::vector<FeedRecord> parsedRecords;
  parsedRecords.reserve(records.size());

  Serial.printf("[HTTP] feedData records count=%u\n", static_cast<unsigned>(records.size()));

  for (JsonObject recordObj : records) {
    JsonVariant idVar = recordObj["id"];
    JsonVariant startTimeVar = recordObj["startTime"];
    JsonVariant endTimeVar = recordObj["endTime"];
    JsonVariant durationVar = recordObj["duration"];

    if (!idVar.is<const char*>() || !startTimeVar.is<const char*>() ||
        !endTimeVar.is<const char*>() || !durationVar.is<long>()) {
      Serial.println("[HTTP] feedData rejected: record field types invalid.");
      sendJsonError(400, "record fields are invalid");
      return;
    }

    FeedRecord record;
    record.id = String(idVar.as<const char*>());
    record.startTime = String(startTimeVar.as<const char*>());
    record.endTime = String(endTimeVar.as<const char*>());
    record.duration = durationVar.as<long>();

    if (record.id.length() != 32 || record.endTime.isEmpty() || record.duration < 0) {
      Serial.printf("[HTTP] feedData rejected: record constraint failed for id=%s, endTime=%s, duration=%ld\n",
                    record.id.c_str(),
                    record.endTime.c_str(),
                    record.duration);
      sendJsonError(400, "record value constraints failed");
      return;
    }

    parsedRecords.push_back(record);
  }

  if (!bleMesh_->replaceFeedCache(serverTime, parsedRecords, weatherDataJson, weatherDataIsNull)) {
    Serial.println("[HTTP] feedData failed: replaceFeedCache returned false.");
    sendJsonError(500, "failed to update feed cache");
    return;
  }

  Serial.printf("[HTTP] feedData saved: records=%u, serverTime=%s\n",
                static_cast<unsigned>(parsedRecords.size()),
                serverTime.c_str());

  DynamicJsonDocument responseDoc(512);
  responseDoc["code"] = 200;
  responseDoc["message"] = "ok";
  responseDoc["savedRecords"] = static_cast<uint32_t>(parsedRecords.size());
  responseDoc["serverTime"] = serverTime;

  String body;
  serializeJson(responseDoc, body);
  webServer_.send(200, "application/json", body);
}

void WifiProvisioningModule::sendJsonError(int statusCode, const char* message) {
  DynamicJsonDocument errorDoc(256);
  errorDoc["code"] = statusCode;
  errorDoc["message"] = message;

  String body;
  serializeJson(errorDoc, body);
  webServer_.send(statusCode, "application/json", body);
}

void WifiProvisioningModule::handleNotFound() {
  const String uri = webServer_.uri();

  if (isCaptivePortalUri(uri)) {
    webServer_.sendHeader("Location", "http://" + WiFi.softAPIP().toString() + "/", true);
    webServer_.send(302, "text/plain", "");
    return;
  }

  if (!serveFileFromSd(uri)) {
    webServer_.send(404, "text/plain;charset=utf-8", "404 Not Found");
  }
}

bool WifiProvisioningModule::serveFileFromSd(const String& requestUri) {
  if (!sdMounted_ || !tfCard_.isMounted()) {
    return false;
  }

  if (!SD_MMC.exists(kHtmlRootPath)) {
    return false;
  }

  String normalizedPath = normalizeUriPath(requestUri);
  if (normalizedPath.isEmpty()) {
    return false;
  }

  const String fullPath = String(kHtmlRootPath) + normalizedPath;
  if (!SD_MMC.exists(fullPath.c_str())) {
    return false;
  }

  File file = SD_MMC.open(fullPath, FILE_READ);
  if (!file || file.isDirectory()) {
    return false;
  }

  webServer_.streamFile(file, contentTypeForPath(fullPath));
  file.close();
  return true;
}

String WifiProvisioningModule::normalizeUriPath(const String& requestUri) const {
  String path = requestUri;

  const int32_t queryPos = path.indexOf('?');
  if (queryPos >= 0) {
    path = path.substring(0, queryPos);
  }

  if (path.isEmpty()) {
    path = "/";
  }

  if (!path.startsWith("/")) {
    path = "/" + path;
  }

  if (path.indexOf("..") >= 0) {
    return "";
  }

  if (path.endsWith("/")) {
    path += "index.html";
  }

  if (path == "/") {
    path = "/index.html";
  }

  return path;
}

bool WifiProvisioningModule::isCaptivePortalUri(const String& requestUri) const {
  return requestUri == "/generate_204" || requestUri == "/hotspot-detect.html" ||
         requestUri == "/connecttest.txt" || requestUri == "/ncsi.txt" || requestUri == "/fwlink";
}

bool WifiProvisioningModule::isDateTimeFormatValid(const String& dateTime) {
  int year = 0;
  int month = 0;
  int day = 0;
  int hour = 0;
  int minute = 0;
  int second = 0;

  const int matched =
      sscanf(dateTime.c_str(), "%d-%d-%d %d:%d:%d", &year, &month, &day, &hour, &minute, &second);
  if (matched != 6) {
    return false;
  }

  if (year < 1970 || month < 1 || month > 12 || day < 1 || day > 31 || hour < 0 || hour > 23 ||
      minute < 0 || minute > 59 || second < 0 || second > 59) {
    return false;
  }

  return true;
}

String WifiProvisioningModule::contentTypeForPath(const String& filePath) const {
  if (filePath.endsWith(".html")) {
    return "text/html;charset=utf-8";
  }
  if (filePath.endsWith(".css")) {
    return "text/css";
  }
  if (filePath.endsWith(".js")) {
    return "application/javascript";
  }
  if (filePath.endsWith(".json")) {
    return "application/json";
  }
  if (filePath.endsWith(".png")) {
    return "image/png";
  }
  if (filePath.endsWith(".jpg") || filePath.endsWith(".jpeg")) {
    return "image/jpeg";
  }
  if (filePath.endsWith(".svg")) {
    return "image/svg+xml";
  }
  if (filePath.endsWith(".ico")) {
    return "image/x-icon";
  }
  return "text/plain";
}

String WifiProvisioningModule::makeApSsid() const {
  const uint16_t suffix = static_cast<uint16_t>(ESP.getEfuseMac() & 0xFFFF);
  char nameBuffer[32] = {0};
  snprintf(nameBuffer, sizeof(nameBuffer), "Cola-gateway-%04X", suffix);
  return String(nameBuffer);
}

bool WifiProvisioningModule::decryptPassword(const String& encryptedPassword,
                                             String* decryptedPassword) const {
  if (decryptedPassword == nullptr || encryptedPassword.isEmpty()) {
    return false;
  }

  size_t decodedLen = 0;
  int ret = mbedtls_base64_decode(nullptr, 0, &decodedLen,
                                  reinterpret_cast<const unsigned char*>(encryptedPassword.c_str()),
                                  encryptedPassword.length());
  if (ret != MBEDTLS_ERR_BASE64_BUFFER_TOO_SMALL && ret != 0) {
    return false;
  }

  if (decodedLen == 0) {
    return false;
  }

  std::unique_ptr<unsigned char[]> cipher(new unsigned char[decodedLen]);
  size_t actualDecodedLen = 0;
  ret = mbedtls_base64_decode(cipher.get(), decodedLen, &actualDecodedLen,
                              reinterpret_cast<const unsigned char*>(encryptedPassword.c_str()),
                              encryptedPassword.length());
  if (ret != 0 || actualDecodedLen == 0) {
    return false;
  }

  mbedtls_pk_context pk;
  mbedtls_entropy_context entropy;
  mbedtls_ctr_drbg_context ctrDrbg;

  mbedtls_pk_init(&pk);
  mbedtls_entropy_init(&entropy);
  mbedtls_ctr_drbg_init(&ctrDrbg);

  bool ok = false;

  do {
    const char* personal = "cola-wifi-rsa";
    ret = mbedtls_ctr_drbg_seed(&ctrDrbg, mbedtls_entropy_func, &entropy,
                                reinterpret_cast<const unsigned char*>(personal),
                                strlen(personal));
    if (ret != 0) {
      break;
    }

    ret = mbedtls_pk_parse_key(
        &pk, reinterpret_cast<const unsigned char*>(kRsaPrivateKeyPem), strlen(kRsaPrivateKeyPem) + 1,
        reinterpret_cast<const unsigned char*>(kRsaPrivateKeyPassword), strlen(kRsaPrivateKeyPassword));
    if (ret != 0 || !mbedtls_pk_can_do(&pk, MBEDTLS_PK_RSA)) {
      break;
    }

    mbedtls_rsa_context* rsa = mbedtls_pk_rsa(pk);

    auto tryDecryptWithMd = [&](mbedtls_md_type_t mdType) -> bool {
      mbedtls_rsa_set_padding(rsa, MBEDTLS_RSA_PKCS_V21, mdType);

      std::unique_ptr<unsigned char[]> output(new unsigned char[512]);
      size_t outputLen = 0;
      const int decryptRet = mbedtls_rsa_rsaes_oaep_decrypt(
          rsa, mbedtls_ctr_drbg_random, &ctrDrbg, MBEDTLS_RSA_PRIVATE, nullptr, 0, &outputLen,
          cipher.get(), output.get(), 512);
      if (decryptRet != 0 || outputLen == 0) {
        return false;
      }

      *decryptedPassword = String(reinterpret_cast<const char*>(output.get()), outputLen);
      return true;
    };

    ok = tryDecryptWithMd(MBEDTLS_MD_SHA256) || tryDecryptWithMd(MBEDTLS_MD_SHA1);
  } while (false);

  mbedtls_pk_free(&pk);
  mbedtls_ctr_drbg_free(&ctrDrbg);
  mbedtls_entropy_free(&entropy);
  return ok;
}

void WifiProvisioningModule::showProvisioningScreen() const {
  display_.showProvisioning(apSsid_, kApPassword);
}

void WifiProvisioningModule::showConnectingScreen(const String& ssid) const {
  display_.showConnecting(ssid);
}

void WifiProvisioningModule::showConnectResult(bool success, const String& message) const {
  display_.showConnectResult(success, message);
}
