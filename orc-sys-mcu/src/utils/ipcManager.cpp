#include "ipcManager.h"
#include "network/mqttManager.h" // For publishSensorData
#include "statusManager.h" // For status struct
#include "objectCache.h" // For caching sensor data
#include "config/ioConfig.h" // For IO configuration push

// Global IPC ready flag - moved to top to fix declaration error
bool ipcReady = false;  // Only start polling after handshake and config push complete

// Continuous sensor polling for cache population
unsigned long lastSensorPollTime = 0;
const unsigned long SENSOR_POLL_INTERVAL = 1000; // Poll every 1 second

// ============================================================================
// Transaction ID Management (v2.6)
// ============================================================================

// Transaction ID counter (starts at 1, skips 0 and 0xFFFF)
static uint16_t nextTransactionId = 1;

// Pending transaction tracking
struct PendingTransaction {
    uint16_t transactionId;
    uint8_t requestType;          // IPC_MSG_* that was sent
    uint8_t expectedResponseType; // IPC_MSG_* expected back
    uint16_t expectedResponseCount; // For bulk operations (number of responses expected)
    uint16_t receivedResponseCount; // Responses received so far
    uint8_t startIndex;           // Starting object index for bulk requests
    unsigned long timestamp;      // For timeout detection
};

// Transaction tracking table (max 16 concurrent operations)
#define MAX_PENDING_TRANSACTIONS 16
static PendingTransaction pendingTransactions[MAX_PENDING_TRANSACTIONS];
static uint8_t pendingTxnCount = 0;

// Transaction timeout (5 seconds)
const unsigned long TRANSACTION_TIMEOUT_MS = 5000;

/**
 * @brief Generate a unique transaction ID
 * @return Transaction ID (1-65534, skips 0 and 0xFFFF)
 */
uint16_t generateTransactionId() {
    uint16_t id = nextTransactionId++;
    
    // Skip reserved values (0 = none, 0xFFFF = broadcast)
    if (id == IPC_TXN_NONE || id == IPC_TXN_BROADCAST) {
        id = 1;
        nextTransactionId = 2;
    }
    
    return id;
}

/**
 * @brief Add a pending transaction to the tracking table
 * @param txnId Transaction ID
 * @param reqType Request message type
 * @param respType Expected response message type
 * @param respCount Number of responses expected (1 for single, N for bulk)
 * @param startIdx Starting object index for bulk requests (0 for single)
 * @return true if added successfully, false if table is full
 */
bool addPendingTransaction(uint16_t txnId, uint8_t reqType, uint8_t respType, uint16_t respCount, uint8_t startIdx) {
    if (pendingTxnCount >= MAX_PENDING_TRANSACTIONS) {
        log(LOG_WARNING, false, "[IPC] Transaction table full! Cannot track txn %d\n", txnId);
        return false;
    }
    
    pendingTransactions[pendingTxnCount++] = {
        txnId, reqType, respType, respCount, 0, startIdx, millis()
    };
    
    return true;
}

/**
 * @brief Find a pending transaction by ID
 * @param txnId Transaction ID to find
 * @return Pointer to transaction or nullptr if not found
 */
PendingTransaction* findPendingTransaction(uint16_t txnId) {
    for (uint8_t i = 0; i < pendingTxnCount; i++) {
        if (pendingTransactions[i].transactionId == txnId) {
            return &pendingTransactions[i];
        }
    }
    return nullptr;
}

/**
 * @brief Remove a completed transaction from the tracking table
 * @param txnId Transaction ID to remove
 */
void completePendingTransaction(uint16_t txnId) {
    for (uint8_t i = 0; i < pendingTxnCount; i++) {
        if (pendingTransactions[i].transactionId == txnId) {
            //log(LOG_DEBUG, false, "[IPC] Completed txn %d\n", txnId);
            
            // Shift remaining transactions down
            for (uint8_t j = i; j < pendingTxnCount - 1; j++) {
                pendingTransactions[j] = pendingTransactions[j + 1];
            }
            pendingTxnCount--;
            return;
        }
    }
}

/**
 * @brief Clean up stalled transactions that have timed out
 * Called periodically from manageIPC()
 */
