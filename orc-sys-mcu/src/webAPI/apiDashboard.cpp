/**
 * @file apiDashboard.cpp
 * @brief Dashboard API implementation
 * 
 * Provides endpoints for dashboard monitoring and global controls.
 * Layout persistence is intentional (user-triggered) to protect flash.
 */

#include "apiDashboard.h"
#include "../network/networkManager.h"
#include "../config/ioConfig.h"
#include "../utils/logger.h"
#include "../utils/objectCache.h"
#include "../utils/ipcManager.h"
#include "../utils/timeManager.h"
#include <ArduinoJson.h>

// =============================================================================
// API Setup
// =============================================================================

void setupDashboardAPI(void)
{
    // Dashboard data
    server.on("/api/dashboard", HTTP_GET, handleGetDashboard);
    server.on("/api/dashboard/alarms", HTTP_GET, handleGetAlarms);
    
    // Layout configuration (intentional save only)
    server.on("/api/dashboard/layout", HTTP_GET, handleGetDashboardLayout);
    server.on("/api/dashboard/layout", HTTP_POST, handleSaveDashboardLayout);
    
    // Global controls
    server.on("/api/dashboard/enable-all", HTTP_POST, handleEnableAll);
    server.on("/api/dashboard/pause", HTTP_POST, handlePauseControllers);
    server.on("/api/dashboard/disable-all", HTTP_POST, handleDisableAll);
    server.on("/api/dashboard/clear-volumes", HTTP_POST, handleClearVolumes);
    
    log(LOG_INFO, false, "[API] Dashboard endpoints registered\n");
}

// =============================================================================
// Helper: Add object to dashboard array
// =============================================================================

static void addDashboardObject(JsonArray& arr, const char* type, uint8_t index, 
                                const char* name, ObjectCache::CachedObject* cached)
{
    JsonObject obj = arr.createNestedObject();
    obj["type"] = type;
    obj["index"] = index;
    obj["name"] = name;
    
    if (cached && cached->valid && cached->lastUpdate > 0) {
        obj["value"] = cached->value;
        obj["unit"] = cached->unit;
        obj["fault"] = (cached->flags & IPC_SENSOR_FLAG_FAULT) ? true : false;
        obj["running"] = (cached->flags & IPC_SENSOR_FLAG_RUNNING) ? true : false;
        obj["direction"] = (cached->flags & IPC_SENSOR_FLAG_DIRECTION) ? true : false;
        obj["online"] = true;
        
        // Include additional values if present (e.g., motor current)
        if (cached->valueCount > 0) {
            JsonArray extras = obj.createNestedArray("additionalValues");
            for (uint8_t i = 0; i < cached->valueCount && i < 4; i++) {
                JsonObject extra = extras.createNestedObject();
                extra["value"] = cached->additionalValues[i];
                extra["unit"] = cached->additionalUnits[i];
            }
        }
        
        // Add message if present
        if (strlen(cached->message) > 0) {
            obj["message"] = cached->message;
        }
    } else {
        obj["value"] = 0;
        obj["unit"] = "";
        obj["fault"] = false;
        obj["online"] = false;
    }
}

// =============================================================================
// Dashboard Data Endpoints
// =============================================================================

