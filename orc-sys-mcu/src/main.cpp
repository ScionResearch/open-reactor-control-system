#include "sys_init.h"

void setup() // Eth interface (keep RTOS tasks out of core 0)
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

// Core 0 - non RTOS tasks
void loop()
{
  manageEthernet();
  ipc.update();
}

// Core 1 - all RTOS tasks
void loop1() {
  delay(100);
}