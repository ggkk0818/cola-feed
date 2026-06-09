#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <freertos/task.h>

#include "modules/bluetooth/BleMeshModule.h"
#include "modules/display/DisplayService.h"
#include "modules/display/LcdModule.h"
#include "modules/i2c/I2cModule.h"
#include "modules/led/RgbLedModule.h"
#include "modules/network/WifiProvisioningModule.h"
#include "modules/storage/TfCardModule.h"

namespace {
LcdModule lcd;
TfCardModule tfCard;
RgbLedModule led;
I2cModule i2c;
DisplayService display(lcd);
BleMeshModule bleMesh(display);
WifiProvisioningModule wifiProvisioning(display, tfCard);

EventGroupHandle_t systemEvents = nullptr;
constexpr EventBits_t kPeripheralReadyBit = BIT0;
constexpr uint32_t kCommTaskStackBytes = 12288;
constexpr uint32_t kPeripheralTaskStackBytes = 8192;
constexpr UBaseType_t kCommTaskPriority = 5;
constexpr UBaseType_t kPeripheralTaskPriority = 2;

void peripheralTask(void* /*parameter*/) {
  Serial.printf("[TASK] peripheralTask running on core %u\n", xPortGetCoreID());

  if (tfCard.begin()) {
    Serial.println("[BOOT] TF card initialized.");
  } else {
    Serial.println("[BOOT] TF card init failed.");
  }

  lcd.begin();
  if (!display.begin()) {
    Serial.println("[BOOT] Display queue init failed.");
  }
  lcd.showBootLogoFromTfCard(tfCard);
  vTaskDelay(pdMS_TO_TICKS(500));

  if (i2c.begin()) {
    Serial.println("[BOOT] I2C bus initialized.");
  } else {
    Serial.println("[BOOT] I2C bus init failed.");
  }

  led.begin();
  // led.setColor(0, 60, 0);
  // led.on();
  led.off();

  xEventGroupSetBits(systemEvents, kPeripheralReadyBit);

  for (;;) {
    display.processNext(pdMS_TO_TICKS(100));
  }
}

void commTask(void* /*parameter*/) {
  Serial.printf("[TASK] commTask waiting for peripherals on core %u\n", xPortGetCoreID());
  xEventGroupWaitBits(systemEvents, kPeripheralReadyBit, pdFALSE, pdTRUE, portMAX_DELAY);
  Serial.printf("[TASK] commTask starting communications on core %u\n", xPortGetCoreID());

  bleMesh.begin();
  wifiProvisioning.attachBleMeshModule(bleMesh);
  wifiProvisioning.setStateChangedCallback(
      [](WifiProvisioningModule::ConnectionState state) { bleMesh.handleWifiStateChange(state); });
  wifiProvisioning.begin();

  for (;;) {
    wifiProvisioning.loop();
    bleMesh.loop();
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}
}  // namespace

void setup() {
  Serial.begin(115200);
  delay(300);

  systemEvents = xEventGroupCreate();
  if (systemEvents == nullptr) {
    Serial.println("[BOOT] Failed to create system event group.");
    return;
  }

  const BaseType_t peripheralTaskCreated =
      xTaskCreatePinnedToCore(peripheralTask,
                              "peripheralTask",
                              kPeripheralTaskStackBytes,
                              nullptr,
                              kPeripheralTaskPriority,
                              nullptr,
                              1);
  if (peripheralTaskCreated != pdPASS) {
    Serial.println("[BOOT] Failed to create peripheralTask.");
  }

  const BaseType_t commTaskCreated = xTaskCreatePinnedToCore(
      commTask, "commTask", kCommTaskStackBytes, nullptr, kCommTaskPriority, nullptr, 0);
  if (commTaskCreated != pdPASS) {
    Serial.println("[BOOT] Failed to create commTask.");
  }
}

void loop() {
  vTaskDelay(pdMS_TO_TICKS(1000));
}
