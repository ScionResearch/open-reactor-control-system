/**
 * @file apiDevices.cpp
 * @brief Device management API implementation
 */

#include "apiDevices.h"
#include "../network/networkManager.h"
#include "../config/ioConfig.h"
#include "../utils/logger.h"
#include "../utils/objectCache.h"
#include "../utils/ipcManager.h"
#include <ArduinoJson.h>

// =============================================================================
// Helper Functions
// =============================================================================

/**
 * @brief Helper function to convert DeviceConfig to IPC_DeviceConfig_t
 */
static void deviceConfigToIPC(const DeviceConfig* device, IPC_DeviceConfig_t* ipcConfig) {
    memset(ipcConfig, 0, sizeof(IPC_DeviceConfig_t));
    
    // Map driver type to IPC device type
    switch (device->driverType) {
        case DEVICE_DRIVER_HAMILTON_PH:  ipcConfig->deviceType = IPC_DEV_HAMILTON_PH; break;
        case DEVICE_DRIVER_HAMILTON_DO:  ipcConfig->deviceType = IPC_DEV_HAMILTON_DO; break;
        case DEVICE_DRIVER_HAMILTON_OD:  ipcConfig->deviceType = IPC_DEV_HAMILTON_OD; break;
        case DEVICE_DRIVER_ALICAT_MFC:   ipcConfig->deviceType = IPC_DEV_ALICAT_MFC; break;
        case DEVICE_DRIVER_PRESSURE_CONTROLLER: ipcConfig->deviceType = IPC_DEV_PRESSURE_CTRL; break;
        default: ipcConfig->deviceType = IPC_DEV_NONE; break;
    }
    
    // Map interface type to IPC bus type
    switch (device->interfaceType) {
        case DEVICE_INTERFACE_MODBUS_RTU:
            ipcConfig->busType = IPC_BUS_MODBUS_RTU;
            ipcConfig->busIndex = device->modbus.portIndex;
            ipcConfig->address = device->modbus.slaveID;
            break;
            
        case DEVICE_INTERFACE_ANALOGUE_IO:
            ipcConfig->busType = IPC_BUS_ANALOG;
            ipcConfig->busIndex = device->analogueIO.dacOutputIndex;
            ipcConfig->address = 0;  // Not used for analog
            break;
            
        case DEVICE_INTERFACE_MOTOR_DRIVEN:
            ipcConfig->busType = IPC_BUS_DIGITAL;
            ipcConfig->busIndex = device->motorDriven.motorIndex;
            ipcConfig->address = 0;  // Not used for motors
            break;
            
        default:
            ipcConfig->busType = IPC_BUS_NONE;
            break;
    }
    
    // Object count will be determined by IO MCU based on device type
    ipcConfig->objectCount = 0;
}

/**
 * @brief Send device control command via IPC
 */
static bool sendDeviceControlCommand(uint16_t controlIndex, DeviceControlCommand command, float setpoint = 0.0f) {
    IPC_DeviceControlCmd_t cmd;
    cmd.transactionId = generateTransactionId();
    cmd.index = controlIndex;
    cmd.objectType = OBJ_T_DEVICE_CONTROL;
    cmd.command = command;
    cmd.setpoint = setpoint;
    memset(cmd.reserved, 0, sizeof(cmd.reserved));
    
    bool sent = ipc.sendPacket(IPC_MSG_DEVICE_CONTROL, (uint8_t*)&cmd, sizeof(cmd));
    
    if (sent) {
        addPendingTransaction(cmd.transactionId, IPC_MSG_DEVICE_CONTROL, IPC_MSG_CONTROL_ACK, 1, controlIndex);
        log(LOG_DEBUG, false, "IPC TX: DeviceControl[%d] command=%d (txn=%d)\n", controlIndex, command, cmd.transactionId);
    }
    
    return sent;
}

// =============================================================================
// Setup Function
// =============================================================================

void setupDevicesAPI(void)
{
    // Get all devices
    server.on("/api/devices", HTTP_GET, handleGetDevices);
    
    // Create new device
    server.on("/api/devices", HTTP_POST, handleCreateDevice);
    
    // Note: GET/PUT/DELETE for specific devices handled dynamically in handleDynamicDeviceRoute
}

