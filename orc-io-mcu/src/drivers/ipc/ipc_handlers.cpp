#include "drv_ipc.h"
#include "../../sys_init.h"
#include "../onboard/drv_rtd.h"  // For RTD configuration
#include "../onboard/drv_gpio.h" // For GPIO configuration
#include "../onboard/drv_output.h" // For output mode switching

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
            
        case IPC_MSG_SENSOR_BULK_READ_REQ:
            ipc_handle_sensor_bulk_read_req(payload, len);
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
            
        case IPC_MSG_CONFIG_ANALOG_INPUT:
            ipc_handle_config_analog_input(payload, len);
            break;
            
        case IPC_MSG_CONFIG_ANALOG_OUTPUT:
            ipc_handle_config_analog_output(payload, len);
            break;
            
        case IPC_MSG_CONFIG_RTD:
            ipc_handle_config_rtd(payload, len);
            break;
            
        case IPC_MSG_CONFIG_GPIO:
            ipc_handle_config_gpio(payload, len);
            break;
            
        case IPC_MSG_CONFIG_DIGITAL_OUTPUT:
            ipc_handle_config_digital_output(payload, len);
            break;
            
        case IPC_MSG_CONFIG_STEPPER:
            ipc_handle_config_stepper(payload, len);
            break;
            
        case IPC_MSG_CONFIG_DCMOTOR:
            ipc_handle_config_dcmotor(payload, len);
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

void ipc_handle_sensor_bulk_read_req(const uint8_t *payload, uint16_t len) {
    if (len < sizeof(IPC_SensorBulkReadReq_t)) {
        Serial.println("[IPC] ERROR: Invalid SENSOR_BULK_READ_REQ size");
        ipc_sendError(IPC_ERR_PARSE_FAIL, "SENSOR_BULK_READ_REQ: Invalid payload size");
        return;
    }
    
    IPC_SensorBulkReadReq_t *req = (IPC_SensorBulkReadReq_t*)payload;
    
    // Validate range
    if (req->startIndex >= MAX_NUM_OBJECTS || req->count == 0) {
        Serial.printf("[IPC] ERROR: Invalid bulk read range: start=%d, count=%d\n", 
                     req->startIndex, req->count);
        ipc_sendError(IPC_ERR_INDEX_INVALID, "Invalid bulk read range");
        return;
    }
    
    // Clamp count to valid range
    uint16_t count = req->count;
    if (req->startIndex + count > MAX_NUM_OBJECTS) {
        count = MAX_NUM_OBJECTS - req->startIndex;
    }
    
    // Send individual sensor responses
    // Wait for IPC TX queue space before each send (queue size = 8 packets)
    uint16_t sentCount = 0;
    for (uint16_t i = 0; i < count; i++) {
        uint16_t index = req->startIndex + i;
        
        // Wait for TX queue to have space (process queue to drain it)
        uint16_t waitCount = 0;
        while (!ipc_txQueueHasSpace() && waitCount < 100) {
            ipc_processTxQueue();  // Drain the queue
            delayMicroseconds(100);
            waitCount++;
        }
        
        /*if (waitCount > 0) {
            Serial.printf("[IPC] DEBUG: Waited %d*100us for queue space (queueCount=%d)\n", 
                         waitCount, ipc_txQueueCount());
        }*/
        
        if (ipc_sendSensorData(index)) {
            sentCount++;
        }
    }
    
    /*Serial.printf("[IPC] ✓ Sent bulk read response: %d-%d (%d requested, %d sent)\n",
                 req->startIndex, req->startIndex + count - 1, count, sentCount);*/
}

