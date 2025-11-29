/**
 * @file apiOutputs.cpp
 * @brief Output control and configuration API implementation
 */

#include "apiOutputs.h"
#include "../network/networkManager.h"
#include "../config/ioConfig.h"
#include "../utils/logger.h"
#include "../utils/objectCache.h"
#include "../utils/ipcManager.h"
#include <ArduinoJson.h>

// =============================================================================
// Setup Function
// =============================================================================

void setupOutputsAPI(void)
{
    // Get all outputs status
    server.on("/api/outputs", HTTP_GET, handleGetOutputs);
    
    // Digital output configuration endpoints (indices 21-25)
    for (uint8_t i = 21; i < 21 + MAX_DIGITAL_OUTPUTS; i++) {
        String configPath = "/api/config/output/" + String(i);
        String statePath = "/api/output/" + String(i) + "/state";
        String valuePath = "/api/output/" + String(i) + "/value";
        
        server.on(configPath.c_str(), HTTP_GET, [i]() { handleGetDigitalOutputConfig(i); });
        server.on(configPath.c_str(), HTTP_POST, [i]() { handleSaveDigitalOutputConfig(i); });
        server.on(statePath.c_str(), HTTP_POST, [i]() { handleSetOutputState(i); });
        server.on(valuePath.c_str(), HTTP_POST, [i]() { handleSetOutputValue(i); });
    }
    
    // DAC output control endpoints (indices 8-9)
    for (uint8_t i = 8; i < 10; i++) {
        String valuePath = "/api/dac/" + String(i) + "/value";
        server.on(valuePath.c_str(), HTTP_POST, [i]() { handleSetAnalogOutputValue(i); });
    }
    
    // Stepper motor endpoints (index 26)
    server.on("/api/config/stepper", HTTP_GET, handleGetStepperConfig);
    server.on("/api/config/stepper", HTTP_POST, handleSaveStepperConfig);
    server.on("/api/stepper/rpm", HTTP_POST, handleSetStepperRPM);
    server.on("/api/stepper/direction", HTTP_POST, handleSetStepperDirection);
    server.on("/api/stepper/start", HTTP_POST, handleStartStepper);
    server.on("/api/stepper/stop", HTTP_POST, handleStopStepper);
    
    // DC motor endpoints (indices 27-30)
    for (uint8_t i = 27; i < 27 + MAX_DC_MOTORS; i++) {
        String configPath = "/api/config/dcmotor/" + String(i);
        String powerPath = "/api/dcmotor/" + String(i) + "/power";
        String dirPath = "/api/dcmotor/" + String(i) + "/direction";
        String startPath = "/api/dcmotor/" + String(i) + "/start";
        String stopPath = "/api/dcmotor/" + String(i) + "/stop";
        
        server.on(configPath.c_str(), HTTP_GET, [i]() { handleGetDCMotorConfig(i); });
        server.on(configPath.c_str(), HTTP_POST, [i]() { handleSaveDCMotorConfig(i); });
        server.on(powerPath.c_str(), HTTP_POST, [i]() { handleSetDCMotorPower(i); });
        server.on(dirPath.c_str(), HTTP_POST, [i]() { handleSetDCMotorDirection(i); });
        server.on(startPath.c_str(), HTTP_POST, [i]() { handleStartDCMotor(i); });
        server.on(stopPath.c_str(), HTTP_POST, [i]() { handleStopDCMotor(i); });
    }
}

// =============================================================================
// Output Status Handler
// =============================================================================