void cleanupStalledTransactions() {
    unsigned long now = millis();
    
    for (uint8_t i = 0; i < pendingTxnCount; ) {
        unsigned long age = now - pendingTransactions[i].timestamp;
        
        if (age > TRANSACTION_TIMEOUT_MS) {
            log(LOG_WARNING, false, "[IPC] Transaction %d timed out after %lu ms (indices %d-%d, received %d/%d)\n",
                pendingTransactions[i].transactionId, age,
                pendingTransactions[i].startIndex,
                pendingTransactions[i].startIndex + pendingTransactions[i].expectedResponseCount - 1,
                pendingTransactions[i].receivedResponseCount,
                pendingTransactions[i].expectedResponseCount);
            
            // Remove timed out transaction
            completePendingTransaction(pendingTransactions[i].transactionId);
            // Don't increment i - array was shifted down
        } else {
            i++;
        }
    }
}

// Inter-processor communication
void init_ipcManager(void) {
  Serial1.setRX(PIN_SI_RX);
  Serial1.setTX(PIN_SI_TX);
  
  Serial1.setFIFOSize(16384);
  
  // Clear object cache to force fresh data from IO MCU
  objectCache.clear();
  log(LOG_INFO, false, "Object cache cleared\n");
  
  ipc.begin(2000000); // 2 Mbps
  
  // Register message handlers
  registerIpcCallbacks();
  
  // Initialize as master - wait for IO MCU HELLO before initiating handshake
  ipcReady = false;
  log(LOG_INFO, false, "IPC master waiting for IO MCU HELLO broadcast\n");
  
  log(LOG_INFO, false, "Inter-processor communication setup complete\n");
  if (statusLocked) return;
  statusLocked = true;
  status.ipcOK = true;
  status.updated = true;
  statusLocked = false;
}

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
  // Rate-limit the warning so it doesn't spam the serial console when IPC is down.
  if (!ipcReady) {
    static unsigned long lastHandshakeWarn = 0;
    unsigned long now = millis();
    if (now - lastHandshakeWarn >= SENSOR_POLL_INTERVAL) {
      log(LOG_WARNING, false, "IPC not ready for polling (handshake incomplete)\n");
      lastHandshakeWarn = now;
    }
    return;
  }
  
  unsigned long now = millis();
  if (now - lastSensorPollTime < SENSOR_POLL_INTERVAL) return;
  lastSensorPollTime = now;
  
  // Request fixed hardware range (0-36: ADC, DAC, RTD, GPIO, outputs, motors, COM ports)
  // Always 37 objects (indices 37-39 are reserved for future use)
  objectCache.requestBulkUpdate(0, 37);
  
  // Request controllers (40-48: temp, pH, flow, DO controllers)
  // Only poll active controllers based on config
  uint8_t controllerRangeSize = getFixedHardwareObjectCount() - 37;  // Returns highest index + 1
  uint8_t controllerExpectedCount = getFixedHardwareExpectedCount() - 37;  // Subtract base hardware
  if (controllerRangeSize > 0) {
    objectCache.requestBulkUpdateSparse(40, controllerRangeSize, controllerExpectedCount);
  }
  
  // Request device control objects (indices 50-69)
  // These provide control status for peripheral devices (setpoint, actual, connected, fault)
  // Control objects are sparse (only even indices used: 50, 52, 54, 56...)
  // Use sparse request to handle mismatch between requested range and expected responses
  uint8_t controlCount = getActiveDeviceControlCount();
  if (controlCount > 0) {
    // Request full device control range (20 indices) but expect only valid responses
    // IO MCU will only respond for actual control objects (e.g., 50, 52, 54, 56)
    objectCache.requestBulkUpdateSparse(50, 20, controlCount);
  }
  
  // Request dynamic device sensor objects (indices 70-99)
  // These are user-created device sensors (Modbus, I2C, etc.)
  // Dynamically count based on active devices from config
  uint8_t sensorCount = getActiveDeviceSensorCount();
  if (sensorCount > 0) {
    objectCache.requestBulkUpdate(70, sensorCount);
  }
  
  //log(LOG_DEBUG, false, "Polling objects: Requested bulk update for indices 0-42 (fixed+controllers), 50-69 (control), and 70-99 (sensors)\n");
}

