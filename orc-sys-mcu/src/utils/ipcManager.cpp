#include "ipcManager.h"
#include "network/mqttManager.h" // For publishSensorData
#include "statusManager.h" // For status struct
#include "objectCache.h" // For caching sensor data

// Inter-processor communication
void init_ipcManager(void) {
  Serial1.setRX(PIN_SI_RX);
  Serial1.setTX(PIN_SI_TX);
  
  // Set UART FIFO size to 4096 bytes to accommodate multiple IPC packets from bulk requests
  // With bulk requests of 21 objects, we need room for 21 responses (~150 bytes each = 3150 bytes)
  // Max packet size with byte stuffing: ~260 bytes raw data * 2 (worst case stuffing) = 520 bytes
  Serial1.setFIFOSize(4096);
  
  // Clear object cache to force fresh data from IO MCU
  objectCache.clear();
  log(LOG_INFO, false, "Object cache cleared\n");
  
  ipc.begin(2000000); // 2 Mbps
  
  // Register message handlers
  registerIpcCallbacks();
  
  // Send HELLO message to SAME51
  ipc.sendHello(IPC_PROTOCOL_VERSION, 0x00010001, "RP2040-ORC-SYS");
  
  log(LOG_INFO, false, "Inter-processor communication setup complete\n");
  if (statusLocked) return;
  statusLocked = true;
  status.ipcOK = true;
  status.updated = true;
  statusLocked = false;
}

// Debug: Test sensor reads via IPC
unsigned long lastSensorTestTime = 0;
uint8_t currentTestIndex = 0;

void testSensorReads(void) {
  static bool testActive = false;  // Disabled - using web API polling instead
  
  if (!testActive) return;
  
  unsigned long now = millis();
  if (now - lastSensorTestTime < 2000) return;  // Test every 2 seconds
  lastSensorTestTime = now;
  
  // Cycle through different sensor types to test
  const char* testNames[] = {
    "ADC 0", "ADC 1", "DAC 0", "RTD 0", "GPIO 0", 
    "Output 0", "Stepper", "Motor 0", "Voltage 0", "Current 0", "Power 0", "Modbus 0"
  };
  uint8_t testIndices[] = {0, 1, 8, 10, 13, 21, 26, 27, 31, 32, 33, 37};
  
  if (currentTestIndex >= sizeof(testIndices)) {
    currentTestIndex = 0;
    log(LOG_INFO, true, "\n=== IPC Sensor Test Cycle Complete ===\n\n");
  }
  
  uint8_t index = testIndices[currentTestIndex];
  log(LOG_INFO, true, "IPC Test: Requesting %s (index %d)...\n", 
      testNames[currentTestIndex], index);
  
  // Send sensor read request
  IPC_SensorReadReq_t request;
  request.index = index;
  ipc.sendPacket(IPC_MSG_SENSOR_READ_REQ, (uint8_t*)&request, sizeof(request));
  
  currentTestIndex++;
}

void manageIPC(void) {
  // Check for incoming messages
  ipc.update();
  
  // Debug: Test sensor reads
  testSensorReads();
}

/**
 * @brief Handler for sensor data messages from SAME51
 */
void handleSensorData(uint8_t messageType, const uint8_t *payload, uint16_t length) {
  if (payload == nullptr || length != sizeof(IPC_SensorData_t)) {
    log(LOG_ERROR, false, "IPC: Invalid sensor data payload\n");
    return;
  }
  
  const IPC_SensorData_t *sensorData = (const IPC_SensorData_t *)payload;
  
  // DEBUG: Print received sensor data (simplified for performance)
  log(LOG_DEBUG, false, "IPC RX: Sensor[%d] = %.2f %s%s\n", 
      sensorData->index, sensorData->value, sensorData->unit,
      (sensorData->flags & IPC_SENSOR_FLAG_FAULT) ? " [FAULT]" : "");
  
  // Update object cache
  objectCache.updateObject(sensorData);
  
  // Forward to MQTT
  publishSensorDataIPC(sensorData);
  
  // TODO: Update status struct based on object type
  // This will be implemented when the status manager is updated for the new IPC protocol
}

/**
 * @brief Handler for PING messages
 */
void handlePing(uint8_t messageType, const uint8_t *payload, uint16_t length) {
  log(LOG_DEBUG, false, "IPC: Received PING from SAME51, sending PONG\n");
  // Respond with PONG
  ipc.sendPong();
}

/**
 * @brief Handler for PONG messages
 */
void handlePong(uint8_t messageType, const uint8_t *payload, uint16_t length) {
  log(LOG_INFO, true, "IPC: Received PONG from SAME51 ✓\n");
}

/**
 * @brief Handler for HELLO messages from SAME51
 */
