/**
 * @file apiSystem.cpp
 * @brief System status and control API implementation
 */

#include "apiSystem.h"
#include "../network/networkManager.h"
#include "../utils/logger.h"
#include "../utils/statusManager.h"
#include "../utils/objectCache.h"
#include "../utils/timeManager.h"
#include "../config/ioConfig.h"
#include "../storage/sdManager.h"
#include <ArduinoJson.h>

// =============================================================================
// Setup Function
// =============================================================================

void setupSystemAPI(void) {
    // Comprehensive status endpoint for the UI
    server.on("/api/status/all", HTTP_GET, handleGetAllStatus);

    // Scalable control endpoint
    server.on("/api/controls", HTTP_POST, handleUpdateControl);

    // System status endpoint for the UI
    server.on("/api/system/status", HTTP_GET, handleSystemStatus);

    // Sensors endpoint for the control tab
    server.on("/api/sensors", HTTP_GET, handleGetSensors);

    // System reboot endpoint
    server.on("/api/system/reboot", HTTP_POST, []() {
        // Send response first before rebooting
        server.send(200, "application/json", "{\"success\":true,\"message\":\"System is rebooting...\"}");
        
        // Add a small delay to ensure response is sent
        delay(500);
        
        // Log the reboot
        log(LOG_INFO, true, "System reboot requested via web interface\n");

        delay(1000);
        
        // Perform system reboot
        rp2040.reboot();
    });

    // Recording Configuration API endpoints
    server.on("/api/config/recording", HTTP_GET, handleGetRecordingConfig);
    server.on("/api/config/recording", HTTP_POST, handleSaveRecordingConfig);
}

// =============================================================================
// Status Handlers
// =============================================================================

void handleGetAllStatus() {
    if (statusLocked) {
        server.send(503, "application/json", "{\"error\":\"Status temporarily unavailable\"}");
        return;
    }
    statusLocked = true;

    StaticJsonDocument<2048> doc;

    // System info
    doc["hostname"] = networkConfig.hostname;
    doc["mac"] = deviceMacAddress;

    // Internal Status
    JsonObject internal = doc.createNestedObject("internal");
    internal["psuOK"] = status.psuOK;
    internal["v20OK"] = status.V20OK;
    internal["v5OK"] = status.V5OK;
    internal["sdCardOK"] = status.sdCardOK;
    internal["ipcOK"] = status.ipcOK;
    internal["ipcConnected"] = status.ipcConnected;
    internal["ipcTimeout"] = status.ipcTimeout;
    internal["rtcOK"] = status.rtcOK;
    internal["mqttConnected"] = status.mqttConnected;

    // Sensor Readings
    JsonObject sensors = doc.createNestedObject("sensors");
    sensors["temperature"] = status.temperatureSensor.celcius;
    sensors["ph"] = status.phSensor.pH;
    sensors["do"] = status.doSensor.oxygen;

    // Control Setpoints
    JsonObject controls = doc.createNestedObject("controls");
    JsonObject tempControl = controls.createNestedObject("temperature");
    tempControl["setpoint"] = status.temperatureControl.sp_celcius;
    tempControl["enabled"] = status.temperatureControl.enabled;

    JsonObject phControl = controls.createNestedObject("ph");
    phControl["setpoint"] = status.phControl.sp_pH;
    phControl["enabled"] = status.phControl.enabled;

    statusLocked = false;

    String response;
    serializeJson(doc, response);
    server.send(200, "application/json", response);
}

void handleUpdateControl() {
    if (!server.hasArg("plain")) {
        server.send(400, "application/json", "{\"error\":\"No data received\"}");
        return;
    }

    StaticJsonDocument<256> doc;
    DeserializationError error = deserializeJson(doc, server.arg("plain"));
    if (error) {
        server.send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
        return;
    }

    const char* type = doc["type"];
    JsonObject config = doc["config"];

    if (!type || config.isNull()) {
        server.send(400, "application/json", "{\"error\":\"Invalid payload structure\"}");
        return;
    }

    bool success = false;
    if (strcmp(type, "temperature") == 0) {
        success = updateTemperatureControl(config);
    } else if (strcmp(type, "ph") == 0) {
        success = updatePhControl(config);
    }

    if (success) {
        server.send(200, "application/json", "{\"success\":true}");
    } else {
        server.send(500, "application/json", "{\"success\":false, \"error\":\"Failed to apply control update\"}");
    }
}

