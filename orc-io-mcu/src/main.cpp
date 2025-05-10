#include "sys_init.h"
#include <FlashStorage_SAMD.h>
#include "drivers/drv_do_sensor.h" // Include DO sensor driver
#include "drivers/drv_ph_sensor.h" // Include pH sensor driver

MCP48FEBxx dac(PIN_DAC_CS, PIN_DAC_SYNC, &SPI);
//TMC5130 stepper = TMC5130(PIN_STP_CS, &SPI1);

uint32_t loopTargetTime = 0;
uint32_t longLoopTargetTime = 0;
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

/*void printTMC5130registers(void) {
  // Print registers
  Serial.println("Reading TMC5130 registers");
  Serial.println("\nGeneral Configuration:");
  Serial.printf("GCONF:       0x%08X\n", stepper.reg.GCONF);
  Serial.printf("GSTAT:       0x%08X\n", stepper.reg.GSTAT);
  Serial.printf("IFCNT:       0x%08X\n", stepper.reg.IFCNT);
  Serial.printf("IOIN:        0x%08X\n", stepper.reg.IOIN);

  Serial.println("\nVelocity Dependent Driver Feature Control Register Set:");
  Serial.printf("TSTEP:       0x%08X\n", stepper.reg.TSTEP);

  Serial.println("\nRamp Generator Motion Control Register Set:");
  Serial.printf("RAMPMODE:    0x%08X\n", stepper.reg.RAMPMODE);
  Serial.printf("XACTUAL:     0x%08X\n", stepper.reg.XACTUAL);
  Serial.printf("VACTUAL:     0x%08X\n", stepper.reg.VACTUAL);
  Serial.printf("XTARGET:     0x%08X\n", stepper.reg.XTARGET);

  Serial.println("\nRamp Generator Driver Feature Control Register Set:");
  Serial.printf("SW_MODE:     0x%08X\n", stepper.reg.SW_MODE);
  Serial.printf("RAMP_STAT:   0x%08X\n", stepper.reg.RAMP_STAT);
  Serial.printf("XLATCH:      0x%08X\n", stepper.reg.XLATCH);

  Serial.println("\nEncoder Registers:");
  Serial.printf("ENCMODE:     0x%08X\n", stepper.reg.ENCMODE);
  Serial.printf("X_ENC:       0x%08X\n", stepper.reg.X_ENC);
  Serial.printf("ENC_STATUS:  0x%08X\n", stepper.reg.ENC_STATUS);
  Serial.printf("ENC_LATCH:   0x%08X\n", stepper.reg.ENC_LATCH);

  Serial.println("\nMotor Driver Registers:");
  Serial.printf("MSCNT:       0x%08X\n", stepper.reg.MSCNT);
  Serial.printf("MSCURACT:    0x%08X\n", stepper.reg.MSCURACT);
  Serial.printf("CHOPCONF:    0x%08X\n", stepper.reg.CHOPCONF);
  Serial.printf("DRV_STATUS:  0x%08X\n", stepper.reg.DRV_STATUS);
  Serial.printf("PWM_SCALE:   0x%08X\n", stepper.reg.PWM_SCALE);
  Serial.printf("LOST_STEPS:  0x%08X\n", stepper.reg.LOST_STEPS);
}*/

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
  if (!stepper_init()) {
    Serial.println("Failed to initialise TMC5130 driver.");
  }
  stepperDevice.maxRPM = 800;
  stepperDevice.stepsPerRev = 200;
  stepperDevice.inverted = false;
  stepperDevice.direction = 0;
  stepperDevice.acceleration = 100;
  stepperDevice.holdCurrent = 150;
  stepperDevice.runCurrent = 300;
  stepperDevice.enabled = true;
  stepperDevice.rpm = 120;

  if (!stepper_update(true)) {
    Serial.println("Failed to update TMC5130 parameters.");
    if (stepperDriver.newMessage) {
      Serial.println(stepperDriver.message);
    }
  }

  // --- Initialize Shared Modbus Master for RS485-1 ---
  Serial.println("Initialising Shared Modbus Master (RS485-1 on Serial1)");
  // Using Serial1 (Pins 31=TX, 32=RX), Baud 19200, No RTS Pin (-1)
  if (!modbus_init(&Serial1, 19200, -1)) {
      Serial.println("Failed to initialize shared Modbus master!");
      // Handle error appropriately - perhaps halt or set a system fault flag
  } else {
      Serial.println("Shared Modbus Master initialized.");

      // --- Initialize DO Sensor Driver ---
      Serial.println("Initialising DO Sensor Driver");
      if (!do_sensor_init(modbusMaster1, &Serial1, 19200, -1, 1000)) { // Pass shared master
          Serial.println("Failed to initialize DO sensor driver!");
          if (doSensorDriver.newMessage) Serial.println(doSensorDriver.message);
      } else {
          Serial.println("DO Sensor driver initialized.");
      }

      // --- Initialize pH Sensor Driver ---
      Serial.println("Initialising pH Sensor Driver");
      if (!ph_sensor_init(modbusMaster1, &Serial1, 19200, -1, 1000)) { // Pass shared master
          Serial.println("Failed to initialize pH sensor driver!");
          if (phSensorDriver.newMessage) Serial.println(phSensorDriver.message);
      } else {
          Serial.println("pH Sensor driver initialized.");
      }
  }

  Serial.println("Setup done");

  loopTargetTime = millis();
  longLoopTargetTime = millis() + 10000;
}