void handleHello(uint8_t messageType, const uint8_t *payload, uint16_t length) {
  if (payload == nullptr || length != sizeof(IPC_Hello_t)) {
    log(LOG_ERROR, false, "IPC: Invalid HELLO payload\n");
    return;
  }
  
  const IPC_Hello_t *hello = (const IPC_Hello_t *)payload;
  
  log(LOG_INFO, true, "IPC: Received HELLO from %s (protocol v%08X, firmware v%08X)\n",
      hello->deviceName, hello->protocolVersion, hello->firmwareVersion);
  
  // Send HELLO_ACK
  IPC_HelloAck_t ack;
  ack.protocolVersion = IPC_PROTOCOL_VERSION;
  ack.firmwareVersion = 0x00010001; // v1.0.1
  ack.maxObjectCount = IPC_MAX_OBJECTS;
  ack.currentObjectCount = 0; // TODO: Get from object index manager
  
  ipc.sendPacket(IPC_MSG_HELLO_ACK, (uint8_t*)&ack, sizeof(ack));
  
  log(LOG_INFO, false, "IPC: Sent HELLO_ACK to SAME51\n");
  
  // Request index sync
  ipc.sendIndexSyncRequest();
}

/**
 * @brief Handler for HELLO_ACK messages from SAME51
 */
void handleHelloAck(uint8_t messageType, const uint8_t *payload, uint16_t length) {
  if (payload == nullptr || length != sizeof(IPC_HelloAck_t)) {
    log(LOG_ERROR, false, "IPC: Invalid HELLO_ACK payload\n");
    return;
  }
  
  const IPC_HelloAck_t *ack = (const IPC_HelloAck_t *)payload;
  
  // Check protocol version compatibility
  if (ack->protocolVersion != IPC_PROTOCOL_VERSION) {
    log(LOG_ERROR, true, "IPC: Protocol version mismatch! Expected 0x%08X, got 0x%08X\n",
        IPC_PROTOCOL_VERSION, ack->protocolVersion);
    return;
  }
  
  log(LOG_INFO, true, "IPC: ✓ Handshake complete! SAME51 firmware v%08X (%u/%u objects)\n",
      ack->firmwareVersion, ack->currentObjectCount, ack->maxObjectCount);
  
  // TODO: Update connection state, start requesting index sync if needed
}

/**
 * @brief Handler for ERROR messages
 */
void handleError(uint8_t messageType, const uint8_t *payload, uint16_t length) {
  if (payload == nullptr || length != sizeof(IPC_Error_t)) {
    log(LOG_ERROR, false, "IPC: Invalid ERROR payload\n");
    return;
  }
  
  const IPC_Error_t *error = (const IPC_Error_t *)payload;
  log(LOG_ERROR, false, "IPC Error [%d]: %s\n", error->errorCode, error->message);
}

/**
 * @brief Handler for fault notifications
 */
void handleFaultNotify(uint8_t messageType, const uint8_t *payload, uint16_t length) {
  if (payload == nullptr || length != sizeof(IPC_FaultNotify_t)) {
    log(LOG_ERROR, false, "IPC: Invalid fault notify payload\n");
    return;
  }
  
  const IPC_FaultNotify_t *fault = (const IPC_FaultNotify_t *)payload;
  
  const char* severityStr[] = {"INFO", "WARNING", "ERROR", "CRITICAL"};
  log(LOG_WARNING, false, "IPC Fault [%s] Object %d: %s\n",
      severityStr[fault->severity], fault->index, fault->message);
  
  // TODO: Forward to MQTT or alarm system
}

/**
 * @brief Registers callback functions for IPC messages.
 */
void registerIpcCallbacks(void) {
  log(LOG_INFO, false, "Registering IPC message handlers...\n");

  // Protocol messages
  ipc.registerHandler(IPC_MSG_PING, handlePing);
  ipc.registerHandler(IPC_MSG_PONG, handlePong);
  ipc.registerHandler(IPC_MSG_HELLO, handleHello);
  ipc.registerHandler(IPC_MSG_HELLO_ACK, handleHelloAck);
  ipc.registerHandler(IPC_MSG_ERROR, handleError);
  
  // Sensor data
  ipc.registerHandler(IPC_MSG_SENSOR_DATA, handleSensorData);
  ipc.registerHandler(IPC_MSG_SENSOR_BATCH, handleSensorData); // Can use same handler
  
  // Fault notifications
  ipc.registerHandler(IPC_MSG_FAULT_NOTIFY, handleFaultNotify);
  
  // TODO: Add more handlers as needed:
  // - IPC_MSG_INDEX_SYNC_DATA
  // - IPC_MSG_DEVICE_STATUS
  // - IPC_MSG_CONTROL_ACK
  // etc.

  log(LOG_INFO, false, "IPC message handlers registered.\n");
}