#include "sys_init.h"

MCP48FEBxx dac(PIN_DAC_CS, PIN_DAC_SYNC, &SPI);

uint32_t loopTargetTime = 0;
uint32_t loopCounter = 0;

void setupDAC(void) {
  SPI.begin();
  pinMode(PIN_ADC_CS, OUTPUT);
  //pinMode(PIN_DAC_CS, OUTPUT);
  pinMode(PIN_DAC_SYNC, OUTPUT);
  pinMode(PIN_PT100_CS_1, OUTPUT);
  pinMode(PIN_PT100_CS_2, OUTPUT);
  pinMode(PIN_PT100_CS_3, OUTPUT);

  digitalWrite(PIN_ADC_CS, HIGH);
  //digitalWrite(PIN_DAC_CS, HIGH);
  digitalWrite(PIN_DAC_SYNC, LOW);
  digitalWrite(PIN_PT100_CS_1, HIGH);
  digitalWrite(PIN_PT100_CS_2, HIGH);
  digitalWrite(PIN_PT100_CS_3, HIGH);
  if (!dac.begin()) Serial.println("Failed to initialise DAC.");
  else Serial.println("DAC initialised.");
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
  // Init serial port:
  Serial.begin(115200);
  while (!Serial) {
    delay(1);
  }
  Serial.println("Starting IO MCU (ATSAME51N20A)...");
  Serial.println("Initialising RTD interface");
  //setupRtdInterface();

  Serial.println("Initialising DAC interface");
  setupDAC();

  Serial.println("Writing DAC channel 0 VREF to internal bandgap");
  if (!dac.setVREF(0, VREF_BANDGAP)) Serial.println("Failed to write DAC channel 0 VREF.");
  Serial.println("Writing DAC channel 1 VREF to internal bandgap");
  if (!dac.setVREF(1, VREF_BANDGAP)) Serial.println("Failed to write DAC channel 1 VREF.");
  uint16_t reg_value = 0;
  dac.readRegister(MCP48FEBxx_REG_VREF, &reg_value);
  Serial.printf("VREF reg value: 0x%04X\n", reg_value);

  Serial.println("Writing DAC channel 0 PD to Normal");
  if (!dac.setPD(0, PD_NORMAL)) Serial.println("Failed to write DAC channel 0 PD.");
  Serial.println("Writing DAC channel 1 PD to Normal");
  if (!dac.setPD(1, PD_NORMAL)) Serial.println("Failed to write DAC channel 1 PD.");
  dac.readRegister(MCP48FEBxx_REG_POWERDOWN, &reg_value);
  Serial.printf("PD reg value: 0x%04X\n", reg_value);

  Serial.println("Writing DAC channel 0 gain to 2x");
  if (!dac.setGain(0, GAIN_2X)) Serial.println("Failed to write DAC channel 0 gain.");
  Serial.println("Writing DAC channel 1 gain to 2x");
  if (!dac.setGain(1, GAIN_2X)) Serial.println("Failed to write DAC channel 1 gain.");
  dac.readRegister(MCP48FEBxx_REG_GAIN_STATUS, &reg_value);
  Serial.printf("GAIN/STATUS reg value: 0x%04X\n", reg_value);

  Serial.println("Writing DAC channel 0 value to 0x1FF");
  if (!dac.writeDAC(0, 0x1FF)) Serial.println("Failed to write DAC channel 0 value to 0x1FF");
  Serial.println("Writing DAC channel 1 value to 0x1FF");
  if (!dac.writeDAC(1, 0x1FF)) Serial.println("Failed to write DAC channel 1 value to 0x1FF");

  Serial.println("Saving register settings to DAC EEPROM");
  uint32_t start_time = millis();
  int write_result = dac.saveRegistersToEEPROM();
  uint32_t write_time = millis() - start_time;
  if (write_result < 0) Serial.printf("Failed to save register settings to DAC EEPROM after %d ms, error %i.\n", write_time, write_result);
  else Serial.printf("Register settings saved to DAC EEPROM in %d ms\n", write_time);
  Serial.println("Done");
  
  loopTargetTime = millis();
}

void loop() {
  if (millis() > loopTargetTime) {
    loopTargetTime += 1000;
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

  }
}