#include <Arduino.h>

unsigned long msgCount = 0;

void setup() {
  Serial.begin(115200);
  unsigned long start = millis();
  while (!Serial && (millis() - start) < 5000) {
    delay(10);
  }
  Serial.println("ESP32-S3 serial test start");
  Serial.println("If you cannot see this, check monitor baud = 115200 and press RST.");
}

void loop() {
  Serial.print("Test message #");
  Serial.print(msgCount++);
  Serial.print(" | Uptime(ms): ");
  Serial.println(millis());
  delay(1000);
}