bool ipc_sendSensorData(uint16_t index) {
    if (index >= MAX_NUM_OBJECTS) {
        Serial.printf("[IPC] DEBUG: Index %d out of range (max %d)\n", index, MAX_NUM_OBJECTS);
        return false;
    }
    
    if (!objIndex[index].valid) {
        Serial.printf("[IPC] DEBUG: Index %d not valid\n", index);
        return false;
    }
    
    void *obj = objIndex[index].obj;
    if (obj == nullptr) {
        Serial.printf("[IPC] DEBUG: Index %d obj pointer is NULL (type=%d)\n", 
                     index, objIndex[index].type);
        return false;
    }
    
    IPC_SensorData_t data;
    memset(&data, 0, sizeof(data));
    
    data.index = index;
    data.objectType = objIndex[index].type;
    data.flags = 0;
    data.timestamp = 0;  // RP2040 will add timestamp
    
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
            if (stepper->running) data.flags |= IPC_SENSOR_FLAG_RUNNING;
            if (stepper->direction) data.flags |= IPC_SENSOR_FLAG_DIRECTION;
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
            if (motor->running) data.flags |= IPC_SENSOR_FLAG_RUNNING;
            if (motor->direction) data.flags |= IPC_SENSOR_FLAG_DIRECTION;
            if (motor->newMessage) {
                data.flags |= IPC_SENSOR_FLAG_NEW_MSG;
                strncpy(data.message, motor->message, sizeof(data.message) - 1);
            }
            
            // Add motor current as additional value
            data.valueCount = 1;
            data.additionalValues[0] = (float)motor->runCurrent / 1000.0f;  // Convert mA to A
            strncpy(data.additionalUnits[0], "A", sizeof(data.additionalUnits[0]) - 1);
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
        //Serial.printf("[IPC] ✓ Sent %s: %.2f %s\n", objIndex[index].name, data.value, data.unit);
    } else {
        Serial.printf("[IPC] DEBUG: Failed to send packet for index %d - TX queue full?\n", index);
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
    // Determine message type by checking object type in payload
    if (len < 4) {  // Need at least index + objectType
        ipc_sendError(IPC_ERR_PARSE_FAIL, "CONTROL_WRITE: Payload too small");
        return;
    }
    
    // All control structures have: index(uint16) at offset 0, objectType(uint8) at offset 2
    uint16_t index = *((uint16_t*)payload);
    uint8_t objectType = *((uint8_t*)(payload + 2));
    
    // Validate index
    if (index >= MAX_NUM_OBJECTS || !objIndex[index].valid) {
        ipc_sendControlAck_v2(index, objectType, 0, false, CTRL_ERR_INVALID_INDEX, "Invalid object index");
        return;
    }
    
    // Verify type matches
    if (objIndex[index].type != objectType) {
        ipc_sendControlAck_v2(index, objectType, 0, false, CTRL_ERR_TYPE_MISMATCH, "Object type mismatch");
        return;
    }
    
    // Route to appropriate handler based on object type
    switch (objectType) {
        case OBJ_T_DIGITAL_OUTPUT:
            ipc_handle_digital_output_control(payload, len);
            break;
            
        case OBJ_T_ANALOG_OUTPUT:
            ipc_handle_analog_output_control(payload, len);
            break;
            
        case OBJ_T_STEPPER_MOTOR:
            ipc_handle_stepper_control(payload, len);
            break;
            
        case OBJ_T_BDC_MOTOR:
            ipc_handle_dcmotor_control(payload, len);
            break;
            
        default:
            // For control loops (PID, etc.) - use old handler
            ipc_handle_control_loop_write(payload, len);
            break;
    }
}

// Original control loop handler (for PID, sequencers, etc.)
void ipc_handle_control_loop_write(const uint8_t *payload, uint16_t len) {
    if (len < sizeof(IPC_ControlWrite_t)) {
        ipc_sendError(IPC_ERR_PARSE_FAIL, "CONTROL_WRITE: Invalid payload size");
        return;
    }
    
    IPC_ControlWrite_t *cmd = (IPC_ControlWrite_t*)payload;
    
    // TODO: Implement specific control loop writes
    ipc_sendError(IPC_ERR_NOT_IMPLEMENTED, "Control loop write not implemented yet");
}

// Digital Output Control Handler
void ipc_handle_digital_output_control(const uint8_t *payload, uint16_t len) {
    if (len != sizeof(IPC_DigitalOutputControl_t)) {
        char errMsg[100];
        sprintf(errMsg, "Invalid size: got %d, expected %d", len, sizeof(IPC_DigitalOutputControl_t));
        ipc_sendError(IPC_ERR_PARSE_FAIL, errMsg);
        return;
    }
    
    const IPC_DigitalOutputControl_t *cmd = (const IPC_DigitalOutputControl_t*)payload;
    
    // Validate index range (21-25)
    if (cmd->index < 21 || cmd->index > 25) {
        ipc_sendControlAck_v2(cmd->index, cmd->objectType, cmd->command, false, 
                            CTRL_ERR_INVALID_INDEX, "Index out of range for digital output");
        return;
    }
    
    DigitalOutput_t *output = (DigitalOutput_t*)objIndex[cmd->index].obj;
    if (!output) {
        ipc_sendControlAck_v2(cmd->index, cmd->objectType, cmd->command, false,
                            CTRL_ERR_INVALID_INDEX, "Output object not found");
        return;
    }
    
    bool success = false;
    char message[100] = "OK";
    
    switch (cmd->command) {
        case DOUT_CMD_SET_STATE:
            // Only works in ON/OFF mode
            if (!output->pwmEnabled) {
                output->state = cmd->state;
                success = true;
            } else {
                strcpy(message, "Output is in PWM mode, use SET_PWM command");
            }
            break;
            
        case DOUT_CMD_SET_PWM:
            // Only works in PWM mode
            if (output->pwmEnabled) {
                if (cmd->pwmDuty >= 0.0f && cmd->pwmDuty <= 100.0f) {
                    output->pwmDuty = cmd->pwmDuty;
                    success = true;
                } else {
                    strcpy(message, "PWM duty out of range (0-100%)");
                }
            } else {
                strcpy(message, "Output is in ON/OFF mode, configure as PWM first");
            }
            break;
            
        case DOUT_CMD_DISABLE:
            output->state = false;
            output->pwmDuty = 0.0f;
            // Don't change pwmEnabled - that's a config setting
            success = true;
            break;
            
        default:
            strcpy(message, "Unknown command");
            break;
    }
    
    ipc_sendControlAck_v2(cmd->index, cmd->objectType, cmd->command, success,
                        success ? CTRL_ERR_NONE : CTRL_ERR_INVALID_CMD, message);
}

