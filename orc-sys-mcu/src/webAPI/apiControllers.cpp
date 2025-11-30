/**
 * @file apiControllers.cpp
 * @brief Controller API implementation (Temperature, pH, Flow, DO controllers)
 */

#include "apiControllers.h"
#include "../network/networkManager.h"
#include "../config/ioConfig.h"
#include "../utils/logger.h"
#include "../utils/objectCache.h"
#include "../utils/ipcManager.h"
#include <ArduinoJson.h>

// =============================================================================
// Setup Function
// =============================================================================

void setupControllersAPI(void)
{
    server.on("/api/controllers", HTTP_GET, handleGetControllers);
    
    // Temperature controllers (indices 40-42)
    // Config endpoints: /api/config/tempcontroller/{index}
    // Control endpoints: /api/controller/{index}/{action}
    for (uint8_t i = 40; i < 40 + MAX_TEMP_CONTROLLERS; i++) {
        String configPath = "/api/config/tempcontroller/" + String(i);
        String setpointPath = "/api/controller/" + String(i) + "/setpoint";
        String enablePath = "/api/controller/" + String(i) + "/enable";
        String disablePath = "/api/controller/" + String(i) + "/disable";
        String startPath = "/api/controller/" + String(i) + "/start";
        String stopPath = "/api/controller/" + String(i) + "/stop";
        String autotunePath = "/api/controller/" + String(i) + "/autotune";
        
        server.on(configPath.c_str(), HTTP_GET, [i]() { handleGetTempControllerConfig(i); });
        server.on(configPath.c_str(), HTTP_POST, [i]() { handleSaveTempControllerConfig(i); });
        server.on(setpointPath.c_str(), HTTP_POST, [i]() { handleUpdateControllerSetpoint(i); });
        server.on(enablePath.c_str(), HTTP_POST, [i]() { handleEnableController(i); });
        server.on(disablePath.c_str(), HTTP_POST, [i]() { handleDisableController(i); });
        server.on(startPath.c_str(), HTTP_POST, [i]() { handleEnableController(i); });
        server.on(stopPath.c_str(), HTTP_POST, [i]() { handleDisableController(i); });
        server.on(autotunePath.c_str(), HTTP_POST, [i]() { handleStartAutotune(i); });
    }
    
    // pH controller (index 43)
    // Config endpoints: /api/config/phcontroller/43
    // Control endpoints: /api/phcontroller/43/{action}
    server.on("/api/config/phcontroller/43", HTTP_GET, handleGetpHControllerConfig);
    server.on("/api/config/phcontroller/43", HTTP_POST, handleSavepHControllerConfig);
    server.on("/api/phcontroller/43/setpoint", HTTP_POST, handleUpdatepHSetpoint);
    server.on("/api/phcontroller/43/enable", HTTP_POST, handleEnablepHController);
    server.on("/api/phcontroller/43/disable", HTTP_POST, handleDisablepHController);
    server.on("/api/phcontroller/43/dose-acid", HTTP_POST, handleDosepHAcid);
    server.on("/api/phcontroller/43/dose-alkaline", HTTP_POST, handleDosepHAlkaline);
    server.on("/api/phcontroller/43/reset-acid-volume", HTTP_POST, handleResetpHAcidVolume);
    server.on("/api/phcontroller/43/reset-alkaline-volume", HTTP_POST, handleResetpHAlkalineVolume);
    
    // Flow controllers (indices 44-47)
    // Config endpoints: /api/config/flowcontroller/{index}
    // Control endpoints: /api/flowcontroller/{index}/{action}
    for (uint8_t i = 44; i < 44 + MAX_FLOW_CONTROLLERS; i++) {
        String configPath = "/api/config/flowcontroller/" + String(i);
        String flowratePath = "/api/flowcontroller/" + String(i) + "/flowrate";
        String enablePath = "/api/flowcontroller/" + String(i) + "/enable";
        String disablePath = "/api/flowcontroller/" + String(i) + "/disable";
        String dosePath = "/api/flowcontroller/" + String(i) + "/dose";
        String resetPath = "/api/flowcontroller/" + String(i) + "/reset-volume";
        
        server.on(configPath.c_str(), HTTP_GET, [i]() { handleGetFlowControllerConfig(i); });
        server.on(configPath.c_str(), HTTP_POST, [i]() { handleSaveFlowControllerConfig(i); });
        server.on(flowratePath.c_str(), HTTP_POST, [i]() { handleSetFlowRate(i); });
        server.on(enablePath.c_str(), HTTP_POST, [i]() { handleEnableFlowController(i); });
        server.on(disablePath.c_str(), HTTP_POST, [i]() { handleDisableFlowController(i); });
        server.on(dosePath.c_str(), HTTP_POST, [i]() { handleManualFlowDose(i); });
        server.on(resetPath.c_str(), HTTP_POST, [i]() { handleResetFlowVolume(i); });
    }
    
    // DO controller (index 48)
    // Config endpoints: /api/config/docontroller/48
    // Control endpoints: /api/docontroller/48/{action}
    server.on("/api/config/docontroller/48", HTTP_GET, handleGetDOControllerConfig);
    server.on("/api/config/docontroller/48", HTTP_POST, handleSaveDOControllerConfig);
    server.on("/api/config/docontroller/48", HTTP_DELETE, handleDeleteDOController);
    server.on("/api/docontroller/48/setpoint", HTTP_POST, handleSetDOSetpoint);
    server.on("/api/docontroller/48/enable", HTTP_POST, handleEnableDOController);
    server.on("/api/docontroller/48/disable", HTTP_POST, handleDisableDOController);
    
    // DO profiles (indices 0-2)
    server.on("/api/doprofiles", HTTP_GET, handleGetAllDOProfiles);
    for (uint8_t i = 0; i < MAX_DO_PROFILES; i++) {
        String path = "/api/doprofile/" + String(i);
        server.on(path.c_str(), HTTP_GET, [i]() { handleGetDOProfile(i); });
        server.on(path.c_str(), HTTP_POST, [i]() { handleSaveDOProfile(i); });
        server.on(path.c_str(), HTTP_DELETE, [i]() { handleDeleteDOProfile(i); });
    }
}

// =============================================================================
// Get All Controllers
// =============================================================================