void handleGetDashboard()
{
    // Larger buffer for full dashboard response
    DynamicJsonDocument* doc = new DynamicJsonDocument(16384);
    if (!doc) {
        server.send(500, "application/json", "{\"error\":\"Memory allocation failed\"}");
        return;
    }
    
    JsonArray objects = doc->createNestedArray("objects");
    uint8_t faultCount = 0;
    
    // === ADC Inputs (indices 0-7) ===
    for (uint8_t i = 0; i < MAX_ADC_INPUTS; i++) {
        if (ioConfig.adcInputs[i].showOnDashboard) {
            ObjectCache::CachedObject* cached = objectCache.getObject(i);
            addDashboardObject(objects, "adc", i, ioConfig.adcInputs[i].name, cached);
            if (cached && (cached->flags & IPC_SENSOR_FLAG_FAULT)) faultCount++;
        }
    }
    
    // === DAC Outputs (indices 8-9) ===
    for (uint8_t i = 0; i < MAX_DAC_OUTPUTS; i++) {
        if (ioConfig.dacOutputs[i].showOnDashboard) {
            uint8_t objIndex = 8 + i;
            ObjectCache::CachedObject* cached = objectCache.getObject(objIndex);
            addDashboardObject(objects, "dac", objIndex, ioConfig.dacOutputs[i].name, cached);
            if (cached && (cached->flags & IPC_SENSOR_FLAG_FAULT)) faultCount++;
        }
    }
    
    // === RTD Sensors (indices 10-12) ===
    for (uint8_t i = 0; i < MAX_RTD_SENSORS; i++) {
        if (ioConfig.rtdSensors[i].showOnDashboard) {
            uint8_t objIndex = 10 + i;
            ObjectCache::CachedObject* cached = objectCache.getObject(objIndex);
            addDashboardObject(objects, "rtd", objIndex, ioConfig.rtdSensors[i].name, cached);
            if (cached && (cached->flags & IPC_SENSOR_FLAG_FAULT)) faultCount++;
        }
    }
    
    // === GPIO Inputs (indices 13-20) ===
    for (uint8_t i = 0; i < MAX_GPIO; i++) {
        if (ioConfig.gpio[i].showOnDashboard) {
            uint8_t objIndex = 13 + i;
            ObjectCache::CachedObject* cached = objectCache.getObject(objIndex);
            addDashboardObject(objects, "gpio", objIndex, ioConfig.gpio[i].name, cached);
            if (cached && (cached->flags & IPC_SENSOR_FLAG_FAULT)) faultCount++;
        }
    }
    
    // === Digital Outputs (indices 21-25) ===
    for (uint8_t i = 0; i < MAX_DIGITAL_OUTPUTS; i++) {
        if (ioConfig.digitalOutputs[i].showOnDashboard) {
            uint8_t objIndex = 21 + i;
            ObjectCache::CachedObject* cached = objectCache.getObject(objIndex);
            addDashboardObject(objects, "digital_output", objIndex, ioConfig.digitalOutputs[i].name, cached);
            if (cached && (cached->flags & IPC_SENSOR_FLAG_FAULT)) faultCount++;
        }
    }
    
    // === Stepper Motor (index 26) ===
    if (ioConfig.stepperMotor.showOnDashboard) {
        ObjectCache::CachedObject* cached = objectCache.getObject(26);
        addDashboardObject(objects, "stepper", 26, ioConfig.stepperMotor.name, cached);
        if (cached && (cached->flags & IPC_SENSOR_FLAG_FAULT)) faultCount++;
    }
    
    // === DC Motors (indices 27-30) ===
    for (uint8_t i = 0; i < MAX_DC_MOTORS; i++) {
        if (ioConfig.dcMotors[i].showOnDashboard) {
            uint8_t objIndex = 27 + i;
            ObjectCache::CachedObject* cached = objectCache.getObject(objIndex);
            addDashboardObject(objects, "dc_motor", objIndex, ioConfig.dcMotors[i].name, cached);
            if (cached && (cached->flags & IPC_SENSOR_FLAG_FAULT)) faultCount++;
        }
    }
    
    // === Energy Sensors (indices 31-32) ===
    for (uint8_t i = 0; i < MAX_ENERGY_SENSORS; i++) {
        if (ioConfig.energySensors[i].showOnDashboard) {
            uint8_t objIndex = 31 + i;
            ObjectCache::CachedObject* cached = objectCache.getObject(objIndex);
            addDashboardObject(objects, "energy", objIndex, ioConfig.energySensors[i].name, cached);
            if (cached && (cached->flags & IPC_SENSOR_FLAG_FAULT)) faultCount++;
        }
    }
    
    // === Temperature Controllers (indices 40-42) ===
    for (uint8_t i = 0; i < MAX_TEMP_CONTROLLERS; i++) {
        if (ioConfig.tempControllers[i].isActive && ioConfig.tempControllers[i].showOnDashboard) {
            uint8_t objIndex = 40 + i;
            ObjectCache::CachedObject* cached = objectCache.getObject(objIndex);
            addDashboardObject(objects, "temp_controller", objIndex, ioConfig.tempControllers[i].name, cached);
            // Add controller-specific data to last object
            JsonObject ctrlObj = objects[objects.size() - 1];
            ctrlObj["setpoint"] = ioConfig.tempControllers[i].setpoint;
            ctrlObj["enabled"] = cached && (cached->flags & IPC_SENSOR_FLAG_RUNNING);
            ctrlObj["processValue"] = cached ? cached->value : 0;
            if (cached && (cached->flags & IPC_SENSOR_FLAG_FAULT)) faultCount++;
        }
    }
    
    // === pH Controller (index 43) ===
    if (ioConfig.phController.isActive && ioConfig.phController.showOnDashboard) {
        ObjectCache::CachedObject* cached = objectCache.getObject(43);
        addDashboardObject(objects, "ph_controller", 43, ioConfig.phController.name, cached);
        // Add controller-specific data
        JsonObject ctrlObj = objects[objects.size() - 1];
        ctrlObj["setpoint"] = ioConfig.phController.setpoint;
        ctrlObj["enabled"] = cached && (cached->flags & IPC_SENSOR_FLAG_RUNNING);
        ctrlObj["processValue"] = cached ? cached->value : 0;
        // Cumulative volume from cached additionalValues[2]
        ctrlObj["cumulativeAcidVolume"] = (cached && cached->valueCount > 2) ? cached->additionalValues[1] : 0.0f;
        ctrlObj["cumulativeBaseVolume"] = (cached && cached->valueCount > 2) ? cached->additionalValues[2] : 0.0f;
        if (cached && (cached->flags & IPC_SENSOR_FLAG_FAULT)) faultCount++;
    }
    
    // === Flow Controllers (indices 44-47) ===
    for (uint8_t i = 0; i < MAX_FLOW_CONTROLLERS; i++) {
        if (ioConfig.flowControllers[i].isActive && ioConfig.flowControllers[i].showOnDashboard) {
            uint8_t objIndex = 44 + i;
            ObjectCache::CachedObject* cached = objectCache.getObject(objIndex);
            addDashboardObject(objects, "flow_controller", objIndex, ioConfig.flowControllers[i].name, cached);
            // Add controller-specific data
            JsonObject ctrlObj = objects[objects.size() - 1];
            ctrlObj["setpoint"] = ioConfig.flowControllers[i].flowRate_mL_min;  // flowRate is the setpoint
            ctrlObj["enabled"] = cached && (cached->flags & IPC_SENSOR_FLAG_RUNNING);
            ctrlObj["processValue"] = cached ? cached->value : 0;
            // Cumulative volume from cached additionalValues[2]
            ctrlObj["cumulativeVolume"] = (cached && cached->valueCount > 2) ? cached->additionalValues[2] : 0.0f;
            if (cached && (cached->flags & IPC_SENSOR_FLAG_FAULT)) faultCount++;
        }
    }
    
    // === DO Controller (index 48) ===
    if (ioConfig.doController.isActive && ioConfig.doController.showOnDashboard) {
        ObjectCache::CachedObject* cached = objectCache.getObject(48);
        addDashboardObject(objects, "do_controller", 48, ioConfig.doController.name, cached);
        // Add controller-specific data
        JsonObject ctrlObj = objects[objects.size() - 1];
        ctrlObj["setpoint"] = ioConfig.doController.setpoint_mg_L;
        ctrlObj["enabled"] = cached && (cached->flags & IPC_SENSOR_FLAG_RUNNING);
        ctrlObj["processValue"] = cached ? cached->value : 0;
        // Get stirrer data from stepper motor cache (index 26)
        ObjectCache::CachedObject* stirrerCache = objectCache.getObject(26);
        ctrlObj["stirrerRpm"] = stirrerCache ? stirrerCache->value : 0;
        if (cached && (cached->flags & IPC_SENSOR_FLAG_FAULT)) faultCount++;
    }
    
    // === Device Sensors (indices 70-99) ===
    for (uint8_t i = 0; i < MAX_DEVICE_SENSORS; i++) {
        if (ioConfig.deviceSensors[i].showOnDashboard) {
            uint8_t objIndex = 70 + i;
            ObjectCache::CachedObject* cached = objectCache.getObject(objIndex);
            // Use override name if set, otherwise use cached name
            const char* name = strlen(ioConfig.deviceSensors[i].name) > 0 ? 
                              ioConfig.deviceSensors[i].name : 
                              (cached ? cached->name : "Unknown Device");
            addDashboardObject(objects, "device_sensor", objIndex, name, cached);
        }
    }
    
    // =======================================================================
    // System-wide alarm detection (ALL objects, not just dashboard-visible)
    // =======================================================================
    JsonArray alarms = doc->createNestedArray("alarms");
    faultCount = 0;  // Reset and count from ALL objects
    
    auto checkFaultGlobal = [&](uint8_t index, const char* type, const char* name) {
        ObjectCache::CachedObject* cached = objectCache.getObject(index);
        if (cached && cached->valid && (cached->flags & IPC_SENSOR_FLAG_FAULT)) {
            JsonObject alarm = alarms.createNestedObject();
            alarm["type"] = type;
            alarm["index"] = index;
            alarm["name"] = name;
            alarm["message"] = strlen(cached->message) > 0 ? cached->message : "Fault detected";
            faultCount++;
        }
    };
    
    // Check ALL objects for faults
    for (uint8_t i = 0; i < MAX_ADC_INPUTS; i++) {
        checkFaultGlobal(i, "adc", ioConfig.adcInputs[i].name);
    }
    for (uint8_t i = 0; i < MAX_DAC_OUTPUTS; i++) {
        checkFaultGlobal(8 + i, "dac", ioConfig.dacOutputs[i].name);
    }
    for (uint8_t i = 0; i < MAX_RTD_SENSORS; i++) {
        checkFaultGlobal(10 + i, "rtd", ioConfig.rtdSensors[i].name);
    }
    for (uint8_t i = 0; i < MAX_GPIO; i++) {
        checkFaultGlobal(13 + i, "gpio", ioConfig.gpio[i].name);
    }
    for (uint8_t i = 0; i < MAX_DIGITAL_OUTPUTS; i++) {
        checkFaultGlobal(21 + i, "digital_output", ioConfig.digitalOutputs[i].name);
    }
    checkFaultGlobal(26, "stepper", ioConfig.stepperMotor.name);
    for (uint8_t i = 0; i < MAX_DC_MOTORS; i++) {
        checkFaultGlobal(27 + i, "dc_motor", ioConfig.dcMotors[i].name);
    }
    for (uint8_t i = 0; i < MAX_ENERGY_SENSORS; i++) {
        checkFaultGlobal(31 + i, "energy", ioConfig.energySensors[i].name);
    }
    for (uint8_t i = 0; i < MAX_TEMP_CONTROLLERS; i++) {
        if (ioConfig.tempControllers[i].isActive) {
            checkFaultGlobal(40 + i, "temp_controller", ioConfig.tempControllers[i].name);
        }
    }
    if (ioConfig.phController.isActive) {
        checkFaultGlobal(43, "ph_controller", ioConfig.phController.name);
    }
    for (uint8_t i = 0; i < MAX_FLOW_CONTROLLERS; i++) {
        if (ioConfig.flowControllers[i].isActive) {
            checkFaultGlobal(44 + i, "flow_controller", ioConfig.flowControllers[i].name);
        }
    }
    if (ioConfig.doController.isActive) {
        checkFaultGlobal(48, "do_controller", ioConfig.doController.name);
    }
    // Device sensors
    for (uint8_t i = 0; i < MAX_DEVICE_SENSORS; i++) {
        ObjectCache::CachedObject* cached = objectCache.getObject(70 + i);
        if (cached && cached->valid) {
            const char* name = strlen(ioConfig.deviceSensors[i].name) > 0 ? 
                              ioConfig.deviceSensors[i].name : cached->name;
            checkFaultGlobal(70 + i, "device_sensor", name);
        }
    }
    
    // Add summary info
    (*doc)["faultCount"] = faultCount;
    (*doc)["objectCount"] = objects.size();
    (*doc)["timestamp"] = millis();
    
    // Send response
    String response;
    serializeJson(*doc, response);
    delete doc;
    
    server.send(200, "application/json", response);
}

