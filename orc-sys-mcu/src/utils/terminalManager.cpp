#include "terminalManager.h"

void init_terminalManager(void) {
  xTaskCreate(manageTerminal, "Term updt", 1024, NULL, 1, NULL);
}

void manageTerminal(void *param)
{
  (void)param;  
  while (!core1setupComplete || !core0setupComplete) vTaskDelay(pdMS_TO_TICKS(100));

  log(LOG_INFO, false, "Terminal task started\n");

  // Task loop
  while (1) {
    if (Serial.available())
    {
      char serialString[10];  // Buffer for incoming serial data
      memset(serialString, 0, sizeof(serialString));
      int bytesRead = Serial.readBytesUntil('\n', serialString, sizeof(serialString) - 1); // Leave room for null terminator
      if (bytesRead > 0 ) {
          serialString[bytesRead] = '\0'; // Add null terminator
          log(LOG_INFO, true,"Received:  %s\n", serialString);
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
            log(LOG_INFO, false, "Available commands: ps (print OS processes), ip (print IP address), sd (print SD card info), reboot\n");
          }
        }
    }
    // Clear the serial buffer each loop.
    while(Serial.available()) Serial.read();
    vTaskDelay(pdMS_TO_TICKS(100));
  }
}

void psPrint(void)
{
    bool logToSD = true;
    const char *taskStateName[5] = {
        "Ready",
        "Blocked",
        "Suspended",
        "Deleted",
        "Invalid"};
    int numberOfTasks = uxTaskGetNumberOfTasks();

    TaskStatus_t *pxTaskStatusArray = new TaskStatus_t[numberOfTasks];
    uint32_t runtime;
    numberOfTasks = uxTaskGetSystemState(pxTaskStatusArray, numberOfTasks, &runtime);
    log(LOG_INFO, logToSD, "Tasks: %d\n", numberOfTasks);
    for (int i = 0; i < numberOfTasks; i++)
    {
        int currentState = pxTaskStatusArray[i].eCurrentState;
        log(LOG_INFO, logToSD, "ID: %d %s | Current state: %s | Priority: %u | Free stack: %u | Runtime: %u\n",
            i, pxTaskStatusArray[i].pcTaskName, taskStateName[currentState], pxTaskStatusArray[i].uxBasePriority,
            pxTaskStatusArray[i].usStackHighWaterMark, pxTaskStatusArray[i].ulRunTimeCounter);
    }
    delete[] pxTaskStatusArray;
}