void handleGetControllers() {
    DynamicJsonDocument doc(8192);
    JsonArray controllers = doc.createNestedArray("controllers");
    
    // Temperature Controllers (40-42)
    for (int i = 0; i < MAX_TEMP_CONTROLLERS; i++) {    
        if (!ioConfig.tempControllers[i].isActive) continue;
        
        uint8_t index = 40 + i;
        JsonObject ctrl = controllers.createNestedObject();
        
        ctrl["index"] = index;
        ctrl["name"] = ioConfig.tempControllers[i].name;
        ctrl["showOnDashboard"] = ioConfig.tempControllers[i].showOnDashboard;
        ctrl["unit"] = ioConfig.tempControllers[i].unit;
        ctrl["setpoint"] = ioConfig.tempControllers[i].setpoint;
        ctrl["controlMethod"] = (uint8_t)ioConfig.tempControllers[i].controlMethod;
        ctrl["hysteresis"] = ioConfig.tempControllers[i].hysteresis;
        ctrl["kP"] = ioConfig.tempControllers[i].kP;
        ctrl["kI"] = ioConfig.tempControllers[i].kI;
        ctrl["kD"] = ioConfig.tempControllers[i].kD;
        
        ObjectCache::CachedObject* obj = objectCache.getObject(index);
        bool enabled = false;
        
        if (obj && obj->valid && obj->lastUpdate > 0) {
            enabled = (obj->flags & IPC_SENSOR_FLAG_RUNNING) ? true : false;
            ctrl["enabled"] = enabled;
            ctrl["fault"] = (obj->flags & IPC_SENSOR_FLAG_FAULT) ? true : false;
            ctrl["message"] = obj->message;
            ctrl["tuning"] = (obj->flags & 0x10) ? true : false;
            
            if (obj->valueCount >= 4) {
                ioConfig.tempControllers[i].kP = obj->additionalValues[1];
                ioConfig.tempControllers[i].kI = obj->additionalValues[2];
                ioConfig.tempControllers[i].kD = obj->additionalValues[3];
            }
            
            if (enabled) {
                ctrl["processValue"] = obj->value;
                ctrl["output"] = obj->valueCount > 0 ? obj->additionalValues[0] : 0.0f;
            }
        } else {
            ctrl["enabled"] = false;
            ctrl["fault"] = false;
            ctrl["tuning"] = false;
        }
        
        if (!enabled) {
            uint8_t pvSourceIndex = ioConfig.tempControllers[i].pvSourceIndex;
            ObjectCache::CachedObject* sensorObj = objectCache.getObject(pvSourceIndex);
            
            if (sensorObj && sensorObj->valid && sensorObj->lastUpdate > 0) {
                ctrl["processValue"] = sensorObj->value;
            } else {
                ctrl["processValue"] = 0.0f;  // Default to 0 instead of null
            }
            
            uint8_t outputIndex = ioConfig.tempControllers[i].outputIndex;
            ObjectCache::CachedObject* outputObj = objectCache.getObject(outputIndex);
            
            if (outputObj && outputObj->valid && outputObj->lastUpdate > 0) {
                ctrl["output"] = outputObj->value;
            } else {
                ctrl["output"] = 0.0f;  // Default to 0 instead of null
            }
        }
    }
    
    // pH Controller (43)
    if (ioConfig.phController.isActive) {
        uint8_t index = 43;
        JsonObject ctrl = controllers.createNestedObject();
        
        ctrl["index"] = index;
        ctrl["name"] = ioConfig.phController.name;
        ctrl["showOnDashboard"] = ioConfig.phController.showOnDashboard;
        ctrl["unit"] = "pH";
        ctrl["setpoint"] = ioConfig.phController.setpoint;
        ctrl["controlMethod"] = 2;
        ctrl["deadband"] = ioConfig.phController.deadband;
        ctrl["acidEnabled"] = ioConfig.phController.acidDosing.enabled;
        ctrl["alkalineEnabled"] = ioConfig.phController.alkalineDosing.enabled;
        
        ObjectCache::CachedObject* obj = objectCache.getObject(index);
        bool enabled = false;
        
        if (obj && obj->valid && obj->lastUpdate > 0) {
            enabled = (obj->flags & IPC_SENSOR_FLAG_RUNNING) ? true : false;
            ctrl["enabled"] = enabled;
            ctrl["fault"] = (obj->flags & IPC_SENSOR_FLAG_FAULT) ? true : false;
            ctrl["message"] = obj->message;
            
            if (enabled) {
                ctrl["processValue"] = obj->value;
                ctrl["output"] = obj->valueCount > 0 ? obj->additionalValues[0] : 0.0f;
                ctrl["acidVolumeTotal_mL"] = obj->valueCount > 1 ? obj->additionalValues[1] : 0.0f;
                ctrl["alkalineVolumeTotal_mL"] = obj->valueCount > 2 ? obj->additionalValues[2] : 0.0f;
            }
        } else {
            ctrl["enabled"] = false;
            ctrl["fault"] = false;
        }
        
        if (!enabled) {
            uint8_t pvSourceIndex = ioConfig.phController.pvSourceIndex;
            ObjectCache::CachedObject* sensorObj = objectCache.getObject(pvSourceIndex);
            
            if (sensorObj && sensorObj->valid && sensorObj->lastUpdate > 0) {
                ctrl["processValue"] = sensorObj->value;
            } else {
                ctrl["processValue"] = 0.0f;
            }
            ctrl["output"] = 0.0f;
            ctrl["acidVolumeTotal_mL"] = 0.0f;
            ctrl["alkalineVolumeTotal_mL"] = 0.0f;
        }
    }
    
    // Flow Controllers (44-47)
    for (int i = 0; i < MAX_FLOW_CONTROLLERS; i++) {
        if (!ioConfig.flowControllers[i].isActive) continue;
        
        uint8_t index = 44 + i;
        JsonObject ctrl = controllers.createNestedObject();
        
        ctrl["index"] = index;
        ctrl["name"] = ioConfig.flowControllers[i].name;
        ctrl["showOnDashboard"] = ioConfig.flowControllers[i].showOnDashboard;
        ctrl["unit"] = "mL/min";
        ctrl["setpoint"] = ioConfig.flowControllers[i].flowRate_mL_min;
        ctrl["controlMethod"] = 3;
        ctrl["outputType"] = ioConfig.flowControllers[i].outputType;
        ctrl["outputIndex"] = ioConfig.flowControllers[i].outputIndex;
        ctrl["motorPower"] = ioConfig.flowControllers[i].motorPower;
        
        // Calibration & dosing parameters
        ctrl["dosingInterval_ms"] = ioConfig.flowControllers[i].minDosingInterval_ms;
        ctrl["calibrationVolume_mL"] = ioConfig.flowControllers[i].calibrationVolume_mL;
        ctrl["calibrationDoseTime_ms"] = ioConfig.flowControllers[i].calibrationDoseTime_ms;
        
        ObjectCache::CachedObject* obj = objectCache.getObject(index);
        
        if (obj && obj->valid && obj->lastUpdate > 0) {
            ctrl["enabled"] = (obj->flags & IPC_SENSOR_FLAG_RUNNING) ? true : false;
            ctrl["fault"] = (obj->flags & IPC_SENSOR_FLAG_FAULT) ? true : false;
            ctrl["message"] = obj->message;
            ctrl["processValue"] = obj->value;
            ctrl["output"] = obj->valueCount > 0 ? obj->additionalValues[0] : 0.0f;
            ctrl["cumulativeVolume_mL"] = obj->valueCount > 2 ? obj->additionalValues[2] : 0.0f;
        } else {
            ctrl["enabled"] = false;
            ctrl["fault"] = false;
            ctrl["processValue"] = 0.0f;
            ctrl["output"] = 0.0f;
            ctrl["cumulativeVolume_mL"] = 0.0f;
        }
    }
    
    // DO Controller (48)
    if (ioConfig.doController.isActive) {
        uint8_t index = 48;
        JsonObject ctrl = controllers.createNestedObject();
        
        ctrl["index"] = index;
        ctrl["name"] = ioConfig.doController.name;
        ctrl["showOnDashboard"] = ioConfig.doController.showOnDashboard;
        ctrl["unit"] = "mg/L";
        ctrl["setpoint"] = ioConfig.doController.setpoint_mg_L;
        ctrl["controlMethod"] = 4;
        ctrl["activeProfileIndex"] = ioConfig.doController.activeProfileIndex;
        ctrl["stirrerEnabled"] = ioConfig.doController.stirrerEnabled;
        ctrl["mfcEnabled"] = ioConfig.doController.mfcEnabled;
        ctrl["stirrerUnit"] = (ioConfig.doController.stirrerType == 0) ? "%" : "RPM";
        
        // Get active profile name
        int8_t profileIdx = ioConfig.doController.activeProfileIndex;
        if (profileIdx >= 0 && profileIdx < MAX_DO_PROFILES && ioConfig.doProfiles[profileIdx].isActive) {
            ctrl["activeProfileName"] = ioConfig.doProfiles[profileIdx].name;
        } else {
            ctrl["activeProfileName"] = "None";
        }
        
        ObjectCache::CachedObject* obj = objectCache.getObject(index);
        
        if (obj && obj->valid && obj->lastUpdate > 0) {
            ctrl["enabled"] = (obj->flags & IPC_SENSOR_FLAG_RUNNING) ? true : false;
            ctrl["fault"] = (obj->flags & IPC_SENSOR_FLAG_FAULT) ? true : false;
            ctrl["message"] = obj->message;
            ctrl["processValue"] = obj->value;
            ctrl["stirrerOutput"] = obj->valueCount > 0 ? obj->additionalValues[0] : 0.0f;
            ctrl["mfcOutput"] = obj->valueCount > 1 ? obj->additionalValues[1] : 0.0f;
            ctrl["error"] = obj->valueCount > 2 ? obj->additionalValues[2] : 0.0f;
            ctrl["output"] = ctrl["error"];
        } else {
            ctrl["enabled"] = false;
            ctrl["fault"] = false;
            ctrl["processValue"] = 0.0f;
            ctrl["output"] = 0.0f;
            ctrl["stirrerOutput"] = 0.0f;
            ctrl["mfcOutput"] = 0.0f;
        }
    }
    
    String response;
    serializeJson(doc, response);
    server.send(200, "application/json", response);
}

// =============================================================================
// Temperature Controller Handlers (Stubs - full implementation in backup)
// =============================================================================

void handleGetTempControllerConfig(uint8_t index) {
    if (index < 40 || index >= 40 + MAX_TEMP_CONTROLLERS) {
        server.send(400, "application/json", "{\"error\":\"Invalid index\"}");
        return;
    }
    int idx = index - 40;
    StaticJsonDocument<512> doc;
    doc["index"] = index;
    doc["name"] = ioConfig.tempControllers[idx].name;
    doc["showOnDashboard"] = ioConfig.tempControllers[idx].showOnDashboard;
    doc["setpoint"] = ioConfig.tempControllers[idx].setpoint;
    doc["controlMethod"] = (uint8_t)ioConfig.tempControllers[idx].controlMethod;
    doc["pvSourceIndex"] = ioConfig.tempControllers[idx].pvSourceIndex;
    doc["outputIndex"] = ioConfig.tempControllers[idx].outputIndex;
    doc["hysteresis"] = ioConfig.tempControllers[idx].hysteresis;
    doc["kP"] = ioConfig.tempControllers[idx].kP;
    doc["kI"] = ioConfig.tempControllers[idx].kI;
    doc["kD"] = ioConfig.tempControllers[idx].kD;
    doc["integralWindup"] = ioConfig.tempControllers[idx].integralWindup;
    doc["outputMin"] = ioConfig.tempControllers[idx].outputMin;
    doc["outputMax"] = ioConfig.tempControllers[idx].outputMax;
    String response;
    serializeJson(doc, response);
    server.send(200, "application/json", response);
}

