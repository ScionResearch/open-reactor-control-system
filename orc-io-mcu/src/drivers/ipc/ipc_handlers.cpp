#include "drv_ipc.h"
#include "../../sys_init.h"

// ============================================================================
// MESSAGE HANDLER DISPATCHER
// ============================================================================

void ipc_handleMessage(uint8_t msgType, const uint8_t *payload, uint16_t len) {
    // Message dispatcher (debug logging in individual handlers)
    
    switch (msgType) {
        case IPC_MSG_PING:
            ipc_handle_ping(payload, len);
            break;
            
        case IPC_MSG_PONG:
            ipc_handle_pong(payload, len);
            break;
            
        case IPC_MSG_HELLO:
            ipc_handle_hello(payload, len);
            break;
            
        case IPC_MSG_INDEX_SYNC_REQ:
            ipc_handle_index_sync_req(payload, len);
            break;
            
        case IPC_MSG_SENSOR_READ_REQ:
            ipc_handle_sensor_read_req(payload, len);
            break;
            
        case IPC_MSG_CONTROL_WRITE:
            ipc_handle_control_write(payload, len);
            break;
            
        case IPC_MSG_CONTROL_READ:
            ipc_handle_control_read(payload, len);
            break;
            
        case IPC_MSG_DEVICE_CREATE:
            ipc_handle_device_create(payload, len);
            break;
            
        case IPC_MSG_DEVICE_DELETE:
            ipc_handle_device_delete(payload, len);
            break;
            
        case IPC_MSG_CONFIG_WRITE:
            ipc_handle_config_write(payload, len);
            break;
            
        case IPC_MSG_CALIBRATE:
            ipc_handle_calibrate(payload, len);
            break;
            
        default:
            // Unknown message type
            ipc_sendError(IPC_ERR_INVALID_MSG, "Unknown message type");
            break;
    }
}

// ============================================================================
// HANDSHAKE & STATUS HANDLERS
// ============================================================================

void ipc_handle_ping(const uint8_t *payload, uint16_t len) {
    #if IPC_DEBUG_ENABLED
    Serial.println("[IPC] Received PING, sending PONG");
    #endif
    // Respond with PONG
    ipc_sendPong();
    ipcDriver.connected = true;
    ipcDriver.lastActivity = millis();
}

void ipc_handle_pong(const uint8_t *payload, uint16_t len) {
    #if IPC_DEBUG_ENABLED
    Serial.println("[IPC] Received PONG");
    #endif
    // PONG received, connection is alive
    ipcDriver.connected = true;
    ipcDriver.lastActivity = millis();
}

void ipc_handle_hello(const uint8_t *payload, uint16_t len) {
    if (len < sizeof(IPC_Hello_t)) {
        ipc_sendError(IPC_ERR_PARSE_FAIL, "HELLO: Invalid payload size");
        return;
    }
    
    IPC_Hello_t *hello = (IPC_Hello_t*)payload;
    
    Serial.printf("[IPC] Received HELLO from %s (protocol v%08lX, firmware v%08lX)\n",
                 hello->deviceName, hello->protocolVersion, hello->firmwareVersion);
    
    // Check protocol version compatibility
    if (hello->protocolVersion != IPC_PROTOCOL_VERSION) {
        Serial.printf("[IPC] ERROR: Protocol version mismatch! Expected 0x%08lX, got 0x%08lX\n",
                     IPC_PROTOCOL_VERSION, hello->protocolVersion);
        ipc_sendError(IPC_ERR_NOT_IMPLEMENTED, "Protocol version mismatch");
        return;
    }
    
    // Send HELLO_ACK
    IPC_HelloAck_t ack;
    ack.protocolVersion = IPC_PROTOCOL_VERSION;
    ack.firmwareVersion = 0x00010000;  // v1.0.0
    ack.maxObjectCount = MAX_NUM_OBJECTS;
    ack.currentObjectCount = numObjects;
    
    ipc_sendPacket(IPC_MSG_HELLO_ACK, (uint8_t*)&ack, sizeof(ack));
    
    Serial.printf("[IPC] ✓ Handshake complete! Sent HELLO_ACK (%u/%u objects)\n",
                 numObjects, MAX_NUM_OBJECTS);
    
    ipcDriver.connected = true;
}