void handleGetAlarms()
{
    DynamicJsonDocument doc(4096);
    JsonArray alarms = doc.createNestedArray("alarms");
    uint8_t faultCount = 0;
    
    // Scan all object types for faults
    auto checkFault = [&](uint8_t index, const char* type, const char* name) {
        ObjectCache::CachedObject* cached = objectCache.getObject(index);
        if (cached && cached->valid && (cached->flags & IPC_SENSOR_FLAG_FAULT)) {
            JsonObject alarm = alarms.createNestedObject();
            alarm["type"] = type;
            alarm["index"] = index;
            alarm["name"] = name;
            alarm["message"] = cached->message;
            faultCount++;
        }
    };
    
    // Check all object types
    for (uint8_t i = 0; i < MAX_ADC_INPUTS; i++) {
        checkFault(i, "adc", ioConfig.adcInputs[i].name);
    }
    for (uint8_t i = 0; i < MAX_DAC_OUTPUTS; i++) {
        checkFault(8 + i, "dac", ioConfig.dacOutputs[i].name);
    }
    for (uint8_t i = 0; i < MAX_RTD_SENSORS; i++) {
        checkFault(10 + i, "rtd", ioConfig.rtdSensors[i].name);
    }
    for (uint8_t i = 0; i < MAX_GPIO; i++) {
        checkFault(13 + i, "gpio", ioConfig.gpio[i].name);
    }
    for (uint8_t i = 0; i < MAX_DIGITAL_OUTPUTS; i++) {
        checkFault(21 + i, "digital_output", ioConfig.digitalOutputs[i].name);
    }
    checkFault(26, "stepper", ioConfig.stepperMotor.name);
    for (uint8_t i = 0; i < MAX_DC_MOTORS; i++) {
        checkFault(27 + i, "dc_motor", ioConfig.dcMotors[i].name);
    }
    for (uint8_t i = 0; i < MAX_ENERGY_SENSORS; i++) {
        checkFault(31 + i, "energy", ioConfig.energySensors[i].name);
    }
    for (uint8_t i = 0; i < MAX_TEMP_CONTROLLERS; i++) {
        if (ioConfig.tempControllers[i].isActive) {
            checkFault(40 + i, "temp_controller", ioConfig.tempControllers[i].name);
        }
    }
    if (ioConfig.phController.isActive) {
        checkFault(43, "ph_controller", ioConfig.phController.name);
    }
    for (uint8_t i = 0; i < MAX_FLOW_CONTROLLERS; i++) {
        if (ioConfig.flowControllers[i].isActive) {
            checkFault(44 + i, "flow_controller", ioConfig.flowControllers[i].name);
        }
    }
    if (ioConfig.doController.isActive) {
        checkFault(48, "do_controller", ioConfig.doController.name);
    }
    
    doc["faultCount"] = faultCount;
    doc["timestamp"] = millis();
    
    String response;
    serializeJson(doc, response);
    server.send(200, "application/json", response);
}