void handleSaveTempControllerConfig(uint8_t index) {
    if (index < 40 || index >= 40 + MAX_TEMP_CONTROLLERS) {
        server.send(400, "application/json", "{\"error\":\"Invalid controller index\"}");
        return;
    }
    
    if (!server.hasArg("plain")) {
        server.send(400, "application/json", "{\"error\":\"No data provided\"}");
        return;
    }
    
    StaticJsonDocument<1024> doc;
    DeserializationError error = deserializeJson(doc, server.arg("plain"));
    if (error) {
        server.send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
        return;
    }
    
    int ctrlIdx = index - 40;
    
    uint8_t newOutputIndex = doc["outputIndex"] | 0;
    if (newOutputIndex > 0) {
        for (int j = 0; j < MAX_TEMP_CONTROLLERS; j++) {
            if (j != ctrlIdx && ioConfig.tempControllers[j].isActive && 
                ioConfig.tempControllers[j].outputIndex == newOutputIndex) {
                server.send(400, "application/json", "{\"error\":\"Output already in use by another controller\"}");
                return;
            }
        }
    }
    
    ioConfig.tempControllers[ctrlIdx].isActive = doc["isActive"] | true;
    strlcpy(ioConfig.tempControllers[ctrlIdx].name, doc["name"] | "", sizeof(ioConfig.tempControllers[ctrlIdx].name));
    ioConfig.tempControllers[ctrlIdx].enabled = false;
    if (doc.containsKey("showOnDashboard")) ioConfig.tempControllers[ctrlIdx].showOnDashboard = doc["showOnDashboard"];
    strlcpy(ioConfig.tempControllers[ctrlIdx].unit, doc["unit"] | "C", sizeof(ioConfig.tempControllers[ctrlIdx].unit));
    ioConfig.tempControllers[ctrlIdx].pvSourceIndex = doc["pvSourceIndex"] | 0;
    ioConfig.tempControllers[ctrlIdx].outputIndex = doc["outputIndex"] | 0;
    ioConfig.tempControllers[ctrlIdx].controlMethod = (ControlMethod)(doc["controlMethod"] | CONTROL_METHOD_PID);
    ioConfig.tempControllers[ctrlIdx].setpoint = doc["setpoint"] | 25.0f;
    ioConfig.tempControllers[ctrlIdx].hysteresis = doc["hysteresis"] | 0.5f;
    ioConfig.tempControllers[ctrlIdx].kP = doc["kP"] | 2.0f;
    ioConfig.tempControllers[ctrlIdx].kI = doc["kI"] | 0.5f;
    ioConfig.tempControllers[ctrlIdx].kD = doc["kD"] | 0.1f;
    ioConfig.tempControllers[ctrlIdx].integralWindup = doc["integralWindup"] | 100.0f;
    ioConfig.tempControllers[ctrlIdx].outputMin = doc["outputMin"] | 0.0f;
    ioConfig.tempControllers[ctrlIdx].outputMax = doc["outputMax"] | 100.0f;
    
    uint8_t outputIdx = ioConfig.tempControllers[ctrlIdx].outputIndex;
    if (outputIdx >= 21 && outputIdx <= 25) {
        int digitalIdx = outputIdx - 21;
        if (ioConfig.tempControllers[ctrlIdx].controlMethod == CONTROL_METHOD_ON_OFF) {
            ioConfig.digitalOutputs[digitalIdx].mode = OUTPUT_MODE_ON_OFF;
        } else {
            ioConfig.digitalOutputs[digitalIdx].mode = OUTPUT_MODE_PWM;
        }
    }
    
    saveIOConfig();
    
    IPC_ConfigTempController_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.transactionId = generateTransactionId();
    cfg.index = index;
    cfg.isActive = ioConfig.tempControllers[ctrlIdx].isActive;
    strncpy(cfg.name, ioConfig.tempControllers[ctrlIdx].name, sizeof(cfg.name) - 1);
    cfg.enabled = ioConfig.tempControllers[ctrlIdx].enabled;
    cfg.pvSourceIndex = ioConfig.tempControllers[ctrlIdx].pvSourceIndex;
    cfg.outputIndex = ioConfig.tempControllers[ctrlIdx].outputIndex;
    cfg.controlMethod = (uint8_t)ioConfig.tempControllers[ctrlIdx].controlMethod;
    cfg.setpoint = ioConfig.tempControllers[ctrlIdx].setpoint;
    cfg.hysteresis = ioConfig.tempControllers[ctrlIdx].hysteresis;
    cfg.kP = ioConfig.tempControllers[ctrlIdx].kP;
    cfg.kI = ioConfig.tempControllers[ctrlIdx].kI;
    cfg.kD = ioConfig.tempControllers[ctrlIdx].kD;
    cfg.integralWindup = ioConfig.tempControllers[ctrlIdx].integralWindup;
    cfg.outputMin = ioConfig.tempControllers[ctrlIdx].outputMin;
    cfg.outputMax = ioConfig.tempControllers[ctrlIdx].outputMax;
    
    bool sent = ipc.sendPacket(IPC_MSG_CONFIG_TEMP_CONTROLLER, (uint8_t*)&cfg, sizeof(cfg));
    if (sent) {
        addPendingTransaction(cfg.transactionId, IPC_MSG_CONFIG_TEMP_CONTROLLER, IPC_MSG_CONTROL_ACK, 1, cfg.index);
        server.send(200, "application/json", "{\"success\":true,\"message\":\"Configuration saved and applied\"}");
    } else {
        server.send(200, "application/json", "{\"success\":true,\"message\":\"Configuration saved but IO MCU update failed\"}");
    }
}

void handleUpdateControllerSetpoint(uint8_t index) {
    if (index < 40 || index >= 40 + MAX_TEMP_CONTROLLERS) {
        server.send(400, "application/json", "{\"error\":\"Invalid controller index\"}");
        return;
    }
    
    if (!server.hasArg("plain")) {
        server.send(400, "application/json", "{\"error\":\"No data provided\"}");
        return;
    }
    
    StaticJsonDocument<256> doc;
    DeserializationError error = deserializeJson(doc, server.arg("plain"));
    if (error) {
        server.send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
        return;
    }
    
    int ctrlIdx = index - 40;
    float setpoint = doc["setpoint"] | ioConfig.tempControllers[ctrlIdx].setpoint;
    
    IPC_TempControllerControl_t cmd;
    memset(&cmd, 0, sizeof(cmd));
    cmd.transactionId = generateTransactionId();
    cmd.index = index;
    cmd.objectType = OBJ_T_TEMPERATURE_CONTROL;
    cmd.command = TEMP_CTRL_CMD_SET_SETPOINT;
    cmd.setpoint = setpoint;
    
    bool sent = ipc.sendPacket(IPC_MSG_CONTROL_WRITE, (uint8_t*)&cmd, sizeof(cmd));
    
    if (sent) {
        addPendingTransaction(cmd.transactionId, IPC_MSG_CONTROL_WRITE, IPC_MSG_CONTROL_ACK, 1, index);
        ioConfig.tempControllers[ctrlIdx].setpoint = setpoint;
        log(LOG_INFO, false, "Controller %d setpoint updated to %.1f\n", index, setpoint);
        server.send(200, "application/json", "{\"success\":true}");
    } else {
        server.send(500, "application/json", "{\"error\":\"Failed to communicate with IO MCU\"}");
    }
}

void handleEnableController(uint8_t index) {
    if (index < 40 || index >= 40 + MAX_TEMP_CONTROLLERS) {
        server.send(400, "application/json", "{\"error\":\"Invalid controller index\"}");
        return;
    }
    
    IPC_TempControllerControl_t cmd;
    memset(&cmd, 0, sizeof(cmd));
    cmd.transactionId = generateTransactionId();
    cmd.index = index;
    cmd.objectType = OBJ_T_TEMPERATURE_CONTROL;
    cmd.command = TEMP_CTRL_CMD_ENABLE;
    
    bool sent = ipc.sendPacket(IPC_MSG_CONTROL_WRITE, (uint8_t*)&cmd, sizeof(cmd));
    
    if (sent) {
        addPendingTransaction(cmd.transactionId, IPC_MSG_CONTROL_WRITE, IPC_MSG_CONTROL_ACK, 1, index);
        log(LOG_INFO, false, "Controller %d enabled (txn=%d)\n", index, cmd.transactionId);
        server.send(200, "application/json", "{\"success\":true,\"message\":\"Controller enabled\"}");
    } else {
        log(LOG_WARNING, false, "Failed to send enable command to controller %d\n", index);
        server.send(500, "application/json", "{\"error\":\"Failed to communicate with IO MCU\"}");
    }
}

void handleDisableController(uint8_t index) {
    if (index < 40 || index >= 40 + MAX_TEMP_CONTROLLERS) {
        server.send(400, "application/json", "{\"error\":\"Invalid controller index\"}");
        return;
    }
    
    IPC_TempControllerControl_t cmd;
    memset(&cmd, 0, sizeof(cmd));
    cmd.transactionId = generateTransactionId();
    cmd.index = index;
    cmd.objectType = OBJ_T_TEMPERATURE_CONTROL;
    cmd.command = TEMP_CTRL_CMD_DISABLE;
    
    bool sent = ipc.sendPacket(IPC_MSG_CONTROL_WRITE, (uint8_t*)&cmd, sizeof(cmd));
    
    if (sent) {
        addPendingTransaction(cmd.transactionId, IPC_MSG_CONTROL_WRITE, IPC_MSG_CONTROL_ACK, 1, index);
        log(LOG_INFO, false, "Controller %d disabled (txn=%d)\n", index, cmd.transactionId);
        server.send(200, "application/json", "{\"success\":true,\"message\":\"Controller disabled\"}");
    } else {
        log(LOG_WARNING, false, "Failed to send disable command to controller %d\n", index);
        server.send(500, "application/json", "{\"error\":\"Failed to communicate with IO MCU\"}");
    }
}

void handleDeleteController(uint8_t index) {
    if (index < 40 || index >= 40 + MAX_TEMP_CONTROLLERS) {
        server.send(400, "application/json", "{\"error\":\"Invalid controller index\"}");
        return;
    }
    
    int ctrlIdx = index - 40;
    
    ioConfig.tempControllers[ctrlIdx].isActive = false;
    ioConfig.tempControllers[ctrlIdx].enabled = false;
    memset(ioConfig.tempControllers[ctrlIdx].name, 0, sizeof(ioConfig.tempControllers[ctrlIdx].name));
    
    saveIOConfig();
    
    IPC_ConfigTempController_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.transactionId = generateTransactionId();
    cfg.index = index;
    cfg.isActive = false;
    
    bool sent = ipc.sendPacket(IPC_MSG_CONFIG_TEMP_CONTROLLER, (uint8_t*)&cfg, sizeof(cfg));
    
    if (sent) {
        addPendingTransaction(cfg.transactionId, IPC_MSG_CONFIG_TEMP_CONTROLLER, IPC_MSG_CONTROL_ACK, 1, cfg.index);
        log(LOG_INFO, false, "Controller %d deleted\n", index);
        server.send(200, "application/json", "{\"success\":true,\"message\":\"Controller deleted\"}");
    } else {
        server.send(200, "application/json", "{\"success\":true,\"message\":\"Controller deleted but IO MCU update failed\"}");
    }
}

