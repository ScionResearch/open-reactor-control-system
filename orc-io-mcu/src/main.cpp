#include <Arduino.h>

uint32_t loopTargetTime = 0;
uint32_t loopCounter = 0;

void setup() {
  // Init serial port:
  Serial.begin(115200);
  while (!Serial) {
    delay(1);
  }
  Serial.println("Starting IO MCU (ATSAME51N20A)...");
  loopTargetTime = millis();
}

void loop() {
  if (millis() > loopTargetTime) {
    loopTargetTime += 1000;
    Serial.printf("Loop %d\n", loopCounter++);
  }
}