// ============================================================================
// OBJECT INDEX HANDLERS
// ============================================================================

void ipc_handle_index_sync_req(const uint8_t *payload, uint16_t len) {
    // Send complete object index synchronization
    ipc_sendIndexSync();
}

bool ipc_sendIndexSync(void) {
    const uint8_t entriesPerPacket = 10;
    uint16_t totalPackets = (numObjects + entriesPerPacket - 1) / entriesPerPacket;
    uint16_t packetNum = 0;
    
    for (uint16_t i = 0; i < numObjects; i += entriesPerPacket) {
        IPC_IndexSync_t syncData;
        syncData.packetNum = packetNum;
        syncData.totalPackets = totalPackets;
        syncData.entryCount = 0;
        
        // Fill entries for this packet
        for (uint8_t j = 0; j < entriesPerPacket && (i + j) < numObjects; j++) {
            ObjectIndex_t *obj = &objIndex[i + j];
            
            if (!obj->valid) continue;  // Skip invalid entries
            
            IPC_IndexEntry_t *entry = &syncData.entries[syncData.entryCount];
            entry->index = i + j;
            entry->objectType = obj->type;
            entry->flags = IPC_INDEX_FLAG_VALID;
            
            // Mark fixed indices (0-39)
            if ((i + j) < 40) {
                entry->flags |= IPC_INDEX_FLAG_FIXED;
            }
            
            strncpy(entry->name, obj->name, sizeof(entry->name) - 1);
            entry->name[sizeof(entry->name) - 1] = '\0';
            
            // Try to extract unit from sensor object
            memset(entry->unit, 0, sizeof(entry->unit));
            if (obj->obj != nullptr) {
                switch (obj->type) {
                    case OBJ_T_ANALOG_INPUT: {
                        AnalogInput_t *sensor = (AnalogInput_t*)obj->obj;
                        strncpy(entry->unit, sensor->unit, sizeof(entry->unit) - 1);
                        break;
                    }
                    case OBJ_T_TEMPERATURE_SENSOR: {
                        TemperatureSensor_t *sensor = (TemperatureSensor_t*)obj->obj;
                        strncpy(entry->unit, sensor->unit, sizeof(entry->unit) - 1);
                        break;
                    }
                    case OBJ_T_PH_SENSOR: {
                        PhSensor_t *sensor = (PhSensor_t*)obj->obj;
                        strncpy(entry->unit, sensor->unit, sizeof(entry->unit) - 1);
                        break;
                    }
                    case OBJ_T_DISSOLVED_OXYGEN_SENSOR: {
                        DissolvedOxygenSensor_t *sensor = (DissolvedOxygenSensor_t*)obj->obj;
                        strncpy(entry->unit, sensor->unit, sizeof(entry->unit) - 1);
                        break;
                    }
                    case OBJ_T_OPTICAL_DENSITY_SENSOR: {
                        OpticalDensitySensor_t *sensor = (OpticalDensitySensor_t*)obj->obj;
                        strncpy(entry->unit, sensor->unit, sizeof(entry->unit) - 1);
                        break;
                    }
                    case OBJ_T_FLOW_SENSOR: {
                        FlowSensor_t *sensor = (FlowSensor_t*)obj->obj;
                        strncpy(entry->unit, sensor->unit, sizeof(entry->unit) - 1);
                        break;
                    }
                    case OBJ_T_PRESSURE_SENSOR: {
                        PressureSensor_t *sensor = (PressureSensor_t*)obj->obj;
                        strncpy(entry->unit, sensor->unit, sizeof(entry->unit) - 1);
                        break;
                    }
                    case OBJ_T_ANALOG_OUTPUT: {
                        AnalogOutput_t *output = (AnalogOutput_t*)obj->obj;
                        strncpy(entry->unit, output->unit, sizeof(entry->unit) - 1);
                        break;
                    }
                    default:
                        break;
                }
            }
            
            syncData.entryCount++;
        }
        
        // Send packet if it has entries
        if (syncData.entryCount > 0) {
            // Calculate actual size based on entry count
            uint16_t payloadSize = sizeof(uint16_t) * 2 + sizeof(uint8_t) + 
                                   (syncData.entryCount * sizeof(IPC_IndexEntry_t));
            ipc_sendPacket(IPC_MSG_INDEX_SYNC_DATA, (uint8_t*)&syncData, payloadSize);
        }
        
        packetNum++;
    }
    
    return true;
}

