#include "sys_init.h"
#include <FlashStorage_SAMD.h>

MCP48FEBxx dac(PIN_DAC_CS, PIN_DAC_SYNC, &SPI);
TMC5130 stepper = TMC5130(PIN_STP_CS, &SPI1);

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

void printTMC5130registers(void) {
  uint32_t data = 0;

  // Print registers
  Serial.println("Reading TMC5130 registers");
  Serial.println("\nGeneral Configuration:");
  Serial.printf("Status: 0x%02X, GCONF:       0x%08X\n", stepper.readRegister(TMC5130_REG_GCONF, &data), data);
  Serial.printf("Status: 0x%02X, GSTAT:       0x%08X\n", stepper.readRegister(TMC5130_REG_GSTAT, &data), data);
  Serial.printf("Status: 0x%02X, IFCNT:       0x%08X\n", stepper.readRegister(TMC5130_REG_IFCNT, &data), data);
  Serial.printf("Status: 0x%02X, IOIN:        0x%08X\n", stepper.readRegister(TMC5130_REG_IOIN, &data), data);

  Serial.println("\nVelocity Dependent Driver Feature Control Register Set:");
  Serial.printf("Status: 0x%02X, TSTEP:       0x%08X\n", stepper.readRegister(TMC5130_REG_TSTEP, &data), data);

  Serial.println("\nRamp Generator Motion Control Register Set:");
  Serial.printf("Status: 0x%02X, RAMPMODE:    0x%08X\n", stepper.readRegister(TMC5130_REG_RAMPMODE, &data), data);
  Serial.printf("Status: 0x%02X, XACTUAL:     0x%08X\n", stepper.readRegister(TMC5130_REG_XACTUAL, &data), data);
  Serial.printf("Status: 0x%02X, VACTUAL:     0x%08X\n", stepper.readRegister(TMC5130_REG_VACTUAL, &data), data);
  Serial.printf("Status: 0x%02X, XTARGET:     0x%08X\n", stepper.readRegister(TMC5130_REG_XTARGET, &data), data);

  Serial.println("\nRamp Generator Driver Feature Control Register Set:");
  Serial.printf("Status: 0x%02X, SW_MODE:     0x%08X\n", stepper.readRegister(TMC5130_REG_SW_MODE, &data), data);
  Serial.printf("Status: 0x%02X, RAMP_STAT:   0x%08X\n", stepper.readRegister(TMC5130_REG_RAMP_STAT, &data), data);
  Serial.printf("Status: 0x%02X, XLATCH:      0x%08X\n", stepper.readRegister(TMC5130_REG_XLATCH, &data), data);

  Serial.println("\nEncoder Registers:");
  Serial.printf("Status: 0x%02X, ENCMODE:     0x%08X\n", stepper.readRegister(TMC5130_REG_ENCMODE, &data), data);
  Serial.printf("Status: 0x%02X, X_ENC:       0x%08X\n", stepper.readRegister(TMC5130_REG_X_ENC, &data), data);
  Serial.printf("Status: 0x%02X, ENC_STATUS:  0x%08X\n", stepper.readRegister(TMC5130_REG_ENC_STATUS, &data), data);
  Serial.printf("Status: 0x%02X, ENC_LATCH:   0x%08X\n", stepper.readRegister(TMC5130_REG_ENC_LATCH, &data), data);

  Serial.println("\nMotor Driver Registers:");
  Serial.printf("Status: 0x%02X, MSCNT:       0x%08X\n", stepper.readRegister(TMC5130_REG_MSCNT, &data), data);
  Serial.printf("Status: 0x%02X, MSCURACT:    0x%08X\n", stepper.readRegister(TMC5130_REG_MSCURACT, &data), data);
  Serial.printf("Status: 0x%02X, CHOPCONF:    0x%08X\n", stepper.readRegister(TMC5130_REG_CHOPCONF, &data), data);;
  Serial.printf("Status: 0x%02X, DRV_STATUS:  0x%08X\n", stepper.readRegister(TMC5130_REG_DRV_STATUS, &data), data);
  Serial.printf("Status: 0x%02X, PWM_SCALE:   0x%08X\n", stepper.readRegister(TMC5130_REG_PWM_SCALE, &data), data);
  Serial.printf("Status: 0x%02X, LOST_STEPS:  0x%08X\n", stepper.readRegister(TMC5130_REG_LOST_STEPS, &data), data);
}

/*void runStepper(void) {
  stepper.writeRegister
}*/

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

  Serial.println("Initialising ADC interface");
  if (!ADC_init()) {
    Serial.println("Failed to initialise ADC driver.");
  } else {
    Serial.println("ADC driver initialised.");
  }

  // PT100 setup testing
  Serial.println("Initialising RTD interface");
  setupRtdInterface();

  // DAC setup testing
  Serial.println("Initialising DAC interface");
  if (!DAC_init()) {
    Serial.println("Failed to initialise DAC driver.");
  } else {
    Serial.println("DAC driver initialised.");
  }

  // ADC setup testing
  Serial.println("Changing ADC inputs 1 & 2 to V");
  strcpy(adcInput[0].unit, "V");
  strcpy(adcInput[1].unit, "V");

  Serial.println("Changing ADC inputs 3 & 4 to mA");
  strcpy(adcInput[2].unit, "mA");
  strcpy(adcInput[3].unit, "mA");

  Serial.println("Changing ADC inputs 5 & 6 to µV");
  strcpy(adcInput[4].unit, "µV");
  strcpy(adcInput[5].unit, "µV");

  Serial.println("Changing ADC inputs 7 to xx");    // Invalid, should default to mV
  strcpy(adcInput[6].unit, "xx");

  // TMC5130 stepper setup testing
  Serial.println("Initialising TMC5130 driver");
  if (!stepper.begin()) {
    Serial.println("Failed to initialise TMC5130 driver.");
  } else {
    Serial.println("TMC5130 driver initialised.");
  }
  printTMC5130registers();

  // TMC5160 write test
  Serial.println("\nSetting I hold and I run to 16:");
  stepper.writeRegister(TMC5130_REG_XACTUAL, 976);
  uint32_t data = 0;
  Serial.printf("Status: 0x%02X, XACTUAL:  0x%08X\n", stepper.readRegister(TMC5130_REG_XACTUAL, &data), data);
  
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

    /*if (!ADC_readInputs()) {
      Serial.printf("Failed to read ADC with error: %s\n", adcDriver.message);
      adcDriver.newMessage = false;
    } else {
      Serial.println("ADC inputs read.");
    }

    dacOutput[0].value += 1000;
    if (dacOutput[0].value > 10000) dacOutput[0].value = 0;
    dacOutput[1].value -=1000;
    if (dacOutput[1].value < 0) dacOutput[1].value = 10000;

    if (!DAC_writeOutputs()) {
      Serial.println("Failed to write DAC outputs.");
    } else {
      Serial.printf("DAC output 0: %dmV, DAC output 1: %dmV\n", (int)dacOutput[0].value, (int)dacOutput[1].value);
    }*/
  }
}