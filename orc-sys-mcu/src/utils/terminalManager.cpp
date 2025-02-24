#include "terminalManager.h"

void init_terminalManager(void) {
  xTaskCreate(manageTerminal, "Term updt", 1024, NULL, 1, NULL);
}

void manageTerminal(void *param)
{
  (void)param;  
  while (!core1setupComplete || !core0setupComplete) vTaskDelay(pdMS_TO_TICKS(100));

  debug_printf(LOG_INFO, "Terminal task started\n");

  // Task loop
  while (1) {
    if (Serial.available())
    {
      char serialString[10];  // Buffer for incoming serial data
      memset(serialString, 0, sizeof(serialString));
      int bytesRead = Serial.readBytesUntil('\n', serialString, sizeof(serialString) - 1); // Leave room for null terminator
      if (bytesRead > 0 ) {
          serialString[bytesRead] = '\0'; // Add null terminator
          debug_printf(LOG_INFO, "Received:  %s\n", serialString);
          if (strcmp(serialString, "ps") == 0) {
            osDebugPrint();
          }
          else if (strcmp(serialString, "reboot") == 0) {
            debug_printf(LOG_INFO, "Rebooting now...\n");
            rp2040.restart();
          }
          else if (strcmp(serialString, "ip") == 0) {
            debug_printf(LOG_INFO, "Ethernet connected, IP address: %s, Gateway: %s\n",
                eth.localIP().toString().c_str(),
                eth.gatewayIP().toString().c_str());
          }
          else if (strcmp(serialString, "sd") == 0) {
            debug_printf(LOG_INFO, "Getting SD card info...\n");
            printSDInfo();
          }
          else {
            debug_printf(LOG_INFO, "Unknown command: %s\n", serialString);
            debug_printf(LOG_INFO, "Available commands: ps (print OS processes), ip (print IP address), sd (print SD card info), reboot\n");
          }
        }
    }
    // Clear the serial buffer each loop.
    while(Serial.available()) Serial.read();
    vTaskDelay(pdMS_TO_TICKS(100));
  }
}
