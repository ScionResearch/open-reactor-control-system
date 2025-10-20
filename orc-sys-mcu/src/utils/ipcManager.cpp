#include "ipcManager.h"
#include "network/mqttManager.h" // For publishSensorData
#include "statusManager.h" // For status struct
#include "objectCache.h" // For caching sensor data
#include "config/ioConfig.h" // For IO configuration push

// Inter-processor communication
void init_ipcManager(void) {
  Serial1.setRX(PIN_SI_RX);
  Serial1.setTX(PIN_SI_TX);
  
  // Set UART FIFO size to 8192 bytes to accommodate multiple IPC packets from bulk requests
  // With bulk requests of 31 objects (sensors + outputs), we need room for 31 responses
  // Each response: ~150 bytes * 31 = 4650 bytes + overhead
  // Max packet size with byte stuffing: ~260 bytes raw data * 2 (worst case stuffing) = 520 bytes
  Serial1.setFIFOSize(8192);
  
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

// Continuous sensor polling for cache population
unsigned long lastSensorPollTime = 0;
const unsigned long SENSOR_POLL_INTERVAL = 1000; // Poll every 1 second
bool ipcReady = false;  // Only start polling after handshake and config push complete

/**
 * @brief Continuously poll all IO MCU objects to keep cache fresh
 * This ensures data is always available for:
 * - Web UI (any tab)
 * - MQTT publishing
 * - Control loops
 * - Data logging
 */
void pollSensors(void) {
  // Wait for IPC handshake and configuration to complete
  if (!ipcReady) return;
  
  unsigned long now = millis();
  if (now - lastSensorPollTime < SENSOR_POLL_INTERVAL) return;
  lastSensorPollTime = now;
  
  // Request all IO MCU objects (indices 0-30)
  // Sensors: ADC (0-7), DAC (8-9), RTD (10-12), GPIO (13-20)
  // Outputs: Digital Outputs (21-25), Stepper (26), DC Motors (27-30)
  objectCache.requestBulkUpdate(0, 31);
  
  //log(LOG_DEBUG, false, "Polling objects: Requested bulk update for indices 0-30 (sensors + outputs)\n");
}

void manageIPC(void) {
  // Check for incoming messages
  ipc.update();
  
  // Continuously poll sensors to keep cache fresh
  pollSensors();
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
  // log(LOG_DEBUG, false, "IPC RX: Sensor[%d] = %.2f %s%s\n", 
  //     sensorData->index, sensorData->value, sensorData->unit,
  //     (sensorData->flags & IPC_SENSOR_FLAG_FAULT) ? " [FAULT]" : "");
  
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
  //log(LOG_DEBUG, false, "IPC: Received PING from SAME51, sending PONG\n");
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
  
  // Push IO configuration to IO MCU now that IPC is established
  pushIOConfigToIOmcu();
  
  // Enable sensor polling now that IPC is ready
  ipcReady = true;
  log(LOG_INFO, false, "IPC: Sensor polling enabled\n");
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
  
  // Control acknowledgments
  ipc.registerHandler(IPC_MSG_CONTROL_ACK, handleControlAck);
  
  // TODO: Add more handlers as needed:
  // - IPC_MSG_INDEX_SYNC_DATA
  // - IPC_MSG_DEVICE_STATUS
  // etc.

  log(LOG_INFO, false, "IPC message handlers registered.\n");
}

/**
 * @brief Handler for control acknowledgment messages from IO MCU
 */
void handleControlAck(uint8_t messageType, const uint8_t *payload, uint16_t length) {
  if (payload == nullptr || length != sizeof(IPC_ControlAck_t)) {
    log(LOG_ERROR, false, "IPC: Invalid control ACK payload\n");
    return;
  }
  
  const IPC_ControlAck_t *ack = (const IPC_ControlAck_t *)payload;
  
  if (ack->success) {
    log(LOG_DEBUG, false, "IPC: Control command ACK for object %d: %s\n", 
        ack->index, ack->message);
  } else {
    log(LOG_WARNING, false, "IPC: Control command FAILED for object %d (error %d): %s\n",
        ack->index, ack->errorCode, ack->message);
  }
  
  // TODO: Could store last error in a status structure for API queries
}

// ============================================================================
// OUTPUT CONTROL COMMAND SENDERS
// ============================================================================

/**
 * @brief Send digital output control command
 * @param index Output index (21-25)
 * @param command DigitalOutputCommand (SET_STATE, SET_PWM, DISABLE)
 * @param state Output state (for SET_STATE)
 * @param pwmDuty PWM duty cycle 0-100% (for SET_PWM)
 * @return true if command was queued
 */
bool sendDigitalOutputCommand(uint16_t index, uint8_t command, bool state, float pwmDuty) {
  IPC_DigitalOutputControl_t cmd;
  memset(&cmd, 0, sizeof(cmd));  // Zero out structure including padding
  cmd.index = index;
  cmd.objectType = OBJ_T_DIGITAL_OUTPUT;
  cmd.command = command;
  cmd.state = state ? 1 : 0;  // Convert bool to uint8_t
  cmd.pwmDuty = pwmDuty;
  
  bool sent = ipc.sendPacket(IPC_MSG_CONTROL_WRITE, (uint8_t*)&cmd, sizeof(cmd));
  
  if (sent) {
    log(LOG_DEBUG, false, "IPC TX: DigitalOutput[%d] command=%d (size=%d)\n", index, command, sizeof(cmd));
  } else {
    log(LOG_WARNING, false, "IPC TX: Failed to send DigitalOutput command (queue full)\n");
  }
  
  return sent;
}

/**
 * @brief Send stepper motor control command
 * @param command StepperCommand (SET_RPM, SET_DIR, START, STOP, UPDATE)
 * @param rpm Target RPM
 * @param direction Motor direction (true=forward, false=reverse)
 * @return true if command was queued
 */
bool sendStepperCommand(uint8_t command, float rpm, bool direction) {
  IPC_StepperControl_t cmd;
  memset(&cmd, 0, sizeof(cmd));  // Zero out structure including padding
  cmd.index = 26;
  cmd.objectType = OBJ_T_STEPPER_MOTOR;
  cmd.command = command;
  cmd.rpm = rpm;
  cmd.direction = direction;
  cmd.enable = (command == STEPPER_CMD_START);
  
  bool sent = ipc.sendPacket(IPC_MSG_CONTROL_WRITE, (uint8_t*)&cmd, sizeof(cmd));
  
  if (sent) {
    log(LOG_DEBUG, false, "IPC TX: Stepper command=%d, rpm=%.1f, dir=%d\n", 
        command, rpm, direction);
  } else {
    log(LOG_WARNING, false, "IPC TX: Failed to send Stepper command (queue full)\n");
  }
  
  return sent;
}

/**
 * @brief Send DC motor control command
 * @param index Motor index (27-30)
 * @param command DCMotorCommand (SET_POWER, SET_DIR, START, STOP, UPDATE)
 * @param power Power percentage 0-100%
 * @param direction Motor direction (true=forward, false=reverse)
 * @return true if command was queued
 */
bool sendDCMotorCommand(uint16_t index, uint8_t command, float power, bool direction) {
  IPC_DCMotorControl_t cmd;
  memset(&cmd, 0, sizeof(cmd));  // Zero out structure including padding
  cmd.index = index;
  cmd.objectType = OBJ_T_BDC_MOTOR;
  cmd.command = command;
  cmd.power = power;
  cmd.direction = direction;
  cmd.enable = (command == DCMOTOR_CMD_START);
  
  bool sent = ipc.sendPacket(IPC_MSG_CONTROL_WRITE, (uint8_t*)&cmd, sizeof(cmd));
  
  if (sent) {
    log(LOG_DEBUG, false, "IPC TX: DCMotor[%d] command=%d, power=%.1f%%, dir=%d\n",
        index, command, power, direction);
  } else {
    log(LOG_WARNING, false, "IPC TX: Failed to send DCMotor command (queue full)\n");
  }
  
  return sent;
}