uint16_t irun = 100;
float rpm = 270;
bool rotating = false;
bool dir = 0;

void loop() {
  if (millis() > loopTargetTime) {
    loopTargetTime += 1000; // Run this block every 1 second

    // --- Update Modbus Sensors ---
    if (doSensorDriver.ready) {
        if (!do_sensor_update()) {
            // Log DO sensor update failure
            if (doSensorDevice.newMessage) {
                Serial.print("DO Sensor Update Error: ");
                Serial.println(doSensorDevice.message);
                doSensorDevice.newMessage = false; // Clear message flag
            }
        } else {
            // Optionally log successful DO readings
            // Serial.printf("DO: %.2f, Temp: %.2f\n", doSensorDevice.dissolvedOxygen, doSensorDevice.temperature);
        }
    }

    if (phSensorDriver.ready) {
        if (!ph_sensor_update()) {
            // Log pH sensor update failure
            if (phSensorDevice.newMessage) {
                Serial.print("pH Sensor Update Error: ");
                Serial.println(phSensorDevice.message);
                phSensorDevice.newMessage = false; // Clear message flag
            }
        } else {
            // Optionally log successful pH readings
            // Serial.printf("pH: %.2f, Temp: %.2f\n", phSensorDevice.pH, phSensorDevice.temperature);
        }
    }

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
    uint32_t drvStatus = 0;
    //stepper.readRegister(TMC5130_REG_DRV_STATUS, &drvStatus);
    uint32_t tStep = 0;
    /*stepper.readRegister(TMC5130_REG_TSTEP, &tStep);
    Serial.printf("Stepper DRV_STATUS: 0x%08X", drvStatus);
    Serial.printf(", SG_RESULT: %u", drvStatus & 0x3FF);
    Serial.printf(", TSTEP: 0x%08X, IRUN: %umA\n", tStep, stepper.config.irun);*/

    /*stepper.setIrun(irun);
    irun += 100;
    if (irun > 1000) irun = 100;*/
  } 

  if (millis() > longLoopTargetTime) {
    longLoopTargetTime += 10000; // Run this block every 10 seconds

    /*rpm -= 50;
    if (rpm < 50) rpm = 500;
    stepper.setRPM(rpm);
    Serial.printf("RPM: %f\n", rpm);*/
    /*if (rotating) {
      Serial.println("Stopping motor...");
      rotating = false;
      stepper.setRPM(0);
    } else {
      Serial.println("Running motor...");
      stepper.invertDirection(dir);
      dir = !dir;
      stepper.setRPM(rpm);
      stepper.run();
      rotating = true;
    }*/

    // Example: Print sensor values every 10 seconds
    if (doSensorDriver.ready && !doSensorDevice.fault) {
        Serial.printf("[10s] DO: %.2f %s, Temp: %.1f C\n",
                      doSensorDevice.dissolvedOxygen,
                      doSensorDevice.unit, // Assuming unit is set somewhere
                      doSensorDevice.temperature);
    }
    if (phSensorDriver.ready && !phSensorDevice.fault) {
        Serial.printf("[10s] pH: %.2f, Temp: %.1f C\n",
                      phSensorDevice.pH,
                      phSensorDevice.temperature);
    }

  }
}