void manageIPC(void) {
  // Check for incoming messages
  ipc.update();
  
  // Monitor connection health and handle reconnection
  static unsigned long lastConnectionCheck = 0;
  unsigned long now = millis();
  
  if (now - lastConnectionCheck > 1000) {  // Check every second
    lastConnectionCheck = now;
    
    // Clean up stalled transactions
    cleanupStalledTransactions();
    
    // Check if connection has been lost (timeout)
    if (ipcReady) {
      IPC_Statistics_t stats;
      ipc.getStatistics(&stats);
      if (now - stats.lastRxTime > 5000) {
        log(LOG_WARNING, true, "IPC: Connection timeout detected, resetting to disconnected state\n");
        ipcReady = false;
        objectCache.clear();
        // SYS MCU will wait for next IO MCU HELLO broadcast
      }
    }
  }
  
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
  
  // DEBUG: Log every received sensor packet with MCU timestamp
  static uint32_t rxPacketCount = 0;
  rxPacketCount++;
  /*log(LOG_INFO, false, "[IPC-RX] PKT#%lu: INDEX=%u, TXN=%u, TIME=%lu us\n", 
      rxPacketCount, sensorData->index, sensorData->transactionId, micros());*/
  
  // Validate transaction ID
  PendingTransaction* txn = findPendingTransaction(sensorData->transactionId);
  if (txn == nullptr) {
    // Unknown transaction - could be from old request, timeout, or count mismatch
    // This is normal for bulk reads if count doesn't match (e.g., sparse objects)
    // Don't log to avoid spam - just process the data
    // Still process the data - it's valid sensor information
  } else if (txn->expectedResponseType != IPC_MSG_SENSOR_DATA) {
    // Wrong response type for this transaction
    log(LOG_WARNING, false, "[IPC] Txn %d expected msg type %02X but got SENSOR_DATA\n",
        sensorData->transactionId, txn->expectedResponseType);
    // Don't return - still process valid sensor data
  } else {
    // Valid transaction - increment received count
    txn->receivedResponseCount++;
    
    // If all expected responses received, complete the transaction
    if (txn->receivedResponseCount >= txn->expectedResponseCount) {
      completePendingTransaction(sensorData->transactionId);
    }
  }
  
  // DEBUG: Print received sensor data (simplified for performance)
  // log(LOG_DEBUG, false, "IPC RX: Sensor[%d] = %.2f %s%s\n", 
  //     sensorData->index, sensorData->value, sensorData->unit,
  //     (sensorData->flags & IPC_SENSOR_FLAG_FAULT) ? " [FAULT]" : "");
  
  // DEBUG: Energy sensors (31-32) - show multi-value data
  /*if (sensorData->index >= 31 && sensorData->index <= 32 && sensorData->valueCount >= 2) {
    log(LOG_INFO, false, "[ENERGY] Sensor[%d]: %.2f %s, %.3f %s, %.2f %s\n",
        sensorData->index,
        sensorData->value, sensorData->unit,
        sensorData->additionalValues[0], sensorData->additionalUnits[0],
        sensorData->additionalValues[1], sensorData->additionalUnits[1]);
  }*/
  
  // Update object cache
  objectCache.updateObject(sensorData);
  
  // Forward to MQTT
  publishSensorDataIPC(sensorData);
  
  // TODO: Update status struct based on object type
  // This will be implemented when the status manager is updated for the new IPC protocol
}

/**
 * @brief Handler for PING messages
 * Only respond if handshake is complete to allow IO MCU timeout detection
 */
void handlePing(uint8_t messageType, const uint8_t *payload, uint16_t length) {
  // Only respond to PING if handshake is complete
  // If we respond before handshake, IO MCU won't timeout and restart HELLO broadcasts
  if (ipcReady) {
    ipc.sendPong();
  }
  // Otherwise, ignore PING to force IO MCU timeout and handshake restart
}

/**
 * @brief Handler for PONG messages
 */