// =============================================================================
// Layout Configuration Endpoints
// =============================================================================

void handleGetDashboardLayout()
{
    DynamicJsonDocument doc(4096);
    JsonArray tiles = doc.createNestedArray("tiles");
    
    // Return ordered tile layout from config
    for (uint8_t i = 0; i < MAX_DASHBOARD_TILES; i++) {
        if (ioConfig.dashboardLayout.tiles[i].inUse) {
            JsonObject tile = tiles.createNestedObject();
            tile["type"] = ioConfig.dashboardLayout.tiles[i].objectType;
            tile["index"] = ioConfig.dashboardLayout.tiles[i].objectIndex;
            tile["position"] = i;
        }
    }
    
    doc["tileCount"] = tiles.size();
    
    String response;
    serializeJson(doc, response);
    server.send(200, "application/json", response);
}

void handleSaveDashboardLayout()
{
    if (!server.hasArg("plain")) {
        server.send(400, "application/json", "{\"error\":\"No body provided\"}");
        return;
    }
    
    DynamicJsonDocument doc(4096);
    DeserializationError error = deserializeJson(doc, server.arg("plain"));
    if (error) {
        server.send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
        return;
    }
    
    // Clear existing layout
    memset(&ioConfig.dashboardLayout, 0, sizeof(ioConfig.dashboardLayout));
    
    // Parse tile array
    if (doc.containsKey("tiles")) {
        JsonArray tiles = doc["tiles"];
        uint8_t tileIdx = 0;
        
        for (JsonObject tile : tiles) {
            if (tileIdx >= MAX_DASHBOARD_TILES) break;
            
            const char* type = tile["type"] | "";
            uint8_t index = tile["index"] | 0;
            
            // Store tile info
            strncpy(ioConfig.dashboardLayout.tiles[tileIdx].objectType, type, 
                    sizeof(ioConfig.dashboardLayout.tiles[tileIdx].objectType) - 1);
            ioConfig.dashboardLayout.tiles[tileIdx].objectIndex = index;
            ioConfig.dashboardLayout.tiles[tileIdx].inUse = true;
            
            tileIdx++;
        }
    }
    
    // Save to flash (intentional save)
    saveIOConfig();
    log(LOG_INFO, false, "[DASHBOARD] Layout saved (%d tiles)\n", 
        doc["tiles"].size());
    
    server.send(200, "application/json", "{\"success\":true}");
}