// Analog Output (DAC) Control Handler
void ipc_handle_analog_output_control(const uint8_t *payload, uint16_t len) {
    if (len != sizeof(IPC_AnalogOutputControl_t)) {
        char errMsg[100];
        sprintf(errMsg, "Invalid size: got %d, expected %d", len, sizeof(IPC_AnalogOutputControl_t));
        ipc_sendError(IPC_ERR_PARSE_FAIL, errMsg);
        return;
    }
    
    const IPC_AnalogOutputControl_t *cmd = (const IPC_AnalogOutputControl_t*)payload;
    
    Serial.printf("[DAC] Control command: index=%d, type=%d, cmd=%d, value=%.1f\n",
                 cmd->index, cmd->objectType, cmd->command, cmd->value);
    
    // Validate index range (8-9)
    if (cmd->index < 8 || cmd->index > 9) {
        Serial.printf("[DAC] ERROR: Index %d out of range\n", cmd->index);
        ipc_sendControlAck_v2(cmd->index, cmd->objectType, cmd->command, false, 
                            CTRL_ERR_INVALID_INDEX, "Index out of range for analog output");
        return;
    }
    
    AnalogOutput_t *output = (AnalogOutput_t*)objIndex[cmd->index].obj;
    Serial.printf("[DAC] Object lookup: index=%d, obj=%p, valid=%d\n", 
                 cmd->index, output, objIndex[cmd->index].valid);
    if (!output) {
        Serial.printf("[DAC] ERROR: Object not found at index %d\n", cmd->index);
        ipc_sendControlAck_v2(cmd->index, cmd->objectType, cmd->command, false,
                            CTRL_ERR_INVALID_INDEX, "Output object not found");
        return;
    }
    
    bool success = false;
    char message[100] = "OK";
    
    switch (cmd->command) {
        case AOUT_CMD_SET_VALUE:
            // Validate value range (0-10240 mV)
            if (cmd->value >= 0.0f && cmd->value <= 10240.0f) {
                output->value = cmd->value;
                success = true;
                Serial.printf("[DAC] Set output %d to %.1f mV\n", cmd->index, cmd->value);
            } else {
                strcpy(message, "Value out of range (0-10240 mV)");
            }
            break;
            
        case AOUT_CMD_DISABLE:
            output->value = 0.0f;
            success = true;
            Serial.printf("[DAC] Disabled output %d\n", cmd->index);
            break;
            
        default:
            strcpy(message, "Unknown command");
            break;
    }
    
    ipc_sendControlAck_v2(cmd->index, cmd->objectType, cmd->command, success,
                        success ? CTRL_ERR_NONE : CTRL_ERR_INVALID_CMD, message);
}