void handlePong(uint8_t messageType, const uint8_t *payload, uint16_t length) {
  // Connection is alive - no need to log every keepalive
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
  
  // Clear object cache for fresh start
  objectCache.clear();
  log(LOG_INFO, false, "IPC: Object cache cleared for fresh start\n");
  
  // Push IO configuration to IO MCU before enabling polling
  // This ensures IO MCU always has current config after any reboot
  pushIOConfigToIOmcu();
  log(LOG_INFO, false, "IPC: Configuration pushed to IO MCU\n");
  
  // Wait for IO MCU to finish processing config messages before starting polling
  // This prevents sensor polling from colliding with config message processing
  delay(200);  // 200ms should be sufficient for config processing
  log(LOG_DEBUG, false, "IPC: Config processing delay complete\n");
  
  // Enable sensor polling now that handshake and config push are complete
  ipcReady = true;
  log(LOG_INFO, false, "IPC: Handshake complete, connection established\n");
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
  
  // Clear object cache and push fresh configuration on every handshake
  // This ensures IO MCU always has current config after any reboot
  objectCache.clear();
  log(LOG_INFO, false, "IPC: Object cache cleared for fresh start\n");
  
  // Push IO configuration to IO MCU now that IPC is established
  pushIOConfigToIOmcu();
  log(LOG_INFO, false, "IPC: Configuration pushed to IO MCU\n");
  
  // Enable sensor polling now that IPC is ready
  ipcReady = true;
  log(LOG_INFO, false, "IPC: Sensor polling enabled - system fully operational\n");
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
  
  // Device management
  ipc.registerHandler(IPC_MSG_DEVICE_STATUS, handleDeviceStatus);
  
  // Index synchronization
  ipc.registerHandler(IPC_MSG_INDEX_SYNC_DATA, handleIndexSyncData);

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
  
  // Validate transaction ID
  PendingTransaction* txn = findPendingTransaction(ack->transactionId);
  if (txn == nullptr) {
    // Unknown transaction - could be from old request or unsolicited ACK
    log(LOG_DEBUG, false, "[IPC] Received CONTROL_ACK with unknown txn %d (object %d)\n", 
        ack->transactionId, ack->index);
    // Still process the ACK - it's valid acknowledgment
  } else if (txn->expectedResponseType != IPC_MSG_CONTROL_ACK) {
    // Wrong response type for this transaction
    log(LOG_WARNING, false, "[IPC] Txn %d expected msg type %02X but got CONTROL_ACK\n",
        ack->transactionId, txn->expectedResponseType);
  } else {
    // Valid transaction - increment received count and complete
    txn->receivedResponseCount++;
    
    if (txn->receivedResponseCount >= txn->expectedResponseCount) {
      completePendingTransaction(ack->transactionId);
    }
  }
  
  if (ack->success) {
    log(LOG_DEBUG, false, "IPC: Control ACK for object %d (txn %d): %s\n", 
        ack->index, ack->transactionId, ack->message);
  } else {
    log(LOG_WARNING, false, "IPC: Control FAILED for object %d (txn %d, error %d): %s\n",
        ack->index, ack->transactionId, ack->errorCode, ack->message);
  }
  
  // TODO: Could store last error in a status structure for API queries
} // <--- Added closing brace here

/**
 * @brief Handler for device status messages from IO MCU
 */
void handleDeviceStatus(uint8_t messageType, const uint8_t *payload, uint16_t length) {
  if (payload == nullptr || length != sizeof(IPC_DeviceStatus_t)) {
    log(LOG_ERROR, false, "IPC: Invalid device status payload\n");
    return;
  }
  
  const IPC_DeviceStatus_t *status = (const IPC_DeviceStatus_t *)payload;
  
  // Complete pending transaction
  if (status->transactionId > 0) {
    completePendingTransaction(status->transactionId);
  }
  
  if (status->active && !status->fault) {
    log(LOG_INFO, true, "IPC: Device at index %d is ACTIVE with %d sensors: %s\n",
        status->startIndex, status->objectCount, status->message);
    
    // Log sensor indices
    if (status->objectCount > 0) {
      log(LOG_DEBUG, false, "  Sensor indices: ");
      for (uint8_t i = 0; i < status->objectCount && i < 4; i++) {
        log(LOG_DEBUG, false, "%d ", status->sensorIndices[i]);
      }
      log(LOG_DEBUG, false, "\n");
    }
  } else if (status->fault) {
    log(LOG_ERROR, true, "IPC: Device at index %d has FAULT: %s\n",
        status->startIndex, status->message);
  } else {
    log(LOG_INFO, true, "IPC: Device at index %d is INACTIVE: %s\n",
        status->startIndex, status->message);
  }
  
  // TODO: Store device status for API queries
  // TODO: Trigger sensor cache refresh for the affected indices
}

/**
 * @brief Handler for index synchronization data messages from IO MCU
 */