// =============================================================================
// Global Control Endpoints
// =============================================================================

void handleEnableAll()
{
    log(LOG_INFO, false, "[DASHBOARD] Enable All Controllers requested\n");
    
    uint8_t enabledCount = 0;
    uint8_t failedCount = 0;
    
    // Enable temperature controllers via IPC
    for (uint8_t i = 0; i < MAX_TEMP_CONTROLLERS; i++) {
        if (ioConfig.tempControllers[i].isActive) {
            IPC_TempControllerControl_t cmd;
            memset(&cmd, 0, sizeof(cmd));
            cmd.transactionId = generateTransactionId();
            cmd.index = 40 + i;
            cmd.objectType = OBJ_T_TEMPERATURE_CONTROL;
            cmd.command = TEMP_CTRL_CMD_ENABLE;
            
            if (ipc.sendPacket(IPC_MSG_CONTROL_WRITE, (uint8_t*)&cmd, sizeof(cmd))) {
                ioConfig.tempControllers[i].enabled = true;
                enabledCount++;
                log(LOG_INFO, false, "  Enabled temp controller %d\n", 40 + i);
            } else {
                failedCount++;
            }
        }
    }
    
    // Enable pH controller via IPC
    if (ioConfig.phController.isActive) {
        IPC_pHControllerControl_t cmd;
        memset(&cmd, 0, sizeof(cmd));
        cmd.transactionId = generateTransactionId();
        cmd.index = 43;
        cmd.objectType = OBJ_T_PH_CONTROL;
        cmd.command = PH_CMD_ENABLE;
        
        if (ipc.sendPacket(IPC_MSG_CONTROL_WRITE, (uint8_t*)&cmd, sizeof(cmd))) {
            ioConfig.phController.enabled = true;
            enabledCount++;
            log(LOG_INFO, false, "  Enabled pH controller\n");
        } else {
            failedCount++;
        }
    }
    
    // Enable flow controllers via IPC
    for (uint8_t i = 0; i < MAX_FLOW_CONTROLLERS; i++) {
        if (ioConfig.flowControllers[i].isActive) {
            IPC_FlowControllerControl_t cmd;
            memset(&cmd, 0, sizeof(cmd));
            cmd.transactionId = generateTransactionId();
            cmd.index = 44 + i;
            cmd.objectType = OBJ_T_FLOW_CONTROL;
            cmd.command = FLOW_CMD_ENABLE;
            
            if (ipc.sendPacket(IPC_MSG_CONTROL_WRITE, (uint8_t*)&cmd, sizeof(cmd))) {
                ioConfig.flowControllers[i].enabled = true;
                enabledCount++;
                log(LOG_INFO, false, "  Enabled flow controller %d\n", 44 + i);
            } else {
                failedCount++;
            }
        }
    }
    
    // Enable DO controller via IPC
    if (ioConfig.doController.isActive) {
        IPC_DOControllerControl_t cmd;
        memset(&cmd, 0, sizeof(cmd));
        cmd.transactionId = generateTransactionId();
        cmd.index = 48;
        cmd.objectType = OBJ_T_DISSOLVED_OXYGEN_CONTROL;
        cmd.command = DO_CMD_ENABLE;
        
        if (ipc.sendPacket(IPC_MSG_CONTROL_WRITE, (uint8_t*)&cmd, sizeof(cmd))) {
            ioConfig.doController.enabled = true;
            enabledCount++;
            log(LOG_INFO, false, "  Enabled DO controller\n");
        } else {
            failedCount++;
        }
    }
    
    DynamicJsonDocument doc(256);
    doc["success"] = true;
    doc["enabled"] = enabledCount;
    doc["failed"] = failedCount;
    
    // Return current RTC timestamp for run timer
    DateTime dt;
    if (getGlobalDateTime(dt)) {
        char timestampStr[20];
        snprintf(timestampStr, sizeof(timestampStr), "%04d-%02d-%02d %02d:%02d:%02d",
                 dt.year, dt.month, dt.day, dt.hour, dt.minute, dt.second);
        doc["startTime"] = timestampStr;
    }
    
    String response;
    serializeJson(doc, response);
    server.send(200, "application/json", response);
}

