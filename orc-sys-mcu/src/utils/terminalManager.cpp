#include "terminalManager.h"

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
      // Strip trailing \r if present (Windows line endings)
      if (bytesRead > 0 && serialString[bytesRead - 1] == '\r') {
        serialString[bytesRead - 1] = '\0';
      }
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
          if (!status.ipcConnected) {
            log(LOG_INFO, false, "IPC status: CONNECTION LOST\n");
          } else if (status.ipcTimeout) {
            log(LOG_INFO, false, "IPC status: TIMEOUT WARNING\n");
          } else {
            log(LOG_INFO, false, "IPC status: OK\n");
          }
          log(LOG_INFO, false, "RTC status: %s\n", status.rtcOK ? "OK" : "ERROR");
          if (!status.modbusConfigured) {
            log(LOG_INFO, false, "Modbus status: NO DEVICES CONFIGURED\n");
          } else if (status.modbusFault) {
            log(LOG_INFO, false, "Modbus status: FAULT\n");
          } else if (status.modbusConnected) {
            log(LOG_INFO, false, "Modbus status: ALL DEVICES CONNECTED\n");
          } else {
            log(LOG_INFO, false, "Modbus status: WAITING FOR CONNECTION\n");
          }
          log(LOG_INFO, false, "Webserver status: %s\n", status.webserverUp ? "OK" : "DOWN");
          log(LOG_INFO, false, "MQTT status: %s\n", status.mqttConnected ? "CONNECTED" : "DOWN");
          statusLocked = false;
        }
      }
      else if (strcmp(serialString, "ping") == 0) {
        log(LOG_INFO, true, "Sending PING to SAME51...\n");
        log(LOG_DEBUG, false, "PING packet format: [0x7E] [LEN_HI][LEN_LO] [TYPE] [CRC_HI][CRC_LO] [0x7E]\n");
        log(LOG_DEBUG, false, "Expected: [0x7E] [0x00][0x03] [0x00] [CRC] [0x7E] (3 bytes = TYPE only)\n");
        log(LOG_INFO, false, "PING sent successfully (waiting for PONG)\n");
        if (!ipc.sendPing()) {
          log(LOG_ERROR, true, "Failed to send PING (TX queue full)\n");
        }
      }
      else if (strcmp(serialString, "hello") == 0) {
        log(LOG_INFO, true, "Sending HELLO to SAME51...\n");
        if (ipc.sendHello(IPC_PROTOCOL_VERSION, 0x00010001, "RP2040-ORC-SYS")) {
          log(LOG_INFO, false, "HELLO sent successfully (waiting for HELLO_ACK)\n");
        } else {
          log(LOG_ERROR, true, "Failed to send HELLO (TX queue full)\n");
        }
      }
      else if (strcmp(serialString, "ping-raw") == 0) {
        log(LOG_INFO, true, "Sending raw PING bytes to Serial1...\n");
        // Manually send a PING packet for debugging
        // Packet: [START=0x7E] [LEN_HI=0x00] [LEN_LO=0x03] [TYPE=0x00] [CRC_HI] [CRC_LO] [END=0x7E]
        uint8_t pingPacket[] = {0x00, 0x03, 0x00};  // LENGTH + TYPE
        uint16_t crc = 0xFFFF;
        for (int i = 0; i < 3; i++) {
          crc ^= (uint16_t)pingPacket[i] << 8;
          for (int j = 0; j < 8; j++) {
            if (crc & 0x8000) {
              crc = (crc << 1) ^ 0x1021;
            } else {
              crc <<= 1;
            }
          }
        }
        log(LOG_INFO, false, "Raw PING: 7E %02X %02X %02X %02X %02X 7E\n", 
            pingPacket[0], pingPacket[1], pingPacket[2], 
            (crc >> 8) & 0xFF, crc & 0xFF);
        Serial1.write(0x7E);
        Serial1.write(pingPacket, 3);
        Serial1.write((crc >> 8) & 0xFF);
        Serial1.write(crc & 0xFF);
        Serial1.write(0x7E);
        log(LOG_INFO, true, "Raw PING sent\n");
      }
      else if (strcmp(serialString, "ipc-stats") == 0) {
        IPC_Statistics_t stats;
        ipc.getStatistics(&stats);
        log(LOG_INFO, false, "=== IPC Statistics ===\n");
        log(LOG_INFO, false, "RX Packets: %lu\n", stats.rxPacketCount);
        log(LOG_INFO, false, "TX Packets: %lu\n", stats.txPacketCount);
        log(LOG_INFO, false, "RX Errors: %lu\n", stats.rxErrorCount);
        log(LOG_INFO, false, "CRC Errors: %lu\n", stats.crcErrorCount);
        log(LOG_INFO, false, "Last RX: %lu ms ago\n", stats.lastRxTime > 0 ? millis() - stats.lastRxTime : 0);
        log(LOG_INFO, false, "Last TX: %lu ms ago\n", stats.lastTxTime > 0 ? millis() - stats.lastTxTime : 0);
      }
      else if (strcmp(serialString, "ipc-dump") == 0) {
        log(LOG_INFO, true, "Reading raw bytes from Serial1 for 2 seconds...\n");
        log(LOG_INFO, false, "Bytes: ");
        uint32_t start = millis();
        int count = 0;
        while (millis() - start < 2000) {
          if (Serial1.available()) {
            uint8_t b = Serial1.read();
            log(LOG_INFO, false, "%02X ", b);
            count++;
            if (count % 16 == 0) log(LOG_INFO, false, "\n       ");
          }
        }
        log(LOG_INFO, false, "\nReceived %d bytes\n", count);
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
        log(LOG_INFO, false, "Available commands:\n");
        log(LOG_INFO, false, "  ip          - Print IP address\n");
        log(LOG_INFO, false, "  sd          - Print SD card info\n");
        log(LOG_INFO, false, "  status      - Print system status\n");
        log(LOG_INFO, false, "  ping        - Send PING to SAME51\n");
        log(LOG_INFO, false, "  hello       - Send HELLO to SAME51 (initiate handshake)\n");
        log(LOG_INFO, false, "  ping-raw    - Send raw PING bytes (debug)\n");
        log(LOG_INFO, false, "  ipc-stats   - Print IPC statistics\n");
        log(LOG_INFO, false, "  ipc-dump    - Dump raw bytes from Serial1 for 2s\n");
        log(LOG_INFO, false, "  ipc-test    - Simulate IPC message (e.g., ipc-test temp 25.5)\n");
        log(LOG_INFO, false, "  reboot      - Reboot system\n");
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