bool ipc_sendIndexAdd(uint16_t index) {
    if (index >= MAX_NUM_OBJECTS || !objIndex[index].valid) {
        return false;
    }
    
    IPC_IndexAdd_t add;
    add.index = index;
    add.objectType = objIndex[index].type;
    add.flags = IPC_INDEX_FLAG_VALID;
    
    if (index < 40) {
        add.flags |= IPC_INDEX_FLAG_FIXED;
    }
    
    strncpy(add.name, objIndex[index].name, sizeof(add.name) - 1);
    add.name[sizeof(add.name) - 1] = '\0';
    
    memset(add.unit, 0, sizeof(add.unit));
    // TODO: Extract unit from sensor object (similar to sync)
    
    return ipc_sendPacket(IPC_MSG_INDEX_ADD, (uint8_t*)&add, sizeof(add));
}

bool ipc_sendIndexRemove(uint16_t index) {
    if (index >= MAX_NUM_OBJECTS) {
        return false;
    }
    
    IPC_IndexRemove_t remove;
    remove.index = index;
    remove.objectType = objIndex[index].type;
    
    return ipc_sendPacket(IPC_MSG_INDEX_REMOVE, (uint8_t*)&remove, sizeof(remove));
}

// ============================================================================
// SENSOR DATA HANDLERS
// ============================================================================

void ipc_handle_sensor_read_req(const uint8_t *payload, uint16_t len) {
    if (len < sizeof(IPC_SensorReadReq_t)) {
        Serial.println("[IPC] ERROR: Invalid SENSOR_READ_REQ size");
        ipc_sendError(IPC_ERR_PARSE_FAIL, "SENSOR_READ_REQ: Invalid payload size");
        return;
    }
    
    IPC_SensorReadReq_t *req = (IPC_SensorReadReq_t*)payload;
    
    // Send sensor data (logging in sendSensorData)
    if (!ipc_sendSensorData(req->index)) {
        Serial.printf("[IPC] ERROR: Failed to read sensor %d\n", req->index);
        ipc_sendError(IPC_ERR_INDEX_INVALID, "Invalid sensor index");
    }
}