void handlePauseControllers()
{
    log(LOG_INFO, false, "[DASHBOARD] Pause Controllers requested (keeps temp running)\n");
    
    uint8_t pausedCount = 0;
    uint8_t failedCount = 0;
    
    // Disable pH controller via IPC (but NOT temperature)
    if (ioConfig.phController.isActive) {
        IPC_pHControllerControl_t cmd;
        memset(&cmd, 0, sizeof(cmd));
        cmd.transactionId = generateTransactionId();
        cmd.index = 43;
        cmd.objectType = OBJ_T_PH_CONTROL;
        cmd.command = PH_CMD_DISABLE;
        
        if (ipc.sendPacket(IPC_MSG_CONTROL_WRITE, (uint8_t*)&cmd, sizeof(cmd))) {
            ioConfig.phController.enabled = false;
            pausedCount++;
            log(LOG_INFO, false, "  Paused pH controller\n");
        } else {
            failedCount++;
        }
    }
    
    // Disable flow controllers via IPC
    for (uint8_t i = 0; i < MAX_FLOW_CONTROLLERS; i++) {
        if (ioConfig.flowControllers[i].isActive) {
            IPC_FlowControllerControl_t cmd;
            memset(&cmd, 0, sizeof(cmd));
            cmd.transactionId = generateTransactionId();
            cmd.index = 44 + i;
            cmd.objectType = OBJ_T_FLOW_CONTROL;
            cmd.command = FLOW_CMD_DISABLE;
            
            if (ipc.sendPacket(IPC_MSG_CONTROL_WRITE, (uint8_t*)&cmd, sizeof(cmd))) {
                ioConfig.flowControllers[i].enabled = false;
                pausedCount++;
                log(LOG_INFO, false, "  Paused flow controller %d\n", 44 + i);
            } else {
                failedCount++;
            }
        }
    }
    
    // Disable DO controller via IPC
    if (ioConfig.doController.isActive) {
        IPC_DOControllerControl_t cmd;
        memset(&cmd, 0, sizeof(cmd));
        cmd.transactionId = generateTransactionId();
        cmd.index = 48;
        cmd.objectType = OBJ_T_DISSOLVED_OXYGEN_CONTROL;
        cmd.command = DO_CMD_DISABLE;
        
        if (ipc.sendPacket(IPC_MSG_CONTROL_WRITE, (uint8_t*)&cmd, sizeof(cmd))) {
            ioConfig.doController.enabled = false;
            pausedCount++;
            log(LOG_INFO, false, "  Paused DO controller\n");
        } else {
            failedCount++;
        }
    }
    
    DynamicJsonDocument doc(256);
    doc["success"] = true;
    doc["paused"] = pausedCount;
    doc["failed"] = failedCount;
    
    String response;
    serializeJson(doc, response);
    server.send(200, "application/json", response);
}