// Stepper Motor Control Handler  
void ipc_handle_stepper_control(const uint8_t *payload, uint16_t len) {
    if (len != sizeof(IPC_StepperControl_t)) {
        ipc_sendError(IPC_ERR_PARSE_FAIL, "Invalid stepper control message size");
        return;
    }
    
    const IPC_StepperControl_t *cmd = (const IPC_StepperControl_t*)payload;
    
    Serial.printf("[IPC] Stepper control: cmd=%d, rpm=%.1f, dir=%d\n", 
                 cmd->command, cmd->rpm, cmd->direction);
    
    // Validate index (must be 26)
    if (cmd->index != 26) {
        ipc_sendControlAck_v2(cmd->index, cmd->objectType, cmd->command, false,
                            CTRL_ERR_INVALID_INDEX, "Invalid stepper motor index");
        return;
    }
    
    StepperDevice_t *stepper = (StepperDevice_t*)objIndex[26].obj;
    if (!stepper) {
        ipc_sendControlAck_v2(cmd->index, cmd->objectType, cmd->command, false,
                            CTRL_ERR_INVALID_INDEX, "Stepper object not found");
        return;
    }
    
    bool success = false;
    char message[100] = "OK";
    ControlErrorCode errorCode = CTRL_ERR_NONE;
    
    switch (cmd->command) {
        case STEPPER_CMD_SET_RPM:
            if (cmd->rpm >= 0.0f && cmd->rpm <= stepper->maxRPM) {
                stepper->rpm = cmd->rpm;
                // If motor is running, apply the new RPM immediately
                if (stepper->enabled && stepper->running) {
                    success = stepper_update(true);
                    if (!success) {
                        strcpy(message, "Failed to update RPM");
                        errorCode = CTRL_ERR_DRIVER_FAULT;
                    }
                } else {
                    // Just store for next start
                    success = true;
                }
            } else {
                snprintf(message, sizeof(message), "RPM out of range (0-%.1f)", stepper->maxRPM);
                errorCode = CTRL_ERR_OUT_OF_RANGE;
            }
            break;
            
        case STEPPER_CMD_SET_DIR:
            stepper->direction = cmd->direction;
            // Only apply direction if motor is currently enabled (running or ready to run)
            // If motor is running, this will update direction dynamically
            // If motor is stopped, direction is just stored for next start
            if (stepper->enabled) {
                success = stepper_update(true);
                if (!success) {
                    strcpy(message, "Failed to apply direction");
                    errorCode = CTRL_ERR_DRIVER_FAULT;

                    // Debug
                    if (stepperDevice.fault) Serial.printf("[STEPPER] Direction apply failed: %s\n", stepperDevice.message);
                }
            } else {
                // Just store direction for next start
                success = true;
                Serial.printf("[STEPPER] Direction stored: %s (motor stopped)\n", 
                             stepper->direction ? "Forward" : "Reverse");
            }
            break;
            
        case STEPPER_CMD_START:
            stepper->rpm = cmd->rpm;
            stepper->direction = cmd->direction;
            stepper->enabled = true;
            success = stepper_update(true);  // Start motor with new parameters
            if (!success) {
                strcpy(message, "Failed to start motor");
                errorCode = CTRL_ERR_DRIVER_FAULT;
            }
            break;
            
        case STEPPER_CMD_STOP:
            stepper->enabled = false;
            success = stepper_update(true);  // Stop motor
            if (!success) {
                strcpy(message, "Failed to stop motor");
                errorCode = CTRL_ERR_DRIVER_FAULT;
            }
            break;
            
        case STEPPER_CMD_UPDATE:
            // Update parameters and apply if enabled (works for both running and stopped)
            stepper->rpm = cmd->rpm;
            stepper->direction = cmd->direction;
            if (stepper->enabled) {
                success = stepper_update(true);  // Apply changes
                if (!success) {
                    strcpy(message, "Failed to update motor");
                    errorCode = CTRL_ERR_DRIVER_FAULT;
                }
            } else {
                // Just store parameters for next start
                success = true;
            }
            break;
            
        default:
            strcpy(message, "Unknown command");
            errorCode = CTRL_ERR_INVALID_CMD;
            break;
    }
    
    ipc_sendControlAck_v2(cmd->index, cmd->objectType, cmd->command, success, errorCode, message);
}