void handleIndexSyncData(uint8_t messageType, const uint8_t *payload, uint16_t length) {
  if (payload == nullptr) {
    log(LOG_ERROR, false, "IPC: Invalid index sync data payload (null)\n");
    return;
  }
  
  // Basic validation - check minimum packet size
  if (length < (sizeof(uint16_t) * 2 + sizeof(uint8_t))) {
    log(LOG_ERROR, false, "IPC: Index sync data payload too small (%d bytes)\n", length);
    return;
  }
  
  const IPC_IndexSync_t *syncData = (const IPC_IndexSync_t *)payload;
  
  log(LOG_INFO, true, "IPC: Received index sync data packet %d/%d with %d entries\n",
      syncData->packetNum, syncData->totalPackets, syncData->entryCount);
  
  // For now, just log the sync data - in the future this could be used to
  // build a dynamic object index or validate the expected objects
  for (uint8_t i = 0; i < syncData->entryCount; i++) {
    const IPC_IndexEntry_t *entry = &syncData->entries[i];
    log(LOG_DEBUG, false, "  [%d] %s (type=%d, flags=0x%02X)\n",
        entry->index, entry->name, entry->objectType, entry->flags);
  }
  
  // If this is the last packet, acknowledge sync completion
  if (syncData->packetNum == (syncData->totalPackets - 1)) {
    log(LOG_INFO, false, "IPC: Index synchronization complete\n");
  }
}

// ... (rest of the code remains the same)

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
  cmd.transactionId = generateTransactionId();
  cmd.index = index;
  cmd.objectType = OBJ_T_DIGITAL_OUTPUT;
  cmd.command = command;
  cmd.state = state ? 1 : 0;  // Convert bool to uint8_t
  cmd.pwmDuty = pwmDuty;
  
  bool sent = ipc.sendPacket(IPC_MSG_CONTROL_WRITE, (uint8_t*)&cmd, sizeof(cmd));
  
  if (sent) {
    addPendingTransaction(cmd.transactionId, IPC_MSG_CONTROL_WRITE, IPC_MSG_CONTROL_ACK, 1, index);
    log(LOG_DEBUG, false, "IPC TX: DigitalOutput[%d] command=%d (txn=%d)\n", index, command, cmd.transactionId);
  } else {
    log(LOG_WARNING, false, "IPC TX: Failed to send DigitalOutput command (queue full)\n");
  }
  
  return sent;
}

/**
 * @brief Send analog output (DAC) control command
 * @param index Analog output index (8-9)
 * @param command AnalogOutputCommand (SET_VALUE, DISABLE)
 * @param value Output value in mV (0-10240)
 * @return true if command was queued
 */
