#include "sys_init.h"
#include <FlashStorage_SAMD.h>

MCP48FEBxx dac(PIN_DAC_CS, PIN_DAC_SYNC, &SPI);

uint32_t loopTargetTime = 0;
uint32_t loopCounter = 0;

void setupCSpins(void) {
  pinMode(PIN_ADC_CS, OUTPUT);
  pinMode(PIN_DAC_CS, OUTPUT);
  pinMode(PIN_PT100_CS_1, OUTPUT);
  pinMode(PIN_PT100_CS_2, OUTPUT);
  pinMode(PIN_PT100_CS_3, OUTPUT);

  digitalWrite(PIN_ADC_CS, HIGH);
  digitalWrite(PIN_DAC_CS, HIGH);
  digitalWrite(PIN_PT100_CS_1, HIGH);
  digitalWrite(PIN_PT100_CS_2, HIGH);
  digitalWrite(PIN_PT100_CS_3, HIGH);
}

void setupRtdInterface(void) {
  if (!init_rtdDriver()) {
    Serial.println("Failed to initialise RTD driver.");
  } else {
    Serial.println("RTD driver initialised.");
  }

  setRtdSensorType(&rtd_interface[1], PT1000);
  setRtdWires(&rtd_interface[1], MAX31865_2WIRE);
  Serial.println("Changed sensor 2 to PT1000, 2 wire.");
  setRtdSensorType(&rtd_interface[2], PT100);
  setRtdWires(&rtd_interface[2], MAX31865_4WIRE);
  Serial.println("Changed sensor 2 to PT100, 4 wire.");
}

void setup() {
  asm(".global _printf_float");
  // Init serial port:
  Serial.begin(115200);
  setupCSpins();    // Initialise CS pins so that devices can be initialised
  while (!Serial) {
    delay(1);
  }
  Serial.println("Starting IO MCU (ATSAME51N20A)...");
  Serial.println("Reading calibration data");
  if (!calibrate_init()) Serial.println("EEPROM was not initialised, defaults loaded");
  else Serial.println("Calibration data read from EEPROM");
  Serial.println("Initialising RTD interface");
  //setupRtdInterface();

  Serial.println("Initialising DAC interface");
  if (!DAC_init()) {
    Serial.println("Failed to initialise DAC driver.");
  } else {
    Serial.println("DAC driver initialised.");
  }
  
  loopTargetTime = millis();
}

void loop() {
  if (millis() > loopTargetTime) {
    loopTargetTime += 5000;
    /*if (!readRtdSensors()) {
      Serial.println("Failed to read RTD sensors.");
    } else {
      for (int i = 0; i < NUM_MAX31865_INTERFACES; i++) {
        if (rtd_sensor[i].newMessage) {
          Serial.print("RTD ");
          Serial.print(i + 1);
          Serial.print(" message: ");
          Serial.println(rtd_sensor[i].message);
          rtd_sensor[i].newMessage = false;
        } else {
          Serial.print("RTD ");
          Serial.print(i + 1);
          Serial.print(" temperature: ");
          Serial.print(rtd_sensor[i].temperature);
          Serial.println(" °C");
        }
      }
    }*/
    dacOutput[0].value += 1000;
    if (dacOutput[0].value > 10000) dacOutput[0].value = 0;
    dacOutput[1].value -=1000;
    if (dacOutput[1].value < 0) dacOutput[1].value = 10000;

    if (!DAC_writeOutputs()) {
      Serial.println("Failed to write DAC outputs.");
    } else {
      Serial.printf("DAC output 0: %dmV, DAC output 1: %dmV\n", (int)dacOutput[0].value, (int)dacOutput[1].value);
    }
  }
}