// =============================================================================
// Dynamic Route Handlers
// =============================================================================

void handleDynamicDeviceRoute(void)
{
    String uri = server.uri();
    HTTPMethod method = server.method();
    
    // Extract index from URI: /api/devices/{index}
    String indexStr = uri.substring(13);  // Skip "/api/devices/"
    
    // Remove any trailing path or query parameters
    int slashPos = indexStr.indexOf('/');
    if (slashPos > 0) {
        indexStr = indexStr.substring(0, slashPos);
    }
    int queryPos = indexStr.indexOf('?');
    if (queryPos > 0) {
        indexStr = indexStr.substring(0, queryPos);
    }
    
    uint8_t dynamicIndex = indexStr.toInt();
    
    // Validate index range
    if (dynamicIndex < DYNAMIC_INDEX_START || dynamicIndex > DYNAMIC_INDEX_END) {
        server.send(400, "application/json", "{\"error\":\"Invalid device index\"}");
        return;
    }
    
    if (method == HTTP_GET) {
        handleGetDevice();
    } else if (method == HTTP_PUT) {
        handleUpdateDevice();
    } else if (method == HTTP_DELETE) {
        handleDeleteDevice();
    } else {
        server.send(405, "application/json", "{\"error\":\"Method not allowed\"}");
    }
}

void handleDynamicDeviceControlRoute(void)
{
    String uri = server.uri();
    
    // Extract index from URI: /api/device/{index}/setpoint
    int startIdx = 12;  // Skip "/api/device/"
    int endIdx = uri.indexOf('/', startIdx);
    if (endIdx < 0) {
        server.send(400, "application/json", "{\"error\":\"Invalid device control path\"}");
        return;
    }
    
    String indexStr = uri.substring(startIdx, endIdx);
    uint16_t controlIndex = indexStr.toInt();
    
    // Check for /setpoint endpoint
    if (uri.indexOf("/setpoint") > 0) {
        handleSetDeviceSetpoint(controlIndex);
    } else {
        server.send(404, "application/json", "{\"error\":\"Unknown device control endpoint\"}");
    }
}

// =============================================================================
// Device CRUD Handlers
// =============================================================================

void handleGetDevices() {
    DynamicJsonDocument doc(4096);
    
    JsonArray devices = doc.createNestedArray("devices");
    
    for (int i = 0; i < MAX_DEVICES; i++) {
        if (!ioConfig.devices[i].isActive) continue;
        
        JsonObject device = devices.createNestedObject();
        device["dynamicIndex"] = ioConfig.devices[i].dynamicIndex;
        device["interfaceType"] = (uint8_t)ioConfig.devices[i].interfaceType;
        device["driverType"] = (uint8_t)ioConfig.devices[i].driverType;
        device["name"] = ioConfig.devices[i].name;
        
        // Get control object data from cache
        uint8_t controlIndex = getDeviceControlIndex(&ioConfig.devices[i]);
        ObjectCache::CachedObject* controlObj = objectCache.getObject(controlIndex);
        
        if (controlObj && controlObj->valid && controlObj->lastUpdate > 0) {
            device["connected"] = (controlObj->flags & IPC_SENSOR_FLAG_CONNECTED) ? true : false;
            device["fault"] = (controlObj->flags & IPC_SENSOR_FLAG_FAULT) ? true : false;
            device["setpoint"] = controlObj->value;
            device["unit"] = controlObj->unit;
            
            if (controlObj->valueCount > 0) {
                device["actualValue"] = controlObj->additionalValues[0];
            } else {
                device["actualValue"] = controlObj->value;
            }
            
            if (strlen(controlObj->message) > 0) {
                device["message"] = controlObj->message;
            }
        } else {
            device["connected"] = false;
            device["fault"] = false;
            device["setpoint"] = 0.0f;
            device["actualValue"] = 0.0f;
            device["unit"] = "";
        }
        
        // Add interface-specific parameters
        if (ioConfig.devices[i].interfaceType == DEVICE_INTERFACE_MODBUS_RTU) {
            device["portIndex"] = ioConfig.devices[i].modbus.portIndex;
            device["slaveID"] = ioConfig.devices[i].modbus.slaveID;
        } else if (ioConfig.devices[i].interfaceType == DEVICE_INTERFACE_ANALOGUE_IO) {
            device["dacOutputIndex"] = ioConfig.devices[i].analogueIO.dacOutputIndex;
            device["unit"] = ioConfig.devices[i].analogueIO.unit;
            device["scale"] = ioConfig.devices[i].analogueIO.scale;
            device["offset"] = ioConfig.devices[i].analogueIO.offset;
        } else if (ioConfig.devices[i].interfaceType == DEVICE_INTERFACE_MOTOR_DRIVEN) {
            device["usesStepper"] = ioConfig.devices[i].motorDriven.usesStepper;
            device["motorIndex"] = ioConfig.devices[i].motorDriven.motorIndex;
        }
    }
    
    String response;
    serializeJson(doc, response);
    server.send(200, "application/json", response);
}