// DC Motor Control Handler
void ipc_handle_dcmotor_control(const uint8_t *payload, uint16_t len) {
    if (len != sizeof(IPC_DCMotorControl_t)) {
        ipc_sendError(IPC_ERR_PARSE_FAIL, "Invalid DC motor control message size");
        return;
    }
    
    const IPC_DCMotorControl_t *cmd = (const IPC_DCMotorControl_t*)payload;
    
    // Validate index range (27-30)
    if (cmd->index < 27 || cmd->index > 30) {
        ipc_sendControlAck_v2(cmd->index, cmd->objectType, cmd->command, false,
                            CTRL_ERR_INVALID_INDEX, "Invalid DC motor index");
        return;
    }
    
    uint8_t motorNum = cmd->index - 27;
    MotorDevice_t *motor = (MotorDevice_t*)objIndex[cmd->index].obj;
    if (!motor) {
        ipc_sendControlAck_v2(cmd->index, cmd->objectType, cmd->command, false,
                            CTRL_ERR_INVALID_INDEX, "Motor object not found");
        return;
    }
    
    bool success = false;
    char message[100] = "OK";
    ControlErrorCode errorCode = CTRL_ERR_NONE;
    
    switch (cmd->command) {
        case DCMOTOR_CMD_SET_POWER:
            if (cmd->power >= 0.0f && cmd->power <= 100.0f) {
                motor->power = cmd->power;
                // If motor is running, apply the new power immediately
                if (motor->enabled && motor->running) {
                    success = motor_run(motorNum, (uint8_t)cmd->power, motor->direction);
                    if (!success) {
                        strcpy(message, "Failed to update power");
                        errorCode = CTRL_ERR_DRIVER_FAULT;
                    } else {
                        Serial.printf("[DC MOTOR] Updated power to %.1f%% while running\n", cmd->power);
                    }
                } else {
                    // Just store for next start
                    success = true;
                    Serial.printf("[DC MOTOR] Power stored: %.1f%% (motor stopped)\n", cmd->power);
                }
            } else {
                strcpy(message, "Power out of range (0-100%)");
                errorCode = CTRL_ERR_OUT_OF_RANGE;
            }
            break;
            
        case DCMOTOR_CMD_SET_DIR:
            motor->direction = cmd->direction;
            // If motor is running, apply the direction change immediately
            if (motor->enabled && motor->running) {
                success = motor_run(motorNum, (uint8_t)motor->power, cmd->direction);
                if (!success) {
                    strcpy(message, "Failed to update direction");
                    errorCode = CTRL_ERR_DRIVER_FAULT;
                }
            } else {
                // Just store for next start
                success = true;
                Serial.printf("[DC MOTOR] Direction stored: %s (motor stopped)\n", 
                             cmd->direction ? "Forward" : "Reverse");
            }
            break;
            
        case DCMOTOR_CMD_START:
            motor->power = cmd->power;
            motor->direction = cmd->direction;
            motor->enabled = true;
            success = motor_run(motorNum, (uint8_t)cmd->power, cmd->direction);
            if (!success) {
                strcpy(message, "Failed to start motor");
                errorCode = CTRL_ERR_DRIVER_FAULT;
            }
            break;
            
        case DCMOTOR_CMD_STOP:
            motor->enabled = false;
            success = motor_stop(motorNum);
            if (!success) {
                strcpy(message, "Failed to stop motor");
                errorCode = CTRL_ERR_DRIVER_FAULT;
            }
            break;
            
        case DCMOTOR_CMD_UPDATE:
            if (motor->running) {
                motor->power = cmd->power;
                motor->direction = cmd->direction;
                success = motor_run(motorNum, (uint8_t)cmd->power, cmd->direction);
                if (!success) {
                    strcpy(message, "Failed to update motor");
                    errorCode = CTRL_ERR_DRIVER_FAULT;
                }
            } else {
                strcpy(message, "Motor not running");
                errorCode = CTRL_ERR_NOT_ENABLED;
            }
            break;
            
        default:
            strcpy(message, "Unknown command");
            errorCode = CTRL_ERR_INVALID_CMD;
            break;
    }
    
    ipc_sendControlAck_v2(cmd->index, cmd->objectType, cmd->command, success, errorCode, message);
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

// Enhanced acknowledgment with error codes (for output control)
bool ipc_sendControlAck_v2(uint16_t index, uint8_t objectType, uint8_t command,
                          bool success, uint8_t errorCode, const char *message) {
    IPC_ControlAck_t ack;
    ack.index = index;
    ack.objectType = objectType;
    ack.command = command;
    ack.success = success;
    ack.errorCode = errorCode;
    strncpy(ack.message, message, sizeof(ack.message) - 1);
    ack.message[sizeof(ack.message) - 1] = '\0';
    
    return ipc_sendPacket(IPC_MSG_CONTROL_ACK, (uint8_t*)&ack, sizeof(ack));
}

// Legacy acknowledgment (for control loops - backward compatible)
bool ipc_sendControlAck(uint16_t index, bool success, const char *message) {
    return ipc_sendControlAck_v2(index, 0, 0, success, 
                                success ? CTRL_ERR_NONE : CTRL_ERR_DRIVER_FAULT, message);
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

/**
 * @brief Handle analog input (ADC) configuration
 */
void ipc_handle_config_analog_input(const uint8_t *payload, uint16_t len) {
    if (len != sizeof(IPC_ConfigAnalogInput_t)) {
        ipc_sendError(IPC_ERR_PARSE_FAIL, "Invalid ADC config message size");
        return;
    }
    
    const IPC_ConfigAnalogInput_t *cfg = (const IPC_ConfigAnalogInput_t*)payload;
    
    // Validate index
    if (cfg->index >= MAX_NUM_OBJECTS || !objIndex[cfg->index].valid) {
        ipc_sendError(IPC_ERR_INDEX_INVALID, "Invalid ADC index");
        return;
    }
    
    AnalogInput_t *sensor = (AnalogInput_t*)objIndex[cfg->index].obj;
    if (sensor && sensor->cal) {
        // Update unit string
        strncpy(sensor->unit, cfg->unit, sizeof(sensor->unit) - 1);
        sensor->unit[sizeof(sensor->unit) - 1] = '\0';
        
        // Update calibration
        sensor->cal->scale = cfg->calScale;
        sensor->cal->offset = cfg->calOffset;
        sensor->cal->timestamp = millis();
        
        Serial.printf("[IPC] ✓ ADC[%d]: %s, cal=(%.3f, %.3f)\n",
                     cfg->index, sensor->unit, sensor->cal->scale, sensor->cal->offset);
    } else {
        ipc_sendError(IPC_ERR_DEVICE_FAIL, "ADC object not initialized");
    }
}

/**
 * @brief Handle analog output (DAC) configuration
 */
void ipc_handle_config_analog_output(const uint8_t *payload, uint16_t len) {
    if (len != sizeof(IPC_ConfigAnalogOutput_t)) {
        ipc_sendError(IPC_ERR_PARSE_FAIL, "Invalid DAC config message size");
        return;
    }
    
    const IPC_ConfigAnalogOutput_t *cfg = (const IPC_ConfigAnalogOutput_t*)payload;
    
    // Validate index
    if (cfg->index >= MAX_NUM_OBJECTS || !objIndex[cfg->index].valid) {
        ipc_sendError(IPC_ERR_INDEX_INVALID, "Invalid DAC index");
        return;
    }
    
    AnalogOutput_t *output = (AnalogOutput_t*)objIndex[cfg->index].obj;
    if (output && output->cal) {
        // Update unit string
        strncpy(output->unit, cfg->unit, sizeof(output->unit) - 1);
        output->unit[sizeof(output->unit) - 1] = '\0';
        
        // Update calibration
        output->cal->scale = cfg->calScale;
        output->cal->offset = cfg->calOffset;
        output->cal->timestamp = millis();
        
        Serial.printf("[IPC] ✓ DAC[%d]: %s, cal=(%.3f, %.3f)\n",
                     cfg->index, output->unit, output->cal->scale, output->cal->offset);
    } else {
        ipc_sendError(IPC_ERR_DEVICE_FAIL, "DAC object not initialized");
    }
}

/**
 * @brief Handle RTD temperature sensor configuration
 */
void ipc_handle_config_rtd(const uint8_t *payload, uint16_t len) {
    if (len != sizeof(IPC_ConfigRTD_t)) {
        ipc_sendError(IPC_ERR_PARSE_FAIL, "Invalid RTD config message size");
        return;
    }
    
    const IPC_ConfigRTD_t *cfg = (const IPC_ConfigRTD_t*)payload;
    
    // Validate index
    if (cfg->index >= MAX_NUM_OBJECTS || !objIndex[cfg->index].valid) {
        ipc_sendError(IPC_ERR_INDEX_INVALID, "Invalid RTD index");
        return;
    }
    
    TemperatureSensor_t *sensor = (TemperatureSensor_t*)objIndex[cfg->index].obj;
    if (sensor && sensor->cal) {
        // Update unit string
        strncpy(sensor->unit, cfg->unit, sizeof(sensor->unit) - 1);
        sensor->unit[sizeof(sensor->unit) - 1] = '\0';
        
        // Update calibration
        sensor->cal->scale = cfg->calScale;
        sensor->cal->offset = cfg->calOffset;
        sensor->cal->timestamp = millis();
        
        // Get RTD driver interface (indices 10-12 map to rtd_interface[0-2])
        int rtdIdx = cfg->index - 10;
        if (rtdIdx >= 0 && rtdIdx < NUM_MAX31865_INTERFACES) {
            // Configure wire type
            max31865_numwires_t wireType;
            switch (cfg->wireConfig) {
                case 2: wireType = MAX31865_2WIRE; break;
                case 4: wireType = MAX31865_4WIRE; break;
                default: wireType = MAX31865_3WIRE; break;
            }
            setRtdWires(&rtd_interface[rtdIdx], wireType);
            
            // Configure sensor type (PT100 or PT1000)
            RtdSensorType sensorType = (cfg->nominalOhms == 1000) ? PT1000 : PT100;
            setRtdSensorType(&rtd_interface[rtdIdx], sensorType);
            
            Serial.printf("[IPC] ✓ RTD[%d]: %s, %d-wire, PT%d, cal=(%.3f, %.3f)\n",
                         cfg->index, sensor->unit, cfg->wireConfig, cfg->nominalOhms, 
                         sensor->cal->scale, sensor->cal->offset);
        } else {
            Serial.printf("[IPC] ✓ RTD[%d]: %s (no driver)\n",
                         cfg->index, sensor->unit);
        }
    } else {
        ipc_sendError(IPC_ERR_DEVICE_FAIL, "RTD object not initialized");
    }
}

/**
 * @brief Handle GPIO configuration
 */
void ipc_handle_config_gpio(const uint8_t *payload, uint16_t len) {
    if (len != sizeof(IPC_ConfigGPIO_t)) {
        ipc_sendError(IPC_ERR_PARSE_FAIL, "Invalid GPIO config message size");
        return;
    }
    
    const IPC_ConfigGPIO_t *cfg = (const IPC_ConfigGPIO_t*)payload;
    
    // Validate index range (13-20 for GPIO inputs)
    if (cfg->index < 13 || cfg->index >= 21) {
        Serial.printf("[IPC] Invalid GPIO index: %d\n", cfg->index);
        ipc_sendError(IPC_ERR_INDEX_INVALID, "GPIO index out of range (13-20)");
        return;
    }
    
    // Validate pull mode
    if (cfg->pullMode > 2) {
        Serial.printf("[IPC] Invalid pull mode: %d\n", cfg->pullMode);
        ipc_sendError(IPC_ERR_PARSE_FAIL, "Invalid pull mode (0-2)");
        return;
    }
    
    // Apply configuration using GPIO driver
    gpio_configure(cfg->index, cfg->name, cfg->pullMode);
    
    const char* pullStr = (cfg->pullMode == 1) ? "PULL-UP" :
                          (cfg->pullMode == 2) ? "PULL-DOWN" : "HIGH-Z";
    Serial.printf("[IPC] GPIO[%d] configured: %s, pull=%s\n", 
                  cfg->index, cfg->name, pullStr);
}

/**
 * @brief Handle digital output configuration
 */
void ipc_handle_config_digital_output(const uint8_t *payload, uint16_t len) {
    if (len != sizeof(IPC_ConfigDigitalOutput_t)) {
        ipc_sendError(IPC_ERR_PARSE_FAIL, "Invalid digital output config message size");
        return;
    }
    
    const IPC_ConfigDigitalOutput_t *cfg = (const IPC_ConfigDigitalOutput_t*)payload;
    
    // Validate index range (21-25 for digital outputs)
    if (cfg->index < 21 || cfg->index > 25) {
        ipc_sendError(IPC_ERR_INDEX_INVALID, "Digital output index out of range (21-25)");
        return;
    }
    
    if (cfg->index >= MAX_NUM_OBJECTS || !objIndex[cfg->index].valid) {
        ipc_sendError(IPC_ERR_INDEX_INVALID, "Invalid digital output index");
        return;
    }
    
    // Update object index name
    strncpy(objIndex[cfg->index].name, cfg->name, sizeof(objIndex[cfg->index].name) - 1);
    objIndex[cfg->index].name[sizeof(objIndex[cfg->index].name) - 1] = '\0';
    
    // Get digital output object
    DigitalOutput_t *output = (DigitalOutput_t*)objIndex[cfg->index].obj;
    if (output) {
        bool wasPWM = output->pwmEnabled;
        
        // Apply mode configuration to output object
        output->pwmEnabled = (cfg->mode == 1);  // 1 = PWM, 0 = ON/OFF
        
        // Handle mode transitions
        if (wasPWM && !output->pwmEnabled) {
            // Switching from PWM to ON/OFF - force pin back to digital mode
            output->state = (output->pwmDuty > 0);
            output->pwmDuty = 0.0f;
            output_force_digital_mode(cfg->index);
        } else if (!wasPWM && output->pwmEnabled) {
            // Switching from ON/OFF to PWM - re-initialize pin for PWM
            if (cfg->index == 25) {
                // Heater output - re-init pin MUX for TCC0
                analogWrite(PIN_HEAT_OUT, 0);
                Serial.printf("[OUTPUT] Heater pin re-initialized for PWM mode\n");
            } else if (cfg->index >= 21 && cfg->index <= 24) {
                // Regular outputs - re-init pin MUX
                int arrayIdx = cfg->index - 21;
                analogWrite(outputDriver.pin[arrayIdx], 0);
                Serial.printf("[OUTPUT] Output %d pin re-initialized for PWM mode\n", cfg->index);
            }
            // Initialize PWM duty to 0
            output->pwmDuty = 0.0f;
        }
        
        const char* modeStr = output->pwmEnabled ? "PWM" : "ON/OFF";
        Serial.printf("[IPC] ✓ DigitalOutput[%d]: %s, mode=%s (cfg->mode=%d, pwmEnabled=%d)\n",
                     cfg->index, cfg->name, modeStr, cfg->mode, output->pwmEnabled);
    } else {
        ipc_sendError(IPC_ERR_DEVICE_FAIL, "Digital output object not initialized");
    }
}

/**
 * @brief Handle stepper motor configuration
 */
void ipc_handle_config_stepper(const uint8_t *payload, uint16_t len) {
    if (len != sizeof(IPC_ConfigStepper_t)) {
        ipc_sendError(IPC_ERR_PARSE_FAIL, "Invalid stepper config message size");
        return;
    }
    
    const IPC_ConfigStepper_t *cfg = (const IPC_ConfigStepper_t*)payload;
    
    // Validate index (must be 26)
    if (cfg->index != 26) {
        ipc_sendError(IPC_ERR_INDEX_INVALID, "Stepper motor must be index 26");
        return;
    }
    
    if (!objIndex[26].valid) {
        ipc_sendError(IPC_ERR_INDEX_INVALID, "Stepper motor not available");
        return;
    }
    
    // Update object index name
    strncpy(objIndex[26].name, cfg->name, sizeof(objIndex[26].name) - 1);
    objIndex[26].name[sizeof(objIndex[26].name) - 1] = '\0';
    
    // Get stepper device object
    StepperDevice_t *stepper = (StepperDevice_t*)objIndex[26].obj;
    if (stepper) {
        // Save current enabled state to prevent inadvertent motor start
        bool wasEnabled = stepper->enabled;
        
        // Apply configuration
        stepper->stepsPerRev = cfg->stepsPerRev;
        stepper->maxRPM = cfg->maxRPM;
        stepper->holdCurrent = cfg->holdCurrent_mA;
        stepper->runCurrent = cfg->runCurrent_mA;
        stepper->acceleration = cfg->acceleration;
        stepper->inverted = cfg->invertDirection;
        // DON'T update enabled state from config - preserve current runtime state
        // stepper->enabled = cfg->enabled;  // Removed to prevent inadvertent enable
        
        // Apply to driver only if motor is currently enabled
        // This prevents the motor from starting just because config was updated
        if (wasEnabled) {
            stepper_update(true);
            Serial.printf("[IPC] ✓ Stepper[26]: %s, maxRPM=%d, steps=%d, Irun=%dmA (updated while enabled)\n",
                         cfg->name, cfg->maxRPM, cfg->stepsPerRev, cfg->runCurrent_mA);
        } else {
            Serial.printf("[IPC] ✓ Stepper[26]: %s, maxRPM=%d, steps=%d, Irun=%dmA (config saved, motor disabled)\n",
                         cfg->name, cfg->maxRPM, cfg->stepsPerRev, cfg->runCurrent_mA);
        }
    } else {
        ipc_sendError(IPC_ERR_DEVICE_FAIL, "Stepper motor object not initialized");
    }
}

/**
 * @brief Handle DC motor configuration
 */
void ipc_handle_config_dcmotor(const uint8_t *payload, uint16_t len) {
    if (len != sizeof(IPC_ConfigDCMotor_t)) {
        ipc_sendError(IPC_ERR_PARSE_FAIL, "Invalid DC motor config message size");
        return;
    }
    
    const IPC_ConfigDCMotor_t *cfg = (const IPC_ConfigDCMotor_t*)payload;
    
    // Validate index range (27-30 for DC motors)
    if (cfg->index < 27 || cfg->index > 30) {
        ipc_sendError(IPC_ERR_INDEX_INVALID, "DC motor index out of range (27-30)");
        return;
    }
    
    if (!objIndex[cfg->index].valid) {
        ipc_sendError(IPC_ERR_INDEX_INVALID, "DC motor not available");
        return;
    }
    
    // Update object index name
    strncpy(objIndex[cfg->index].name, cfg->name, sizeof(objIndex[cfg->index].name) - 1);
    objIndex[cfg->index].name[sizeof(objIndex[cfg->index].name) - 1] = '\0';
    
    // Get motor device object
    MotorDevice_t *motor = (MotorDevice_t*)objIndex[cfg->index].obj;
    if (motor) {
        // Apply configuration
        motor->inverted = cfg->invertDirection;
        motor->enabled = cfg->enabled;
        
        Serial.printf("[IPC] ✓ DCMotor[%d]: %s, invert=%s\n",
                     cfg->index, cfg->name, cfg->invertDirection ? "YES" : "NO");
    } else {
        ipc_sendError(IPC_ERR_DEVICE_FAIL, "DC motor object not initialized");
    }
}
