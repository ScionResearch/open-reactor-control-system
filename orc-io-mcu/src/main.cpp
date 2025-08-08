
#include "sys_init.h"
#include <FlashStorage_SAMD.h>
#include <limits.h>
#include "../lib/IPCprotocol/IPCProtocol.h"
#include "../lib/IPCprotocol/IPCDataStructs.h"

// Most of this is debug code!!!! Very much a work in progress

void printStuff(void) {
  char msg[128];
  
  if (output_task) {
    sprintf(msg, "Output task µs last: %lu, min: %lu, max: %lu, avg: %0.2f\n", output_task->getLastExecTime(), output_task->getMinExecTime(), output_task->getMaxExecTime(), output_task->getAverageExecTime());
    Serial.print(msg);
  } else Serial.println("Output task not created.");

  if (gpio_task) {
    sprintf(msg, "GPIO task µs last: %lu, min: %lu, max: %lu, avg: %0.2f\n", gpio_task->getLastExecTime(), gpio_task->getMinExecTime(), gpio_task->getMaxExecTime(), gpio_task->getAverageExecTime());
    Serial.print(msg);
  } else Serial.println("GPIO task not created.");

  if (modbus_task) {
    sprintf(msg, "Modbus task µs last: %lu, min: %lu, max: %lu, avg: %0.2f\n", modbus_task->getLastExecTime(), modbus_task->getMinExecTime(), modbus_task->getMaxExecTime(), modbus_task->getAverageExecTime());
    Serial.print(msg);
  } else Serial.println("Modbus task not created.");

  if (phProbe_task) {
    sprintf(msg, "PH probe task µs last: %lu, min: %lu, max: %lu, avg: %0.2f\n", phProbe_task->getLastExecTime(), phProbe_task->getMinExecTime(), phProbe_task->getMaxExecTime(), phProbe_task->getAverageExecTime());
    Serial.print(msg);
  } else Serial.println("PH probe task not created.");

  if (levelProbe_task) {
    sprintf(msg, "Level probe task µs last: %lu, min: %lu, max: %lu, avg: %0.2f\n", levelProbe_task->getLastExecTime(), levelProbe_task->getMinExecTime(), levelProbe_task->getMaxExecTime(), levelProbe_task->getAverageExecTime());
    Serial.print(msg);
  } else Serial.println("Level probe task not created.");
  
  if (PARsensor_task) {
    sprintf(msg, "PAR sensor task µs last: %lu, min: %lu, max: %lu, avg: %0.2f\n", PARsensor_task->getLastExecTime(), PARsensor_task->getMinExecTime(), PARsensor_task->getMaxExecTime(), PARsensor_task->getAverageExecTime());
    Serial.print(msg);
  } else Serial.println("PAR sensor task not created.");

  if (RTDsensor_task) {
    sprintf(msg, "RTD task µs last: %lu, min: %lu, max: %lu, avg: %0.2f\n", RTDsensor_task->getLastExecTime(), RTDsensor_task->getMinExecTime(), RTDsensor_task->getMaxExecTime(), RTDsensor_task->getAverageExecTime());
    Serial.print(msg);
  } else Serial.println("RTD task not created.");

  if (printStuff_task) {
    sprintf(msg, "Print stuff task µs last: %lu, min: %lu, max: %lu, avg: %0.2f\n", printStuff_task->getLastExecTime(), printStuff_task->getMinExecTime(), printStuff_task->getMaxExecTime(), printStuff_task->getAverageExecTime());
    Serial.print(msg);
  }
}


// Instantiate IPCProtocol for Serial1
IPCProtocol ipc(Serial1);

void sendPhSensorToSysMCU(float pH, bool online) {
    Message msg;
    msg.msgId = MSG_PH_SENSOR;
    msg.objId = 0; // Use 0 for single sensor, or index if multiple
    msg.dataLength = sizeof(PHSensor);
    PHSensor sensor = {pH, online};
    memcpy(msg.data, &sensor, sizeof(sensor));
    msg.crc = ipc.calculateCRC(msg);
    ipc.sendMessage(msg);
}