bool ipc_sendSensorData(uint16_t index) {
    if (index >= MAX_NUM_OBJECTS) {
        Serial.printf("[IPC] Index %d out of range (max %d)\n", index, MAX_NUM_OBJECTS);
        return false;
    }
    
    if (!objIndex[index].valid) {
        return false;
    }
    
    IPC_SensorData_t data;
    memset(&data, 0, sizeof(data));
    
    data.index = index;
    data.objectType = objIndex[index].type;
    data.flags = 0;
    data.timestamp = 0;  // RP2040 will add timestamp
    
    void *obj = objIndex[index].obj;
    if (obj == nullptr) {
        return false;
    }
    
    // Extract sensor data based on type
    switch (objIndex[index].type) {
        case OBJ_T_ANALOG_INPUT: {
            AnalogInput_t *sensor = (AnalogInput_t*)obj;
            data.value = sensor->value;
            strncpy(data.unit, sensor->unit, sizeof(data.unit) - 1);
            if (sensor->fault) data.flags |= IPC_SENSOR_FLAG_FAULT;
            if (sensor->newMessage) {
                data.flags |= IPC_SENSOR_FLAG_NEW_MSG;
                strncpy(data.message, sensor->message, sizeof(data.message) - 1);
            }
            break;
        }
        
        case OBJ_T_TEMPERATURE_SENSOR: {
            TemperatureSensor_t *sensor = (TemperatureSensor_t*)obj;
            data.value = sensor->temperature;
            strncpy(data.unit, sensor->unit, sizeof(data.unit) - 1);
            if (sensor->fault) data.flags |= IPC_SENSOR_FLAG_FAULT;
            if (sensor->newMessage) {
                data.flags |= IPC_SENSOR_FLAG_NEW_MSG;
                strncpy(data.message, sensor->message, sizeof(data.message) - 1);
            }
            break;
        }
        
        case OBJ_T_PH_SENSOR: {
            PhSensor_t *sensor = (PhSensor_t*)obj;
            data.value = sensor->ph;
            strncpy(data.unit, sensor->unit, sizeof(data.unit) - 1);
            if (sensor->fault) data.flags |= IPC_SENSOR_FLAG_FAULT;
            if (sensor->newMessage) {
                data.flags |= IPC_SENSOR_FLAG_NEW_MSG;
                strncpy(data.message, sensor->message, sizeof(data.message) - 1);
            }
            break;
        }
        
        case OBJ_T_DISSOLVED_OXYGEN_SENSOR: {
            DissolvedOxygenSensor_t *sensor = (DissolvedOxygenSensor_t*)obj;
            data.value = sensor->dissolvedOxygen;
            strncpy(data.unit, sensor->unit, sizeof(data.unit) - 1);
            if (sensor->fault) data.flags |= IPC_SENSOR_FLAG_FAULT;
            if (sensor->newMessage) {
                data.flags |= IPC_SENSOR_FLAG_NEW_MSG;
                strncpy(data.message, sensor->message, sizeof(data.message) - 1);
            }
            break;
        }
        
        case OBJ_T_OPTICAL_DENSITY_SENSOR: {
            OpticalDensitySensor_t *sensor = (OpticalDensitySensor_t*)obj;
            data.value = sensor->opticalDensity;
            strncpy(data.unit, sensor->unit, sizeof(data.unit) - 1);
            if (sensor->fault) data.flags |= IPC_SENSOR_FLAG_FAULT;
            if (sensor->newMessage) {
                data.flags |= IPC_SENSOR_FLAG_NEW_MSG;
                strncpy(data.message, sensor->message, sizeof(data.message) - 1);
            }
            break;
        }
        
        case OBJ_T_FLOW_SENSOR: {
            FlowSensor_t *sensor = (FlowSensor_t*)obj;
            data.value = sensor->flow;
            strncpy(data.unit, sensor->unit, sizeof(data.unit) - 1);
            if (sensor->fault) data.flags |= IPC_SENSOR_FLAG_FAULT;
            if (sensor->newMessage) {
                data.flags |= IPC_SENSOR_FLAG_NEW_MSG;
                strncpy(data.message, sensor->message, sizeof(data.message) - 1);
            }
            break;
        }
        
        case OBJ_T_PRESSURE_SENSOR: {
            PressureSensor_t *sensor = (PressureSensor_t*)obj;
            data.value = sensor->pressure;
            strncpy(data.unit, sensor->unit, sizeof(data.unit) - 1);
            if (sensor->fault) data.flags |= IPC_SENSOR_FLAG_FAULT;
            if (sensor->newMessage) {
                data.flags |= IPC_SENSOR_FLAG_NEW_MSG;
                strncpy(data.message, sensor->message, sizeof(data.message) - 1);
            }
            break;
        }
        
        case OBJ_T_ANALOG_OUTPUT: {
            AnalogOutput_t *output = (AnalogOutput_t*)obj;
            data.value = output->value;
            strncpy(data.unit, output->unit, sizeof(data.unit) - 1);
            if (output->fault) data.flags |= IPC_SENSOR_FLAG_FAULT;
            if (output->newMessage) {
                data.flags |= IPC_SENSOR_FLAG_NEW_MSG;
                strncpy(data.message, output->message, sizeof(data.message) - 1);
            }
            break;
        }
        
        case OBJ_T_DIGITAL_OUTPUT: {
            DigitalOutput_t *output = (DigitalOutput_t*)obj;
            data.value = output->state ? 1.0f : 0.0f;
            if (output->pwmEnabled) {
                data.value = output->pwmDuty;  // Return duty cycle if PWM enabled
            }
            strcpy(data.unit, output->pwmEnabled ? "%" : "bool");
            if (output->fault) data.flags |= IPC_SENSOR_FLAG_FAULT;
            if (output->newMessage) {
                data.flags |= IPC_SENSOR_FLAG_NEW_MSG;
                strncpy(data.message, output->message, sizeof(data.message) - 1);
            }
            break;
        }
        
        case OBJ_T_DIGITAL_INPUT: {
            DigitalIO_t *gpio = (DigitalIO_t*)obj;
            data.value = gpio->state ? 1.0f : 0.0f;
            strcpy(data.unit, gpio->output ? "out" : "in");
            if (gpio->fault) data.flags |= IPC_SENSOR_FLAG_FAULT;
            if (gpio->newMessage) {
                data.flags |= IPC_SENSOR_FLAG_NEW_MSG;
                strncpy(data.message, gpio->message, sizeof(data.message) - 1);
            }
            break;
        }
        
        case OBJ_T_STEPPER_MOTOR: {
            StepperDevice_t *stepper = (StepperDevice_t*)obj;
            data.value = stepper->rpm;
            strncpy(data.unit, stepper->unit, sizeof(data.unit) - 1);
            if (stepper->fault) data.flags |= IPC_SENSOR_FLAG_FAULT;
            if (stepper->newMessage) {
                data.flags |= IPC_SENSOR_FLAG_NEW_MSG;
                strncpy(data.message, stepper->message, sizeof(data.message) - 1);
            }
            break;
        }
        
        case OBJ_T_BDC_MOTOR: {
            MotorDevice_t *motor = (MotorDevice_t*)obj;
            data.value = motor->power;
            strncpy(data.unit, motor->unit, sizeof(data.unit) - 1);
            if (motor->fault) data.flags |= IPC_SENSOR_FLAG_FAULT;
            if (motor->newMessage) {
                data.flags |= IPC_SENSOR_FLAG_NEW_MSG;
                strncpy(data.message, motor->message, sizeof(data.message) - 1);
            }
            break;
        }
        
        case OBJ_T_VOLTAGE_SENSOR: {
            VoltageSensor_t *sensor = (VoltageSensor_t*)obj;
            data.value = sensor->voltage;
            strncpy(data.unit, sensor->unit, sizeof(data.unit) - 1);
            if (sensor->fault) data.flags |= IPC_SENSOR_FLAG_FAULT;
            if (sensor->newMessage) {
                data.flags |= IPC_SENSOR_FLAG_NEW_MSG;
                strncpy(data.message, sensor->message, sizeof(data.message) - 1);
            }
            break;
        }
        
        case OBJ_T_CURRENT_SENSOR: {
            CurrentSensor_t *sensor = (CurrentSensor_t*)obj;
            data.value = sensor->current;
            strncpy(data.unit, sensor->unit, sizeof(data.unit) - 1);
            if (sensor->fault) data.flags |= IPC_SENSOR_FLAG_FAULT;
            if (sensor->newMessage) {
                data.flags |= IPC_SENSOR_FLAG_NEW_MSG;
                strncpy(data.message, sensor->message, sizeof(data.message) - 1);
            }
            break;
        }
        
        case OBJ_T_POWER_SENSOR: {
            PowerSensor_t *sensor = (PowerSensor_t*)obj;
            data.value = sensor->power;
            strncpy(data.unit, sensor->unit, sizeof(data.unit) - 1);
            if (sensor->fault) data.flags |= IPC_SENSOR_FLAG_FAULT;
            if (sensor->newMessage) {
                data.flags |= IPC_SENSOR_FLAG_NEW_MSG;
                strncpy(data.message, sensor->message, sizeof(data.message) - 1);
            }
            break;
        }
        
        case OBJ_T_SERIAL_RS232_PORT:
        case OBJ_T_SERIAL_RS485_PORT: {
            SerialCom_t *port = (SerialCom_t*)obj;
            data.value = port->baudRate;  // Return baud rate as primary value
            strcpy(data.unit, "baud");
            if (port->fault) data.flags |= IPC_SENSOR_FLAG_FAULT;
            if (port->newMessage) {
                data.flags |= IPC_SENSOR_FLAG_NEW_MSG;
                strncpy(data.message, port->message, sizeof(data.message) - 1);
            }
            break;
        }
        
        default:
            Serial.printf("[IPC] ERROR: No handler for object type %d\n", objIndex[index].type);
            return false;
    }
    
    bool sent = ipc_sendPacket(IPC_MSG_SENSOR_DATA, (uint8_t*)&data, sizeof(data));
    if (sent) {
        Serial.printf("[IPC] ✓ Sent %s: %.2f %s\n", objIndex[index].name, data.value, data.unit);
    }
    return sent;
}

