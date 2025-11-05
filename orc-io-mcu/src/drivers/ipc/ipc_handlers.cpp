#include "drv_ipc.h"
#include "../../sys_init.h"
#include "../onboard/drv_rtd.h"  // For RTD configuration
#include "../onboard/drv_gpio.h" // For GPIO configuration
#include "../onboard/drv_output.h" // For output mode switching
#include "../device_manager.h" // For dynamic device management
#include "../../controllers/controller_manager.h" // For temperature controller management
#include "../peripheral/drv_modbus_alicat_mfc.h" // For AlicatMFC setpoint control
#include "../peripheral/drv_analogue_pressure.h" // For Pressure Controller config

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
            
        case IPC_MSG_DEVICE_CONFIG:
            ipc_handle_device_config(payload, len);
            break;
            
        case IPC_MSG_DEVICE_CONTROL:
            ipc_handle_device_control(payload, len);
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
            
        case IPC_MSG_CONFIG_COMPORT:
            ipc_handle_config_comport(payload, len);
            break;
            
        case IPC_MSG_CONFIG_TEMP_CONTROLLER:
            ipc_handle_config_temp_controller(payload, len);
            break;
            
        case IPC_MSG_CONFIG_PH_CONTROLLER:
            ipc_handle_config_ph_controller(payload, len);
            break;
            
        case IPC_MSG_CONFIG_FLOW_CONTROLLER:
            ipc_handle_config_flow_controller(payload, len);
            break;
            
        case IPC_MSG_CONFIG_PRESSURE_CTRL:
            ipc_handle_config_pressure_ctrl(payload, len);
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
        // Commented out - too spammy when polling dynamic device range
        // Serial.printf("[IPC] DEBUG: Index %d out of range (max %d)\n", index, MAX_NUM_OBJECTS);
        return false;
    }
    
    if (!objIndex[index].valid) {
        // Commented out - expected when polling empty device slots (indices 60-79)
        // Serial.printf("[IPC] DEBUG: Index %d not valid\n", index);
        return false;
    }
    
    void *obj = objIndex[index].obj;
    if (obj == nullptr) {
        // Commented out - too spammy
        // Serial.printf("[IPC] DEBUG: Index %d obj pointer is NULL (type=%d)\n", 
        //              index, objIndex[index].type);
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
        
        case OBJ_T_TEMPERATURE_CONTROL: {
            TemperatureControl_t *ctrl = (TemperatureControl_t*)obj;
            data.value = ctrl->currentTemp;  // Process value (temperature)
            strncpy(data.unit, "C", sizeof(data.unit) - 1);
            
            // Flags
            if (ctrl->fault) data.flags |= IPC_SENSOR_FLAG_FAULT;
            if (ctrl->enabled) data.flags |= IPC_SENSOR_FLAG_RUNNING;  // Reuse running flag for enabled
            if (ctrl->autotuning) data.flags |= (1 << 4);  // Bit 4 for autotuning
            
            if (ctrl->newMessage) {
                data.flags |= IPC_SENSOR_FLAG_NEW_MSG;
                strncpy(data.message, ctrl->message, sizeof(data.message) - 1);
                ctrl->newMessage = false;
            }
            
            // Add output and PID gains as additional values
            data.valueCount = 4;
            data.additionalValues[0] = ctrl->currentOutput;  // Output percentage
            data.additionalValues[1] = ctrl->kp;             // PID gains
            data.additionalValues[2] = ctrl->ki;
            data.additionalValues[3] = ctrl->kd;
            strncpy(data.additionalUnits[0], "%", sizeof(data.additionalUnits[0]) - 1);
            strncpy(data.additionalUnits[1], "", sizeof(data.additionalUnits[1]) - 1);
            strncpy(data.additionalUnits[2], "", sizeof(data.additionalUnits[2]) - 1);
            strncpy(data.additionalUnits[3], "", sizeof(data.additionalUnits[3]) - 1);
            break;
        }
        
        case OBJ_T_PH_CONTROL: {
            pHControl_t *ctrl = (pHControl_t*)obj;
            data.value = ctrl->currentpH;  // Process value (pH)
            strncpy(data.unit, "pH", sizeof(data.unit) - 1);
            
            // Add additional values: currentOutput, acidVolumeTotal, alkalineVolumeTotal
            data.valueCount = 3;
            data.additionalValues[0] = ctrl->currentOutput;  // 0=off, 1=dosing acid, 2=dosing alkaline
            data.additionalValues[1] = ctrl->acidCumulativeVolume_mL;
            data.additionalValues[2] = ctrl->alkalineCumulativeVolume_mL;
            
            if (ctrl->fault) data.flags |= IPC_SENSOR_FLAG_FAULT;
            if (ctrl->enabled) data.flags |= IPC_SENSOR_FLAG_RUNNING;  // Report enabled state
            if (ctrl->newMessage) {
                data.flags |= IPC_SENSOR_FLAG_NEW_MSG;
                strncpy(data.message, ctrl->message, sizeof(data.message) - 1);
                ctrl->newMessage = false;
            }
            break;
        }
        
        case OBJ_T_FLOW_CONTROL: {
            FlowControl_t *ctrl = (FlowControl_t*)obj;
            data.value = ctrl->flowRate_mL_min;  // Setpoint (flow rate)
            strncpy(data.unit, "mL/min", sizeof(data.unit) - 1);
            
            // Add additional values: currentOutput, calculatedInterval, cumulativeVolume
            data.valueCount = 3;
            data.additionalValues[0] = ctrl->currentOutput;  // 0=off, 1=dosing
            data.additionalValues[1] = (float)ctrl->calculatedInterval_ms;  // Interval in ms
            data.additionalValues[2] = ctrl->cumulativeVolume_mL;  // Total volume pumped
            
            if (ctrl->fault) data.flags |= IPC_SENSOR_FLAG_FAULT;
            if (ctrl->enabled) data.flags |= IPC_SENSOR_FLAG_RUNNING;  // Report enabled state
            if (ctrl->newMessage) {
                data.flags |= IPC_SENSOR_FLAG_NEW_MSG;
                strncpy(data.message, ctrl->message, sizeof(data.message) - 1);
                ctrl->newMessage = false;
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
        
        case OBJ_T_ENERGY_SENSOR: {
            EnergySensor_t *sensor = (EnergySensor_t*)obj;
            // Primary value: Voltage (V)
            data.value = sensor->voltage;
            strncpy(data.unit, sensor->unit, sizeof(data.unit) - 1);
            
            // Additional values: Current (A) and Power (W)
            data.valueCount = 2;
            data.additionalValues[0] = sensor->current;
            strncpy(data.additionalUnits[0], "A", sizeof(data.additionalUnits[0]) - 1);
            data.additionalValues[1] = sensor->power;
            strncpy(data.additionalUnits[1], "W", sizeof(data.additionalUnits[1]) - 1);
            
            // DEBUG: Log energy sensor transmission (temporary)
            // Serial.printf("[IPC] Energy[%d]: %.2fV, %.3fA, %.2fW\n", 
            //              index, sensor->voltage, sensor->current, sensor->power);
            
            if (sensor->fault) data.flags |= IPC_SENSOR_FLAG_FAULT;
            if (sensor->newMessage) {
                data.flags |= IPC_SENSOR_FLAG_NEW_MSG;
                strncpy(data.message, sensor->message, sizeof(data.message) - 1);
            }
            break;
        }
        
        case OBJ_T_DEVICE_CONTROL: {
            DeviceControl_t *ctrl = (DeviceControl_t*)obj;
            // Primary value: Setpoint
            data.value = ctrl->setpoint;
            strncpy(data.unit, ctrl->setpointUnit, sizeof(data.unit) - 1);
            
            // Additional value: Actual value
            if (ctrl->actualValue != ctrl->setpoint) {
                data.valueCount = 1;
                data.additionalValues[0] = ctrl->actualValue;
                strncpy(data.additionalUnits[0], ctrl->setpointUnit, sizeof(data.additionalUnits[0]) - 1);
            }
            
            if (ctrl->fault) data.flags |= IPC_SENSOR_FLAG_FAULT;
            if (!ctrl->connected) data.flags |= IPC_SENSOR_FLAG_FAULT;  // Treat disconnected as fault
            if (ctrl->newMessage) {
                data.flags |= IPC_SENSOR_FLAG_NEW_MSG;
                strncpy(data.message, ctrl->message, sizeof(data.message) - 1);
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
        ipc_sendControlAck(index, objectType, 0, false, CTRL_ERR_INVALID_INDEX, "Invalid object index");
        return;
    }
    
    // Verify type matches
    if (objIndex[index].type != objectType) {
        ipc_sendControlAck(index, objectType, 0, false, CTRL_ERR_TYPE_MISMATCH, "Object type mismatch");
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
            
        case OBJ_T_TEMPERATURE_CONTROL:
            ipc_handle_temp_controller_control(payload, len);
            break;
            
        case OBJ_T_PH_CONTROL:
            ipc_handle_ph_controller_control(payload, len);
            break;
            
        case OBJ_T_FLOW_CONTROL:
            ipc_handle_flow_controller_control(payload, len);
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
    
    // TODO: Implement specific control loop writes
    // IPC_ControlWrite_t *cmd = (IPC_ControlWrite_t*)payload;
    (void)payload; // Suppress unused warning
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
        ipc_sendControlAck(cmd->index, cmd->objectType, cmd->command, false, 
                            CTRL_ERR_INVALID_INDEX, "Index out of range for digital output");
        return;
    }
    
    DigitalOutput_t *output = (DigitalOutput_t*)objIndex[cmd->index].obj;
    if (!output) {
        ipc_sendControlAck(cmd->index, cmd->objectType, cmd->command, false,
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
    
    ipc_sendControlAck(cmd->index, cmd->objectType, cmd->command, success,
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
        ipc_sendControlAck(cmd->index, cmd->objectType, cmd->command, false, 
                            CTRL_ERR_INVALID_INDEX, "Index out of range for analog output");
        return;
    }
    
    AnalogOutput_t *output = (AnalogOutput_t*)objIndex[cmd->index].obj;
    Serial.printf("[DAC] Object lookup: index=%d, obj=%p, valid=%d\n", 
                 cmd->index, output, objIndex[cmd->index].valid);
    if (!output) {
        Serial.printf("[DAC] ERROR: Object not found at index %d\n", cmd->index);
        ipc_sendControlAck(cmd->index, cmd->objectType, cmd->command, false,
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
    
    ipc_sendControlAck(cmd->index, cmd->objectType, cmd->command, success,
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
        ipc_sendControlAck(cmd->index, cmd->objectType, cmd->command, false,
                            CTRL_ERR_INVALID_INDEX, "Invalid stepper motor index");
        return;
    }
    
    StepperDevice_t *stepper = (StepperDevice_t*)objIndex[26].obj;
    if (!stepper) {
        ipc_sendControlAck(cmd->index, cmd->objectType, cmd->command, false,
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
                Serial.printf("[STEPPER] Start failed: %s\n", stepperDevice.message);
                strcpy(message, "Failed to start motor");
                errorCode = CTRL_ERR_DRIVER_FAULT;
            }
            break;
            
        case STEPPER_CMD_STOP:
            stepper->enabled = false;
            success = stepper_update(true);  // Stop motor
            if (!success) {
                Serial.printf("[STEPPER] Stop failed: %s\n", stepperDevice.message);
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
    
    ipc_sendControlAck(cmd->index, cmd->objectType, cmd->command, success, errorCode, message);
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
        ipc_sendControlAck(cmd->index, cmd->objectType, cmd->command, false,
                            CTRL_ERR_INVALID_INDEX, "Invalid DC motor index");
        return;
    }
    
    uint8_t motorNum = cmd->index - 27;
    MotorDevice_t *motor = (MotorDevice_t*)objIndex[cmd->index].obj;
    if (!motor) {
        ipc_sendControlAck(cmd->index, cmd->objectType, cmd->command, false,
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
    
    ipc_sendControlAck(cmd->index, cmd->objectType, cmd->command, success, errorCode, message);
}

// Temperature Controller Control Handler
void ipc_handle_temp_controller_control(const uint8_t *payload, uint16_t len) {
    if (len != sizeof(IPC_TempControllerControl_t)) {
        ipc_sendError(IPC_ERR_PARSE_FAIL, "Invalid temp controller control message size");
        return;
    }
    
    const IPC_TempControllerControl_t *cmd = (const IPC_TempControllerControl_t*)payload;
    
    // Validate index range (40-42)
    if (cmd->index < 40 || cmd->index >= 40 + MAX_TEMP_CONTROLLERS) {
        ipc_sendControlAck(cmd->index, cmd->objectType, cmd->command, false,
                            CTRL_ERR_INVALID_INDEX, "Invalid controller index");
        return;
    }
    
    bool success = false;
    char message[100] = "OK";
    ControlErrorCode errorCode = CTRL_ERR_NONE;
    
    switch (cmd->command) {
        case TEMP_CTRL_CMD_SET_SETPOINT:
            success = ControllerManager::setSetpoint(cmd->index, cmd->setpoint);
            if (success) {
                Serial.printf("[TEMP CTRL] Setpoint updated: %.1f\n", cmd->setpoint);
            } else {
                strcpy(message, "Failed to set setpoint");
                errorCode = CTRL_ERR_DRIVER_FAULT;
            }
            break;
            
        case TEMP_CTRL_CMD_ENABLE:
            success = ControllerManager::enableController(cmd->index);
            if (success) {
                Serial.printf("[TEMP CTRL] Controller %d enabled\n", cmd->index);
            } else {
                strcpy(message, "Failed to enable controller");
                errorCode = CTRL_ERR_DRIVER_FAULT;
            }
            break;
            
        case TEMP_CTRL_CMD_DISABLE:
            success = ControllerManager::disableController(cmd->index);
            if (success) {
                Serial.printf("[TEMP CTRL] Controller %d disabled\n", cmd->index);
            } else {
                strcpy(message, "Failed to disable controller");
                errorCode = CTRL_ERR_DRIVER_FAULT;
            }
            break;
            
        case TEMP_CTRL_CMD_START_AUTOTUNE:
            success = ControllerManager::startAutotune(cmd->index, cmd->setpoint, cmd->autotuneOutputStep);
            if (success) {
                Serial.printf("[TEMP CTRL] Autotune started: setpoint=%.1f, step=%.1f%%\n",
                             cmd->setpoint, cmd->autotuneOutputStep);
            } else {
                strcpy(message, "Failed to start autotune");
                errorCode = CTRL_ERR_DRIVER_FAULT;
            }
            break;
            
        case TEMP_CTRL_CMD_STOP_AUTOTUNE:
            success = ControllerManager::stopAutotune(cmd->index);
            if (success) {
                Serial.printf("[TEMP CTRL] Autotune stopped\n");
            } else {
                strcpy(message, "Failed to stop autotune");
                errorCode = CTRL_ERR_DRIVER_FAULT;
            }
            break;
            
        default:
            strcpy(message, "Unknown command");
            errorCode = CTRL_ERR_INVALID_CMD;
            break;
    }
    
    ipc_sendControlAck(cmd->index, cmd->objectType, cmd->command, success, errorCode, message);
}

void ipc_handle_control_read(const uint8_t *payload, uint16_t len) {
    if (len < sizeof(IPC_ControlRead_t)) {
        ipc_sendError(IPC_ERR_PARSE_FAIL, "CONTROL_READ: Invalid payload size");
        return;
    }
    
    // TODO: Implement control read functionality
    // IPC_ControlRead_t *cmd = (IPC_ControlRead_t*)payload;
    (void)payload; // Suppress unused warning
    ipc_sendError(IPC_ERR_NOT_IMPLEMENTED, "CONTROL_READ not implemented yet");
}

// Enhanced acknowledgment with error codes (for output control)
bool ipc_sendControlAck(uint16_t index, uint8_t objectType, uint8_t command,
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

// ============================================================================
// DEVICE MANAGEMENT HANDLERS
// ============================================================================

void ipc_handle_device_create(const uint8_t *payload, uint16_t len) {
    if (len != sizeof(IPC_DeviceCreate_t)) {
        char errMsg[100];
        sprintf(errMsg, "Invalid size: got %d, expected %d", len, sizeof(IPC_DeviceCreate_t));
        ipc_sendError(IPC_ERR_PARSE_FAIL, errMsg);
        return;
    }
    
    const IPC_DeviceCreate_t *req = (const IPC_DeviceCreate_t*)payload;
    
    Serial.printf("[IPC] Device CREATE request: type=%d, startIndex=%d, bus=%d, addr=%d\n",
                 req->config.deviceType, req->startIndex, 
                 req->config.busType, req->config.address);
    
    // Validate index range - allow control (50-69) and sensor (70-99) ranges
    if ((req->startIndex < 50 || req->startIndex > 69) && 
        (req->startIndex < 70 || req->startIndex > 99)) {
        Serial.printf("[IPC] ERROR: Invalid start index %d (must be 50-69 or 70-99)\n", req->startIndex);
        ipc_sendDeviceStatus(req->startIndex, false, true, 0, nullptr, "Invalid index range");
        return;
    }
    
    // Create device through Device Manager
    bool success = DeviceManager::createDevice(req->startIndex, &req->config);
    
    // Get device info for status response
    ManagedDevice* dev = DeviceManager::findDevice(req->startIndex);
    if (dev) {
        // Build sensor indices array
        uint8_t sensorIndices[4] = {0};
        for (uint8_t i = 0; i < dev->sensorCount && i < 4; i++) {
            sensorIndices[i] = dev->startSensorIndex + i;
        }
        
        ipc_sendDeviceStatus(dev->startSensorIndex, dev->active, dev->fault, 
                           dev->sensorCount, sensorIndices, dev->message);
    } else {
        ipc_sendDeviceStatus(req->startIndex, false, true, 0, nullptr, 
                           "Device creation failed");
    }
    
    (void)success;  // Suppress unused variable warning
}

void ipc_handle_device_delete(const uint8_t *payload, uint16_t len) {
    if (len != sizeof(IPC_DeviceDelete_t)) {
        char errMsg[100];
        sprintf(errMsg, "Invalid size: got %d, expected %d", len, sizeof(IPC_DeviceDelete_t));
        ipc_sendError(IPC_ERR_PARSE_FAIL, errMsg);
        return;
    }
    
    const IPC_DeviceDelete_t *req = (const IPC_DeviceDelete_t*)payload;
    
    Serial.printf("[IPC] Device DELETE request: startIndex=%d\n", req->startIndex);
    
    // Validate index range - allow control (50-69) and sensor (70-99) ranges
    if ((req->startIndex < 50 || req->startIndex > 69) && 
        (req->startIndex < 70 || req->startIndex > 99)) {
        Serial.printf("[IPC] ERROR: Invalid start index %d (must be 50-69 or 70-99)\n", req->startIndex);
        ipc_sendDeviceStatus(req->startIndex, false, true, 0, nullptr, "Invalid index range");
        return;
    }
    
    // Delete device through Device Manager
    bool success = DeviceManager::deleteDevice(req->startIndex);
    
    // Send status response
    ipc_sendDeviceStatus(req->startIndex, !success, success, 0, nullptr,
                       success ? "Device deleted" : "Delete failed");
}

void ipc_handle_device_config(const uint8_t *payload, uint16_t len) {
    if (len != sizeof(IPC_DeviceConfigUpdate_t)) {
        char errMsg[100];
        sprintf(errMsg, "Invalid size: got %d, expected %d", len, sizeof(IPC_DeviceConfigUpdate_t));
        ipc_sendError(IPC_ERR_PARSE_FAIL, errMsg);
        return;
    }
    
    const IPC_DeviceConfigUpdate_t *req = (const IPC_DeviceConfigUpdate_t*)payload;
    
    Serial.printf("[IPC] Device CONFIG request: startIndex=%d\n", req->startIndex);
    
    // Validate index range - allow control (50-69) and sensor (70-99) ranges
    if ((req->startIndex < 50 || req->startIndex > 69) && 
        (req->startIndex < 70 || req->startIndex > 99)) {
        Serial.printf("[IPC] ERROR: Invalid start index %d (must be 50-69 or 70-99)\n", req->startIndex);
        ipc_sendDeviceStatus(req->startIndex, false, true, 0, nullptr, "Invalid index range");
        return;
    }
    
    // Update device configuration through Device Manager
    bool success = DeviceManager::configureDevice(req->startIndex, &req->config);
    
    // Get updated device info
    ManagedDevice* dev = DeviceManager::findDevice(req->startIndex);
    if (dev) {
        uint8_t sensorIndices[4] = {0};
        for (uint8_t i = 0; i < dev->sensorCount && i < 4; i++) {
            sensorIndices[i] = dev->startSensorIndex + i;
        }
        
        ipc_sendDeviceStatus(dev->startSensorIndex, dev->active, dev->fault,
                           dev->sensorCount, sensorIndices, dev->message);
    } else {
        ipc_sendDeviceStatus(req->startIndex, false, true, 0, nullptr,
                           success ? "Config updated" : "Config update failed");
    }
}

void ipc_handle_device_control(const uint8_t *payload, uint16_t len) {
    if (len != sizeof(IPC_DeviceControlCmd_t)) {
        char errMsg[100];
        sprintf(errMsg, "Invalid size: got %d, expected %d", len, sizeof(IPC_DeviceControlCmd_t));
        ipc_sendError(IPC_ERR_PARSE_FAIL, errMsg);
        return;
    }
    
    const IPC_DeviceControlCmd_t* cmd = (const IPC_DeviceControlCmd_t*)payload;
    
    // Validate control object index (50-69)
    if (cmd->index < 50 || cmd->index >= 70) {
        Serial.printf("[IPC] ERROR: Invalid control index %d (must be 50-69)\n", cmd->index);
        ipc_sendControlAck(cmd->index, cmd->objectType, cmd->command, false, 
                            CTRL_ERR_INVALID_INDEX, "Control index out of range");
        return;
    }
    
    // Check object is valid and is a device control object
    if (!objIndex[cmd->index].valid || objIndex[cmd->index].type != OBJ_T_DEVICE_CONTROL) {
        Serial.printf("[IPC] ERROR: Index %d is not a valid device control object\n", cmd->index);
        ipc_sendControlAck(cmd->index, cmd->objectType, cmd->command, false,
                            CTRL_ERR_TYPE_MISMATCH, "Not a device control object");
        return;
    }
    
    DeviceControl_t* control = (DeviceControl_t*)objIndex[cmd->index].obj;
    bool success = false;
    char message[100] = {0};
    ControlErrorCode errorCode = CTRL_ERR_NONE;
    
    // Dispatch based on command type
    switch(cmd->command) {
        case DEV_CMD_SET_SETPOINT: {
            // Find device instance from control index
            ManagedDevice* dev = DeviceManager::findDeviceByControlIndex(cmd->index);
            if (!dev) {
                Serial.printf("[DEV CTRL] ERROR: No device found for control index %d\n", cmd->index);
                errorCode = CTRL_ERR_INVALID_INDEX;
                strcpy(message, "Device not found");
                break;
            }
            
            // Dispatch to device-specific handler based on device type
            switch(control->deviceType) {
                case IPC_DEV_ALICAT_MFC: {
                    Serial.printf("[DEV CTRL] Set MFC setpoint: index=%d, value=%.2f\n", 
                                 cmd->index, cmd->setpoint);
                    
                    // Get device instance
                    AlicatMFC* mfc = (AlicatMFC*)dev->deviceInstance;
                    if (!mfc) {
                        Serial.printf("[DEV CTRL] ERROR: MFC device instance is null\n");
                        errorCode = CTRL_ERR_DRIVER_FAULT;
                        strcpy(message, "MFC device instance not found");
                        break;
                    }
                    
                    // Store setpoint in control object for feedback
                    control->setpoint = cmd->setpoint;
                    
                    // Write setpoint to hardware
                    bool writeSuccess = mfc->writeSetpoint(cmd->setpoint);
                    if (writeSuccess) {
                        success = true;
                        snprintf(message, sizeof(message), "Setpoint %.2f SLPM sent to MFC", cmd->setpoint);
                        Serial.printf("[DEV CTRL] MFC setpoint command queued successfully\n");
                    } else {
                        success = false;
                        errorCode = CTRL_ERR_DRIVER_FAULT;
                        strcpy(message, "Failed to queue setpoint command");
                        Serial.printf("[DEV CTRL] ERROR: Failed to queue MFC setpoint command\n");
                    }
                    break;
                }
                
                case IPC_DEV_PRESSURE_CTRL: {
                    Serial.printf("[DEV CTRL] Set pressure setpoint: index=%d, value=%.2f\n", 
                                 cmd->index, cmd->setpoint);
                    
                    // Get device instance
                    AnaloguePressureController* pc = (AnaloguePressureController*)dev->deviceInstance;
                    if (!pc) {
                        Serial.printf("[DEV CTRL] ERROR: Pressure controller instance is null\n");
                        errorCode = CTRL_ERR_DRIVER_FAULT;
                        strcpy(message, "Pressure controller instance not found");
                        break;
                    }
                    
                    // Store setpoint in control object for feedback
                    control->setpoint = cmd->setpoint;
                    
                    // Write setpoint (converts to mV and writes to DAC)
                    bool writeSuccess = pc->writeSetpoint(cmd->setpoint);
                    if (writeSuccess) {
                        success = true;
                        snprintf(message, sizeof(message), 
                                "Pressure %.2f %s set", cmd->setpoint, control->setpointUnit);
                        Serial.printf("[DEV CTRL] Pressure setpoint written successfully\n");
                    } else {
                        success = false;
                        errorCode = CTRL_ERR_OUT_OF_RANGE;
                        strcpy(message, "Failed to set pressure (out of range or DAC error)");
                        Serial.printf("[DEV CTRL] ERROR: Failed to write pressure setpoint\n");
                    }
                    break;
                }
                
                default:
                    Serial.printf("[DEV CTRL] ERROR: Device type %d does not support setpoint control\n", 
                                 control->deviceType);
                    errorCode = CTRL_ERR_INVALID_CMD;
                    strcpy(message, "Device does not support setpoint control");
                    break;
            }
            break;
        }
        
        case DEV_CMD_RESET_FAULT: {
            control->fault = false;
            control->newMessage = false;
            strcpy(control->message, "Fault cleared");
            success = true;
            strcpy(message, "Fault reset");
            Serial.printf("[DEV CTRL] Fault cleared for device at index %d\n", cmd->index);
            break;
        }
        
        default:
            Serial.printf("[DEV CTRL] ERROR: Unknown command %d\n", cmd->command);
            errorCode = CTRL_ERR_INVALID_CMD;
            strcpy(message, "Unknown device control command");
            break;
    }
    
    // Send acknowledgment
    ipc_sendControlAck(cmd->index, cmd->objectType, cmd->command, success, errorCode, message);
}

bool ipc_sendDeviceStatus(uint8_t startIndex, bool active, bool fault,
                          uint8_t objectCount, const uint8_t *sensorIndices,
                          const char *message) {
    IPC_DeviceStatus_t status;
    status.startIndex = startIndex;
    status.active = active;
    status.fault = fault;
    status.objectCount = objectCount;
    
    // Copy sensor indices
    for (uint8_t i = 0; i < 4; i++) {
        if (sensorIndices && i < objectCount) {
            status.sensorIndices[i] = sensorIndices[i];
        } else {
            status.sensorIndices[i] = 0;
        }
    }
    
    // Copy message
    if (message) {
        strncpy(status.message, message, sizeof(status.message) - 1);
        status.message[sizeof(status.message) - 1] = '\0';
    } else {
        status.message[0] = '\0';
    }
    
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
    
    // TODO: Implement configuration write logic
    // IPC_ConfigWrite_t *cmd = (IPC_ConfigWrite_t*)payload;
    (void)payload; // Suppress unused warning
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
        
        // Handle mode transitions or initial configuration
        if (wasPWM && !output->pwmEnabled) {
            // Switching from PWM to ON/OFF - force pin back to digital mode
            output->state = (output->pwmDuty > 0);
            output->pwmDuty = 0.0f;
            output_force_digital_mode(cfg->index);
        } else if (!wasPWM && output->pwmEnabled) {
            // Switching from ON/OFF to PWM mode
            // Just initialize PWM duty - output_update() will handle pin configuration
            output->pwmDuty = 0.0f;
            Serial.printf("[OUTPUT] Set output %d to PWM mode, output_update() will configure pin\\n", cfg->index);
        } else if (!wasPWM && !output->pwmEnabled) {
            // Both old and new mode are ON/OFF - ensure pin is in digital mode
            // This handles initial config where analogWrite() may have been called during init
            output_force_digital_mode(cfg->index);
            Serial.printf("[OUTPUT] Ensured output %d is in digital mode for ON/OFF operation\n", cfg->index);
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
            bool success = stepper_update(true);
            if (!success) {
                // Config validation failed - send detailed error back to SYS MCU
                char errorMsg[128];
                snprintf(errorMsg, sizeof(errorMsg), "Config validation failed: %s", stepper->message);
                Serial.printf("[IPC] ✗ Stepper config rejected: %s\n", stepper->message);
                Serial.printf("[IPC]   (holdCurrent=%dmA [max 1000], runCurrent=%dmA [max 1800], maxRPM=%d, accel=%d)\n",
                             cfg->holdCurrent_mA, cfg->runCurrent_mA, cfg->maxRPM, cfg->acceleration);
                ipc_sendError(IPC_ERR_PARAM_INVALID, errorMsg);
                return;
            }
            Serial.printf("[IPC] ✓ Stepper[26]: %s, maxRPM=%d, steps=%d, Irun=%dmA (updated while enabled)\n",
                         cfg->name, cfg->maxRPM, cfg->stepsPerRev, cfg->runCurrent_mA);
        } else {
            // Motor is disabled - just save config without validation
            // The validation will happen when the user tries to enable it
            // This prevents accidentally starting the motor during config updates
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

/**
 * @brief Handle COM port configuration
 */
void ipc_handle_config_comport(const uint8_t *payload, uint16_t len) {
    if (len != sizeof(IPC_ConfigComPort_t)) {
        ipc_sendError(IPC_ERR_PARSE_FAIL, "Invalid COM port config message size");
        return;
    }
    
    const IPC_ConfigComPort_t *cfg = (const IPC_ConfigComPort_t*)payload;
    
    // Validate index range (0-3 for COM ports)
    if (cfg->index >= 4) {
        ipc_sendError(IPC_ERR_INDEX_INVALID, "COM port index out of range (0-3)");
        return;
    }
    
    // Get modbus driver for this port
    if (cfg->index >= 4) {
        ipc_sendError(IPC_ERR_INDEX_INVALID, "Invalid modbus driver index");
        return;
    }
    
    // Apply configuration to port
    if (modbusDriver[cfg->index].portObj) {
        modbusDriver[cfg->index].portObj->baudRate = cfg->baudRate;
        modbusDriver[cfg->index].portObj->dataBits = cfg->dataBits;
        modbusDriver[cfg->index].portObj->stopBits = cfg->stopBits;
        modbusDriver[cfg->index].portObj->parity = cfg->parity;
        
        // Flag that configuration has changed
        modbusDriver[cfg->index].configChanged = true;
        
        Serial.printf("[IPC] ✓ COM Port[%d]: baud=%d, parity=%d, stop=%.1f\n",
                     cfg->index, cfg->baudRate, cfg->parity, cfg->stopBits);
        
        // Note: The actual serial port reconfiguration will happen in modbus_manage()
        // when it detects the configChanged flag
    } else {
        ipc_sendError(IPC_ERR_DEVICE_FAIL, "COM port object not initialized");
    }
}

/**
 * @brief Handle temperature controller configuration
 * 
 * Creates, updates, or deletes temperature controller instances (indices 40-42).
 * Uses ControllerManager to manage lifecycle and registration.
 */
void ipc_handle_config_temp_controller(const uint8_t *payload, uint16_t len) {
    Serial.printf("[IPC] Temp controller config: received %d bytes, expected %d bytes\n", 
                  len, sizeof(IPC_ConfigTempController_t));
    
    if (len != sizeof(IPC_ConfigTempController_t)) {
        char errorMsg[128];
        snprintf(errorMsg, sizeof(errorMsg), "Invalid size: got %d, expected %d", 
                len, sizeof(IPC_ConfigTempController_t));
        ipc_sendError(IPC_ERR_PARSE_FAIL, errorMsg);
        return;
    }
    
    const IPC_ConfigTempController_t *cfg = (const IPC_ConfigTempController_t*)payload;
    
    // Validate index range (40-42 for temperature controllers)
    if (cfg->index < 40 || cfg->index >= 40 + MAX_TEMP_CONTROLLERS) {
        ipc_sendError(IPC_ERR_INDEX_INVALID, "Temperature controller index out of range (40-42)");
        return;
    }
    
    if (cfg->isActive) {
        // Create or update controller
        if (ControllerManager::createController(cfg->index, cfg)) {
            Serial.printf("[IPC] ✓ TempController[%d]: %s, method=%s, sensor=%d, output=%d\n",
                         cfg->index, cfg->name,
                         cfg->controlMethod == 0 ? "On/Off" : "PID",
                         cfg->pvSourceIndex, cfg->outputIndex);
        } else {
            char errorMsg[128];
            snprintf(errorMsg, sizeof(errorMsg), "Failed to create controller %d", cfg->index);
            ipc_sendError(IPC_ERR_DEVICE_FAIL, errorMsg);
        }
    } else {
        // Delete controller
        if (ControllerManager::deleteController(cfg->index)) {
            Serial.printf("[IPC] ✓ Deleted temperature controller %d\n", cfg->index);
        } else {
            char errorMsg[128];
            snprintf(errorMsg, sizeof(errorMsg), "Failed to delete controller %d", cfg->index);
            ipc_sendError(IPC_ERR_DEVICE_FAIL, errorMsg);
        }
    }
}

/**
 * @brief Handle pressure controller configuration
 * 
 * Applies calibration to an existing pressure controller device instance.
 * The device must already be created via IPC_MSG_DEVICE_CREATE.
 */
void ipc_handle_config_pressure_ctrl(const uint8_t *payload, uint16_t len) {
    if (len != sizeof(IPC_ConfigPressureCtrl_t)) {
        ipc_sendError(IPC_ERR_PARSE_FAIL, "Invalid pressure controller config message size");
        return;
    }
    
    const IPC_ConfigPressureCtrl_t *cfg = (const IPC_ConfigPressureCtrl_t*)payload;
    
    // Validate control index range (50-69)
    if (cfg->controlIndex < 50 || cfg->controlIndex >= 70) {
        ipc_sendError(IPC_ERR_INDEX_INVALID, "Control index out of range (50-69)");
        return;
    }
    
    // Find device by control index
    ManagedDevice* dev = DeviceManager::findDeviceByControlIndex(cfg->controlIndex);
    if (!dev) {
        char errMsg[100];
        snprintf(errMsg, sizeof(errMsg), "No device found at control index %d", cfg->controlIndex);
        ipc_sendError(IPC_ERR_INDEX_INVALID, errMsg);
        return;
    }
    
    // Verify device type
    if (dev->type != IPC_DEV_PRESSURE_CTRL) {
        char errMsg[100];
        snprintf(errMsg, sizeof(errMsg), "Device at index %d is not a pressure controller (type=%d)", 
                cfg->controlIndex, dev->type);
        ipc_sendError(IPC_ERR_TYPE_MISMATCH, errMsg);
        return;
    }
    
    // Get device instance
    AnaloguePressureController* pc = (AnaloguePressureController*)dev->deviceInstance;
    if (!pc) {
        ipc_sendError(IPC_ERR_DEVICE_FAIL, "Pressure controller instance is null");
        return;
    }
    
    // Apply calibration (scale, offset, unit)
    pc->setCalibration(cfg->scale, cfg->offset, cfg->unit);
    
    Serial.printf("[IPC] ✓ Pressure Controller[%d]: scale=%.6f, offset=%.2f, unit=%s, DAC=%d\n",
                 cfg->controlIndex, cfg->scale, cfg->offset, cfg->unit, cfg->dacIndex);
}

// ============================================================================
// pH CONTROLLER HANDLERS
// ============================================================================

/**
 * @brief Handle pH controller configuration message
 * 
 * Creates, updates, or deletes pH controller instance (index 43).
 * Uses ControllerManager to manage lifecycle and registration.
 */
void ipc_handle_config_ph_controller(const uint8_t *payload, uint16_t len) {
    Serial.printf("[IPC] pH controller config: received %d bytes, expected %d bytes\n", 
                  len, sizeof(IPC_ConfigpHController_t));
    
    if (len != sizeof(IPC_ConfigpHController_t)) {
        ipc_sendError(IPC_ERR_PARSE_FAIL, "Invalid pH controller config message size");
        return;
    }
    
    const IPC_ConfigpHController_t *cfg = (const IPC_ConfigpHController_t*)payload;
    
    // Validate controller index (must be 43)
    if (cfg->index != 43) {
        char errMsg[100];
        snprintf(errMsg, sizeof(errMsg), "Invalid pH controller index %d (must be 43)", cfg->index);
        ipc_sendError(IPC_ERR_INDEX_INVALID, errMsg);
        return;
    }
    
    bool success;
    
    if (!cfg->isActive) {
        // Delete controller
        success = ControllerManager::deletepHController();
        if (success) {
            Serial.printf("[IPC] ✓ pH Controller deleted\n");
        } else {
            ipc_sendError(IPC_ERR_DEVICE_FAIL, "Failed to delete pH controller");
        }
    } else {
        // Create or update controller - validate configuration first
        
        // Validate dosing configuration - at least one must be enabled
        if (!cfg->acidEnabled && !cfg->alkalineEnabled) {
            ipc_sendError(IPC_ERR_PARAM_INVALID, "At least one dosing direction (acid or alkaline) must be enabled");
            return;
        }
        
        // Validate acid dosing output indices if enabled
        if (cfg->acidEnabled) {
            if (cfg->acidOutputType == 0 && (cfg->acidOutputIndex < 21 || cfg->acidOutputIndex > 25)) {
                ipc_sendError(IPC_ERR_INDEX_INVALID, "Acid digital output index must be 21-25");
                return;
            }
            if (cfg->acidOutputType == 1 && (cfg->acidOutputIndex < 27 || cfg->acidOutputIndex > 30)) {
                ipc_sendError(IPC_ERR_INDEX_INVALID, "Acid DC motor index must be 27-30");
                return;
            }
        }
        
        // Validate alkaline dosing output indices if enabled
        if (cfg->alkalineEnabled) {
            if (cfg->alkalineOutputType == 0 && (cfg->alkalineOutputIndex < 21 || cfg->alkalineOutputIndex > 25)) {
                ipc_sendError(IPC_ERR_INDEX_INVALID, "Alkaline digital output index must be 21-25");
                return;
            }
            if (cfg->alkalineOutputType == 1 && (cfg->alkalineOutputIndex < 27 || cfg->alkalineOutputIndex > 30)) {
                ipc_sendError(IPC_ERR_INDEX_INVALID, "Alkaline DC motor index must be 27-30");
                return;
            }
        }
        
        // Validation passed - create or update controller
        success = ControllerManager::createpHController(cfg);
        if (success) {
            Serial.printf("[IPC] ✓ pH Controller[%d]: %s, setpoint=%.2f, deadband=%.2f\n",
                         cfg->index, cfg->name, cfg->setpoint, cfg->deadband);
        } else {
            ipc_sendError(IPC_ERR_DEVICE_FAIL, "Failed to create/update pH controller");
        }
    }
}

/**
 * @brief Handle pH controller runtime control message
 * 
 * Processes runtime commands for pH controller: setpoint, enable, disable, manual dosing
 */
void ipc_handle_ph_controller_control(const uint8_t *payload, uint16_t len) {
    if (len != sizeof(IPC_pHControllerControl_t)) {
        ipc_sendError(IPC_ERR_PARSE_FAIL, "Invalid pH controller control message size");
        return;
    }
    
    const IPC_pHControllerControl_t *cmd = (const IPC_pHControllerControl_t*)payload;
    
    // Validate index
    if (cmd->index != 43) {
        char errMsg[100];
        snprintf(errMsg, sizeof(errMsg), "Invalid pH controller index %d (must be 43)", cmd->index);
        ipc_sendError(IPC_ERR_INDEX_INVALID, errMsg);
        return;
    }
    
    // Validate object type
    if (cmd->objectType != OBJ_T_PH_CONTROL) {
        ipc_sendError(IPC_ERR_TYPE_MISMATCH, "Object type mismatch");
        return;
    }
    
    bool success = false;
    char message[128] = "";
    uint8_t errorCode = CTRL_ERR_NONE;
    
    switch (cmd->command) {
        case PH_CMD_SET_SETPOINT:
            if (cmd->setpoint >= 0.0f && cmd->setpoint <= 14.0f) {
                success = ControllerManager::setpHSetpoint(cmd->setpoint);
                if (success) {
                    snprintf(message, sizeof(message), "Setpoint updated to %.2f", cmd->setpoint);
                    Serial.printf("[pH CTRL] Setpoint set to %.2f\n", cmd->setpoint);
                } else {
                    strcpy(message, "Failed to update setpoint");
                    errorCode = IPC_ERR_DEVICE_FAIL;
                }
            } else {
                strcpy(message, "Setpoint out of range (0-14 pH)");
                errorCode = CTRL_ERR_OUT_OF_RANGE;
            }
            break;
            
        case PH_CMD_ENABLE:
            success = ControllerManager::enablepHController();
            if (success) {
                strcpy(message, "pH controller enabled");
            } else {
                strcpy(message, "Failed to enable controller");
                errorCode = IPC_ERR_DEVICE_FAIL;
            }
            break;
            
        case PH_CMD_DISABLE:
            success = ControllerManager::disablepHController();
            if (success) {
                strcpy(message, "pH controller disabled");
            } else {
                strcpy(message, "Failed to disable controller");
                errorCode = IPC_ERR_DEVICE_FAIL;
            }
            break;
            
        case PH_CMD_DOSE_ACID:
            success = ControllerManager::dosepHAcid();
            if (success) {
                strcpy(message, "Manual acid dose started");
            } else {
                strcpy(message, "Failed to start acid dose");
                errorCode = IPC_ERR_DEVICE_FAIL;
            }
            break;
            
        case PH_CMD_DOSE_ALKALINE:
            success = ControllerManager::dosepHAlkaline();
            if (success) {
                strcpy(message, "Manual alkaline dose started");
            } else {
                strcpy(message, "Failed to start alkaline dose");
                errorCode = IPC_ERR_DEVICE_FAIL;
            }
            break;
            
        case PH_CMD_RESET_ACID_VOLUME:
            success = ControllerManager::resetpHAcidVolume();
            if (success) {
                strcpy(message, "Acid cumulative volume reset to 0.0 mL");
            } else {
                strcpy(message, "Failed to reset acid volume");
                errorCode = IPC_ERR_DEVICE_FAIL;
            }
            break;
            
        case PH_CMD_RESET_BASE_VOLUME:
            success = ControllerManager::resetpHAlkalineVolume();
            if (success) {
                strcpy(message, "Alkaline cumulative volume reset to 0.0 mL");
            } else {
                strcpy(message, "Failed to reset alkaline volume");
                errorCode = IPC_ERR_DEVICE_FAIL;
            }
            break;
            
        default:
            strcpy(message, "Invalid command");
            errorCode = CTRL_ERR_INVALID_CMD;
            break;
    }
    
    // Send acknowledgment
    ipc_sendControlAck(cmd->index, cmd->objectType, cmd->command, success, errorCode, message);
}

/**
 * @brief Handle flow controller configuration message
 * 
 * Creates, updates, or deletes flow controller instances (indices 44-47).
 * Uses ControllerManager to manage lifecycle and registration.
 */
void ipc_handle_config_flow_controller(const uint8_t *payload, uint16_t len) {
    Serial.printf("[IPC] Flow controller config: received %d bytes, expected %d bytes\n", 
                  len, sizeof(IPC_ConfigFlowController_t));
    
    if (len != sizeof(IPC_ConfigFlowController_t)) {
        ipc_sendError(IPC_ERR_PARSE_FAIL, "Invalid flow controller config message size");
        return;
    }
    
    const IPC_ConfigFlowController_t *cfg = (const IPC_ConfigFlowController_t*)payload;
    
    // Validate controller index (44-47)
    if (cfg->index < 44 || cfg->index >= 44 + MAX_FLOW_CONTROLLERS) {
        char errMsg[100];
        snprintf(errMsg, sizeof(errMsg), "Invalid flow controller index %d (must be 44-47)", cfg->index);
        ipc_sendError(IPC_ERR_INDEX_INVALID, errMsg);
        return;
    }
    
    bool success;
    
    if (!cfg->isActive) {
        // Delete controller
        success = ControllerManager::deleteFlowController(cfg->index);
        if (success) {
            Serial.printf("[IPC] ✓ Flow Controller[%d] deleted\n", cfg->index);
        } else {
            ipc_sendError(IPC_ERR_DEVICE_FAIL, "Failed to delete flow controller");
        }
    } else {
        // Create or update controller - validate configuration first
        
        // Validate output configuration
        if (cfg->outputType == 0 && (cfg->outputIndex < 21 || cfg->outputIndex > 25)) {
            ipc_sendError(IPC_ERR_INDEX_INVALID, "Digital output index must be 21-25");
            return;
        }
        if (cfg->outputType == 1 && (cfg->outputIndex < 27 || cfg->outputIndex > 30)) {
            ipc_sendError(IPC_ERR_INDEX_INVALID, "DC motor index must be 27-30");
            return;
        }
        
        // Validate calibration data
        if (cfg->calibrationVolume_mL <= 0.0f) {
            ipc_sendError(IPC_ERR_PARAM_INVALID, "Calibration volume must be > 0");
            return;
        }
        if (cfg->calibrationDoseTime_ms == 0) {
            ipc_sendError(IPC_ERR_PARAM_INVALID, "Calibration dose time must be > 0");
            return;
        }
        
        // Validate safety limits
        if (cfg->minDosingInterval_ms == 0) {
            ipc_sendError(IPC_ERR_PARAM_INVALID, "Minimum dosing interval must be > 0");
            return;
        }
        if (cfg->maxDosingTime_ms == 0) {
            ipc_sendError(IPC_ERR_PARAM_INVALID, "Maximum dosing time must be > 0");
            return;
        }
        
        // Validation passed - create or update controller
        success = ControllerManager::createFlowController(cfg->index, cfg);
        if (success) {
            Serial.printf("[IPC] ✓ FlowController[%d]: %s, flow=%.2f mL/min\n",
                         cfg->index, cfg->name, cfg->flowRate_mL_min);
        } else {
            ipc_sendError(IPC_ERR_DEVICE_FAIL, "Failed to create/update flow controller");
        }
    }
}

/**
 * @brief Handle flow controller runtime control message
 * 
 * Processes runtime commands for flow controllers: flow rate, enable, disable, manual dose
 */
void ipc_handle_flow_controller_control(const uint8_t *payload, uint16_t len) {
    if (len != sizeof(IPC_FlowControllerControl_t)) {
        ipc_sendError(IPC_ERR_PARSE_FAIL, "Invalid flow controller control message size");
        return;
    }
    
    const IPC_FlowControllerControl_t *cmd = (const IPC_FlowControllerControl_t*)payload;
    
    // Validate index (44-47)
    if (cmd->index < 44 || cmd->index >= 44 + MAX_FLOW_CONTROLLERS) {
        char errMsg[100];
        snprintf(errMsg, sizeof(errMsg), "Invalid flow controller index %d (must be 44-47)", cmd->index);
        ipc_sendError(IPC_ERR_INDEX_INVALID, errMsg);
        return;
    }
    
    // Validate object type
    if (cmd->objectType != OBJ_T_FLOW_CONTROL) {
        ipc_sendError(IPC_ERR_TYPE_MISMATCH, "Object type mismatch");
        return;
    }
    
    bool success = false;
    char message[128] = "";
    uint8_t errorCode = CTRL_ERR_NONE;
    
    switch (cmd->command) {
        case FLOW_CMD_SET_FLOW_RATE:
            if (cmd->flowRate_mL_min >= 0.0f) {
                success = ControllerManager::setFlowRate(cmd->index, cmd->flowRate_mL_min);
                if (success) {
                    snprintf(message, sizeof(message), "Flow rate updated to %.2f mL/min", cmd->flowRate_mL_min);
                    Serial.printf("[FLOW CTRL %d] Flow rate set to %.2f mL/min\n", 
                                 cmd->index, cmd->flowRate_mL_min);
                } else {
                    strcpy(message, "Failed to update flow rate");
                    errorCode = IPC_ERR_DEVICE_FAIL;
                }
            } else {
                strcpy(message, "Flow rate must be >= 0");
                errorCode = CTRL_ERR_OUT_OF_RANGE;
            }
            break;
            
        case FLOW_CMD_ENABLE:
            success = ControllerManager::enableFlowController(cmd->index);
            if (success) {
                strcpy(message, "Flow controller enabled");
            } else {
                strcpy(message, "Failed to enable controller");
                errorCode = IPC_ERR_DEVICE_FAIL;
            }
            break;
            
        case FLOW_CMD_DISABLE:
            success = ControllerManager::disableFlowController(cmd->index);
            if (success) {
                strcpy(message, "Flow controller disabled");
            } else {
                strcpy(message, "Failed to disable controller");
                errorCode = IPC_ERR_DEVICE_FAIL;
            }
            break;
            
        case FLOW_CMD_MANUAL_DOSE:
            success = ControllerManager::manualFlowDose(cmd->index);
            if (success) {
                strcpy(message, "Manual dose started");
            } else {
                strcpy(message, "Failed to start manual dose");
                errorCode = IPC_ERR_DEVICE_FAIL;
            }
            break;
            
        case FLOW_CMD_RESET_VOLUME:
            success = ControllerManager::resetFlowVolume(cmd->index);
            if (success) {
                strcpy(message, "Cumulative volume reset to 0.0 mL");
            } else {
                strcpy(message, "Failed to reset volume");
                errorCode = IPC_ERR_DEVICE_FAIL;
            }
            break;
            
        case FLOW_CMD_RECALIBRATE:
            // For recalibrate, we just trigger parameter recalculation
            // The calibration values should already be updated via config message
            {
                ManagedFlowController* ctrl = ControllerManager::findFlowController(cmd->index);
                if (ctrl != nullptr && ctrl->controllerInstance != nullptr) {
                    ctrl->controllerInstance->calculateDosingParameters();
                    success = true;
                    strcpy(message, "Recalibration applied");
                } else {
                    strcpy(message, "Controller not found");
                    errorCode = IPC_ERR_DEVICE_FAIL;
                }
            }
            break;
            
        default:
            strcpy(message, "Invalid command");
            errorCode = CTRL_ERR_INVALID_CMD;
            break;
    }
    
    // Send acknowledgment
    ipc_sendControlAck(cmd->index, cmd->objectType, cmd->command, success, errorCode, message);
}