void handleGetDevice() {
    String uri = server.uri();
    String indexStr = uri.substring(13);
    
    int queryPos = indexStr.indexOf('?');
    if (queryPos > 0) {
        indexStr = indexStr.substring(0, queryPos);
    }
    
    uint8_t dynamicIndex = indexStr.toInt();
    
    if (dynamicIndex < DYNAMIC_INDEX_START || dynamicIndex > DYNAMIC_INDEX_END) {
        server.send(400, "application/json", "{\"error\":\"Invalid device index\"}");
        return;
    }
    
    int8_t deviceIdx = findDeviceByIndex(dynamicIndex);
    if (deviceIdx < 0) {
        server.send(404, "application/json", "{\"error\":\"Device not found\"}");
        return;
    }
    
    StaticJsonDocument<512> doc;
    doc["dynamicIndex"] = ioConfig.devices[deviceIdx].dynamicIndex;
    doc["interfaceType"] = (uint8_t)ioConfig.devices[deviceIdx].interfaceType;
    doc["driverType"] = (uint8_t)ioConfig.devices[deviceIdx].driverType;
    doc["name"] = ioConfig.devices[deviceIdx].name;
    doc["online"] = false;  // TODO: Get actual status
    
    if (ioConfig.devices[deviceIdx].interfaceType == DEVICE_INTERFACE_MODBUS_RTU) {
        doc["portIndex"] = ioConfig.devices[deviceIdx].modbus.portIndex;
        doc["slaveID"] = ioConfig.devices[deviceIdx].modbus.slaveID;
    } else if (ioConfig.devices[deviceIdx].interfaceType == DEVICE_INTERFACE_ANALOGUE_IO) {
        doc["dacOutputIndex"] = ioConfig.devices[deviceIdx].analogueIO.dacOutputIndex;
        doc["unit"] = ioConfig.devices[deviceIdx].analogueIO.unit;
        doc["scale"] = ioConfig.devices[deviceIdx].analogueIO.scale;
        doc["offset"] = ioConfig.devices[deviceIdx].analogueIO.offset;
    } else if (ioConfig.devices[deviceIdx].interfaceType == DEVICE_INTERFACE_MOTOR_DRIVEN) {
        doc["usesStepper"] = ioConfig.devices[deviceIdx].motorDriven.usesStepper;
        doc["motorIndex"] = ioConfig.devices[deviceIdx].motorDriven.motorIndex;
    }
    
    String response;
    serializeJson(doc, response);
    server.send(200, "application/json", response);
}