void handleDisableAll()
{
    log(LOG_INFO, false, "[DASHBOARD] Disable All Controllers requested\n");
    
    uint8_t disabledCount = 0;
    uint8_t failedCount = 0;
    
    // Disable temperature controllers via IPC
    for (uint8_t i = 0; i < MAX_TEMP_CONTROLLERS; i++) {
        if (ioConfig.tempControllers[i].isActive) {
            IPC_TempControllerControl_t cmd;
            memset(&cmd, 0, sizeof(cmd));
            cmd.transactionId = generateTransactionId();
            cmd.index = 40 + i;
            cmd.objectType = OBJ_T_TEMPERATURE_CONTROL;
            cmd.command = TEMP_CTRL_CMD_DISABLE;
            
            if (ipc.sendPacket(IPC_MSG_CONTROL_WRITE, (uint8_t*)&cmd, sizeof(cmd))) {
                ioConfig.tempControllers[i].enabled = false;
                disabledCount++;
                log(LOG_INFO, false, "  Disabled temp controller %d\n", 40 + i);
            } else {
                failedCount++;
            }
        }
    }
    
    // Disable pH controller via IPC
    if (ioConfig.phController.isActive) {
        IPC_pHControllerControl_t cmd;
        memset(&cmd, 0, sizeof(cmd));
        cmd.transactionId = generateTransactionId();
        cmd.index = 43;
        cmd.objectType = OBJ_T_PH_CONTROL;
        cmd.command = PH_CMD_DISABLE;
        
        if (ipc.sendPacket(IPC_MSG_CONTROL_WRITE, (uint8_t*)&cmd, sizeof(cmd))) {
            ioConfig.phController.enabled = false;
            disabledCount++;
            log(LOG_INFO, false, "  Disabled pH controller\n");
        } else {
            failedCount++;
        }
    }
    
    // Disable flow controllers via IPC
    for (uint8_t i = 0; i < MAX_FLOW_CONTROLLERS; i++) {
        if (ioConfig.flowControllers[i].isActive) {
            IPC_FlowControllerControl_t cmd;
            memset(&cmd, 0, sizeof(cmd));
            cmd.transactionId = generateTransactionId();
            cmd.index = 44 + i;
            cmd.objectType = OBJ_T_FLOW_CONTROL;
            cmd.command = FLOW_CMD_DISABLE;
            
            if (ipc.sendPacket(IPC_MSG_CONTROL_WRITE, (uint8_t*)&cmd, sizeof(cmd))) {
                ioConfig.flowControllers[i].enabled = false;
                disabledCount++;
                log(LOG_INFO, false, "  Disabled flow controller %d\n", 44 + i);
            } else {
                failedCount++;
            }
        }
    }
    
    // Disable DO controller via IPC
    if (ioConfig.doController.isActive) {
        IPC_DOControllerControl_t cmd;
        memset(&cmd, 0, sizeof(cmd));
        cmd.transactionId = generateTransactionId();
        cmd.index = 48;
        cmd.objectType = OBJ_T_DISSOLVED_OXYGEN_CONTROL;
        cmd.command = DO_CMD_DISABLE;
        
        if (ipc.sendPacket(IPC_MSG_CONTROL_WRITE, (uint8_t*)&cmd, sizeof(cmd))) {
            ioConfig.doController.enabled = false;
            disabledCount++;
            log(LOG_INFO, false, "  Disabled DO controller\n");
        } else {
            failedCount++;
        }
    }
    
    DynamicJsonDocument doc(256);
    doc["success"] = true;
    doc["disabled"] = disabledCount;
    doc["failed"] = failedCount;
    
    String response;
    serializeJson(doc, response);
    server.send(200, "application/json", response);
}