bool ipc_sendSensorBatch(const uint16_t *indices, uint8_t count) {
    if (count == 0 || count > 20) {
        return false;
    }
    
    IPC_SensorBatch_t batch;
    batch.count = 0;
    
    for (uint8_t i = 0; i < count; i++) {
        uint16_t idx = indices[i];
        if (idx >= MAX_NUM_OBJECTS || !objIndex[idx].valid) {
            continue;
        }
        
        // TODO: Extract value based on type (similar to sendSensorData)
        // For now, skip batch implementation
    }
    
    if (batch.count > 0) {
        uint16_t size = sizeof(uint8_t) + (batch.count * sizeof(IPC_SensorBatchEntry_t));
        return ipc_sendPacket(IPC_MSG_SENSOR_BATCH, (uint8_t*)&batch, size);
    }
    
    return false;
}

// ============================================================================
// CONTROL DATA HANDLERS
// ============================================================================

void ipc_handle_control_write(const uint8_t *payload, uint16_t len) {
    if (len < sizeof(IPC_ControlWrite_t)) {
        ipc_sendError(IPC_ERR_PARSE_FAIL, "CONTROL_WRITE: Invalid payload size");
        return;
    }
    
    IPC_ControlWrite_t *cmd = (IPC_ControlWrite_t*)payload;
    
    // Validate index
    if (cmd->index >= MAX_NUM_OBJECTS || !objIndex[cmd->index].valid) {
        ipc_sendControlAck(cmd->index, false, "Invalid index");
        return;
    }
    
    // Verify type
    if (objIndex[cmd->index].type != cmd->objectType) {
        ipc_sendControlAck(cmd->index, false, "Type mismatch");
        return;
    }
    
    // Handle control write based on type
    // TODO: Implement specific control writes for each type
    ipc_sendControlAck(cmd->index, true, "OK");
}