void handleStartController(uint8_t index) {
    handleEnableController(index);
}

void handleStopController(uint8_t index) {
    handleDisableController(index);
}

void handleStartAutotune(uint8_t index) {
    if (index < 40 || index >= 40 + MAX_TEMP_CONTROLLERS) {
        server.send(400, "application/json", "{\"error\":\"Invalid controller index\"}");
        return;
    }
    
    int ctrlIdx = index - 40;
    
    if (ioConfig.tempControllers[ctrlIdx].controlMethod != CONTROL_METHOD_PID) {
        server.send(400, "application/json", "{\"error\":\"Autotune only available for PID controllers\"}");
        return;
    }
    
    float targetSetpoint = ioConfig.tempControllers[ctrlIdx].setpoint;
    float outputStep = 100.0f;
    
    if (server.hasArg("plain")) {
        StaticJsonDocument<256> doc;
        DeserializationError error = deserializeJson(doc, server.arg("plain"));
        if (!error) {
            targetSetpoint = doc["setpoint"] | targetSetpoint;
            outputStep = doc["outputStep"] | outputStep;
        }
    }
    
    IPC_TempControllerControl_t cmd;
    memset(&cmd, 0, sizeof(cmd));
    cmd.transactionId = generateTransactionId();
    cmd.index = index;
    cmd.objectType = OBJ_T_TEMPERATURE_CONTROL;
    cmd.command = TEMP_CTRL_CMD_START_AUTOTUNE;
    cmd.setpoint = targetSetpoint;
    cmd.autotuneOutputStep = outputStep;
    
    bool sent = ipc.sendPacket(IPC_MSG_CONTROL_WRITE, (uint8_t*)&cmd, sizeof(cmd));
    
    if (sent) {
        addPendingTransaction(cmd.transactionId, IPC_MSG_CONTROL_WRITE, IPC_MSG_CONTROL_ACK, 1, index);
        log(LOG_INFO, false, "Controller %d autotune started\n", index);
        server.send(200, "application/json", "{\"success\":true,\"message\":\"Autotune started\"}");
    } else {
        server.send(500, "application/json", "{\"error\":\"Failed to communicate with IO MCU\"}");
    }
}

// pH Controller handlers
void handleGetpHControllerConfig() {
    StaticJsonDocument<1024> doc;
    
    doc["index"] = 43;
    doc["isActive"] = ioConfig.phController.isActive;
    doc["name"] = ioConfig.phController.name;
    doc["enabled"] = ioConfig.phController.enabled;
    doc["showOnDashboard"] = ioConfig.phController.showOnDashboard;
    doc["pvSourceIndex"] = ioConfig.phController.pvSourceIndex;
    doc["setpoint"] = ioConfig.phController.setpoint;
    doc["deadband"] = ioConfig.phController.deadband;
    
    JsonObject acid = doc.createNestedObject("acidDosing");
    acid["enabled"] = ioConfig.phController.acidDosing.enabled;
    acid["outputType"] = ioConfig.phController.acidDosing.outputType;
    acid["outputIndex"] = ioConfig.phController.acidDosing.outputIndex;
    acid["motorPower"] = ioConfig.phController.acidDosing.motorPower;
    acid["dosingTime_ms"] = ioConfig.phController.acidDosing.dosingTime_ms;
    acid["dosingInterval_ms"] = ioConfig.phController.acidDosing.dosingInterval_ms;
    acid["volumePerDose_mL"] = ioConfig.phController.acidDosing.volumePerDose_mL;
    acid["mfcFlowRate_mL_min"] = ioConfig.phController.acidDosing.mfcFlowRate_mL_min;
    
    JsonObject alkaline = doc.createNestedObject("alkalineDosing");
    alkaline["enabled"] = ioConfig.phController.alkalineDosing.enabled;
    alkaline["outputType"] = ioConfig.phController.alkalineDosing.outputType;
    alkaline["outputIndex"] = ioConfig.phController.alkalineDosing.outputIndex;
    alkaline["motorPower"] = ioConfig.phController.alkalineDosing.motorPower;
    alkaline["dosingTime_ms"] = ioConfig.phController.alkalineDosing.dosingTime_ms;
    alkaline["dosingInterval_ms"] = ioConfig.phController.alkalineDosing.dosingInterval_ms;
    alkaline["volumePerDose_mL"] = ioConfig.phController.alkalineDosing.volumePerDose_mL;
    alkaline["mfcFlowRate_mL_min"] = ioConfig.phController.alkalineDosing.mfcFlowRate_mL_min;
    
    String response;
    serializeJson(doc, response);
    server.send(200, "application/json", response);
}

void handleSavepHControllerConfig() {
    if (!server.hasArg("plain")) {
        server.send(400, "application/json", "{\"error\":\"No data provided\"}");
        return;
    }
    
    StaticJsonDocument<1024> doc;
    DeserializationError error = deserializeJson(doc, server.arg("plain"));
    if (error) {
        server.send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
        return;
    }
    
    bool acidEnabled = doc["acidDosing"]["enabled"] | false;
    bool alkalineEnabled = doc["alkalineDosing"]["enabled"] | false;
    if (!acidEnabled && !alkalineEnabled) {
        server.send(400, "application/json", "{\"error\":\"At least one dosing direction must be enabled\"}");
        return;
    }
    
    ioConfig.phController.isActive = doc["isActive"] | true;
    strlcpy(ioConfig.phController.name, doc["name"] | "", sizeof(ioConfig.phController.name));
    ioConfig.phController.enabled = false;
    if (doc.containsKey("showOnDashboard")) ioConfig.phController.showOnDashboard = doc["showOnDashboard"];
    ioConfig.phController.pvSourceIndex = doc["pvSourceIndex"] | 0;
    ioConfig.phController.setpoint = doc["setpoint"] | 7.0f;
    ioConfig.phController.deadband = doc["deadband"] | 0.2f;
    
    JsonObject acid = doc["acidDosing"];
    ioConfig.phController.acidDosing.enabled = acid["enabled"] | false;
    ioConfig.phController.acidDosing.outputType = acid["outputType"] | 0;
    ioConfig.phController.acidDosing.outputIndex = acid["outputIndex"] | 21;
    ioConfig.phController.acidDosing.motorPower = acid["motorPower"] | 50;
    ioConfig.phController.acidDosing.dosingTime_ms = acid["dosingTime_ms"] | 1000;
    ioConfig.phController.acidDosing.dosingInterval_ms = acid["dosingInterval_ms"] | 60000;
    ioConfig.phController.acidDosing.volumePerDose_mL = acid["volumePerDose_mL"] | 0.5f;
    ioConfig.phController.acidDosing.mfcFlowRate_mL_min = acid["mfcFlowRate_mL_min"] | 100.0f;
    
    JsonObject alkaline = doc["alkalineDosing"];
    ioConfig.phController.alkalineDosing.enabled = alkaline["enabled"] | false;
    ioConfig.phController.alkalineDosing.outputType = alkaline["outputType"] | 0;
    ioConfig.phController.alkalineDosing.outputIndex = alkaline["outputIndex"] | 22;
    ioConfig.phController.alkalineDosing.motorPower = alkaline["motorPower"] | 50;
    ioConfig.phController.alkalineDosing.dosingTime_ms = alkaline["dosingTime_ms"] | 1000;
    ioConfig.phController.alkalineDosing.dosingInterval_ms = alkaline["dosingInterval_ms"] | 60000;
    ioConfig.phController.alkalineDosing.volumePerDose_mL = alkaline["volumePerDose_mL"] | 0.5f;
    ioConfig.phController.alkalineDosing.mfcFlowRate_mL_min = alkaline["mfcFlowRate_mL_min"] | 100.0f;
    
    saveIOConfig();
    
    IPC_ConfigpHController_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.transactionId = generateTransactionId();
    cfg.index = 43;
    cfg.isActive = ioConfig.phController.isActive;
    strncpy(cfg.name, ioConfig.phController.name, sizeof(cfg.name) - 1);
    cfg.enabled = ioConfig.phController.enabled;
    cfg.pvSourceIndex = ioConfig.phController.pvSourceIndex;
    cfg.setpoint = ioConfig.phController.setpoint;
    cfg.deadband = ioConfig.phController.deadband;
    cfg.acidEnabled = ioConfig.phController.acidDosing.enabled;
    cfg.acidOutputType = ioConfig.phController.acidDosing.outputType;
    cfg.acidOutputIndex = ioConfig.phController.acidDosing.outputIndex;
    cfg.acidMotorPower = ioConfig.phController.acidDosing.motorPower;
    cfg.acidDosingTime_ms = ioConfig.phController.acidDosing.dosingTime_ms;
    cfg.acidDosingInterval_ms = ioConfig.phController.acidDosing.dosingInterval_ms;
    cfg.acidVolumePerDose_mL = ioConfig.phController.acidDosing.volumePerDose_mL;
    cfg.acidMfcFlowRate_mL_min = ioConfig.phController.acidDosing.mfcFlowRate_mL_min;
    cfg.alkalineEnabled = ioConfig.phController.alkalineDosing.enabled;
    cfg.alkalineOutputType = ioConfig.phController.alkalineDosing.outputType;
    cfg.alkalineOutputIndex = ioConfig.phController.alkalineDosing.outputIndex;
    cfg.alkalineMotorPower = ioConfig.phController.alkalineDosing.motorPower;
    cfg.alkalineDosingTime_ms = ioConfig.phController.alkalineDosing.dosingTime_ms;
    cfg.alkalineDosingInterval_ms = ioConfig.phController.alkalineDosing.dosingInterval_ms;
    cfg.alkalineVolumePerDose_mL = ioConfig.phController.alkalineDosing.volumePerDose_mL;
    cfg.alkalineMfcFlowRate_mL_min = ioConfig.phController.alkalineDosing.mfcFlowRate_mL_min;
    
    bool sent = ipc.sendPacket(IPC_MSG_CONFIG_PH_CONTROLLER, (uint8_t*)&cfg, sizeof(cfg));
    if (sent) {
        addPendingTransaction(cfg.transactionId, IPC_MSG_CONFIG_PH_CONTROLLER, IPC_MSG_CONTROL_ACK, 1, cfg.index);
        server.send(200, "application/json", "{\"success\":true,\"message\":\"Configuration saved and applied\"}");
    } else {
        server.send(200, "application/json", "{\"success\":true,\"message\":\"Configuration saved but IO MCU update failed\"}");
    }
}