void handleClearVolumes()
{
    log(LOG_INFO, false, "[DASHBOARD] Clear Volumes requested\n");
    
    uint8_t clearedCount = 0;
    uint8_t failedCount = 0;
    
    // Send reset volume command to all active flow controllers
    for (uint8_t i = 0; i < MAX_FLOW_CONTROLLERS; i++) {
        if (ioConfig.flowControllers[i].isActive) {
            uint8_t objIndex = 44 + i;
            
            // Build and send IPC command
            IPC_FlowControllerControl_t cmd;
            memset(&cmd, 0, sizeof(cmd));
            cmd.transactionId = generateTransactionId();
            cmd.index = objIndex;
            cmd.objectType = OBJ_T_FLOW_CONTROL;
            cmd.command = FLOW_CMD_RESET_VOLUME;
            
            if (ipc.sendPacket(IPC_MSG_CONTROL_WRITE, (uint8_t*)&cmd, sizeof(cmd))) {
                clearedCount++;
            } else {
                failedCount++;
            }
        }
    }
    
    DynamicJsonDocument doc(256);
    doc["success"] = true;
    doc["cleared"] = clearedCount;
    doc["failed"] = failedCount;
    
    String response;
    serializeJson(doc, response);
    server.send(200, "application/json", response);
}