void ipc_handle_control_read(const uint8_t *payload, uint16_t len) {
    if (len < sizeof(IPC_ControlRead_t)) {
        ipc_sendError(IPC_ERR_PARSE_FAIL, "CONTROL_READ: Invalid payload size");
        return;
    }
    
    IPC_ControlRead_t *cmd = (IPC_ControlRead_t*)payload;
    
    // TODO: Implement control read functionality
    ipc_sendError(IPC_ERR_NOT_IMPLEMENTED, "CONTROL_READ not implemented yet");
}

bool ipc_sendControlAck(uint16_t index, bool success, const char *message) {
    IPC_ControlAck_t ack;
    ack.index = index;
    ack.success = success ? 1 : 0;
    strncpy(ack.message, message, sizeof(ack.message) - 1);
    ack.message[sizeof(ack.message) - 1] = '\0';
    
    return ipc_sendPacket(IPC_MSG_CONTROL_ACK, (uint8_t*)&ack, sizeof(ack));
}

// ============================================================================
// DEVICE MANAGEMENT HANDLERS
// ============================================================================

void ipc_handle_device_create(const uint8_t *payload, uint16_t len) {
    if (len < sizeof(IPC_DeviceCreate_t)) {
        ipc_sendError(IPC_ERR_PARSE_FAIL, "DEVICE_CREATE: Invalid payload size");
        return;
    }
    
    IPC_DeviceCreate_t *cmd = (IPC_DeviceCreate_t*)payload;
    
    // TODO: Implement device creation logic
    // This will involve creating peripheral device instances dynamically
    ipc_sendError(IPC_ERR_NOT_IMPLEMENTED, "DEVICE_CREATE not implemented yet");
}