void phProbeHandler(bool valid, uint16_t *data) {
  if (!valid) {
    Serial.println("Invalid ph probe data.");
    sendPhSensorToSysMCU(-1.0f, false);
    return;
  }
  float temperature = static_cast<int>(data[0]) / 100.0f;
  float pH = static_cast<int>(data[1]) / 100.0f;
  char msg[64];
  sprintf(msg, "PH probe data: Temperature: %0.2f °C, pH: %0.2f\n", temperature, pH);
  Serial.print(msg);
  sendPhSensorToSysMCU(pH, true);
}

void phProbeRequest(void) {
  Serial.println("Queuing pH probe modbus request");
  uint8_t slaveID = 12;
  uint8_t functionCode = 4;
  uint16_t address = 0;
  static uint16_t data[2];
  if (!modbusDriver[2].modbus.pushRequest(slaveID, functionCode, address, data, 2, phProbeHandler)) {
    Serial.println("ERROR - queue full");
  } else {
    char msg[48];
    sprintf(msg, "Current queue size: %d\n", modbusDriver[2].modbus.getQueueCount());
    Serial.print(msg);
  }
}


void sendLevelSensorToSysMCU(float level, bool online) {
    Message msg;
    msg.msgId = MSG_LEVEL_SENSOR;
    msg.objId = 0;
    msg.dataLength = sizeof(LevelSensor);
    LevelSensor sensor = {level, online};
    memcpy(msg.data, &sensor, sizeof(sensor));
    msg.crc = ipc.calculateCRC(msg);
    ipc.sendMessage(msg);
}

void levelProbeHandler(bool valid, uint16_t *data) {
  if (!valid) {
    Serial.println("Invalid level probe data.");
    sendLevelSensorToSysMCU(-1.0f, false);
    return;
  }
  int value = static_cast<int>(data[4]);
  float level = value / pow(10, data[3]);
  const char *unit[7] = {"", "cm", "mm", "Mpa", "Pa", "kPa", "MA"};
  int baud[8] = {1200, 2400, 4800, 9600, 19200, 38400, 57600, 115200};
  char msg[128];
  sprintf(msg, "Level probe data: Level: %0.2f %s, ID: %d, Baud: %d, Zero pt: %d\n", level, unit[data[2]], data[0], baud[data[1]], data[5]);
  Serial.print(msg);
  sendLevelSensorToSysMCU(level, true);
}

void levelProbeRequest(void) {
  Serial.println("Queuing level probe modbus request");
  uint8_t slaveID = 26;
  uint8_t functionCode = 3;
  uint16_t address = 0;
  static uint16_t data[6];
  if (!modbusDriver[2].modbus.pushRequest(slaveID, functionCode, address, data, 6, levelProbeHandler)) {
    Serial.println("ERROR - queue full");
  } else {
    char msg[64];
    sprintf(msg, "Current queue size: %d\n", modbusDriver[2].modbus.getQueueCount());
    Serial.print(msg);
  }
}


void sendPARSensorToSysMCU(float par, bool online) {
    Message msg;
    msg.msgId = MSG_PAR_SENSOR;
    msg.objId = 0;
    msg.dataLength = sizeof(PARSensor);
    PARSensor sensor = {par, online};
    memcpy(msg.data, &sensor, sizeof(sensor));
    msg.crc = ipc.calculateCRC(msg);
    ipc.sendMessage(msg);
}

void PARsensorHandler(bool valid, uint16_t *data) {
  if (!valid) {
    Serial.println("Invalid PAR sensor data.");
    sendPARSensorToSysMCU(-1.0f, false);
    return;
  }
  char msg[64];
  sprintf(msg, "PAR sensor data: %d µmol/m²/s\n", data[0]);
  Serial.print(msg);
  sendPARSensorToSysMCU((float)data[0], true);
}

void PARsensorRequest(void) {
  Serial.println("Queuing PAR sensor modbus request");
  uint8_t slaveID = 34;
  uint8_t functionCode = 3;
  uint16_t address = 0;
  static uint16_t data[1];
  if (!modbusDriver[2].modbus.pushRequest(slaveID, functionCode, address, data, 1, PARsensorHandler)) {
    Serial.println("ERROR - queue full");
  } else {
    char msg[64];
    sprintf(msg, "Current queue size: %d\n", modbusDriver[2].modbus.getQueueCount());
    Serial.print(msg);
  }
}