void handleCreateDevice() {
    DynamicJsonDocument doc(1024);
    DeserializationError error = deserializeJson(doc, server.arg("plain"));
    
    if (error) {
        server.send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
        return;
    }
    
    if (!doc.containsKey("interfaceType") || !doc.containsKey("driverType") || !doc.containsKey("name")) {
        server.send(400, "application/json", "{\"error\":\"Missing required fields\"}");
        return;
    }
    
    uint8_t interfaceType = doc["interfaceType"];
    uint8_t driverType = doc["driverType"];
    String name = doc["name"].as<String>();
    
    if (name.length() == 0 || name.length() > 39) {
        server.send(400, "application/json", "{\"error\":\"Device name must be 1-39 characters\"}");
        return;
    }
    
    int8_t dynamicIndex = allocateDynamicIndex((DeviceDriverType)driverType);
    if (dynamicIndex < 0) {
        server.send(400, "application/json", "{\"error\":\"No available consecutive device slots for this device type\"}");
        return;
    }
    
    int emptySlot = -1;
    for (int i = 0; i < MAX_DEVICES; i++) {
        if (!ioConfig.devices[i].isActive) {
            emptySlot = i;
            break;
        }
    }
    
    if (emptySlot < 0) {
        server.send(500, "application/json", "{\"error\":\"Internal error: no device slot available\"}");
        return;
    }
    
    ioConfig.devices[emptySlot].isActive = true;
    ioConfig.devices[emptySlot].dynamicIndex = dynamicIndex;
    ioConfig.devices[emptySlot].interfaceType = (DeviceInterfaceType)interfaceType;
    ioConfig.devices[emptySlot].driverType = (DeviceDriverType)driverType;
    strncpy(ioConfig.devices[emptySlot].name, name.c_str(), sizeof(ioConfig.devices[emptySlot].name) - 1);
    ioConfig.devices[emptySlot].name[sizeof(ioConfig.devices[emptySlot].name) - 1] = '\0';
    
    if (interfaceType == DEVICE_INTERFACE_MODBUS_RTU) {
        ioConfig.devices[emptySlot].modbus.portIndex = doc["portIndex"] | 0;
        ioConfig.devices[emptySlot].modbus.slaveID = doc["slaveID"] | 1;
    } else if (interfaceType == DEVICE_INTERFACE_ANALOGUE_IO) {
        ioConfig.devices[emptySlot].analogueIO.dacOutputIndex = doc["dacOutputIndex"] | 0;
        strncpy(ioConfig.devices[emptySlot].analogueIO.unit, doc["unit"] | "bar", 
                sizeof(ioConfig.devices[emptySlot].analogueIO.unit) - 1);
        ioConfig.devices[emptySlot].analogueIO.scale = doc["scale"] | 100.0f;
        ioConfig.devices[emptySlot].analogueIO.offset = doc["offset"] | 0.0f;
    } else if (interfaceType == DEVICE_INTERFACE_MOTOR_DRIVEN) {
        ioConfig.devices[emptySlot].motorDriven.usesStepper = doc["usesStepper"] | false;
        ioConfig.devices[emptySlot].motorDriven.motorIndex = doc["motorIndex"] | 27;
    }
    
    saveIOConfig();
    
    IPC_DeviceConfig_t ipcConfig;
    deviceConfigToIPC(&ioConfig.devices[emptySlot], &ipcConfig);
    
    bool sent = sendDeviceCreateCommand(dynamicIndex, &ipcConfig);
    if (!sent) {
        log(LOG_WARNING, true, "Failed to send device create command to IO MCU\n");
    }
    
    log(LOG_INFO, true, "Device created: %s (index %d, driver %d)\n", 
        name.c_str(), dynamicIndex, driverType);
    
    StaticJsonDocument<256> response;
    response["success"] = true;
    response["dynamicIndex"] = dynamicIndex;
    response["message"] = "Device created successfully";
    
    String responseStr;
    serializeJson(response, responseStr);
    server.send(201, "application/json", responseStr);
}

