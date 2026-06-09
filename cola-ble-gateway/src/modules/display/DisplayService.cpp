#include "modules/display/DisplayService.h"

#include <Adafruit_ST77xx.h>
#include <cstring>

namespace {
constexpr UBaseType_t kDisplayQueueLength = 8;
}

DisplayService::DisplayService(LcdModule& lcd) : lcd_(lcd) {}

bool DisplayService::begin() {
  if (queue_ != nullptr) {
    return true;
  }

  queue_ = xQueueCreate(kDisplayQueueLength, sizeof(DisplayCommand));
  return queue_ != nullptr;
}

void DisplayService::processNext(TickType_t waitTicks) {
  if (queue_ == nullptr) {
    vTaskDelay(waitTicks);
    return;
  }

  DisplayCommand command;
  if (xQueueReceive(queue_, &command, waitTicks) == pdTRUE) {
    render(command);
  }
}

void DisplayService::showProvisioning(const String& apSsid, const char* password) {
  DisplayCommand command;
  command.type = CommandType::kProvisioning;
  copyString(command.primary, sizeof(command.primary), apSsid);
  copyString(command.secondary, sizeof(command.secondary), String(password == nullptr ? "" : password));
  enqueue(command);
}

void DisplayService::showConnecting(const String& ssid) {
  DisplayCommand command;
  command.type = CommandType::kConnecting;
  copyString(command.primary, sizeof(command.primary), ssid);
  enqueue(command);
}

void DisplayService::showConnectResult(bool success, const String& message) {
  DisplayCommand command;
  command.type = CommandType::kConnectResult;
  command.success = success;
  copyString(command.primary, sizeof(command.primary), message);
  enqueue(command);
}

void DisplayService::showBleMeshStatus(uint16_t clientCount) {
  DisplayCommand command;
  command.type = CommandType::kBleMeshStatus;
  command.count = clientCount;
  enqueue(command);
}

bool DisplayService::enqueue(const DisplayCommand& command) {
  if (queue_ == nullptr) {
    return false;
  }

  if (xQueueSendToBack(queue_, &command, 0) == pdTRUE) {
    return true;
  }

  DisplayCommand dropped;
  xQueueReceive(queue_, &dropped, 0);
  return xQueueSendToBack(queue_, &command, 0) == pdTRUE;
}

void DisplayService::copyString(char* destination, size_t destinationSize, const String& value) {
  if (destination == nullptr || destinationSize == 0) {
    return;
  }

  const size_t bytesToCopy = value.length() >= destinationSize ? destinationSize - 1 : value.length();
  memcpy(destination, value.c_str(), bytesToCopy);
  destination[bytesToCopy] = '\0';
}

bool DisplayService::containsNonAscii(const char* text) {
  if (text == nullptr) {
    return false;
  }

  for (size_t i = 0; text[i] != '\0'; ++i) {
    if (static_cast<uint8_t>(text[i]) & 0x80) {
      return true;
    }
  }
  return false;
}

void DisplayService::printMaybeUtf8(const char* text, int16_t x, int16_t y, uint16_t color) {
  if (containsNonAscii(text)) {
    lcd_.printUtf8(String(text), x, y, color);
    return;
  }

  lcd_.printText(String(text), x, y, color, 1);
}

void DisplayService::render(const DisplayCommand& command) {
  switch (command.type) {
    case CommandType::kProvisioning:
      renderProvisioning(command);
      break;
    case CommandType::kConnecting:
      renderConnecting(command);
      break;
    case CommandType::kConnectResult:
      renderConnectResult(command);
      break;
    case CommandType::kBleMeshStatus:
      renderBleMeshStatus(command);
      break;
  }
}

void DisplayService::renderProvisioning(const DisplayCommand& command) {
  lcd_.clearScreen(ST77XX_BLACK);
  lcd_.printText("Provisioning", 0, 18, ST77XX_YELLOW, 1);
  lcd_.printText("WiFi:", 0, 38, ST77XX_WHITE, 1);
  printMaybeUtf8(command.primary, 36, 38, ST77XX_WHITE);
  lcd_.printText("PWD:", 0, 54, ST77XX_WHITE, 1);
  lcd_.printText(String(command.secondary), 36, 54, ST77XX_WHITE, 1);
}

void DisplayService::renderConnecting(const DisplayCommand& command) {
  lcd_.clearScreen(ST77XX_BLACK);
  lcd_.printText("Connecting...", 0, 20, ST77XX_WHITE, 1);
  printMaybeUtf8(command.primary, 0, 44, ST77XX_WHITE);
}

void DisplayService::renderConnectResult(const DisplayCommand& command) {
  lcd_.clearScreen(ST77XX_BLACK);
  lcd_.printText(command.success ? "Connected" : "Connect failed",
                 0,
                 20,
                 command.success ? ST77XX_GREEN : ST77XX_RED,
                 1);
  printMaybeUtf8(command.primary, 0, 44, ST77XX_WHITE);
}

void DisplayService::renderBleMeshStatus(const DisplayCommand& command) {
  lcd_.clearScreen(ST77XX_BLACK);
  lcd_.printText("BLE mesh", 0, 18, ST77XX_CYAN, 1);
  lcd_.printText("Clients:", 0, 42, ST77XX_WHITE, 1);
  lcd_.printText(String(command.count), 60, 42, ST77XX_GREEN, 1);
}