void handleSystemStatus() {
    // NOTE: This handler only READS from status/sdInfo structs, it doesn't write.
    // Therefore we don't need to acquire statusLocked - reads are safe without it.

    StaticJsonDocument<1024> doc;

    // Power info
    JsonObject power = doc.createNestedObject("power");
    power["mainVoltage"] = status.Vpsu;
    power["mainVoltageOK"] = status.psuOK;
    power["v20Voltage"] = status.V20;
    power["v20VoltageOK"] = status.V20OK;
    power["v5Voltage"] = status.V5;
    power["v5VoltageOK"] = status.V5OK;

    // RTC info
    JsonObject rtc = doc.createNestedObject("rtc");
    rtc["ok"] = status.rtcOK;
    rtc["time"] = getISO8601Timestamp(100);

    // Subsystem status
    doc["mqtt"] = status.mqttConnected;
    
    // IPC status with detailed state
    JsonObject ipc = doc.createNestedObject("ipc");
    ipc["ok"] = status.ipcOK;
    ipc["connected"] = status.ipcConnected;
    ipc["timeout"] = status.ipcTimeout;
    
    // Modbus status with detailed state
    JsonObject modbus = doc.createNestedObject("modbus");
    modbus["configured"] = status.modbusConfigured;
    modbus["connected"] = status.modbusConnected;
    modbus["fault"] = status.modbusFault;

    // SD card info
    JsonObject sd = doc.createNestedObject("sd");
    sd["inserted"] = sdInfo.inserted;
    sd["ready"] = sdInfo.ready;
    sd["capacityGB"] = sdInfo.cardSizeBytes * 0.000000001;
    sd["freeSpaceGB"] = sdInfo.cardFreeBytes * 0.000000001;
    sd["logFileSizeKB"] = sdInfo.logSizeBytes * 0.001;

    String response;
    serializeJson(doc, response);
    server.send(200, "application/json", response);
}

void handleGetSensors() {
    if (statusLocked) {
        server.send(503, "application/json", "{\"error\":\"Status temporarily unavailable\"}");
        return;
    }
    statusLocked = true;

    StaticJsonDocument<1024> doc;

    // Current sensor readings from status struct
    doc["temp"] = status.temperatureSensor.celcius;
    doc["ph"] = status.phSensor.pH;
    doc["do"] = status.doSensor.oxygen;
    doc["stirrer"] = status.stirrerSpeedSensor.rpm;
    doc["pressure"] = status.pressureSensor.kPa;
    doc["gasFlow"] = status.gasFlowSensor.mlPerMinute;
    doc["weight"] = status.weightSensor.grams;
    doc["opticalDensity"] = status.odSensor.OD;
    
    // Power sensor readings
    doc["powerVolts"] = status.powerSensor.voltage;
    doc["powerAmps"] = status.powerSensor.current;
    doc["powerWatts"] = status.powerSensor.power;
    
    // Online status for each sensor
    doc["tempOnline"] = status.temperatureSensor.online;
    doc["phOnline"] = status.phSensor.online;
    doc["doOnline"] = status.doSensor.online;
    doc["stirrerOnline"] = status.stirrerSpeedSensor.online;
    doc["pressureOnline"] = status.pressureSensor.online;
    doc["gasFlowOnline"] = status.gasFlowSensor.online;
    doc["weightOnline"] = status.weightSensor.online;
    doc["odOnline"] = status.odSensor.online;
    doc["powerOnline"] = status.powerSensor.online;

    statusLocked = false;

    String response;
    serializeJson(doc, response);
    server.send(200, "application/json", response);
}

// =============================================================================
// Recording Configuration Handlers
// =============================================================================