void handleDeletepHController() {
    ioConfig.phController.isActive = false;
    ioConfig.phController.enabled = false;
    memset(ioConfig.phController.name, 0, sizeof(ioConfig.phController.name));
    saveIOConfig();
    
    IPC_ConfigpHController_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.transactionId = generateTransactionId();
    cfg.index = 43;
    cfg.isActive = false;
    
    bool sent = ipc.sendPacket(IPC_MSG_CONFIG_PH_CONTROLLER, (uint8_t*)&cfg, sizeof(cfg));
    if (sent) {
        addPendingTransaction(cfg.transactionId, IPC_MSG_CONFIG_PH_CONTROLLER, IPC_MSG_CONTROL_ACK, 1, cfg.index);
        server.send(200, "application/json", "{\"success\":true,\"message\":\"pH controller deleted\"}");
    } else {
        server.send(200, "application/json", "{\"success\":true,\"message\":\"pH controller deleted but IO MCU update failed\"}");
    }
}

void handleUpdatepHSetpoint() {
    if (!server.hasArg("plain")) {
        server.send(400, "application/json", "{\"error\":\"No data provided\"}");
        return;
    }
    
    StaticJsonDocument<256> doc;
    DeserializationError error = deserializeJson(doc, server.arg("plain"));
    if (error) {
        server.send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
        return;
    }
    
    float setpoint = doc["setpoint"] | 7.0f;
    
    IPC_pHControllerControl_t cmd;
    memset(&cmd, 0, sizeof(cmd));
    cmd.transactionId = generateTransactionId();
    cmd.index = 43;
    cmd.objectType = OBJ_T_PH_CONTROL;
    cmd.command = PH_CMD_SET_SETPOINT;
    cmd.setpoint = setpoint;
    
    bool sent = ipc.sendPacket(IPC_MSG_CONTROL_WRITE, (uint8_t*)&cmd, sizeof(cmd));
    if (sent) {
        addPendingTransaction(cmd.transactionId, IPC_MSG_CONTROL_WRITE, IPC_MSG_CONTROL_ACK, 1, 43);
        ioConfig.phController.setpoint = setpoint;
        server.send(200, "application/json", "{\"success\":true}");
    } else {
        server.send(500, "application/json", "{\"error\":\"Failed to send IPC command\"}");
    }
}
void handleEnablepHController() {
    IPC_pHControllerControl_t cmd;
    memset(&cmd, 0, sizeof(cmd));
    cmd.transactionId = generateTransactionId();
    cmd.index = 43;
    cmd.objectType = OBJ_T_PH_CONTROL;
    cmd.command = PH_CMD_ENABLE;
    
    bool sent = ipc.sendPacket(IPC_MSG_CONTROL_WRITE, (uint8_t*)&cmd, sizeof(cmd));
    
    if (sent) {
        addPendingTransaction(cmd.transactionId, IPC_MSG_CONTROL_WRITE, IPC_MSG_CONTROL_ACK, 1, 43);
        log(LOG_INFO, false, "pH controller enabled (txn=%d)\n", cmd.transactionId);
        server.send(200, "application/json", "{\"success\":true}");
    } else {
        server.send(500, "application/json", "{\"error\":\"Failed to send command to IO MCU\"}");
    }
}

void handleDisablepHController() {
    IPC_pHControllerControl_t cmd;
    memset(&cmd, 0, sizeof(cmd));
    cmd.transactionId = generateTransactionId();
    cmd.index = 43;
    cmd.objectType = OBJ_T_PH_CONTROL;
    cmd.command = PH_CMD_DISABLE;
    
    bool sent = ipc.sendPacket(IPC_MSG_CONTROL_WRITE, (uint8_t*)&cmd, sizeof(cmd));
    
    if (sent) {
        addPendingTransaction(cmd.transactionId, IPC_MSG_CONTROL_WRITE, IPC_MSG_CONTROL_ACK, 1, 43);
        log(LOG_INFO, false, "pH controller disabled (txn=%d)\n", cmd.transactionId);
        server.send(200, "application/json", "{\"success\":true}");
    } else {
        server.send(500, "application/json", "{\"error\":\"Failed to send command to IO MCU\"}");
    }
}

void handleDosepHAcid() {
    IPC_pHControllerControl_t cmd;
    memset(&cmd, 0, sizeof(cmd));
    cmd.transactionId = generateTransactionId();
    cmd.index = 43;
    cmd.objectType = OBJ_T_PH_CONTROL;
    cmd.command = PH_CMD_DOSE_ACID;
    
    bool sent = ipc.sendPacket(IPC_MSG_CONTROL_WRITE, (uint8_t*)&cmd, sizeof(cmd));
    
    if (sent) {
        addPendingTransaction(cmd.transactionId, IPC_MSG_CONTROL_WRITE, IPC_MSG_CONTROL_ACK, 1, 43);
        server.send(200, "application/json", "{\"success\":true}");
    } else {
        server.send(500, "application/json", "{\"error\":\"Failed to send command to IO MCU\"}");
    }
}

void handleDosepHAlkaline() {
    IPC_pHControllerControl_t cmd;
    memset(&cmd, 0, sizeof(cmd));
    cmd.transactionId = generateTransactionId();
    cmd.index = 43;
    cmd.objectType = OBJ_T_PH_CONTROL;
    cmd.command = PH_CMD_DOSE_ALKALINE;
    
    bool sent = ipc.sendPacket(IPC_MSG_CONTROL_WRITE, (uint8_t*)&cmd, sizeof(cmd));
    
    if (sent) {
        addPendingTransaction(cmd.transactionId, IPC_MSG_CONTROL_WRITE, IPC_MSG_CONTROL_ACK, 1, 43);
        server.send(200, "application/json", "{\"success\":true}");
    } else {
        server.send(500, "application/json", "{\"error\":\"Failed to send command to IO MCU\"}");
    }
}

void handleResetpHAcidVolume() {
    IPC_pHControllerControl_t cmd;
    memset(&cmd, 0, sizeof(cmd));
    cmd.transactionId = generateTransactionId();
    cmd.index = 43;
    cmd.objectType = OBJ_T_PH_CONTROL;
    cmd.command = PH_CMD_RESET_ACID_VOLUME;
    
    bool sent = ipc.sendPacket(IPC_MSG_CONTROL_WRITE, (uint8_t*)&cmd, sizeof(cmd));
    
    if (sent) {
        addPendingTransaction(cmd.transactionId, IPC_MSG_CONTROL_WRITE, IPC_MSG_CONTROL_ACK, 1, 43);
        server.send(200, "application/json", "{\"success\":true}");
    } else {
        server.send(500, "application/json", "{\"error\":\"Failed to send command to IO MCU\"}");
    }
}

void handleResetpHAlkalineVolume() {
    IPC_pHControllerControl_t cmd;
    memset(&cmd, 0, sizeof(cmd));
    cmd.transactionId = generateTransactionId();
    cmd.index = 43;
    cmd.objectType = OBJ_T_PH_CONTROL;
    cmd.command = PH_CMD_RESET_BASE_VOLUME;
    
    bool sent = ipc.sendPacket(IPC_MSG_CONTROL_WRITE, (uint8_t*)&cmd, sizeof(cmd));
    
    if (sent) {
        addPendingTransaction(cmd.transactionId, IPC_MSG_CONTROL_WRITE, IPC_MSG_CONTROL_ACK, 1, 43);
        server.send(200, "application/json", "{\"success\":true}");
    } else {
        server.send(500, "application/json", "{\"error\":\"Failed to send command to IO MCU\"}");
    }
}

void handleManualpHAcidDose() {
    handleDosepHAcid();
}

void handleManualpHAlkalineDose() {
    handleDosepHAlkaline();
}