bool sendAnalogOutputCommand(uint16_t index, uint8_t command, float value) {
  IPC_AnalogOutputControl_t cmd;
  memset(&cmd, 0, sizeof(cmd));  // Zero out structure including padding
  cmd.transactionId = generateTransactionId();
  cmd.index = index;
  cmd.objectType = OBJ_T_ANALOG_OUTPUT;
  cmd.command = command;
  cmd.value = value;
  
  bool sent = ipc.sendPacket(IPC_MSG_CONTROL_WRITE, (uint8_t*)&cmd, sizeof(cmd));
  
  if (sent) {
    addPendingTransaction(cmd.transactionId, IPC_MSG_CONTROL_WRITE, IPC_MSG_CONTROL_ACK, 1, index);
    log(LOG_DEBUG, false, "IPC TX: AnalogOutput[%d] command=%d, value=%.1f mV (txn=%d)\n", 
        index, command, value, cmd.transactionId);
  } else {
    log(LOG_WARNING, false, "IPC TX: Failed to send AnalogOutput command (queue full)\n");
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
  cmd.transactionId = generateTransactionId();
  cmd.index = 26;
  cmd.objectType = OBJ_T_STEPPER_MOTOR;
  cmd.command = command;
  cmd.rpm = rpm;
  cmd.direction = direction;
  cmd.enable = (command == STEPPER_CMD_START);
  
  bool sent = ipc.sendPacket(IPC_MSG_CONTROL_WRITE, (uint8_t*)&cmd, sizeof(cmd));
  
  if (sent) {
    addPendingTransaction(cmd.transactionId, IPC_MSG_CONTROL_WRITE, IPC_MSG_CONTROL_ACK, 1, 26);
    log(LOG_DEBUG, false, "IPC TX: Stepper command=%d, rpm=%.1f, dir=%d (txn=%d)\n", 
        command, rpm, direction, cmd.transactionId);
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
  cmd.transactionId = generateTransactionId();
  cmd.index = index;
  cmd.objectType = OBJ_T_BDC_MOTOR;
  cmd.command = command;
  cmd.power = power;
  cmd.direction = direction;
  cmd.enable = (command == DCMOTOR_CMD_START);
  
  bool sent = ipc.sendPacket(IPC_MSG_CONTROL_WRITE, (uint8_t*)&cmd, sizeof(cmd));
  
  if (sent) {
    addPendingTransaction(cmd.transactionId, IPC_MSG_CONTROL_WRITE, IPC_MSG_CONTROL_ACK, 1, index);
    log(LOG_DEBUG, false, "IPC TX: DCMotor[%d] command=%d, power=%.1f%%, dir=%d (txn=%d)\n",
        index, command, power, direction, cmd.transactionId);
  } else {
    log(LOG_WARNING, false, "IPC TX: Failed to send DCMotor command (queue full)\n");
  }
  
  return sent;
}

// ============================================================================
// DEVICE MANAGEMENT COMMAND SENDERS
// ============================================================================

/**
 * @brief Send device create command to IO MCU
 * @param startIndex Preferred starting index (60-79)
 * @param config Device configuration
 * @return true if command was queued
 */
bool sendDeviceCreateCommand(uint8_t startIndex, const IPC_DeviceConfig_t* config) {
  IPC_DeviceCreate_t cmd;
  cmd.transactionId = generateTransactionId();
  cmd.startIndex = startIndex;
  memcpy(&cmd.config, config, sizeof(IPC_DeviceConfig_t));
  
  bool sent = ipc.sendPacket(IPC_MSG_DEVICE_CREATE, (uint8_t*)&cmd, sizeof(cmd));
  
  if (sent) {
    addPendingTransaction(cmd.transactionId, IPC_MSG_DEVICE_CREATE, IPC_MSG_DEVICE_STATUS, 1, startIndex);
    log(LOG_INFO, true, "IPC TX: Create device at index %d (type=%d, txn=%d)\n", 
        startIndex, config->deviceType, cmd.transactionId);
  } else {
    log(LOG_WARNING, false, "IPC TX: Failed to send device create command\n");
  }
  
  return sent;
}

/**
 * @brief Send device delete command to IO MCU
 * @param startIndex Device starting index to delete (60-79)
 * @return true if command was queued
 */
bool sendDeviceDeleteCommand(uint8_t startIndex) {
  IPC_DeviceDelete_t cmd;
  cmd.transactionId = generateTransactionId();
  cmd.startIndex = startIndex;
  
  bool sent = ipc.sendPacket(IPC_MSG_DEVICE_DELETE, (uint8_t*)&cmd, sizeof(cmd));
  
  if (sent) {
    addPendingTransaction(cmd.transactionId, IPC_MSG_DEVICE_DELETE, IPC_MSG_DEVICE_STATUS, 1, startIndex);
    log(LOG_INFO, true, "IPC TX: Delete device at index %d (txn=%d)\n", startIndex, cmd.transactionId);
  } else {
    log(LOG_WARNING, false, "IPC TX: Failed to send device delete command\n");
  }
  
  return sent;
}

/**
 * @brief Send device configuration update to IO MCU
 * @param startIndex Device start index (70-99)
 * @param config Device configuration
 * @return true if command was queued
 */
bool sendDeviceConfigCommand(uint8_t startIndex, const IPC_DeviceConfig_t* config) {
  IPC_DeviceConfigUpdate_t update;
  update.transactionId = generateTransactionId();
  update.startIndex = startIndex;
  memcpy(&update.config, config, sizeof(IPC_DeviceConfig_t));
  
  bool sent = ipc.sendPacket(IPC_MSG_DEVICE_CONFIG, (uint8_t*)&update, sizeof(IPC_DeviceConfigUpdate_t));
  
  if (sent) {
    addPendingTransaction(update.transactionId, IPC_MSG_DEVICE_CONFIG, IPC_MSG_DEVICE_STATUS, 1, startIndex);
    log(LOG_INFO, true, "IPC TX: Update device config (index=%d, type=%d, txn=%d)\n", 
        startIndex, config->deviceType, update.transactionId);
  } else {
    log(LOG_WARNING, false, "IPC TX: Failed to send device config command\n");
  }
  
  return sent;
}

/**
 * @brief Send device query command to IO MCU
 * @param startIndex Device starting index to query (60-79)
 * @return true if command was queued
 */
bool sendDeviceQueryCommand(uint8_t startIndex) {
  IPC_DeviceQuery_t cmd;
  cmd.startIndex = startIndex;
  
  bool sent = ipc.sendPacket(IPC_MSG_DEVICE_QUERY, (uint8_t*)&cmd, sizeof(cmd));
  
  if (sent) {
    log(LOG_DEBUG, false, "IPC TX: Query device at index %d\n", startIndex);
  } else {
    log(LOG_WARNING, false, "IPC TX: Failed to send device query command\n");
  }
  
  return sent;
}