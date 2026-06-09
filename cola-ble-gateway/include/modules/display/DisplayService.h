#pragma once

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

#include "modules/display/LcdModule.h"

class DisplayService {
 public:
  explicit DisplayService(LcdModule& lcd);

  bool begin();
  void processNext(TickType_t waitTicks);

  void showProvisioning(const String& apSsid, const char* password);
  void showConnecting(const String& ssid);
  void showConnectResult(bool success, const String& message);
  void showBleMeshStatus(uint16_t clientCount);

 private:
  enum class CommandType : uint8_t {
    kProvisioning,
    kConnecting,
    kConnectResult,
    kBleMeshStatus,
  };

  struct DisplayCommand {
    CommandType type = CommandType::kProvisioning;
    char primary[96] = {0};
    char secondary[96] = {0};
    bool success = false;
    uint16_t count = 0;
  };

  bool enqueue(const DisplayCommand& command);
  static void copyString(char* destination, size_t destinationSize, const String& value);
  static bool containsNonAscii(const char* text);
  void printMaybeUtf8(const char* text, int16_t x, int16_t y, uint16_t color);
  void render(const DisplayCommand& command);
  void renderProvisioning(const DisplayCommand& command);
  void renderConnecting(const DisplayCommand& command);
  void renderConnectResult(const DisplayCommand& command);
  void renderBleMeshStatus(const DisplayCommand& command);

  LcdModule& lcd_;
  QueueHandle_t queue_ = nullptr;
};