// Flow Controller handlers
void handleGetFlowControllerConfig(uint8_t index) {
    if (index < 44 || index >= 44 + MAX_FLOW_CONTROLLERS) {
        server.send(400, "application/json", "{\"error\":\"Invalid flow controller index\"}");
        return;
    }
    int arrIdx = index - 44;
    
    StaticJsonDocument<512> doc;
    doc["index"] = index;
    doc["isActive"] = ioConfig.flowControllers[arrIdx].isActive;
    doc["name"] = ioConfig.flowControllers[arrIdx].name;
    doc["enabled"] = ioConfig.flowControllers[arrIdx].enabled;
    doc["showOnDashboard"] = ioConfig.flowControllers[arrIdx].showOnDashboard;
    doc["flowRate_mL_min"] = ioConfig.flowControllers[arrIdx].flowRate_mL_min;
    doc["outputType"] = ioConfig.flowControllers[arrIdx].outputType;
    doc["outputIndex"] = ioConfig.flowControllers[arrIdx].outputIndex;
    doc["motorPower"] = ioConfig.flowControllers[arrIdx].motorPower;
    doc["calibrationDoseTime_ms"] = ioConfig.flowControllers[arrIdx].calibrationDoseTime_ms;
    doc["calibrationMotorPower"] = ioConfig.flowControllers[arrIdx].calibrationMotorPower;
    doc["calibrationVolume_mL"] = ioConfig.flowControllers[arrIdx].calibrationVolume_mL;
    doc["minDosingInterval_ms"] = ioConfig.flowControllers[arrIdx].minDosingInterval_ms;
    doc["maxDosingTime_ms"] = ioConfig.flowControllers[arrIdx].maxDosingTime_ms;
    
    String response;
    serializeJson(doc, response);
    server.send(200, "application/json", response);
}

void handleSaveFlowControllerConfig(uint8_t index) {
    if (index < 44 || index >= 44 + MAX_FLOW_CONTROLLERS) {
        server.send(400, "application/json", "{\"error\":\"Invalid flow controller index\"}");
        return;
    }
    if (!server.hasArg("plain")) {
        server.send(400, "application/json", "{\"error\":\"No data provided\"}");
        return;
    }
    
    StaticJsonDocument<1024> doc;
    DeserializationError error = deserializeJson(doc, server.arg("plain"));
    if (error) {
        server.send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
        return;
    }
    
    int arrIdx = index - 44;
    float calibVol = doc["calibrationVolume_mL"] | 1.0f;
    if (calibVol <= 0.0f) {
        server.send(400, "application/json", "{\"error\":\"Calibration volume must be > 0\"}");
        return;
    }
    
    ioConfig.flowControllers[arrIdx].isActive = doc["isActive"] | true;
    strlcpy(ioConfig.flowControllers[arrIdx].name, doc["name"] | "", sizeof(ioConfig.flowControllers[arrIdx].name));
    ioConfig.flowControllers[arrIdx].enabled = false;
    if (doc.containsKey("showOnDashboard")) ioConfig.flowControllers[arrIdx].showOnDashboard = doc["showOnDashboard"];
    ioConfig.flowControllers[arrIdx].flowRate_mL_min = doc["flowRate_mL_min"] | 10.0f;
    ioConfig.flowControllers[arrIdx].outputType = doc["outputType"] | 1;
    ioConfig.flowControllers[arrIdx].outputIndex = doc["outputIndex"] | (27 + arrIdx);
    ioConfig.flowControllers[arrIdx].motorPower = doc["motorPower"] | 50;
    ioConfig.flowControllers[arrIdx].calibrationDoseTime_ms = doc["calibrationDoseTime_ms"] | 1000;
    ioConfig.flowControllers[arrIdx].calibrationMotorPower = doc["calibrationMotorPower"] | 50;
    ioConfig.flowControllers[arrIdx].calibrationVolume_mL = calibVol;
    ioConfig.flowControllers[arrIdx].minDosingInterval_ms = doc["minDosingInterval_ms"] | 1000;
    ioConfig.flowControllers[arrIdx].maxDosingTime_ms = doc["maxDosingTime_ms"] | 30000;
    
    saveIOConfig();
    
    IPC_ConfigFlowController_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.transactionId = generateTransactionId();
    cfg.index = index;
    cfg.isActive = ioConfig.flowControllers[arrIdx].isActive;
    strncpy(cfg.name, ioConfig.flowControllers[arrIdx].name, sizeof(cfg.name) - 1);
    cfg.enabled = ioConfig.flowControllers[arrIdx].enabled;
    cfg.flowRate_mL_min = ioConfig.flowControllers[arrIdx].flowRate_mL_min;
    cfg.outputType = ioConfig.flowControllers[arrIdx].outputType;
    cfg.outputIndex = ioConfig.flowControllers[arrIdx].outputIndex;
    cfg.motorPower = ioConfig.flowControllers[arrIdx].motorPower;
    cfg.calibrationDoseTime_ms = ioConfig.flowControllers[arrIdx].calibrationDoseTime_ms;
    cfg.calibrationMotorPower = ioConfig.flowControllers[arrIdx].calibrationMotorPower;
    cfg.calibrationVolume_mL = ioConfig.flowControllers[arrIdx].calibrationVolume_mL;
    cfg.minDosingInterval_ms = ioConfig.flowControllers[arrIdx].minDosingInterval_ms;
    cfg.maxDosingTime_ms = ioConfig.flowControllers[arrIdx].maxDosingTime_ms;
    
    bool sent = ipc.sendPacket(IPC_MSG_CONFIG_FLOW_CONTROLLER, (uint8_t*)&cfg, sizeof(cfg));
    if (sent) {
        addPendingTransaction(cfg.transactionId, IPC_MSG_CONFIG_FLOW_CONTROLLER, IPC_MSG_CONTROL_ACK, 1, cfg.index);
        server.send(200, "application/json", "{\"success\":true,\"message\":\"Configuration saved and applied\"}");
    } else {
        server.send(200, "application/json", "{\"success\":true,\"message\":\"Configuration saved but IO MCU update failed\"}");
    }
}

void handleDeleteFlowController(uint8_t index) {
    if (index < 44 || index >= 44 + MAX_FLOW_CONTROLLERS) {
        server.send(400, "application/json", "{\"error\":\"Invalid flow controller index\"}");
        return;
    }
    int arrIdx = index - 44;
    
    ioConfig.flowControllers[arrIdx].isActive = false;
    ioConfig.flowControllers[arrIdx].enabled = false;
    memset(ioConfig.flowControllers[arrIdx].name, 0, sizeof(ioConfig.flowControllers[arrIdx].name));
    saveIOConfig();
    
    IPC_ConfigFlowController_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.transactionId = generateTransactionId();
    cfg.index = index;
    cfg.isActive = false;
    
    bool sent = ipc.sendPacket(IPC_MSG_CONFIG_FLOW_CONTROLLER, (uint8_t*)&cfg, sizeof(cfg));
    if (sent) {
        addPendingTransaction(cfg.transactionId, IPC_MSG_CONFIG_FLOW_CONTROLLER, IPC_MSG_CONTROL_ACK, 1, cfg.index);
        server.send(200, "application/json", "{\"success\":true,\"message\":\"Flow controller deleted\"}");
    } else {
        server.send(200, "application/json", "{\"success\":true,\"message\":\"Flow controller deleted but IO MCU update failed\"}");
    }
}

void handleSetFlowRate(uint8_t index) {
    if (!server.hasArg("plain")) {
        server.send(400, "application/json", "{\"error\":\"No data provided\"}");
        return;
    }
    
    StaticJsonDocument<256> doc;
    DeserializationError error = deserializeJson(doc, server.arg("plain"));
    if (error) {
        server.send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
        return;
    }
    
    float flowRate = doc["flowRate"] | 10.0f;
    
    IPC_FlowControllerControl_t cmd;
    memset(&cmd, 0, sizeof(cmd));
    cmd.transactionId = generateTransactionId();
    cmd.index = index;
    cmd.objectType = OBJ_T_FLOW_CONTROL;
    cmd.command = FLOW_CMD_SET_FLOW_RATE;
    cmd.flowRate_mL_min = flowRate;
    
    bool sent = ipc.sendPacket(IPC_MSG_CONTROL_WRITE, (uint8_t*)&cmd, sizeof(cmd));
    if (sent) {
        addPendingTransaction(cmd.transactionId, IPC_MSG_CONTROL_WRITE, IPC_MSG_CONTROL_ACK, 1, index);
        server.send(200, "application/json", "{\"success\":true}");
    } else {
        server.send(500, "application/json", "{\"error\":\"Failed to send command to IO MCU\"}");
    }
}

void handleEnableFlowController(uint8_t index) {
    IPC_FlowControllerControl_t cmd;
    memset(&cmd, 0, sizeof(cmd));
    cmd.transactionId = generateTransactionId();
    cmd.index = index;
    cmd.objectType = OBJ_T_FLOW_CONTROL;
    cmd.command = FLOW_CMD_ENABLE;
    
    bool sent = ipc.sendPacket(IPC_MSG_CONTROL_WRITE, (uint8_t*)&cmd, sizeof(cmd));
    if (sent) {
        addPendingTransaction(cmd.transactionId, IPC_MSG_CONTROL_WRITE, IPC_MSG_CONTROL_ACK, 1, index);
        server.send(200, "application/json", "{\"success\":true}");
    } else {
        server.send(500, "application/json", "{\"error\":\"Failed to send command to IO MCU\"}");
    }
}

void handleDisableFlowController(uint8_t index) {
    IPC_FlowControllerControl_t cmd;
    memset(&cmd, 0, sizeof(cmd));
    cmd.transactionId = generateTransactionId();
    cmd.index = index;
    cmd.objectType = OBJ_T_FLOW_CONTROL;
    cmd.command = FLOW_CMD_DISABLE;
    
    bool sent = ipc.sendPacket(IPC_MSG_CONTROL_WRITE, (uint8_t*)&cmd, sizeof(cmd));
    if (sent) {
        addPendingTransaction(cmd.transactionId, IPC_MSG_CONTROL_WRITE, IPC_MSG_CONTROL_ACK, 1, index);
        server.send(200, "application/json", "{\"success\":true}");
    } else {
        server.send(500, "application/json", "{\"error\":\"Failed to send command to IO MCU\"}");
    }
}

