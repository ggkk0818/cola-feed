#include "modules/network/NetworkModule.h"

#include <ArduinoJson.h>
#include <ESP.h>

NetworkModule::NetworkModule()
    : webServer_(80),
      webServerStarted_(false),
    discoveryUdpStarted_(false),
      lastWifiConnectedForDiscovery_(false),
      lastDiscoveryBroadcastMs_(0) {}

bool NetworkModule::httpGet(const String& url, String& responseBody, int& statusCode, uint32_t timeoutMs) {
  HTTPClient httpClient;
  httpClient.setTimeout(timeoutMs);

  if (!httpClient.begin(url)) {
    statusCode = -1;
    responseBody = "";
    return false;
  }

  statusCode = httpClient.GET();
  if (statusCode > 0) {
    responseBody = httpClient.getString();
  } else {
    responseBody = "";
  }

  httpClient.end();
  return statusCode > 0;
}

void NetworkModule::beginWebServer() {
  if (webServerStarted_) {
    return;
  }

  webServer_.begin();
  webServerStarted_ = true;
}

void NetworkModule::on(const String& uri, HTTPMethod method, WebServer::THandlerFunction handler) {
  webServer_.on(uri, method, handler);
}

void NetworkModule::onNotFound(WebServer::THandlerFunction handler) {
  webServer_.onNotFound(handler);
}

bool NetworkModule::hasArg(const String& argName) {
  return webServer_.hasArg(argName);
}

String NetworkModule::arg(const String& argName) {
  return webServer_.arg(argName);
}

void NetworkModule::send(int code, const String& contentType, const String& content) {
  webServer_.send(code, contentType, content);
}

void NetworkModule::handleClient() {
  if (!webServerStarted_) {
    return;
  }

  webServer_.handleClient();
}

bool NetworkModule::isWebServerStarted() const {
  return webServerStarted_;
}

void NetworkModule::handleDiscoveryBroadcast(bool wifiConnected, const String& localIp) {
  if (!wifiConnected) {
    lastWifiConnectedForDiscovery_ = false;
    lastDiscoveryBroadcastMs_ = 0;
    return;
  }

  const unsigned long nowMs = millis();
  const bool firstBroadcastAfterConnected = !lastWifiConnectedForDiscovery_;
  const bool intervalReached =
      (nowMs - lastDiscoveryBroadcastMs_) >= kDiscoveryBroadcastIntervalMs;
  if (!firstBroadcastAfterConnected && !intervalReached) {
    return;
  }

  JsonDocument doc;
  doc["type"] = "discover";
  doc["chip_id"] = String((uint32_t)ESP.getEfuseMac(), HEX);
  doc["device_name"] = "Cola-ePaper";
  doc["ip"] = localIp;

  String payload;
  serializeJson(doc, payload);

  if (!discoveryUdpStarted_) {
    discoveryUdpStarted_ = udp_.begin(kDiscoveryBroadcastPort) == 1;
  }
  if (!discoveryUdpStarted_) {
    return;
  }

  udp_.beginPacket(IPAddress(255, 255, 255, 255), kDiscoveryBroadcastPort);
  udp_.write(reinterpret_cast<const uint8_t*>(payload.c_str()), payload.length());
  udp_.endPacket();

  lastWifiConnectedForDiscovery_ = true;
  lastDiscoveryBroadcastMs_ = nowMs;
}
