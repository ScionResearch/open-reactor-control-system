#include "terminalManager.h"
#include "../network/mqttManager.h" // For publishSensorData
#include "../lib/IPCprotocol/IPCDataStructs.h" // For sensor data structs

bool terminalReady = false;

void init_terminalManager(void) {
  while (!serialReady) {
    delay(10);
  }
  terminalReady = true;
  log(LOG_INFO, false, "Terminal task started\n");
}

void manageTerminal(void)
{
  if (serialLocked || !terminalReady) return;
  serialLocked = true;
  if (Serial.available())
  {
    char serialString[10];  // Buffer for incoming serial data
    memset(serialString, 0, sizeof(serialString));
    int bytesRead = Serial.readBytesUntil('\n', serialString, sizeof(serialString) - 1); // Leave room for null terminator
    serialLocked = false;
    if (bytesRead > 0 ) {
      serialString[bytesRead] = '\0'; // Add null terminator
      log(LOG_INFO, true,"Received:  %s\n", serialString);
      if (strcmp(serialString, "reboot") == 0) {
        log(LOG_INFO, true, "Rebooting now...\n");
        rp2040.restart();
      }
      else if (strcmp(serialString, "ip") == 0) {
        printNetConfig(networkConfig);
      }
      else if (strcmp(serialString, "sd") == 0) {
        log(LOG_INFO, false, "Getting SD card info...\n");
        printSDInfo();
      }
      else if (strcmp(serialString, "status") == 0) {
        log(LOG_INFO, false, "Getting status...\n");
        if (statusLocked) {
          log(LOG_INFO, false, "Status is locked\n");
        } else {
          statusLocked = true;
          log(LOG_INFO, false, "24V supply %0.1fV status: %s\n", status.Vpsu, status.psuOK ? "OK" : "OUT OF RANGE");
          log(LOG_INFO, false, "20V supply %0.1fV status: %s\n", status.V20, status.V20OK ? "OK" : "OUT OF RANGE");
          log(LOG_INFO, false, "5V supply %0.1fV status: %s\n", status.V5, status.V5OK ? "OK" : "OUT OF RANGE");
          log(LOG_INFO, false, "IPC status: %s\n", status.ipcOK ? "OK" : "ERROR");
          log(LOG_INFO, false, "RTC status: %s\n", status.rtcOK ? "OK" : "ERROR");
          log(LOG_INFO, false, "Modbus status: %s\n", status.modbusConnected ? "CONNECTED" : "DOWN");
          log(LOG_INFO, false, "Webserver status: %s\n", status.webserverUp ? "OK" : "DOWN");
          log(LOG_INFO, false, "MQTT status: %s\n", status.mqttConnected ? "CONNECTED" : "DOWN");
          statusLocked = false;
        }
      }
      else {
      // --- NEW TEST COMMAND LOGIC ---
      char command[20], type[20];
      float value;
      if (sscanf(serialString, "%s %s %f", command, type, &value) == 3 && strcmp(command, "ipc-test") == 0) {
        log(LOG_INFO, true, "Simulating IPC message: type=%s, value=%.2f\n", type, value);
        Message testMsg;
        testMsg.objId = 0; // Test with object ID 0

        if (strcmp(type, "temp") == 0) {
          testMsg.msgId = MSG_TEMPERATURE_SENSOR;
          TemperatureSensor data = {value, true};
          testMsg.dataLength = sizeof(data);
          memcpy(testMsg.data, &data, testMsg.dataLength);
          publishSensorData(testMsg); // Directly call the handler for testing
        } else if (strcmp(type, "ph") == 0) {
          testMsg.msgId = MSG_PH_SENSOR;
          PHSensor data = {value, true};
          testMsg.dataLength = sizeof(data);
          memcpy(testMsg.data, &data, testMsg.dataLength);
          publishSensorData(testMsg);
        } else {
          log(LOG_WARNING, true, "Unknown ipc-test type: %s\n", type);
        }

      } // --- END OF NEW TEST COMMAND LOGIC ---
      else {
        log(LOG_INFO, false, "Unknown command: %s\n", serialString);
        log(LOG_INFO, false, "Available commands: ip (print IP address), sd (print SD card info), status, reboot\n");
      }
      }
    }
    // Clear the serial buffer each loop.
    serialLocked = true;
    while(Serial.available()) Serial.read();
    serialLocked = false;
  }
  serialLocked = false;
}