void handleManualFlowDose(uint8_t index) {
    IPC_FlowControllerControl_t cmd;
    memset(&cmd, 0, sizeof(cmd));
    cmd.transactionId = generateTransactionId();
    cmd.index = index;
    cmd.objectType = OBJ_T_FLOW_CONTROL;
    cmd.command = FLOW_CMD_MANUAL_DOSE;
    
    bool sent = ipc.sendPacket(IPC_MSG_CONTROL_WRITE, (uint8_t*)&cmd, sizeof(cmd));
    if (sent) {
        addPendingTransaction(cmd.transactionId, IPC_MSG_CONTROL_WRITE, IPC_MSG_CONTROL_ACK, 1, index);
        server.send(200, "application/json", "{\"success\":true}");
    } else {
        server.send(500, "application/json", "{\"error\":\"Failed to send command to IO MCU\"}");
    }
}

void handleResetFlowVolume(uint8_t index) {
    IPC_FlowControllerControl_t cmd;
    memset(&cmd, 0, sizeof(cmd));
    cmd.transactionId = generateTransactionId();
    cmd.index = index;
    cmd.objectType = OBJ_T_FLOW_CONTROL;
    cmd.command = FLOW_CMD_RESET_VOLUME;
    
    bool sent = ipc.sendPacket(IPC_MSG_CONTROL_WRITE, (uint8_t*)&cmd, sizeof(cmd));
    if (sent) {
        addPendingTransaction(cmd.transactionId, IPC_MSG_CONTROL_WRITE, IPC_MSG_CONTROL_ACK, 1, index);
        server.send(200, "application/json", "{\"success\":true}");
    } else {
        server.send(500, "application/json", "{\"error\":\"Failed to send command to IO MCU\"}");
    }
}

// DO Controller handlers
void handleGetDOControllerConfig() {
    StaticJsonDocument<512> doc;
    
    doc["index"] = 48;
    doc["isActive"] = ioConfig.doController.isActive;
    doc["name"] = ioConfig.doController.name;
    doc["enabled"] = ioConfig.doController.enabled;
    doc["showOnDashboard"] = ioConfig.doController.showOnDashboard;
    doc["setpoint_mg_L"] = ioConfig.doController.setpoint_mg_L;
    doc["activeProfileIndex"] = ioConfig.doController.activeProfileIndex;
    doc["stirrerEnabled"] = ioConfig.doController.stirrerEnabled;
    doc["stirrerType"] = ioConfig.doController.stirrerType;
    doc["stirrerIndex"] = ioConfig.doController.stirrerIndex;
    doc["stirrerMaxRPM"] = ioConfig.doController.stirrerMaxRPM;
    doc["mfcEnabled"] = ioConfig.doController.mfcEnabled;
    doc["mfcDeviceIndex"] = ioConfig.doController.mfcDeviceIndex;
    
    if (ioConfig.doController.activeProfileIndex < MAX_DO_PROFILES) {
        doc["activeProfileName"] = ioConfig.doProfiles[ioConfig.doController.activeProfileIndex].name;
    }
    
    String response;
    serializeJson(doc, response);
    server.send(200, "application/json", response);
}

void handleSaveDOControllerConfig() {
    if (!server.hasArg("plain")) {
        server.send(400, "application/json", "{\"error\":\"No data provided\"}");
        return;
    }
    
    DynamicJsonDocument* doc = new DynamicJsonDocument(4096);
    if (!doc) {
        server.send(500, "application/json", "{\"error\":\"Memory allocation failed\"}");
        return;
    }
    
    DeserializationError error = deserializeJson(*doc, server.arg("plain"));
    if (error) {
        delete doc;
        server.send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
        return;
    }
    
    if (doc->containsKey("activeProfileIndex")) {
        uint8_t profIdx = (*doc)["activeProfileIndex"];
        if (profIdx >= MAX_DO_PROFILES) {
            delete doc;
            server.send(400, "application/json", "{\"error\":\"Invalid profile index\"}");
            return;
        }
    }
    
    ioConfig.doController.isActive = (*doc)["isActive"] | true;
    strlcpy(ioConfig.doController.name, (*doc)["name"] | "DO Controller", sizeof(ioConfig.doController.name));
    ioConfig.doController.enabled = false;
    if (doc->containsKey("showOnDashboard")) ioConfig.doController.showOnDashboard = (*doc)["showOnDashboard"];
    ioConfig.doController.setpoint_mg_L = (*doc)["setpoint_mg_L"] | 8.0f;
    if (doc->containsKey("activeProfileIndex")) ioConfig.doController.activeProfileIndex = (*doc)["activeProfileIndex"];
    
    if (doc->containsKey("stirrerEnabled")) {
        ioConfig.doController.stirrerEnabled = (*doc)["stirrerEnabled"];
        if (ioConfig.doController.stirrerEnabled) {
            ioConfig.doController.stirrerType = (*doc)["stirrerType"] | 0;
            ioConfig.doController.stirrerIndex = (*doc)["stirrerIndex"] | 27;
            ioConfig.doController.stirrerMaxRPM = (*doc)["stirrerMaxRPM"] | 300.0f;
        }
    }
    
    if (doc->containsKey("mfcEnabled")) {
        ioConfig.doController.mfcEnabled = (*doc)["mfcEnabled"];
        if (ioConfig.doController.mfcEnabled) {
            ioConfig.doController.mfcDeviceIndex = (*doc)["mfcDeviceIndex"] | 50;
        }
    }
    
    delete doc;
    saveIOConfig();
    
    IPC_ConfigDOController_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.transactionId = generateTransactionId();
    cfg.index = 48;
    cfg.isActive = ioConfig.doController.isActive;
    strncpy(cfg.name, ioConfig.doController.name, sizeof(cfg.name) - 1);
    cfg.enabled = false;
    cfg.showOnDashboard = ioConfig.doController.showOnDashboard;
    cfg.setpoint_mg_L = ioConfig.doController.setpoint_mg_L;
    
    uint8_t profileIdx = ioConfig.doController.activeProfileIndex;
    if (profileIdx < MAX_DO_PROFILES && ioConfig.doProfiles[profileIdx].isActive) {
        int numPoints = ioConfig.doProfiles[profileIdx].numPoints;
        cfg.numPoints = (numPoints < MAX_DO_PROFILE_POINTS) ? numPoints : MAX_DO_PROFILE_POINTS;
        for (int j = 0; j < cfg.numPoints; j++) {
            cfg.profileErrorValues[j] = ioConfig.doProfiles[profileIdx].points[j].error_mg_L;
            cfg.profileStirrerValues[j] = ioConfig.doProfiles[profileIdx].points[j].stirrerOutput;
            cfg.profileMFCValues[j] = ioConfig.doProfiles[profileIdx].points[j].mfcOutput_mL_min;
        }
    }
    
    cfg.stirrerEnabled = ioConfig.doController.stirrerEnabled;
    cfg.stirrerType = ioConfig.doController.stirrerType;
    cfg.stirrerIndex = ioConfig.doController.stirrerIndex;
    cfg.stirrerMaxRPM = ioConfig.doController.stirrerMaxRPM;
    cfg.mfcEnabled = ioConfig.doController.mfcEnabled;
    cfg.mfcDeviceIndex = ioConfig.doController.mfcDeviceIndex;
    
    bool sent = ipc.sendPacket(IPC_MSG_CONFIG_DO_CONTROLLER, (uint8_t*)&cfg, sizeof(cfg));
    if (sent) {
        addPendingTransaction(cfg.transactionId, IPC_MSG_CONFIG_DO_CONTROLLER, IPC_MSG_CONTROL_ACK, 1, cfg.index);
        server.send(200, "application/json", "{\"success\":true,\"message\":\"Configuration saved and applied\"}");
    } else {
        server.send(200, "application/json", "{\"success\":true,\"message\":\"Configuration saved but IO MCU update failed\"}");
    }
}

void handleSetDOSetpoint() {
    if (!server.hasArg("plain")) {
        server.send(400, "application/json", "{\"error\":\"No data provided\"}");
        return;
    }
    
    StaticJsonDocument<256> doc;
    DeserializationError error = deserializeJson(doc, server.arg("plain"));
    if (error) {
        server.send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
        return;
    }
    
    float setpoint = doc["setpoint"] | 8.0f;
    
    IPC_DOControllerControl_t cmd;
    memset(&cmd, 0, sizeof(cmd));
    cmd.transactionId = generateTransactionId();
    cmd.index = 48;
    cmd.objectType = OBJ_T_DISSOLVED_OXYGEN_CONTROL;
    cmd.command = DO_CMD_SET_SETPOINT;
    cmd.setpoint_mg_L = setpoint;
    
    bool sent = ipc.sendPacket(IPC_MSG_CONTROL_WRITE, (uint8_t*)&cmd, sizeof(cmd));
    if (sent) {
        addPendingTransaction(cmd.transactionId, IPC_MSG_CONTROL_WRITE, IPC_MSG_CONTROL_ACK, 1, 48);
        server.send(200, "application/json", "{\"success\":true}");
    } else {
        server.send(500, "application/json", "{\"error\":\"Failed to send command to IO MCU\"}");
    }
}

void handleEnableDOController() {
    IPC_DOControllerControl_t cmd;
    memset(&cmd, 0, sizeof(cmd));
    cmd.transactionId = generateTransactionId();
    cmd.index = 48;
    cmd.objectType = OBJ_T_DISSOLVED_OXYGEN_CONTROL;
    cmd.command = DO_CMD_ENABLE;
    
    bool sent = ipc.sendPacket(IPC_MSG_CONTROL_WRITE, (uint8_t*)&cmd, sizeof(cmd));
    
    if (sent) {
        addPendingTransaction(cmd.transactionId, IPC_MSG_CONTROL_WRITE, IPC_MSG_CONTROL_ACK, 1, 48);
        log(LOG_INFO, false, "DO controller enabled (txn=%d)\n", cmd.transactionId);
        server.send(200, "application/json", "{\"success\":true}");
    } else {
        server.send(500, "application/json", "{\"error\":\"Failed to send command to IO MCU\"}");
    }
}