void handleUpdateDevice() {
    String uri = server.uri();
    String indexStr = uri.substring(13);
    
    int queryPos = indexStr.indexOf('?');
    if (queryPos > 0) {
        indexStr = indexStr.substring(0, queryPos);
    }
    
    uint8_t dynamicIndex = indexStr.toInt();
    
    if (dynamicIndex < DYNAMIC_INDEX_START || dynamicIndex > DYNAMIC_INDEX_END) {
        server.send(400, "application/json", "{\"error\":\"Invalid device index\"}");
        return;
    }
    
    int8_t deviceIdx = findDeviceByIndex(dynamicIndex);
    if (deviceIdx < 0) {
        server.send(404, "application/json", "{\"error\":\"Device not found\"}");
        return;
    }
    
    String body = server.arg("plain");
    StaticJsonDocument<512> doc;
    DeserializationError error = deserializeJson(doc, body);
    
    if (error) {
        server.send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
        return;
    }
    
    if (doc.containsKey("name")) {
        const char* name = doc["name"];
        if (strlen(name) == 0 || strlen(name) >= sizeof(ioConfig.devices[deviceIdx].name)) {
            server.send(400, "application/json", "{\"error\":\"Invalid device name\"}");
            return;
        }
        strlcpy(ioConfig.devices[deviceIdx].name, name, sizeof(ioConfig.devices[deviceIdx].name));
    }
    
    DeviceInterfaceType interfaceType = ioConfig.devices[deviceIdx].interfaceType;
    
    if (interfaceType == DEVICE_INTERFACE_MODBUS_RTU) {
        if (doc.containsKey("portIndex")) {
            uint8_t portIndex = doc["portIndex"];
            if (portIndex > 3) {
                server.send(400, "application/json", "{\"error\":\"Invalid port index\"}");
                return;
            }
            ioConfig.devices[deviceIdx].modbus.portIndex = portIndex;
        }
        
        if (doc.containsKey("slaveID")) {
            uint8_t slaveID = doc["slaveID"];
            if (slaveID < 1 || slaveID > 247) {
                server.send(400, "application/json", "{\"error\":\"Invalid slave ID\"}");
                return;
            }
            ioConfig.devices[deviceIdx].modbus.slaveID = slaveID;
        }
    } 
    else if (interfaceType == DEVICE_INTERFACE_ANALOGUE_IO) {
        if (doc.containsKey("dacOutputIndex")) {
            ioConfig.devices[deviceIdx].analogueIO.dacOutputIndex = doc["dacOutputIndex"];
        }
        if (doc.containsKey("unit")) {
            strlcpy(ioConfig.devices[deviceIdx].analogueIO.unit, doc["unit"], 
                    sizeof(ioConfig.devices[deviceIdx].analogueIO.unit));
        }
        if (doc.containsKey("scale")) {
            ioConfig.devices[deviceIdx].analogueIO.scale = doc["scale"];
        }
        if (doc.containsKey("offset")) {
            ioConfig.devices[deviceIdx].analogueIO.offset = doc["offset"];
        }
    } 
    else if (interfaceType == DEVICE_INTERFACE_MOTOR_DRIVEN) {
        if (doc.containsKey("usesStepper")) {
            ioConfig.devices[deviceIdx].motorDriven.usesStepper = doc["usesStepper"];
        }
        if (doc.containsKey("motorIndex")) {
            uint8_t motorIndex = doc["motorIndex"];
            if (motorIndex != 26 && (motorIndex < 27 || motorIndex > 30)) {
                server.send(400, "application/json", "{\"error\":\"Invalid motor index\"}");
                return;
            }
            ioConfig.devices[deviceIdx].motorDriven.motorIndex = motorIndex;
        }
    }
    
    saveIOConfig();

    IPC_DeviceConfig_t ipcConfig;
    deviceConfigToIPC(&ioConfig.devices[deviceIdx], &ipcConfig);

    bool sent = sendDeviceConfigCommand(dynamicIndex, &ipcConfig);
    if (!sent) {
        log(LOG_WARNING, true, "Failed to send device config update to IO MCU\n");
    }

    // Handle pressure controller calibration update
    if (ioConfig.devices[deviceIdx].driverType == DEVICE_DRIVER_PRESSURE_CONTROLLER &&
        ioConfig.devices[deviceIdx].interfaceType == DEVICE_INTERFACE_ANALOGUE_IO) {
        
        IPC_ConfigPressureCtrl_t calibCfg;
        calibCfg.controlIndex = getDeviceControlIndex(&ioConfig.devices[deviceIdx]);
        calibCfg.dacIndex = ioConfig.devices[deviceIdx].analogueIO.dacOutputIndex;
        strncpy(calibCfg.unit, ioConfig.devices[deviceIdx].analogueIO.unit, sizeof(calibCfg.unit) - 1);
        calibCfg.unit[sizeof(calibCfg.unit) - 1] = '\0';
        calibCfg.scale = ioConfig.devices[deviceIdx].analogueIO.scale;
        calibCfg.offset = ioConfig.devices[deviceIdx].analogueIO.offset;
        
        bool calibSent = ipc.sendPacket(IPC_MSG_CONFIG_PRESSURE_CTRL, (uint8_t*)&calibCfg, sizeof(calibCfg));
        if (calibSent) {
            log(LOG_INFO, false, "Sent pressure controller calibration update: scale=%.6f, offset=%.2f, unit=%s\n",
                calibCfg.scale, calibCfg.offset, calibCfg.unit);
        } else {
            log(LOG_WARNING, true, "Failed to send pressure controller calibration update\n");
        }
    }

    log(LOG_INFO, true, "Device updated: %s (index %d)\n", 
        ioConfig.devices[deviceIdx].name, dynamicIndex);

    StaticJsonDocument<256> response;
    response["success"] = true;
    response["message"] = "Device updated successfully";
    response["dynamicIndex"] = dynamicIndex;
    
    String responseStr;
    serializeJson(response, responseStr);
    server.send(200, "application/json", responseStr);
}

