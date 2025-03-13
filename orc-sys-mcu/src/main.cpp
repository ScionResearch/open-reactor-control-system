#include "sys_init.h"

// Timing variables for cooperative multitasking
unsigned long lastSDManagerTime = 0;
unsigned long lastLEDManagerTime = 0;
unsigned long lastTimeManagerTime = 0;
unsigned long lastPowerManagerTime = 0;
unsigned long lastTerminalTime = 0;
unsigned long lastNetworkTime = 0;
unsigned long lastIpcManagerTime = 0;

void setup() // Eth interface (keep hardware-specific initialization on core 0)
{
  init_core0(); // All core 0 initialisation in this function

  log(LOG_INFO, false, "Core 0 setup complete\n");
  core0setupComplete = true;
  while (!core1setupComplete) delay(100);
  if (networkConfig.ntpEnabled) handleNTPUpdates(true);
  log(LOG_INFO, false, "<---System initialisation complete --->\n\n");
}

void setup1()
{
  while (!serialReady) delay(100);
  log(LOG_INFO, false, "Core 1 setup started\n");
  init_core1(); // All core 1 initialisation in this function

  // Set System Status
  setLEDcolour(LED_SYSTEM_STATUS, LED_STATUS_STARTUP);
  // Modbus not yet implemented
  setLEDcolour(LED_MODBUS_STATUS, LED_STATUS_OFF);
  // MQTT not yet implemented
  setLEDcolour(LED_MQTT_STATUS, LED_STATUS_OFF); 

  log(LOG_INFO, false, "Core 1 setup complete\n");
  core1setupComplete = true;
  while (!core0setupComplete) delay(100);
}

// Core 0 - network and coordination
void loop()
{
  unsigned long currentMillis = millis();
  
  // Network handling
  if (currentMillis - lastNetworkTime >= TASK_INTERVAL_NETWORK) {
    handleNetworkManager();
    lastNetworkTime = currentMillis;
  }
  
  // IPC handling
  if (currentMillis - lastIpcManagerTime >= TASK_INTERVAL_IPC) {
    handleIpcManager();
    lastIpcManagerTime = currentMillis;
  }
}

// Core 1 - all other subsystems
void loop1() {
  unsigned long currentMillis = millis();
  
  // SD card management
  if (currentMillis - lastSDManagerTime >= TASK_INTERVAL_SD_MANAGER) {
    handleSDManager();
    lastSDManagerTime = currentMillis;
  }
  
  // LED management
  if (currentMillis - lastLEDManagerTime >= TASK_INTERVAL_LED_MANAGER) {
    handleLEDManager();
    lastLEDManagerTime = currentMillis;
  }
  
  // Time management
  if (currentMillis - lastTimeManagerTime >= TASK_INTERVAL_TIME_MANAGER) {
    handleTimeManager();
    lastTimeManagerTime = currentMillis;
  }
  
  // Power management
  if (currentMillis - lastPowerManagerTime >= TASK_INTERVAL_POWER_MANAGER) {
    handlePowerManager();
    lastPowerManagerTime = currentMillis;
  }
  
  // Terminal handling
  if (currentMillis - lastTerminalTime >= TASK_INTERVAL_TERMINAL) {
    handleTerminalManager();
    lastTerminalTime = currentMillis;
  }
  
  yield(); // Allow core to process background tasks
}