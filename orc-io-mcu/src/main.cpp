#include "sys_init.h"
#include <FlashStorage_SAMD.h>

// Most of this is debug code!!!! Very much a work in progress

void printStuff(void) {  
  // Print CPU usage summary
  //Serial.println("\n=== CPU Usage Report ===");
  //Serial.printf("Total CPU Usage: %0.2f%%\n", tasks.getTotalCpuUsagePercent());
  //Serial.println("Task information ↓↓↓\n");
  
  /*if (analog_input_task) {
    Serial.printf("Analog input task µs last: %d, min: %d, max: %d, avg: %0.2f, CPU: %0.2f%%\n", 
                  analog_input_task->getLastExecTime(), analog_input_task->getMinExecTime(), 
                  analog_input_task->getMaxExecTime(), analog_input_task->getAverageExecTime(),
                  analog_input_task->getCpuUsagePercent());
    for (int i = 0; i < 8; i++) Serial.printf("A in %d: %0.3f %s\n", i+1, adcDriver.inputObj[i]->value, adcDriver.inputObj[i]->unit);
  } else Serial.println("Analog input task not created.");

  if (output_task) {
    Serial.printf("Output task µs last: %d, min: %d, max: %d, avg: %0.2f, CPU: %0.2f%%\n", 
                  output_task->getLastExecTime(), output_task->getMinExecTime(), 
                  output_task->getMaxExecTime(), output_task->getAverageExecTime(),
                  output_task->getCpuUsagePercent());
  } else Serial.println("Output task not created.");

  if (gpio_task) {
    Serial.printf("GPIO task µs last: %d, min: %d, max: %d, avg: %0.2f, CPU: %0.2f%%\n", 
                  gpio_task->getLastExecTime(), gpio_task->getMinExecTime(), 
                  gpio_task->getMaxExecTime(), gpio_task->getAverageExecTime(),
                  gpio_task->getCpuUsagePercent());
  } else Serial.println("GPIO task not created.");

  if (modbus_task) {
    Serial.printf("Modbus task µs last: %d, min: %d, max: %d, avg: %0.2f, CPU: %0.2f%%\n", 
                  modbus_task->getLastExecTime(), modbus_task->getMinExecTime(), 
                  modbus_task->getMaxExecTime(), modbus_task->getAverageExecTime(),
                  modbus_task->getCpuUsagePercent());
  } else Serial.println("Modbus task not created.");

  if (RTDsensor_task) {
    Serial.printf("RTD task µs last: %d, min: %d, max: %d, avg: %0.2f, CPU: %0.2f%%\n", 
                  RTDsensor_task->getLastExecTime(), RTDsensor_task->getMinExecTime(), 
                  RTDsensor_task->getMaxExecTime(), RTDsensor_task->getAverageExecTime(),
                  RTDsensor_task->getCpuUsagePercent());
  for (int i = 0; i < 3; i++) Serial.printf("RTD %d: %0.2f °C\n", i+1, rtd_interface[i].temperatureObj->temperature);
  } else Serial.println("RTD task not created.");

  if (phProbe_task && phProbe) {
    Serial.printf("pH probe task µs last: %d, min: %d, max: %d, avg: %0.2f, CPU: %0.2f%%\n", 
                  phProbe_task->getLastExecTime(), phProbe_task->getMinExecTime(), 
                  phProbe_task->getMaxExecTime(), phProbe_task->getAverageExecTime(),
                  phProbe_task->getCpuUsagePercent());
    Serial.printf("pH: %0.2f %s, Temperature: %0.2f %s\n", 
                  phProbe->getPhSensor().ph, phProbe->getPhSensor().unit,
                  phProbe->getTemperatureSensor().temperature, phProbe->getTemperatureSensor().unit);
    if (phProbe->hasFault()) {
      Serial.printf("pH probe FAULT: %s\n", phProbe->getMessage());
      phProbe->clearMessages();
    } else if (phProbe->hasNewMessage()) {
      Serial.printf("pH probe Message: %s\n", phProbe->getMessage());
      phProbe->clearMessages();
    }
  } else {
    Serial.println("pH probe task not created.");
  }

  if(mfc_task && alicatMFC) {
    Serial.printf("MFC task µs last: %d, min: %d, max: %d, avg: %0.2f, CPU: %0.2f%%\n", 
                  mfc_task->getLastExecTime(), mfc_task->getMinExecTime(), 
                  mfc_task->getMaxExecTime(), mfc_task->getAverageExecTime(),
                  mfc_task->getCpuUsagePercent());
    Serial.printf("Alicat MFC Flow: %0.3f %s, Pressure: %0.3f %s, Setpoint: %0.3f %s\n", 
                  alicatMFC->getFlowSensor().flow, alicatMFC->getFlowSensor().unit,
                  alicatMFC->getPressureSensor().pressure, alicatMFC->getPressureSensor().unit,
                  alicatMFC->getSetpoint(), alicatMFC->getSetpointUnit());
    if (alicatMFC->hasFault()) {
      Serial.printf("MFC FAULT: %s\n", alicatMFC->getMessage());
      alicatMFC->clearMessage();
    } else if (alicatMFC->hasNewMessage()) {
      Serial.printf("MFC Message: %s\n", alicatMFC->getMessage());
      alicatMFC->clearMessage();
    }
  } else {
    Serial.println("MFC task not created.");
  }*/

  /*if (ipc_task) {
    Serial.printf("IPC task µs last: %d, min: %d, max: %d, avg: %0.2f, CPU: %0.2f%%\n", 
                  ipc_task->getLastExecTime(), ipc_task->getMinExecTime(), 
                  ipc_task->getMaxExecTime(), ipc_task->getAverageExecTime(),
                  ipc_task->getCpuUsagePercent());
    Serial.printf("IPC Connected: %s, RX: %u, TX: %u, Errors: %u, CRC: %u\n",
                  ipc_isConnected() ? "YES" : "NO",
                  ipcDriver.rxPacketCount, ipcDriver.txPacketCount,
                  ipcDriver.rxErrorCount, ipcDriver.crcErrorCount);
  } else Serial.println("IPC task not created.");*/

  /*if (printStuff_task) {
    Serial.printf("Print stuff task µs last: %d, min: %d, max: %d, avg: %0.2f, CPU: %0.2f%%\n", 
                  printStuff_task->getLastExecTime(), printStuff_task->getMinExecTime(), 
                  printStuff_task->getMaxExecTime(), printStuff_task->getAverageExecTime(),
                  printStuff_task->getCpuUsagePercent());
  }  */
  //Serial.println("\n========================");
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

  Serial.println("Initialising ADC interface");
  ADC_init();
  Serial.printf("Result: %s\n", adcDriver.message);

  // PT100 setup testing
  Serial.println("Initialising RTD interface");
  setupRtdInterface();

  // DAC setup testing
  Serial.println("Initialising DAC interface");
  if (!DAC_init()) {
    Serial.println("Failed to initialise DAC driver.");
    Serial.printf("Result: %s, Result Ch1: %s, Result Ch2: %s\n", dacDriver.message, dacDriver.outputObj[0]->message, dacDriver.outputObj[1]->message);
  } else {
    Serial.println("DAC driver initialised.");
  }

  // TMC5130 stepper setup testing
  Serial.println("Initialising TMC5130 stepper driver");
  if (!stepper_init()) {
    Serial.println("Failed to initialise TMC5130 driver.");
    if (stepperDevice.fault) Serial.printf("Fault: %s\n", stepperDevice.message);
  } else {
    Serial.println("TMC5130 stepper driver initialised.");
  }

  // DRV8235 motor driver initialisation
  Serial.println("Initialising DRV8235 motor drivers");
  if (!motor_init()) {
    Serial.println("Failed to initialise DRV8235 motor drivers.");
    for (int i = 0; i < 4; i++) {
      if (motorDevice[i].fault) Serial.printf("Motor %d fault: %s\n", i+1, motorDevice[i].message);
    }
  } else {
    Serial.println("DRV8235 motor drivers initialised.");
  }

  // INA260 power sensor initialisation
  Serial.println("Initialising INA260 power sensors");
  if (!pwrSensor_init()) {
    Serial.println("Failed to initialise INA260 power sensors.");
    for (int i = 0; i < 2; i++) {
      if (pwr_energy[i].fault) Serial.printf("Power sensor %d fault: %s\n", i+1, pwr_energy[i].message);
    }
  } else {
    Serial.println("INA260 power sensors initialised.");
  }

  // Outputs initialisation
  Serial.println("Initialising outputs");
  output_init();

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

  Serial.println("Initialising GPIO pins");
  gpio_init();

  Serial.println("Starting Modbus interface");
  modbus_init();

  Serial.println("Starting IPC interface");
  if (!ipc_init()) {
    Serial.println("Failed to initialise IPC driver.");
  } else {
    Serial.println("IPC driver initialised at 2 Mbps.");
  }

  Serial.println("Initialising Device Manager");
  if (!DeviceManager::init()) {
    Serial.println("Failed to initialise Device Manager.");
  } else {
    Serial.println("Device Manager initialised");
  }

  Serial.println("Initialising Controller Manager");
  if (!ControllerManager::init()) {
    Serial.println("Failed to initialise Controller Manager.");
  } else {
    Serial.println("Controller Manager initialised");
  }

  Serial.println("Adding tasks to scheduler");
  analog_input_task = tasks.addTask(ADC_update, 10, true, false);
  analog_output_task = tasks.addTask(DAC_update, 100, true, false);
  output_task = tasks.addTask(output_update, 100, true, false);
  gpio_task = tasks.addTask(gpio_update, 100, true, true);
  modbus_task = tasks.addTask(modbus_manage, 10, true, true);
  ipc_task = tasks.addTask(ipc_update, 5, true, true);  // 5ms, high priority
  RTDsensor_task = tasks.addTask(RTD_manage, 200, true, false);
  motor_task = tasks.addTask([]() { motor_update(); }, 10, true, false);
  pwrSensor_task = tasks.addTask([]() { pwrSensor_update(); }, 1000, true, false);
  
  // Debug/monitoring tasks
  printStuff_task = tasks.addTask(printStuff, 1000, true, false);

  Serial.println("Setup done");
}

void loop() {
  tasks.update();
}