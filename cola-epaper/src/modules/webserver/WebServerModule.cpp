#include "modules/webserver/WebServerModule.h"

#include <ArduinoJson.h>

WebServerModule::WebServerModule()
    : started_(false),
      networkModule_(nullptr),
      feedController_(nullptr),
      drawingModule_(nullptr),
      wifiModule_(nullptr) {}

void WebServerModule::begin(NetworkModule& networkModule, FeedController& feedController,
                            DrawingModule& drawingModule, WifiModule& wifiModule) {
  if (started_) {
    return;
  }

  networkModule_ = &networkModule;
  feedController_ = &feedController;
  drawingModule_ = &drawingModule;
  wifiModule_ = &wifiModule;

  registerRoutes(networkModule);
  networkModule.beginWebServer();
  started_ = true;
}

bool WebServerModule::isStarted() const {
  return started_;
}

void WebServerModule::registerRoutes(NetworkModule& networkModule) {
  networkModule.on("/", HTTP_GET, [this]() { handleRootPage(); });
  networkModule.on("/api/feedData", HTTP_PUT, [this]() { handleFeedDataPut(); });
  networkModule.onNotFound([this]() { sendJsonError(404, "resource not found"); });
}

void WebServerModule::handleRootPage() {
  if (!networkModule_) {
    return;
  }

  networkModule_->send(200, "text/html; charset=utf-8", buildConfigPlaceholderHtml());
}

void WebServerModule::handleFeedDataPut() {
  if (!networkModule_ || !feedController_ || !drawingModule_ || !wifiModule_) {
    return;
  }

  if (!networkModule_->hasArg("plain")) {
    sendJsonError(400, "missing request body");
    return;
  }

  const String requestBody = networkModule_->arg("plain");
  if (requestBody.isEmpty()) {
    sendJsonError(400, "empty request body");
    return;
  }

  DynamicJsonDocument requestDoc(1024U + requestBody.length());
  const DeserializationError parseError = deserializeJson(requestDoc, requestBody);
  if (parseError) {
    sendJsonError(400, "invalid json body");
    return;
  }

  JsonVariant serverTimeVar = requestDoc["serverTime"];
  JsonVariant recordsVar = requestDoc["records"];
  if (!serverTimeVar.is<const char*>() || !recordsVar.is<JsonArray>()) {
    sendJsonError(400, "serverTime must be string and records must be array");
    return;
  }

  const String serverTime = String(serverTimeVar.as<const char*>());
  if (!isDateTimeFormatValid(serverTime)) {
    sendJsonError(400, "invalid serverTime format");
    return;
  }

  JsonArray records = recordsVar.as<JsonArray>();

  std::vector<FeedRecord> parsedRecords;
  parsedRecords.reserve(records.size());

  for (JsonObject recordObj : records) {
    JsonVariant idVar = recordObj["id"];
    JsonVariant startTimeVar = recordObj["startTime"];
    JsonVariant endTimeVar = recordObj["endTime"];
    JsonVariant durationVar = recordObj["duration"];

    if (!idVar.is<const char*>() || !startTimeVar.is<const char*>() ||
        !endTimeVar.is<const char*>() || !durationVar.is<long>()) {
      sendJsonError(400, "record fields are invalid");
      return;
    }

    FeedRecord record;
    record.id = String(idVar.as<const char*>());
    record.startTime = String(startTimeVar.as<const char*>());
    record.endTime = String(endTimeVar.as<const char*>());
    record.duration = durationVar.as<long>();

    if (record.id.length() != 32 || record.endTime.isEmpty() || record.duration < 0) {
      sendJsonError(400, "record value constraints failed");
      return;
    }

    parsedRecords.push_back(record);
  }

  feedController_->clearFeedData();
  for (size_t index = 0; index < parsedRecords.size(); ++index) {
    if (!feedController_->pushFeedData(parsedRecords[index])) {
      sendJsonError(500, "failed to store feed record");
      return;
    }
  }

  if (!feedController_->setServerTime(serverTime)) {
    sendJsonError(500, "failed to update server time");
    return;
  }

  feedController_->requestRenderNow();
  const bool wifiConnected = wifiModule_->isConnected();
  const String localIp = wifiConnected ? wifiModule_->localIp() : String("");
  feedController_->renderFeedScreenIfNeeded(*drawingModule_, wifiConnected, localIp);

  DynamicJsonDocument responseDoc(256);
  responseDoc["ok"] = true;
  responseDoc["savedRecords"] = parsedRecords.size();
  responseDoc["serverTime"] = serverTime;

  String responseBody;
  serializeJson(responseDoc, responseBody);
  networkModule_->send(200, "application/json", responseBody);
}

void WebServerModule::sendJsonError(int statusCode, const char* message) {
  if (!networkModule_) {
    return;
  }

  DynamicJsonDocument errorDoc(192);
  errorDoc["ok"] = false;
  errorDoc["error"] = message;

  String responseBody;
  serializeJson(errorDoc, responseBody);
  networkModule_->send(statusCode, "application/json", responseBody);
}

bool WebServerModule::isDateTimeFormatValid(const String& dateTime) {
  int year = 0;
  int month = 0;
  int day = 0;
  int hour = 0;
  int minute = 0;
  int second = 0;

  const int matched = sscanf(dateTime.c_str(), "%d-%d-%d %d:%d:%d", &year, &month, &day, &hour,
                             &minute, &second);
  if (matched != 6) {
    return false;
  }

  if (year < 1970 || month < 1 || month > 12 || day < 1 || day > 31 || hour < 0 || hour > 23 ||
      minute < 0 || minute > 59 || second < 0 || second > 59) {
    return false;
  }

  return true;
}

String WebServerModule::buildConfigPlaceholderHtml() const {
  String html;
  html.reserve(512);
  html += "<!DOCTYPE html><html lang='zh-CN'><head><meta charset='utf-8'>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
  html += "<title>Cola Feed Device</title><style>body{font-family:Arial,sans-serif;";
  html += "margin:0;padding:32px;background:#f4f7fb;color:#1f2a37;}";
  html += ".card{max-width:640px;margin:0 auto;background:#fff;padding:24px;";
  html += "border-radius:12px;box-shadow:0 8px 24px rgba(15,23,42,.08);}h1{margin:0 0 12px;}";
  html += "p{line-height:1.6;margin:0;}</style></head><body><div class='card'>";
  html += "<h1>Cola Feed 配网页面</h1>";
  html += "<p>设备 Web 服务已启动。该页面用于后续开发 WiFi 配网与设备管理功能，";
  html += "当前版本仅提供状态占位展示。</p></div></body></html>";
  return html;
}