void ipc_handle_device_delete(const uint8_t *payload, uint16_t len) {
    if (len < sizeof(IPC_DeviceDelete_t)) {
        ipc_sendError(IPC_ERR_PARSE_FAIL, "DEVICE_DELETE: Invalid payload size");
        return;
    }
    
    IPC_DeviceDelete_t *cmd = (IPC_DeviceDelete_t*)payload;
    
    // TODO: Implement device deletion logic
    ipc_sendError(IPC_ERR_NOT_IMPLEMENTED, "DEVICE_DELETE not implemented yet");
}

bool ipc_sendDeviceStatus(const uint16_t *indices, uint8_t indexCount, 
                          bool success, const char *message) {
    IPC_DeviceStatus_t status;
    status.indexCount = indexCount < 4 ? indexCount : 4;
    status.success = success ? 1 : 0;
    
    for (uint8_t i = 0; i < status.indexCount; i++) {
        status.assignedIndex[i] = indices[i];
    }
    
    strncpy(status.message, message, sizeof(status.message) - 1);
    status.message[sizeof(status.message) - 1] = '\0';
    
    return ipc_sendPacket(IPC_MSG_DEVICE_STATUS, (uint8_t*)&status, sizeof(status));
}

// ============================================================================
// FAULT & MESSAGE HANDLERS
// ============================================================================

bool ipc_sendFault(uint16_t index, uint8_t severity, const char *message) {
    if (index >= MAX_NUM_OBJECTS || !objIndex[index].valid) {
        return false;
    }
    
    IPC_FaultNotify_t fault;
    fault.index = index;
    fault.objectType = objIndex[index].type;
    fault.severity = severity;
    fault.timestamp = 0;  // RP2040 will add timestamp
    strncpy(fault.message, message, sizeof(fault.message) - 1);
    fault.message[sizeof(fault.message) - 1] = '\0';
    
    return ipc_sendPacket(IPC_MSG_FAULT_NOTIFY, (uint8_t*)&fault, sizeof(fault));
}

// ============================================================================
// CONFIGURATION HANDLERS
// ============================================================================

void ipc_handle_config_write(const uint8_t *payload, uint16_t len) {
    if (len < sizeof(IPC_ConfigWrite_t)) {
        ipc_sendError(IPC_ERR_PARSE_FAIL, "CONFIG_WRITE: Invalid payload size");
        return;
    }
    
    IPC_ConfigWrite_t *cmd = (IPC_ConfigWrite_t*)payload;
    
    // TODO: Implement configuration write logic
    ipc_sendError(IPC_ERR_NOT_IMPLEMENTED, "CONFIG_WRITE not implemented yet");
}

void ipc_handle_calibrate(const uint8_t *payload, uint16_t len) {
    if (len < sizeof(IPC_Calibrate_t)) {
        ipc_sendError(IPC_ERR_PARSE_FAIL, "CALIBRATE: Invalid payload size");
        return;
    }
    
    IPC_Calibrate_t *cmd = (IPC_Calibrate_t*)payload;
    
    // Validate index
    if (cmd->index >= MAX_NUM_OBJECTS || !objIndex[cmd->index].valid) {
        ipc_sendError(IPC_ERR_INDEX_INVALID, "Invalid calibration index");
        return;
    }
    
    // Apply calibration based on object type
    ObjectType type = objIndex[cmd->index].type;
    
    if (type == OBJ_T_ANALOG_INPUT) {
        AnalogInput_t *sensor = (AnalogInput_t*)objIndex[cmd->index].obj;
        if (sensor && sensor->cal) {
            sensor->cal->scale = cmd->scale;
            sensor->cal->offset = cmd->offset;
            sensor->cal->timestamp = cmd->timestamp;
            ipc_sendControlAck(cmd->index, true, "Calibration updated");
            return;
        }
    } else if (type == OBJ_T_ANALOG_OUTPUT) {
        AnalogOutput_t *output = (AnalogOutput_t*)objIndex[cmd->index].obj;
        if (output && output->cal) {
            output->cal->scale = cmd->scale;
            output->cal->offset = cmd->offset;
            output->cal->timestamp = cmd->timestamp;
            ipc_sendControlAck(cmd->index, true, "Calibration updated");
            return;
        }
    }
    
    ipc_sendError(IPC_ERR_DEVICE_FAIL, "Object does not support calibration");
}