void handleGetRecordingConfig() {
    StaticJsonDocument<768> doc;
    
    doc["enabled"] = recordingConfig.enabled;
    
    JsonObject inputs = doc.createNestedObject("inputs");
    inputs["enabled"] = recordingConfig.inputs.enabled;
    inputs["interval"] = recordingConfig.inputs.interval;
    
    JsonObject outputs = doc.createNestedObject("outputs");
    outputs["enabled"] = recordingConfig.outputs.enabled;
    outputs["interval"] = recordingConfig.outputs.interval;
    
    JsonObject motors = doc.createNestedObject("motors");
    motors["enabled"] = recordingConfig.motors.enabled;
    motors["interval"] = recordingConfig.motors.interval;
    
    JsonObject sensors = doc.createNestedObject("sensors");
    sensors["enabled"] = recordingConfig.sensors.enabled;
    sensors["interval"] = recordingConfig.sensors.interval;
    
    JsonObject energy = doc.createNestedObject("energy");
    energy["enabled"] = recordingConfig.energy.enabled;
    energy["interval"] = recordingConfig.energy.interval;
    
    JsonObject controllers = doc.createNestedObject("controllers");
    controllers["enabled"] = recordingConfig.controllers.enabled;
    controllers["interval"] = recordingConfig.controllers.interval;
    
    JsonObject devices = doc.createNestedObject("devices");
    devices["enabled"] = recordingConfig.devices.enabled;
    devices["interval"] = recordingConfig.devices.interval;
    
    String response;
    serializeJson(doc, response);
    server.send(200, "application/json", response);
}

void handleSaveRecordingConfig() {
    if (!server.hasArg("plain")) {
        server.send(400, "application/json", "{\"error\":\"No data received\"}");
        return;
    }
    
    StaticJsonDocument<768> doc;
    DeserializationError error = deserializeJson(doc, server.arg("plain"));
    
    if (error) {
        server.send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
        return;
    }
    
    // Update recording configuration
    recordingConfig.enabled = doc["enabled"] | false;
    
    JsonObject inputs = doc["inputs"];
    recordingConfig.inputs.enabled = inputs["enabled"] | false;
    recordingConfig.inputs.interval = inputs["interval"] | 60;
    if (recordingConfig.inputs.interval < RECORDING_MIN_INTERVAL)
        recordingConfig.inputs.interval = RECORDING_MIN_INTERVAL;
    
    JsonObject outputs = doc["outputs"];
    recordingConfig.outputs.enabled = outputs["enabled"] | false;
    recordingConfig.outputs.interval = outputs["interval"] | 60;
    if (recordingConfig.outputs.interval < RECORDING_MIN_INTERVAL)
        recordingConfig.outputs.interval = RECORDING_MIN_INTERVAL;
    
    JsonObject motors = doc["motors"];
    recordingConfig.motors.enabled = motors["enabled"] | false;
    recordingConfig.motors.interval = motors["interval"] | 60;
    if (recordingConfig.motors.interval < RECORDING_MIN_INTERVAL)
        recordingConfig.motors.interval = RECORDING_MIN_INTERVAL;
    
    JsonObject sensors = doc["sensors"];
    recordingConfig.sensors.enabled = sensors["enabled"] | false;
    recordingConfig.sensors.interval = sensors["interval"] | 60;
    if (recordingConfig.sensors.interval < RECORDING_MIN_INTERVAL)
        recordingConfig.sensors.interval = RECORDING_MIN_INTERVAL;
    
    JsonObject energy = doc["energy"];
    recordingConfig.energy.enabled = energy["enabled"] | false;
    recordingConfig.energy.interval = energy["interval"] | 60;
    if (recordingConfig.energy.interval < RECORDING_MIN_INTERVAL)
        recordingConfig.energy.interval = RECORDING_MIN_INTERVAL;
    
    JsonObject controllers = doc["controllers"];
    recordingConfig.controllers.enabled = controllers["enabled"] | false;
    recordingConfig.controllers.interval = controllers["interval"] | 60;
    if (recordingConfig.controllers.interval < RECORDING_MIN_INTERVAL)
        recordingConfig.controllers.interval = RECORDING_MIN_INTERVAL;
    
    JsonObject devices = doc["devices"];
    recordingConfig.devices.enabled = devices["enabled"] | false;
    recordingConfig.devices.interval = devices["interval"] | 60;
    if (recordingConfig.devices.interval < RECORDING_MIN_INTERVAL)
        recordingConfig.devices.interval = RECORDING_MIN_INTERVAL;
    
    // Save to network config (which includes recording config)
    saveNetworkConfig();
    
    log(LOG_INFO, true, "Recording configuration saved: master=%s\n", 
        recordingConfig.enabled ? "enabled" : "disabled");
    
    server.send(200, "application/json", "{\"success\":true,\"message\":\"Recording configuration saved\"}");
}