void handleGetOutputs() {
    StaticJsonDocument<2048> doc;
    
    // DAC Analog Outputs (indices 8-9)
    JsonArray dacOutputs = doc.createNestedArray("dacOutputs");
    for (int i = 0; i < MAX_DAC_OUTPUTS; i++) {
        JsonObject dac = dacOutputs.createNestedObject();
        uint16_t index = 8 + i;
        dac["index"] = index;
        dac["name"] = ioConfig.dacOutputs[i].name;
        dac["unit"] = ioConfig.dacOutputs[i].unit;
        dac["d"] = ioConfig.dacOutputs[i].showOnDashboard;
        
        ObjectCache::CachedObject* dacObj = objectCache.getObject(index);
        if (dacObj && dacObj->valid && dacObj->lastUpdate > 0) {
            dac["value"] = dacObj->value;
        } else {
            dac["value"] = 0.0f;
        }
    }
    
    // Digital Outputs (indices 21-25)
    JsonArray digitalOutputs = doc.createNestedArray("digitalOutputs");
    for (int i = 0; i < MAX_DIGITAL_OUTPUTS; i++) {
        JsonObject output = digitalOutputs.createNestedObject();
        uint16_t index = 21 + i;
        output["index"] = index;
        output["name"] = ioConfig.digitalOutputs[i].name;
        output["mode"] = (uint8_t)ioConfig.digitalOutputs[i].mode;
        output["d"] = ioConfig.digitalOutputs[i].showOnDashboard;
        
        ObjectCache::CachedObject* cachedObj = objectCache.getObject(index);
        if (cachedObj && cachedObj->valid && cachedObj->lastUpdate > 0) {
            output["value"] = cachedObj->value;
            output["state"] = cachedObj->value > 0;
        } else {
            output["state"] = false;
            output["value"] = 0;
        }
    }
    
    // Stepper Motor (index 26)
    JsonObject stepper = doc.createNestedObject("stepperMotor");
    stepper["name"] = ioConfig.stepperMotor.name;
    stepper["d"] = ioConfig.stepperMotor.showOnDashboard;
    stepper["maxRPM"] = ioConfig.stepperMotor.maxRPM;
    
    ObjectCache::CachedObject* stepperObj = objectCache.getObject(26);
    if (stepperObj && stepperObj->valid && stepperObj->lastUpdate > 0) {
        stepper["rpm"] = stepperObj->value;
        stepper["running"] = (stepperObj->flags & IPC_SENSOR_FLAG_RUNNING) ? true : false;
        stepper["direction"] = (stepperObj->flags & IPC_SENSOR_FLAG_DIRECTION) ? true : false;
    } else {
        stepper["running"] = false;
        stepper["rpm"] = 0;
        stepper["direction"] = true;
    }
    
    // DC Motors (indices 27-30)
    JsonArray dcMotors = doc.createNestedArray("dcMotors");
    for (int i = 0; i < MAX_DC_MOTORS; i++) {
        JsonObject motor = dcMotors.createNestedObject();
        uint16_t index = 27 + i;
        motor["index"] = index;
        motor["name"] = ioConfig.dcMotors[i].name;
        motor["d"] = ioConfig.dcMotors[i].showOnDashboard;
        
        ObjectCache::CachedObject* motorObj = objectCache.getObject(index);
        if (motorObj && motorObj->valid && motorObj->lastUpdate > 0) {
            motor["power"] = motorObj->value;
            motor["running"] = (motorObj->flags & IPC_SENSOR_FLAG_RUNNING) ? true : false;
            motor["direction"] = (motorObj->flags & IPC_SENSOR_FLAG_DIRECTION) ? true : false;
            
            if (motorObj->valueCount > 0) {
                motor["current"] = motorObj->additionalValues[0];
            } else {
                motor["current"] = 0.0f;
            }
        } else {
            motor["running"] = false;
            motor["power"] = 0;
            motor["direction"] = true;
            motor["current"] = 0.0f;
        }
    }
    
    String response;
    serializeJson(doc, response);
    server.send(200, "application/json", response);
}

// =============================================================================
// Digital Output Handlers
// =============================================================================

void handleGetDigitalOutputConfig(uint8_t index) {
    if (index < 21 || index >= 21 + MAX_DIGITAL_OUTPUTS) {
        server.send(400, "application/json", "{\"error\":\"Invalid output index\"}");
        return;
    }
    
    int outputIdx = index - 21;
    StaticJsonDocument<256> doc;
    doc["index"] = index;
    doc["name"] = ioConfig.digitalOutputs[outputIdx].name;
    doc["mode"] = (uint8_t)ioConfig.digitalOutputs[outputIdx].mode;
    doc["enabled"] = ioConfig.digitalOutputs[outputIdx].enabled;
    doc["showOnDashboard"] = ioConfig.digitalOutputs[outputIdx].showOnDashboard;
    
    String response;
    serializeJson(doc, response);
    server.send(200, "application/json", response);
}

