#include "modules/network/NetworkModule.h"

NetworkModule::NetworkModule()
    : webServer_(80), webServerStarted_(false) {}

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

void NetworkModule::handleClient() {
  if (!webServerStarted_) {
    return;
  }

  webServer_.handleClient();
}

bool NetworkModule::isWebServerStarted() const {
  return webServerStarted_;
}
