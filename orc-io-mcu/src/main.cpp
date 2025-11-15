#include "sys_init.h"

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

void setup() {
  uint32_t startTime = millis();
  asm(".global _printf_float");
  // Init serial port:
  Serial.begin(115200);
  setupCSpins();    // Initialise CS pins so that devices can be initialised
  
  uint32_t serialWaitTime = 5000;
  while (!Serial) {
    delay(1);
    if (millis() - startTime > serialWaitTime) break;
  }

  Serial.println("Starting IO MCU (ATSAME51N20A)...");

  Serial.print("Initialising ADC interface... ");
  ADC_init();
  Serial.printf("%s\n", adcDriver.message);

  // PT100 setup testing
  Serial.print("Initialising RTD interface... ");
  if (!init_rtdDriver()) {
    Serial.println("Failed to initialise RTD driver.");
  } else {
    Serial.println("RTD driver initialised.");
  }

  // DAC setup testing
  Serial.print("Initialising DAC interface... ");
  if (!DAC_init()) {
    Serial.print("Failed to initialise DAC driver: ");
    Serial.printf("%s, Result Ch1: %s, Result Ch2: %s\n", dacDriver.message, dacDriver.outputObj[0]->message, dacDriver.outputObj[1]->message);
  } else {
    Serial.println("DAC driver initialised.");
  }

  // TMC5130 stepper setup testing
  Serial.print("Initialising TMC5130 stepper driver... ");
  if (!stepper_init()) {
    Serial.print("Failed to initialise TMC5130 driver: ");
    if (stepperDevice.fault) Serial.printf("Fault: %s\n", stepperDevice.message);
  } else {
    Serial.println("TMC5130 stepper driver initialised.");
  }

  // DRV8235 motor driver initialisation
  Serial.print("Initialising DRV8235 motor drivers... ");
  if (!motor_init()) {
    Serial.print("Failed to initialise DRV8235 motor drivers: ");
    for (int i = 0; i < 4; i++) {
      if (motorDevice[i].fault) Serial.printf("Motor %d fault: %s\n", i+1, motorDevice[i].message);
    }
  } else {
    Serial.println("DRV8235 motor drivers initialised.");
  }

  // INA260 power sensor initialisation
  Serial.print("Initialising INA260 power sensors... ");
  if (!pwrSensor_init()) {
    Serial.print("Failed to initialise INA260 power sensors: ");
    for (int i = 0; i < 2; i++) {
      if (pwr_energy[i].fault) Serial.printf("Power sensor %d fault: %s\n", i+1, pwr_energy[i].message);
    }
  } else {
    Serial.println("INA260 power sensors initialised.");
  }

  // Outputs initialisation
  Serial.print("Initialising outputs... ");
  output_init();
  Serial.println("Outputs initialised.");

  Serial.println("Setting output initial states");
  for (int i = 0; i < 4; i++) {
    outputDriver.outputObj[i]->pwmEnabled = false;
    outputDriver.outputObj[i]->pwmDuty = 0;
    outputDriver.outputObj[i]->state = false;
  }

  Serial.println("Setting heater output initial state");
  heaterOutput[0].pwmEnabled = false;
  heaterOutput[0].pwmDuty = 0;
  heaterOutput[0].state = false;

  Serial.print("Initialising GPIO pins... ");
  gpio_init();
  Serial.println("GPIO pins initialised.");

  Serial.print("Starting Modbus interface... ");
  if (!modbus_init()) {
    Serial.print("Failed to initialise Modbus driver: ");
    for (int i = 0; i < 4; i++) {
      if (modbusPort[i].fault) Serial.printf("Port %i Fault: %s\n", i+1, modbusPort[i].message);
    }
  } else {
    Serial.println("Modbus interface started.");
  }

  Serial.print("Starting IPC interface... ");
  if (!ipc_init()) {
    Serial.println("Failed to initialise IPC driver.");
  } else {
    Serial.println("IPC driver initialised at 2 Mbps.");
  }

  Serial.print("Initialising Device Manager... ");
  if (!DeviceManager::init()) {
    Serial.println("Failed to initialise Device Manager.");
  } else {
    Serial.println("Device Manager initialised");
  }

  Serial.print("Initialising Controller Manager... ");
  if (!ControllerManager::init()) {
    Serial.println("Failed to initialise Controller Manager.");
  } else {
    Serial.println("Controller Manager initialised");
  }

  Serial.print("Adding tasks to scheduler... ");
  analog_input_task = tasks.addTask(ADC_update, 10, true, false);
  analog_output_task = tasks.addTask(DAC_update, 100, true, false);
  output_task = tasks.addTask(output_update, 100, true, false);
  gpio_task = tasks.addTask(gpio_update, 100, true, true);
  modbus_task = tasks.addTask(modbus_manage, 10, true, true);
  ipc_task = tasks.addTask(ipc_update, 5, true, true);
  RTDsensor_task = tasks.addTask(RTD_manage, 200, true, false);
  stepper_task = tasks.addTask(stepper_update, 1000, true, false);
  motor_task = tasks.addTask(motor_update, 10, true, false);
  pwrSensor_task = tasks.addTask(pwrSensor_update, 1000, true, false);
  
  Serial.println("Setup done, waiting for System MCU to initialise...");
}

void loop() {
  tasks.update();
}