void handleSaveDigitalOutputConfig(uint8_t index) {
    if (index < 21 || index >= 21 + MAX_DIGITAL_OUTPUTS) {
        server.send(400, "application/json", "{\"error\":\"Invalid output index\"}");
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
    
    int outputIdx = index - 21;
    
    if (doc.containsKey("name")) {
        strlcpy(ioConfig.digitalOutputs[outputIdx].name, doc["name"] | "", 
                sizeof(ioConfig.digitalOutputs[outputIdx].name));
    }
    
    if (doc.containsKey("mode")) {
        ioConfig.digitalOutputs[outputIdx].mode = (OutputMode)(doc["mode"] | 0);
    }
    
    if (doc.containsKey("enabled")) {
        ioConfig.digitalOutputs[outputIdx].enabled = doc["enabled"] | true;
    }
    
    if (doc.containsKey("showOnDashboard")) {
        ioConfig.digitalOutputs[outputIdx].showOnDashboard = doc["showOnDashboard"] | false;
    }
    
    saveIOConfig();
    
    IPC_ConfigDigitalOutput_t cfg;
    cfg.index = index;
    strncpy(cfg.name, ioConfig.digitalOutputs[outputIdx].name, sizeof(cfg.name) - 1);
    cfg.name[sizeof(cfg.name) - 1] = '\0';
    cfg.mode = (uint8_t)ioConfig.digitalOutputs[outputIdx].mode;
    cfg.enabled = ioConfig.digitalOutputs[outputIdx].enabled;
    
    bool sent = ipc.sendPacket(IPC_MSG_CONFIG_DIGITAL_OUTPUT, (uint8_t*)&cfg, sizeof(cfg));
    
    if (sent) {
        log(LOG_INFO, false, "Pushed DigitalOutput[%d] config to IO MCU\n", index);
        server.send(200, "application/json", "{\"success\":true,\"message\":\"Config saved and pushed\"}");
    } else {
        log(LOG_WARNING, false, "Failed to push DigitalOutput[%d] config (queue full)\n", index);
        server.send(200, "application/json", "{\"success\":true,\"warning\":\"Saved but IPC queue full\"}");
    }
}

void handleSetOutputState(uint8_t index) {
    if (index < 21 || index >= 21 + MAX_DIGITAL_OUTPUTS) {
        server.send(400, "application/json", "{\"error\":\"Invalid output index\"}");
        return;
    }
    
    if (!server.hasArg("plain")) {
        server.send(400, "application/json", "{\"error\":\"No data provided\"}");
        return;
    }
    
    StaticJsonDocument<128> doc;
    DeserializationError error = deserializeJson(doc, server.arg("plain"));
    
    if (error || !doc.containsKey("state")) {
        server.send(400, "application/json", "{\"error\":\"Invalid request\"}");
        return;
    }
    
    bool state = doc["state"];
    
    bool sent = sendDigitalOutputCommand(index, DOUT_CMD_SET_STATE, state, 0);
    
    if (sent) {
        log(LOG_INFO, false, "Set output %d state: %s\n", index, state ? "ON" : "OFF");
        server.send(200, "application/json", "{\"success\":true,\"message\":\"Command sent\"}");
    } else {
        log(LOG_WARNING, false, "Failed to set output %d: IPC queue full\n", index);
        server.send(503, "application/json", "{\"error\":\"IPC queue full, try again\"}");
    }
}

void handleSetOutputValue(uint8_t index) {
    if (index < 21 || index >= 21 + MAX_DIGITAL_OUTPUTS) {
        server.send(400, "application/json", "{\"error\":\"Invalid output index\"}");
        return;
    }
    
    if (!server.hasArg("plain")) {
        server.send(400, "application/json", "{\"error\":\"No data provided\"}");
        return;
    }
    
    StaticJsonDocument<128> doc;
    DeserializationError error = deserializeJson(doc, server.arg("plain"));
    
    if (error || !doc.containsKey("value")) {
        server.send(400, "application/json", "{\"error\":\"Invalid request\"}");
        return;
    }
    
    float value = doc["value"] | 0.0f;
    
    if (value < 0.0f || value > 100.0f) {
        server.send(400, "application/json", "{\"error\":\"Value must be 0-100%\"}");
        return;
    }
    
    bool sent = sendDigitalOutputCommand(index, DOUT_CMD_SET_PWM, false, value);
    
    if (sent) {
        log(LOG_INFO, false, "Set output %d PWM value: %.1f%%\n", index, value);
        server.send(200, "application/json", "{\"success\":true,\"message\":\"Command sent\"}");
    } else {
        log(LOG_WARNING, false, "Failed to set output %d PWM: IPC queue full\n", index);
        server.send(503, "application/json", "{\"error\":\"IPC queue full, try again\"}");
    }
}

// =============================================================================
// Analog Output (DAC) Handlers
// =============================================================================

void handleSetAnalogOutputValue(uint8_t index) {
    if (index < 8 || index > 9) {
        server.send(400, "application/json", "{\"error\":\"Invalid DAC index\"}");
        return;
    }
    
    if (!server.hasArg("plain")) {
        server.send(400, "application/json", "{\"error\":\"No data provided\"}");
        return;
    }
    
    StaticJsonDocument<128> doc;
    DeserializationError error = deserializeJson(doc, server.arg("plain"));
    
    if (error || !doc.containsKey("value")) {
        server.send(400, "application/json", "{\"error\":\"Invalid request\"}");
        return;
    }
    
    float value = doc["value"] | 0.0f;
    
    if (value < 0.0f || value > 10240.0f) {
        server.send(400, "application/json", "{\"error\":\"Value must be 0-10240 mV\"}");
        return;
    }
    
    bool sent = sendAnalogOutputCommand(index, AOUT_CMD_SET_VALUE, value);
    
    if (sent) {
        log(LOG_INFO, false, "Set DAC %d value: %.1f mV\n", index, value);
        server.send(200, "application/json", "{\"success\":true,\"message\":\"Command sent\"}");
    } else {
        log(LOG_WARNING, false, "Failed to set DAC %d: IPC queue full\n", index);
        server.send(503, "application/json", "{\"error\":\"IPC queue full, try again\"}");
    }
}

// =============================================================================
// Stepper Motor Handlers
// =============================================================================

void handleGetStepperConfig() {
    StaticJsonDocument<512> doc;
    doc["name"] = ioConfig.stepperMotor.name;
    doc["stepsPerRev"] = ioConfig.stepperMotor.stepsPerRev;
    doc["maxRPM"] = ioConfig.stepperMotor.maxRPM;
    doc["holdCurrent_mA"] = ioConfig.stepperMotor.holdCurrent_mA;
    doc["runCurrent_mA"] = ioConfig.stepperMotor.runCurrent_mA;
    doc["acceleration"] = ioConfig.stepperMotor.acceleration;
    doc["invertDirection"] = ioConfig.stepperMotor.invertDirection;
    doc["enabled"] = ioConfig.stepperMotor.enabled;
    doc["showOnDashboard"] = ioConfig.stepperMotor.showOnDashboard;
    
    doc["stealthChopEnabled"] = ioConfig.stepperMotor.stealthChopEnabled;
    doc["coolStepEnabled"] = ioConfig.stepperMotor.coolStepEnabled;
    doc["fullStepEnabled"] = ioConfig.stepperMotor.fullStepEnabled;
    doc["stealthChopMaxRPM"] = ioConfig.stepperMotor.stealthChopMaxRPM;
    doc["coolStepMinRPM"] = ioConfig.stepperMotor.coolStepMinRPM;
    doc["fullStepMinRPM"] = ioConfig.stepperMotor.fullStepMinRPM;
    
    String response;
    serializeJson(doc, response);
    server.send(200, "application/json", response);
}

void handleSaveStepperConfig() {
    if (!server.hasArg("plain")) {
        server.send(400, "application/json", "{\"error\":\"No data provided\"}");
        return;
    }
    
    StaticJsonDocument<512> doc;
    DeserializationError error = deserializeJson(doc, server.arg("plain"));
    
    if (error) {
        server.send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
        return;
    }
    
    if (doc.containsKey("name")) {
        strlcpy(ioConfig.stepperMotor.name, doc["name"] | "", 
                sizeof(ioConfig.stepperMotor.name));
    }
    
    if (doc.containsKey("stepsPerRev")) {
        ioConfig.stepperMotor.stepsPerRev = doc["stepsPerRev"] | 200;
    }
    
    if (doc.containsKey("maxRPM")) {
        ioConfig.stepperMotor.maxRPM = doc["maxRPM"] | 500;
    }
    
    if (doc.containsKey("holdCurrent_mA")) {
        ioConfig.stepperMotor.holdCurrent_mA = doc["holdCurrent_mA"] | 50;
    }
    
    if (doc.containsKey("runCurrent_mA")) {
        ioConfig.stepperMotor.runCurrent_mA = doc["runCurrent_mA"] | 100;
    }
    
    if (doc.containsKey("acceleration")) {
        ioConfig.stepperMotor.acceleration = doc["acceleration"] | 100;
    }
    
    if (doc.containsKey("invertDirection")) {
        ioConfig.stepperMotor.invertDirection = doc["invertDirection"] | false;
    }
    
    if (doc.containsKey("enabled")) {
        ioConfig.stepperMotor.enabled = doc["enabled"] | true;
    }
    
    if (doc.containsKey("showOnDashboard")) {
        ioConfig.stepperMotor.showOnDashboard = doc["showOnDashboard"] | false;
    }
    
    if (doc.containsKey("stealthChopEnabled")) {
        ioConfig.stepperMotor.stealthChopEnabled = doc["stealthChopEnabled"] | false;
    }
    
    if (doc.containsKey("coolStepEnabled")) {
        ioConfig.stepperMotor.coolStepEnabled = doc["coolStepEnabled"] | false;
    }
    
    if (doc.containsKey("fullStepEnabled")) {
        ioConfig.stepperMotor.fullStepEnabled = doc["fullStepEnabled"] | false;
    }
    
    if (doc.containsKey("stealthChopMaxRPM")) {
        ioConfig.stepperMotor.stealthChopMaxRPM = doc["stealthChopMaxRPM"] | 100.0;
    }
    
    if (doc.containsKey("coolStepMinRPM")) {
        ioConfig.stepperMotor.coolStepMinRPM = doc["coolStepMinRPM"] | 200.0;
    }
    
    if (doc.containsKey("fullStepMinRPM")) {
        ioConfig.stepperMotor.fullStepMinRPM = doc["fullStepMinRPM"] | 300.0;
    }
    
    // Validate TMC5130 constraints
    if (ioConfig.stepperMotor.holdCurrent_mA < 1 || ioConfig.stepperMotor.holdCurrent_mA > 1000) {
        server.send(400, "application/json", "{\"error\":\"Hold current must be 1-1000 mA\"}");
        return;
    }
    
    if (ioConfig.stepperMotor.runCurrent_mA < 1 || ioConfig.stepperMotor.runCurrent_mA > 1800) {
        server.send(400, "application/json", "{\"error\":\"Run current must be 1-1800 mA\"}");
        return;
    }
    
    if (ioConfig.stepperMotor.maxRPM < 1 || ioConfig.stepperMotor.maxRPM > 3000) {
        return;
    }
    
    if (ioConfig.stepperMotor.acceleration < 1 || 
        ioConfig.stepperMotor.acceleration > ioConfig.stepperMotor.maxRPM) {
        server.send(400, "application/json", "{\"error\":\"Acceleration must be 1-maxRPM RPM/s\"}");
        return;
    }
    
    if (ioConfig.stepperMotor.stepsPerRev < 1 || ioConfig.stepperMotor.stepsPerRev > 10000) {
        server.send(400, "application/json", "{\"error\":\"Steps per revolution must be 1-10000\"}");
        return;
    }
    
    // Validate RPM thresholds
    if (ioConfig.stepperMotor.stealthChopMaxRPM >= ioConfig.stepperMotor.coolStepMinRPM) {
        server.send(400, "application/json", "{\"error\":\"StealthChop Max RPM must be less than CoolStep Min RPM\"}");
        return;
    }
    
    if (ioConfig.stepperMotor.coolStepMinRPM >= ioConfig.stepperMotor.fullStepMinRPM) {
        server.send(400, "application/json", "{\"error\":\"CoolStep Min RPM must be less than FullStep Min RPM\"}");
        return;
    }
    
    if (ioConfig.stepperMotor.fullStepMinRPM >= ioConfig.stepperMotor.maxRPM) {
        server.send(400, "application/json", "{\"error\":\"FullStep Min RPM must be less than Max RPM\"}");
        return;
    }
    
    saveIOConfig();
    
    IPC_ConfigStepper_t cfg;
    cfg.index = 26;
    strncpy(cfg.name, ioConfig.stepperMotor.name, sizeof(cfg.name) - 1);
    cfg.name[sizeof(cfg.name) - 1] = '\0';
    cfg.stepsPerRev = ioConfig.stepperMotor.stepsPerRev;
    cfg.maxRPM = ioConfig.stepperMotor.maxRPM;
    cfg.holdCurrent_mA = ioConfig.stepperMotor.holdCurrent_mA;
    cfg.runCurrent_mA = ioConfig.stepperMotor.runCurrent_mA;
    cfg.acceleration = ioConfig.stepperMotor.acceleration;
    cfg.invertDirection = ioConfig.stepperMotor.invertDirection;
    cfg.enabled = ioConfig.stepperMotor.enabled;
    cfg.stealthChopEnabled = ioConfig.stepperMotor.stealthChopEnabled;
    cfg.coolStepEnabled = ioConfig.stepperMotor.coolStepEnabled;
    cfg.fullStepEnabled = ioConfig.stepperMotor.fullStepEnabled;
    cfg.stealthChopMaxRPM = ioConfig.stepperMotor.stealthChopMaxRPM;
    cfg.coolStepMinRPM = ioConfig.stepperMotor.coolStepMinRPM;
    cfg.fullStepMinRPM = ioConfig.stepperMotor.fullStepMinRPM;
    
    bool sent = ipc.sendPacket(IPC_MSG_CONFIG_STEPPER, (uint8_t*)&cfg, sizeof(cfg));
    
    if (sent) {
        log(LOG_INFO, false, "Pushed Stepper config to IO MCU\n");
        server.send(200, "application/json", "{\"success\":true,\"message\":\"Config saved and pushed\"}");
    } else {
        log(LOG_WARNING, false, "Failed to push Stepper config (queue full)\n");
        server.send(200, "application/json", "{\"success\":true,\"warning\":\"Saved but IPC queue full\"}");
    }
}

void handleSetStepperRPM() {
    if (!server.hasArg("plain")) {
        server.send(400, "application/json", "{\"error\":\"No data provided\"}");
        return;
    }
    
    StaticJsonDocument<128> doc;
    DeserializationError error = deserializeJson(doc, server.arg("plain"));
    
    if (error || !doc.containsKey("rpm")) {
        server.send(400, "application/json", "{\"error\":\"Invalid request\"}");
        return;
    }
    
    float rpm = doc["rpm"] | 0.0f;
    
    if (rpm > ioConfig.stepperMotor.maxRPM) {
        server.send(400, "application/json", "{\"error\":\"RPM exceeds maximum\"}");
        return;
    }
    
    bool sent = sendStepperCommand(STEPPER_CMD_SET_RPM, rpm, true);
    
    if (sent) {
        log(LOG_INFO, false, "Set stepper RPM: %.1f\n", rpm);
        server.send(200, "application/json", "{\"success\":true,\"message\":\"Command sent\"}");
    } else {
        server.send(503, "application/json", "{\"error\":\"IPC queue full, try again\"}");
    }
}

void handleSetStepperDirection() {
    if (!server.hasArg("plain")) {
        server.send(400, "application/json", "{\"error\":\"No data provided\"}");
        return;
    }
    
    StaticJsonDocument<128> doc;
    DeserializationError error = deserializeJson(doc, server.arg("plain"));
    
    if (error || !doc.containsKey("forward")) {
        server.send(400, "application/json", "{\"error\":\"Invalid request\"}");
        return;
    }
    
    bool forward = doc["forward"];
    
    bool sent = sendStepperCommand(STEPPER_CMD_SET_DIR, 0, forward);
    
    if (sent) {
        log(LOG_INFO, false, "Set stepper direction: %s\n", forward ? "Forward" : "Reverse");
        server.send(200, "application/json", "{\"success\":true,\"message\":\"Command sent\"}");
    } else {
        server.send(503, "application/json", "{\"error\":\"IPC queue full, try again\"}");
    }
}

void handleStartStepper() {
    if (!server.hasArg("plain")) {
        server.send(400, "application/json", "{\"error\":\"No data provided\"}");
        return;
    }
    
    StaticJsonDocument<128> doc;
    DeserializationError error = deserializeJson(doc, server.arg("plain"));
    
    if (error) {
        server.send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
        return;
    }
    
    float rpm = doc["rpm"] | 0.0f;
    bool forward = doc["forward"] | true;
    
    if (rpm > ioConfig.stepperMotor.maxRPM) {
        server.send(400, "application/json", "{\"error\":\"RPM exceeds maximum\"}");
        return;
    }
    
    ObjectCache::CachedObject* stepperObj = objectCache.getObject(26);
    bool isRunning = false;
    if (stepperObj && stepperObj->valid) {
        isRunning = (stepperObj->flags & IPC_SENSOR_FLAG_RUNNING) ? true : false;
    }
    
    uint8_t command = isRunning ? STEPPER_CMD_UPDATE : STEPPER_CMD_START;
    bool sent = sendStepperCommand(command, rpm, forward);
    
    if (sent) {
        log(LOG_INFO, false, "%s stepper: RPM=%.1f, Direction=%s\n", 
            isRunning ? "Update" : "Start", rpm, forward ? "Forward" : "Reverse");
        server.send(200, "application/json", "{\"success\":true,\"message\":\"Command sent\"}");
    } else {
        log(LOG_WARNING, false, "Failed to %s stepper: IPC queue full\n", 
            isRunning ? "update" : "start");
        server.send(503, "application/json", "{\"error\":\"IPC queue full, try again\"}");
    }
}

void handleStopStepper() {
    bool sent = sendStepperCommand(STEPPER_CMD_STOP, 0, false);
    
    if (sent) {
        log(LOG_INFO, false, "Stop stepper motor\n");
        server.send(200, "application/json", "{\"success\":true,\"message\":\"Command sent\"}");
    } else {
        log(LOG_WARNING, false, "Failed to stop stepper: IPC queue full\n");
        server.send(503, "application/json", "{\"error\":\"IPC queue full, try again\"}");
    }
}

// =============================================================================
// DC Motor Handlers
// =============================================================================

void handleGetDCMotorConfig(uint8_t index) {
    if (index < 27 || index >= 27 + MAX_DC_MOTORS) {
        server.send(400, "application/json", "{\"error\":\"Invalid motor index\"}");
        return;
    }
    
    int motorIdx = index - 27;
    StaticJsonDocument<256> doc;
    doc["index"] = index;
    doc["name"] = ioConfig.dcMotors[motorIdx].name;
    doc["invertDirection"] = ioConfig.dcMotors[motorIdx].invertDirection;
    doc["enabled"] = ioConfig.dcMotors[motorIdx].enabled;
    doc["showOnDashboard"] = ioConfig.dcMotors[motorIdx].showOnDashboard;
    
    String response;
    serializeJson(doc, response);
    server.send(200, "application/json", response);
}

void handleSaveDCMotorConfig(uint8_t index) {
    if (index < 27 || index >= 27 + MAX_DC_MOTORS) {
        server.send(400, "application/json", "{\"error\":\"Invalid motor index\"}");
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
    
    int motorIdx = index - 27;
    
    if (doc.containsKey("name")) {
        strlcpy(ioConfig.dcMotors[motorIdx].name, doc["name"] | "", 
                sizeof(ioConfig.dcMotors[motorIdx].name));
    }
    
    if (doc.containsKey("invertDirection")) {
        ioConfig.dcMotors[motorIdx].invertDirection = doc["invertDirection"] | false;
    }
    
    if (doc.containsKey("enabled")) {
        ioConfig.dcMotors[motorIdx].enabled = doc["enabled"] | true;
    }
    
    if (doc.containsKey("showOnDashboard")) {
        ioConfig.dcMotors[motorIdx].showOnDashboard = doc["showOnDashboard"] | false;
    }
    
    saveIOConfig();
    
    IPC_ConfigDCMotor_t cfg;
    cfg.index = index;
    strncpy(cfg.name, ioConfig.dcMotors[motorIdx].name, sizeof(cfg.name) - 1);
    cfg.name[sizeof(cfg.name) - 1] = '\0';
    cfg.invertDirection = ioConfig.dcMotors[motorIdx].invertDirection;
    cfg.enabled = ioConfig.dcMotors[motorIdx].enabled;
    
    bool sent = ipc.sendPacket(IPC_MSG_CONFIG_DCMOTOR, (uint8_t*)&cfg, sizeof(cfg));
    
    if (sent) {
        log(LOG_INFO, false, "Pushed DCMotor[%d] config to IO MCU\n", index);
        server.send(200, "application/json", "{\"success\":true,\"message\":\"Config saved and pushed\"}");
    } else {
        log(LOG_WARNING, false, "Failed to push DCMotor[%d] config (queue full)\n", index);
        server.send(200, "application/json", "{\"success\":true,\"warning\":\"Saved but IPC queue full\"}");
    }
}

void handleSetDCMotorPower(uint8_t index) {
    if (index < 27 || index >= 27 + MAX_DC_MOTORS) {
        server.send(400, "application/json", "{\"error\":\"Invalid motor index\"}");
        return;
    }
    
    if (!server.hasArg("plain")) {
        server.send(400, "application/json", "{\"error\":\"No data provided\"}");
        return;
    }
    
    StaticJsonDocument<128> doc;
    DeserializationError error = deserializeJson(doc, server.arg("plain"));
    
    if (error || !doc.containsKey("power")) {
        server.send(400, "application/json", "{\"error\":\"Invalid request\"}");
        return;
    }
    
    float power = doc["power"] | 0.0f;
    
    if (power < 0.0f || power > 100.0f) {
        server.send(400, "application/json", "{\"error\":\"Power must be 0-100%\"}");
        return;
    }
    
    bool sent = sendDCMotorCommand(index, DCMOTOR_CMD_SET_POWER, power, true);
    
    if (sent) {
        log(LOG_INFO, false, "Set DC motor %d power: %.1f%%\n", index, power);
        server.send(200, "application/json", "{\"success\":true,\"message\":\"Command sent\"}");
    } else {
        server.send(503, "application/json", "{\"error\":\"IPC queue full, try again\"}");
    }
}

void handleSetDCMotorDirection(uint8_t index) {
    if (index < 27 || index >= 27 + MAX_DC_MOTORS) {
        server.send(400, "application/json", "{\"error\":\"Invalid motor index\"}");
        return;
    }
    
    if (!server.hasArg("plain")) {
        server.send(400, "application/json", "{\"error\":\"No data provided\"}");
        return;
    }
    
    StaticJsonDocument<128> doc;
    DeserializationError error = deserializeJson(doc, server.arg("plain"));
    
    if (error || !doc.containsKey("forward")) {
        server.send(400, "application/json", "{\"error\":\"Invalid request\"}");
        return;
    }
    
    bool forward = doc["forward"];
    
    bool sent = sendDCMotorCommand(index, DCMOTOR_CMD_SET_DIR, 0, forward);
    
    if (sent) {
        log(LOG_INFO, false, "Set DC motor %d direction: %s\n", 
            index, forward ? "Forward" : "Reverse");
        server.send(200, "application/json", "{\"success\":true,\"message\":\"Command sent\"}");
    } else {
        server.send(503, "application/json", "{\"error\":\"IPC queue full, try again\"}");
    }
}

void handleStartDCMotor(uint8_t index) {
    if (index < 27 || index >= 27 + MAX_DC_MOTORS) {
        server.send(400, "application/json", "{\"error\":\"Invalid motor index\"}");
        return;
    }
    
    if (!server.hasArg("plain")) {
        server.send(400, "application/json", "{\"error\":\"No data provided\"}");
        return;
    }
    
    StaticJsonDocument<128> doc;
    DeserializationError error = deserializeJson(doc, server.arg("plain"));
    
    if (error) {
        server.send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
        return;
    }
    
    float power = doc["power"] | 0.0f;
    bool forward = doc["forward"] | true;
    
    if (power < 0.0f || power > 100.0f) {
        server.send(400, "application/json", "{\"error\":\"Power must be 0-100%\"}");
        return;
    }
    
    bool sent = sendDCMotorCommand(index, DCMOTOR_CMD_START, power, forward);
    
    if (sent) {
        log(LOG_INFO, false, "Start DC motor %d: %.1f%%, %s\n", 
            index, power, forward ? "Forward" : "Reverse");
        server.send(200, "application/json", "{\"success\":true,\"message\":\"Command sent\"}");
    } else {
        log(LOG_WARNING, false, "Failed to start DC motor %d: IPC queue full\n", index);
        server.send(503, "application/json", "{\"error\":\"IPC queue full, try again\"}");
    }
}

void handleStopDCMotor(uint8_t index) {
    if (index < 27 || index >= 27 + MAX_DC_MOTORS) {
        server.send(400, "application/json", "{\"error\":\"Invalid motor index\"}");
        return;
    }
    
    bool sent = sendDCMotorCommand(index, DCMOTOR_CMD_STOP, 0, false);
    
    if (sent) {
        log(LOG_INFO, false, "Stop DC motor %d\n", index);
        server.send(200, "application/json", "{\"success\":true,\"message\":\"Command sent\"}");
    } else {
        log(LOG_WARNING, false, "Failed to stop DC motor %d: IPC queue full\n", index);
        server.send(503, "application/json", "{\"error\":\"IPC queue full, try again\"}");
    }
}
