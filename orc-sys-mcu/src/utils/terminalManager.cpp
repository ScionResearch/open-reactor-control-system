#include "terminalManager.h"

void init_terminalManager(void) {
  log(LOG_INFO, false, "Terminal manager initialized\n");
}

void handleTerminalManager(void) {
  if (Serial.available()) {
    char serialString[10];  // Buffer for incoming serial data
    memset(serialString, 0, sizeof(serialString));
    int bytesRead = Serial.readBytesUntil('\n', serialString, sizeof(serialString) - 1); // Leave room for null terminator
    if (bytesRead > 0) {
      serialString[bytesRead] = '\0'; // Add null terminator
      log(LOG_INFO, true, "Received:  %s\n", serialString);
      if (strcmp(serialString, "ps") == 0) {
        psPrint();
      }
      else if (strcmp(serialString, "reboot") == 0) {
        log(LOG_INFO, true, "Rebooting now...\n");
        rp2040.restart();
      }
      else if (strcmp(serialString, "ip") == 0) {
        log(LOG_INFO, true, "Ethernet connected, IP address: %s, Gateway: %s\n",
            eth.localIP().toString().c_str(),
            eth.gatewayIP().toString().c_str());
      }
      else if (strcmp(serialString, "sd") == 0) {
        log(LOG_INFO, false, "Getting SD card info...\n");
        printSDInfo();
      }
      else {
        log(LOG_INFO, false, "Unknown command: %s\n", serialString);
        log(LOG_INFO, false, "Available commands: ps (print system tasks), ip (print IP address), sd (print SD card info), reboot\n");
      }
    }
  }
  // Clear the serial buffer each loop.
  while(Serial.available()) Serial.read();
}

void psPrint(void) {
  bool logToSD = true;
  
  // Print system info - now using handler functions instead of FreeRTOS tasks
  log(LOG_INFO, logToSD, "System Handlers Status:\n");
  log(LOG_INFO, logToSD, "SD Manager Handler:    Interval: %d ms\n", TASK_INTERVAL_SD_MANAGER);
  log(LOG_INFO, logToSD, "LED Manager Handler:   Interval: %d ms\n", TASK_INTERVAL_LED_MANAGER);
  log(LOG_INFO, logToSD, "Time Manager Handler:  Interval: %d ms\n", TASK_INTERVAL_TIME_MANAGER);
  log(LOG_INFO, logToSD, "Power Manager Handler: Interval: %d ms\n", TASK_INTERVAL_POWER_MANAGER);
  log(LOG_INFO, logToSD, "Terminal Handler:      Interval: %d ms\n", TASK_INTERVAL_TERMINAL);
  log(LOG_INFO, logToSD, "Network Handler:       Interval: %d ms\n", TASK_INTERVAL_NETWORK);
  
  // Print memory usage
  log(LOG_INFO, logToSD, "\nMemory Usage:\n");
  log(LOG_INFO, logToSD, "Free Memory: %d bytes\n", rp2040.getFreeHeap());
  log(LOG_INFO, logToSD, "Total Memory: %d bytes\n", rp2040.getTotalHeap());
  log(LOG_INFO, logToSD, "Used Memory: %d bytes\n", rp2040.getTotalHeap() - rp2040.getFreeHeap());
  
  // Print system uptime
  unsigned long uptimeSeconds = millis() / 1000;
  unsigned long uptimeMinutes = uptimeSeconds / 60;
  unsigned long uptimeHours = uptimeMinutes / 60;
  unsigned long uptimeDays = uptimeHours / 24;
  
  uptimeSeconds %= 60;
  uptimeMinutes %= 60;
  uptimeHours %= 24;
  
  log(LOG_INFO, logToSD, "\nSystem Uptime: %lu days, %lu hours, %lu minutes, %lu seconds\n", 
      uptimeDays, uptimeHours, uptimeMinutes, uptimeSeconds);
}