void handleDeleteDevice() {
    String uri = server.uri();
    String indexStr = uri.substring(13);
    
    int queryPos = indexStr.indexOf('?');
    if (queryPos > 0) {
        indexStr = indexStr.substring(0, queryPos);
    }
    
    uint8_t dynamicIndex = indexStr.toInt();
    
    if (dynamicIndex < DYNAMIC_INDEX_START || dynamicIndex > DYNAMIC_INDEX_END) {
        server.send(400, "application/json", "{\"error\":\"Invalid device index\"}");
        return;
    }
    
    int8_t deviceIdx = findDeviceByIndex(dynamicIndex);
    if (deviceIdx < 0) {
        server.send(404, "application/json", "{\"error\":\"Device not found\"}");
        return;
    }
    
    String deviceName = String(ioConfig.devices[deviceIdx].name);
    
    freeDynamicIndex(dynamicIndex);
    objectCache.invalidateRange(dynamicIndex, 4);
    
    bool sent = sendDeviceDeleteCommand(dynamicIndex);
    if (!sent) {
        log(LOG_WARNING, true, "Failed to send device delete command to IO MCU\n");
    }
    
    saveIOConfig();
    
    log(LOG_INFO, true, "Device deleted: %s (index %d), cache invalidated\n", deviceName.c_str(), dynamicIndex);
    
    server.send(200, "application/json", "{\"success\":true,\"message\":\"Device deleted successfully\"}");
}

// =============================================================================
// Device Control Handlers
// =============================================================================

void handleSetDeviceSetpoint(uint16_t controlIndex) {
    if (controlIndex < 50 || controlIndex >= 70) {
        server.send(400, "application/json", "{\"error\":\"Invalid control index\"}");
        return;
    }
    
    StaticJsonDocument<256> doc;
    DeserializationError error = deserializeJson(doc, server.arg("plain"));
    
    if (error) {
        server.send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
        return;
    }
    
    if (!doc.containsKey("setpoint")) {
        server.send(400, "application/json", "{\"error\":\"Missing setpoint parameter\"}");
        return;
    }
    
    float setpoint = doc["setpoint"];
    
    bool sent = sendDeviceControlCommand(controlIndex, DEV_CMD_SET_SETPOINT, setpoint);
    
    if (sent) {
        log(LOG_INFO, false, "Set device %d setpoint: %.2f\n", controlIndex, setpoint);
        server.send(200, "application/json", "{\"success\":true,\"message\":\"Setpoint command sent\"}");
    } else {
        log(LOG_WARNING, false, "Failed to set device %d setpoint: IPC queue full\n", controlIndex);
        server.send(503, "application/json", "{\"error\":\"IPC queue full, try again\"}");
    }
}