void sendTemperatureSensorToSysMCU(int idx, float celcius, bool online) {
    Message msg;
    msg.msgId = MSG_TEMPERATURE_SENSOR;
    msg.objId = idx;
    msg.dataLength = sizeof(TemperatureSensor);
    TemperatureSensor sensor = {celcius, online};
    memcpy(msg.data, &sensor, sizeof(sensor));
    msg.crc = ipc.calculateCRC(msg);
    ipc.sendMessage(msg);
}

void RTD_manage(void) {
  readRtdSensors();
  for (int i = 0; i < 3; i++) {
    if (rtd_interface[i].temperatureObj->fault) {
      Serial.println(rtd_interface[i].temperatureObj->message);
      sendTemperatureSensorToSysMCU(i, -242.02f, false);
    } else {
      char msg[64];
      sprintf(msg, "RTD %d: %0.2f °C\n", i+1, rtd_interface[i].temperatureObj->temperature);
      Serial.print(msg);
      sendTemperatureSensorToSysMCU(i, rtd_interface[i].temperatureObj->temperature, true);
    }
  }
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
  ipc.begin(19200); // Match baudrate to sys MCU
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
  ADC_init();
  char msg[128];
  sprintf(msg, "Result: %s\n", adcDriver.message);
  Serial.print(msg);

  // PT100 setup testing
  Serial.println("Initialising RTD interface");
  setupRtdInterface();

  // DAC setup testing
  Serial.println("Initialising DAC interface");
  if (!DAC_init()) {
    Serial.println("Failed to initialise DAC driver.");
    char msg[256];
    sprintf(msg, "Result: %s, Result Ch1: %s, Result Ch2: %s\n", dacDriver.message, dacDriver.outputObj[0]->message, dacDriver.outputObj[1]->message);
    Serial.print(msg);
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

  // DRV8235 initialisation
  Serial.println("Initialising DRV8235 driver");
  if (!motor_init()) {
    Serial.println("Failed to initialise DRV8235 driver.");
  } else {
    Serial.println("DRV8235 driver initialised.");
  }

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

  Serial.println("Starting Modbus interface");
  modbus_init();

  Serial.println("Adding tasks to scheduler");
  output_task = tasks.addTask(output_update, 100, true, false);
  gpio_task = tasks.addTask(gpio_update, 100, true, true);
  modbus_task = tasks.addTask(modbus_manage, 10, true, true);
  phProbe_task = tasks.addTask(phProbeRequest, 2000, true, false);
  levelProbe_task = tasks.addTask(levelProbeRequest, 2000, true, false);
  PARsensor_task = tasks.addTask(PARsensorRequest, 2000, true, false);
  printStuff_task = tasks.addTask(printStuff, 2000, true, false);
  RTDsensor_task = tasks.addTask(RTD_manage, 1000, true, false);


  // --- Initialize Shared Modbus Master for RS485-1 ---
  Serial.println("Initialising Shared Modbus Master (RS485-1 on Serial1)");
  // Using Serial1 (Pins 31=TX, 32=RX), Baud 19200, No RTS Pin (-1)
  if (!modbus_init()) {
      Serial.println("Failed to initialize shared Modbus master!");
      // Handle error appropriately - perhaps halt or set a system fault flag
  } else {
      Serial.println("Shared Modbus Master initialized.");

      // --- Initialize DO Sensor Driver ---
      Serial.println("Initialising DO Sensor Driver");
      // TODO: Implement do_sensor_init function
      // if (!do_sensor_init()) {
      //     Serial.println("Failed to initialize DO sensor driver!");
      //     if (doSensorDriver.newMessage) Serial.println(doSensorDriver.message);
      // } else {
      //     Serial.println("DO Sensor driver initialized.");
      // }

      // --- Initialize pH Sensor Driver ---
      Serial.println("Initialising pH Sensor Driver");
      // TODO: Implement ph_sensor_init function
      // if (!ph_sensor_init()) {
      //     Serial.println("Failed to initialize pH sensor driver!");
      //     if (phSensorDriver.newMessage) Serial.println(phSensorDriver.message);
      // } else {
      //     Serial.println("pH Sensor driver initialized.");
      // }
  }

  Serial.println("Setup done");
}

void loop() {
  tasks.update();
}