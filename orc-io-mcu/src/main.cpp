#include "sys_init.h"
#include <FlashStorage_SAMD.h>

// Most of this is debug code!!!! Very much a work in progress

uint32_t loopTargetTime = 0;
uint32_t longLoopTargetTime = 0;
uint32_t loopCounter = 0;

void printStuff(void) {
  Serial.printf("Pin states (GPIO): %d  %d  %d  %d  %d  %d  %d  %d\n", gpio[0].state, gpio[1].state, gpio[2].state, gpio[3].state, gpio[4].state, gpio[5].state, gpio[6].state, gpio[7].state);
  Serial.printf("Pin states (Exp) : %d  %d  %d  %d  %d  %d  %d  %d  %d  %d  %d  %d  %d  %d  %d\n", gpioExp[0].state, gpioExp[1].state, gpioExp[2].state, gpioExp[3].state, gpioExp[4].state, gpioExp[5].state, gpioExp[6].state, gpioExp[7].state, gpioExp[8].state, gpioExp[9].state, gpioExp[10].state, gpioExp[11].state, gpioExp[12].state, gpioExp[13].state, gpioExp[14].state);
}

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
  Serial.println("Setup done");

  // DRV8235 initialisation
  Serial.println("Initialising DRV8235 driver");
  if (!motor_init()) {
    Serial.println("Failed to initialise DRV8235 driver.");
  } else Serial.println("DRV8235 driver initialised.");

  motorDriver[0].device->enabled = true;
  
  motor_run(0, 20, false);

  // INA260 initialisation
  Serial.println("Initialising INA260 driver");
  if (!pwrSensor_init()) {
    Serial.println("Failed to initialise INA260 driver.");
  }

  // Outputs initialisation
  Serial.println("Initialising outputs");
  output_init();

  Serial.println("Setting PWM values");
  outputDriver.outputObj[0]->pwmEnabled = true;
  outputDriver.outputObj[0]->pwmDuty = 50;
  outputDriver.outputObj[0]->state = false;
  outputDriver.outputObj[1]->pwmEnabled = true;
  outputDriver.outputObj[1]->pwmDuty = 75;
  outputDriver.outputObj[1]->state = false;
  outputDriver.outputObj[2]->pwmEnabled = true;
  outputDriver.outputObj[2]->pwmDuty = 25;
  outputDriver.outputObj[2]->state = false;
  outputDriver.outputObj[3]->pwmEnabled = true;
  outputDriver.outputObj[3]->pwmDuty = 0;
  outputDriver.outputObj[3]->state = false;

  Serial.println("Setting heater output");
  heaterOutput[0].pwmEnabled = true;
  heaterOutput[0].pwmDuty = 20;

  Serial.println("Initialising GPIO pins");
  gpio_init();

  Serial.println("Adding tasks to scheduler");
  tasks.addTask(output_update, 100, true, false);
  tasks.addTask(gpio_update, 100, true, true);
  tasks.addTask(printStuff, 2000, true, false);

  Serial.println("Setup done");
  
  loopTargetTime = millis();
  longLoopTargetTime = millis() + 5000;
}

void loop() {
  tasks.update();
}