void handleDisableDOController() {
    IPC_DOControllerControl_t cmd;
    memset(&cmd, 0, sizeof(cmd));
    cmd.transactionId = generateTransactionId();
    cmd.index = 48;
    cmd.objectType = OBJ_T_DISSOLVED_OXYGEN_CONTROL;
    cmd.command = DO_CMD_DISABLE;
    
    bool sent = ipc.sendPacket(IPC_MSG_CONTROL_WRITE, (uint8_t*)&cmd, sizeof(cmd));
    
    if (sent) {
        addPendingTransaction(cmd.transactionId, IPC_MSG_CONTROL_WRITE, IPC_MSG_CONTROL_ACK, 1, 48);
        log(LOG_INFO, false, "DO controller disabled (txn=%d)\n", cmd.transactionId);
        server.send(200, "application/json", "{\"success\":true}");
    } else {
        server.send(500, "application/json", "{\"error\":\"Failed to send command to IO MCU\"}");
    }
}

void handleDeleteDOController() {
    ioConfig.doController.isActive = false;
    ioConfig.doController.enabled = false;
    memset(ioConfig.doController.name, 0, sizeof(ioConfig.doController.name));
    saveIOConfig();
    
    IPC_ConfigDOController_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.index = 48;
    cfg.isActive = false;
    
    bool sent = ipc.sendPacket(IPC_MSG_CONFIG_DO_CONTROLLER, (uint8_t*)&cfg, sizeof(cfg));
    if (sent) {
        server.send(200, "application/json", "{\"success\":true,\"message\":\"DO controller deleted\"}");
    } else {
        server.send(200, "application/json", "{\"success\":true,\"message\":\"DO controller deleted but IO MCU update failed\"}");
    }
}

// DO Profile handlers
void handleGetAllDOProfiles() {
    if (ioConfig.magicNumber != IO_CONFIG_MAGIC_NUMBER) {
        server.send(200, "application/json", "{\"profiles\":[]}");
        return;
    }
    
    DynamicJsonDocument* doc = new DynamicJsonDocument(4096);
    if (!doc) {
        server.send(500, "application/json", "{\"error\":\"Memory allocation failed\"}");
        return;
    }
    
    JsonArray profiles = doc->createNestedArray("profiles");
    
    for (int i = 0; i < MAX_DO_PROFILES; i++) {
        JsonObject profile = profiles.createNestedObject();
        profile["index"] = i;
        profile["isActive"] = ioConfig.doProfiles[i].isActive;
        profile["name"] = ioConfig.doProfiles[i].name;
        
        uint8_t numPoints = ioConfig.doProfiles[i].numPoints;
        if (numPoints > MAX_DO_PROFILE_POINTS) numPoints = MAX_DO_PROFILE_POINTS;
        profile["numPoints"] = numPoints;
        
        JsonArray errors = profile.createNestedArray("errors");
        JsonArray stirrers = profile.createNestedArray("stirrers");
        JsonArray mfcs = profile.createNestedArray("mfcs");
        
        for (int j = 0; j < numPoints; j++) {
            errors.add(ioConfig.doProfiles[i].points[j].error_mg_L);
            stirrers.add(ioConfig.doProfiles[i].points[j].stirrerOutput);
            mfcs.add(ioConfig.doProfiles[i].points[j].mfcOutput_mL_min);
        }
    }
    
    if (doc->overflowed()) {
        delete doc;
        server.send(500, "application/json", "{\"error\":\"JSON buffer overflow\"}");
        return;
    }
    
    String response;
    serializeJson(*doc, response);
    delete doc;
    server.send(200, "application/json", response);
}

void handleGetDOProfile(uint8_t index) {
    if (index >= MAX_DO_PROFILES) {
        server.send(400, "application/json", "{\"error\":\"Invalid profile index\"}");
        return;
    }
    
    StaticJsonDocument<1024> doc;
    doc["index"] = index;
    doc["isActive"] = ioConfig.doProfiles[index].isActive;
    doc["name"] = ioConfig.doProfiles[index].name;
    doc["numPoints"] = ioConfig.doProfiles[index].numPoints;
    
    int maxPoints = (ioConfig.doProfiles[index].numPoints < MAX_DO_PROFILE_POINTS) 
                    ? ioConfig.doProfiles[index].numPoints : MAX_DO_PROFILE_POINTS;
    
    JsonArray errors = doc.createNestedArray("errors");
    JsonArray stirrers = doc.createNestedArray("stirrers");
    JsonArray mfcs = doc.createNestedArray("mfcs");
    
    for (int j = 0; j < maxPoints; j++) {
        errors.add(ioConfig.doProfiles[index].points[j].error_mg_L);
        stirrers.add(ioConfig.doProfiles[index].points[j].stirrerOutput);
        mfcs.add(ioConfig.doProfiles[index].points[j].mfcOutput_mL_min);
    }
    
    String response;
    serializeJson(doc, response);
    server.send(200, "application/json", response);
}

void handleSaveDOProfile(uint8_t index) {
    if (index >= MAX_DO_PROFILES) {
        server.send(400, "application/json", "{\"error\":\"Invalid profile index\"}");
        return;
    }
    
    if (!server.hasArg("plain")) {
        server.send(400, "application/json", "{\"error\":\"No data provided\"}");
        return;
    }
    
    StaticJsonDocument<1024> doc;
    DeserializationError error = deserializeJson(doc, server.arg("plain"));
    if (error) {
        server.send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
        return;
    }
    
    uint8_t numPoints = doc["numPoints"] | 0;
    if (numPoints > MAX_DO_PROFILE_POINTS) {
        server.send(400, "application/json", "{\"error\":\"Too many profile points (max 20)\"}");
        return;
    }
    
    ioConfig.doProfiles[index].isActive = doc["isActive"] | true;
    strlcpy(ioConfig.doProfiles[index].name, doc["name"] | "", sizeof(ioConfig.doProfiles[index].name));
    ioConfig.doProfiles[index].numPoints = numPoints;
    
    JsonArray errors = doc["errors"];
    JsonArray stirrers = doc["stirrers"];
    JsonArray mfcs = doc["mfcs"];
    
    if (errors && stirrers && mfcs) {
        for (int j = 0; j < numPoints && j < MAX_DO_PROFILE_POINTS; j++) {
            ioConfig.doProfiles[index].points[j].error_mg_L = errors[j] | 0.0f;
            ioConfig.doProfiles[index].points[j].stirrerOutput = stirrers[j] | 0.0f;
            ioConfig.doProfiles[index].points[j].mfcOutput_mL_min = mfcs[j] | 0.0f;
        }
    }
    
    saveIOConfig();
    
    if (ioConfig.doController.isActive && ioConfig.doController.activeProfileIndex == index) {
        IPC_ConfigDOController_t cfg;
        memset(&cfg, 0, sizeof(cfg));
        cfg.index = 48;
        cfg.isActive = true;
        strncpy(cfg.name, ioConfig.doController.name, sizeof(cfg.name) - 1);
        cfg.enabled = ioConfig.doController.enabled;
        cfg.setpoint_mg_L = ioConfig.doController.setpoint_mg_L;
        cfg.numPoints = (numPoints < MAX_DO_PROFILE_POINTS) ? numPoints : MAX_DO_PROFILE_POINTS;
        for (int j = 0; j < cfg.numPoints; j++) {
            cfg.profileErrorValues[j] = ioConfig.doProfiles[index].points[j].error_mg_L;
            cfg.profileStirrerValues[j] = ioConfig.doProfiles[index].points[j].stirrerOutput;
            cfg.profileMFCValues[j] = ioConfig.doProfiles[index].points[j].mfcOutput_mL_min;
        }
        cfg.stirrerEnabled = ioConfig.doController.stirrerEnabled;
        cfg.stirrerType = ioConfig.doController.stirrerType;
        cfg.stirrerIndex = ioConfig.doController.stirrerIndex;
        cfg.stirrerMaxRPM = ioConfig.doController.stirrerMaxRPM;
        cfg.mfcEnabled = ioConfig.doController.mfcEnabled;
        cfg.mfcDeviceIndex = ioConfig.doController.mfcDeviceIndex;
        ipc.sendPacket(IPC_MSG_CONFIG_DO_CONTROLLER, (uint8_t*)&cfg, sizeof(cfg));
    }
    
    server.send(200, "application/json", "{\"success\":true,\"message\":\"Profile saved\"}");
}

void handleDeleteDOProfile(uint8_t index) {
    if (index >= MAX_DO_PROFILES) {
        server.send(400, "application/json", "{\"error\":\"Invalid profile index\"}");
        return;
    }
    
    if (ioConfig.doController.isActive && ioConfig.doController.activeProfileIndex == index) {
        server.send(400, "application/json", "{\"error\":\"Cannot delete active profile. Switch to another profile first.\"}");
        return;
    }
    
    ioConfig.doProfiles[index].isActive = false;
    memset(ioConfig.doProfiles[index].name, 0, sizeof(ioConfig.doProfiles[index].name));
    ioConfig.doProfiles[index].numPoints = 0;
    memset(ioConfig.doProfiles[index].points, 0, sizeof(ioConfig.doProfiles[index].points));
    
    saveIOConfig();
    server.send(200, "application/json", "{\"success\":true,\"message\":\"Profile deleted\"}");
}
