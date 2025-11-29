/**
 * @file apiInputs.cpp
 * @brief Input configuration API implementation
 */

#include "apiInputs.h"
#include "../network/networkManager.h"
#include "../config/ioConfig.h"
#include "../utils/logger.h"
#include "../utils/objectCache.h"
#include "../utils/ipcManager.h"
#include <ArduinoJson.h>

// =============================================================================
// Setup Function
// =============================================================================

void setupInputsAPI(void)
{
    // Get all inputs status
    server.on("/api/inputs", HTTP_GET, handleGetInputs);
    
    // ADC configuration endpoints (indices 0-7)
    for (uint8_t i = 0; i < MAX_ADC_INPUTS; i++) {
        String getPath = "/api/config/adc/" + String(i);
        String postPath = "/api/config/adc/" + String(i);
        
        server.on(getPath.c_str(), HTTP_GET, [i]() { handleGetADCConfig(i); });
        server.on(postPath.c_str(), HTTP_POST, [i]() { handleSaveADCConfig(i); });
    }
    
    // DAC configuration endpoints (indices 8-9)
    for (uint8_t i = 8; i < 10; i++) {
        String getPath = "/api/config/dac/" + String(i);
        String postPath = "/api/config/dac/" + String(i);
        
        server.on(getPath.c_str(), HTTP_GET, [i]() { handleGetDACConfig(i); });
        server.on(postPath.c_str(), HTTP_POST, [i]() { handleSaveDACConfig(i); });
    }
    
    // RTD configuration endpoints (indices 10-12)
    for (uint8_t i = 10; i < 10 + MAX_RTD_SENSORS; i++) {
        String getPath = "/api/config/rtd/" + String(i);
        String postPath = "/api/config/rtd/" + String(i);
        
        server.on(getPath.c_str(), HTTP_GET, [i]() { handleGetRTDConfig(i); });
        server.on(postPath.c_str(), HTTP_POST, [i]() { handleSaveRTDConfig(i); });
    }
    
    // GPIO configuration endpoints (indices 13-20)
    for (uint8_t i = 13; i < 13 + MAX_GPIO; i++) {
        String getPath = "/api/config/gpio/" + String(i);
        String postPath = "/api/config/gpio/" + String(i);
        
        server.on(getPath.c_str(), HTTP_GET, [i]() { handleGetGPIOConfig(i); });
        server.on(postPath.c_str(), HTTP_POST, [i]() { handleSaveGPIOConfig(i); });
    }
    
    // Energy sensor configuration endpoints (indices 31-32)
    for (uint8_t i = 31; i < 31 + MAX_ENERGY_SENSORS; i++) {
        String getPath = "/api/config/energy/" + String(i);
        String postPath = "/api/config/energy/" + String(i);
        
        server.on(getPath.c_str(), HTTP_GET, [i]() { handleGetEnergySensorConfig(i); });
        server.on(postPath.c_str(), HTTP_POST, [i]() { handleSaveEnergySensorConfig(i); });
    }
    
    // Device sensor configuration endpoints (indices 70-99)
    for (uint8_t i = 70; i < 70 + MAX_DEVICE_SENSORS; i++) {
        String getPath = "/api/config/devicesensor/" + String(i);
        String postPath = "/api/config/devicesensor/" + String(i);
        
        server.on(getPath.c_str(), HTTP_GET, [i]() { handleGetDeviceSensorConfig(i); });
        server.on(postPath.c_str(), HTTP_POST, [i]() { handleSaveDeviceSensorConfig(i); });
    }
    
    // COM port configuration endpoints (indices 0-3)
    server.on("/api/comports", HTTP_GET, handleGetComPorts);
    for (uint8_t i = 0; i < MAX_COM_PORTS; i++) {
        String getPath = "/api/config/comport/" + String(i);
        String postPath = "/api/config/comport/" + String(i);
        
        server.on(getPath.c_str(), HTTP_GET, [i]() { handleGetComPortConfig(i); });
        server.on(postPath.c_str(), HTTP_POST, [i]() { handleSaveComPortConfig(i); });
    }
}

// =============================================================================
// Input Status Handler
// =============================================================================

void handleGetInputs() {
    DynamicJsonDocument* doc = new DynamicJsonDocument(6144);
    if (!doc) {
        server.send(500, "application/json", "{\"error\":\"Memory allocation failed\"}");
        return;
    }
    
    // Analog Inputs (ADC) - Indices 0-7
    JsonArray adc = doc->createNestedArray("adc");
    for (uint8_t i = 0; i < 8; i++) {
        ObjectCache::CachedObject* obj = objectCache.getObject(i);
        if (obj && obj->valid) {
            JsonObject o = adc.createNestedObject();
            o["i"] = i;
            o["v"] = obj->value;
            o["n"] = ioConfig.adcInputs[i].name;
            char cleanUnit[8];
            strncpy(cleanUnit, obj->unit, sizeof(cleanUnit) - 1);
            cleanUnit[sizeof(cleanUnit) - 1] = '\0';
            o["u"] = cleanUnit;
            o["d"] = ioConfig.adcInputs[i].showOnDashboard;
            if (obj->flags & IPC_SENSOR_FLAG_FAULT) o["f"] = 1;
        }
    }
    
    // RTD Temperature Sensors - Indices 10-12
    JsonArray rtd = doc->createNestedArray("rtd");
    for (uint8_t i = 10; i < 13; i++) {
        ObjectCache::CachedObject* obj = objectCache.getObject(i);
        if (obj && obj->valid) {
            JsonObject o = rtd.createNestedObject();
            o["i"] = i;
            o["v"] = obj->value;
            o["n"] = ioConfig.rtdSensors[i - 10].name;
            char cleanUnit[8];
            strncpy(cleanUnit, obj->unit, sizeof(cleanUnit) - 1);
            cleanUnit[sizeof(cleanUnit) - 1] = '\0';
            o["u"] = cleanUnit;
            o["d"] = ioConfig.rtdSensors[i - 10].showOnDashboard;
            if (obj->flags & IPC_SENSOR_FLAG_FAULT) o["f"] = 1;
        }
    }
    
    // Digital GPIO - Indices 13-20
    JsonArray gpio = doc->createNestedArray("gpio");
    for (uint8_t i = 13; i < 21; i++) {
        ObjectCache::CachedObject* obj = objectCache.getObject(i);
        if (obj && obj->valid) {
            JsonObject o = gpio.createNestedObject();
            o["i"] = i;
            o["n"] = ioConfig.gpio[i - 13].name;
            o["s"] = (obj->value > 0.5) ? 1 : 0;
            o["d"] = ioConfig.gpio[i - 13].showOnDashboard;
            if (obj->flags & IPC_SENSOR_FLAG_FAULT) o["f"] = 1;
        }
    }
    
    // Energy Sensors - Indices 31-32
    JsonArray energy = doc->createNestedArray("energy");
    for (uint8_t i = 31; i < 33; i++) {
        ObjectCache::CachedObject* obj = objectCache.getObject(i);
        if (obj && obj->valid) {
            JsonObject o = energy.createNestedObject();
            o["i"] = i;
            o["n"] = ioConfig.energySensors[i - 31].name;
            o["v"] = obj->value;
            if (obj->valueCount >= 2) {
                o["c"] = obj->additionalValues[0];
                o["p"] = obj->additionalValues[1];
            } else {
                o["c"] = 0.0f;
                o["p"] = 0.0f;
            }
            o["d"] = ioConfig.energySensors[i - 31].showOnDashboard;
            if (obj->flags & IPC_SENSOR_FLAG_FAULT) o["f"] = 1;
        }
    }
    
    // Dynamic Device Sensors - Indices 70-99
    JsonArray devices = doc->createNestedArray("devices");
    for (uint8_t i = 70; i <= 99; i++) {
        ObjectCache::CachedObject* obj = objectCache.getObject(i);
        if (obj && obj->valid) {
            JsonObject o = devices.createNestedObject();
            o["i"] = i;
            o["v"] = obj->value;
            
            uint8_t sensorIndex = i - 70;
            char cleanName[40];
            if (ioConfig.deviceSensors[sensorIndex].nameOverridden && 
                strlen(ioConfig.deviceSensors[sensorIndex].name) > 0) {
                strncpy(cleanName, ioConfig.deviceSensors[sensorIndex].name, sizeof(cleanName) - 1);
            } else {
                strncpy(cleanName, obj->name, sizeof(cleanName) - 1);
            }
            cleanName[sizeof(cleanName) - 1] = '\0';
            o["n"] = cleanName;
            
            char cleanUnit[8];
            strncpy(cleanUnit, obj->unit, sizeof(cleanUnit) - 1);
            cleanUnit[sizeof(cleanUnit) - 1] = '\0';
            o["u"] = cleanUnit;
            
            o["t"] = obj->objectType;
            
            if (i >= 70 && i < 90) {
                uint8_t controlIdx = i - 20;
                o["c"] = controlIdx;
            }
            
            o["d"] = ioConfig.deviceSensors[sensorIndex].showOnDashboard;
            
            if (obj->flags & IPC_SENSOR_FLAG_FAULT) o["f"] = 1;
        }
    }
    
    String response;
    serializeJson(*doc, response);
    
    if (doc->overflowed()) {
        log(LOG_ERROR, true, "JSON document overflow in /api/inputs!\n");
    }
    
    delete doc;
    server.send(200, "application/json", response);
}

// =============================================================================
// ADC Configuration Handlers
// =============================================================================

void handleGetADCConfig(uint8_t index) {
    if (index >= MAX_ADC_INPUTS) {
        server.send(400, "application/json", "{\"error\":\"Invalid ADC index\"}");
        return;
    }
    
    StaticJsonDocument<256> doc;
    doc["index"] = index;
    doc["name"] = ioConfig.adcInputs[index].name;
    doc["unit"] = ioConfig.adcInputs[index].unit;
    doc["enabled"] = ioConfig.adcInputs[index].enabled;
    doc["showOnDashboard"] = ioConfig.adcInputs[index].showOnDashboard;
    
    JsonObject cal = doc.createNestedObject("cal");
    cal["scale"] = ioConfig.adcInputs[index].cal.scale;
    cal["offset"] = ioConfig.adcInputs[index].cal.offset;
    
    String response;
    serializeJson(doc, response);
    server.send(200, "application/json", response);
}

void handleSaveADCConfig(uint8_t index) {
    if (index >= MAX_ADC_INPUTS) {
        server.send(400, "application/json", "{\"error\":\"Invalid ADC index\"}");
        return;
    }
    
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
    
    if (doc.containsKey("name")) {
        strncpy(ioConfig.adcInputs[index].name, doc["name"], sizeof(ioConfig.adcInputs[index].name) - 1);
        ioConfig.adcInputs[index].name[sizeof(ioConfig.adcInputs[index].name) - 1] = '\0';
    }
    
    if (doc.containsKey("unit")) {
        strncpy(ioConfig.adcInputs[index].unit, doc["unit"], sizeof(ioConfig.adcInputs[index].unit) - 1);
        ioConfig.adcInputs[index].unit[sizeof(ioConfig.adcInputs[index].unit) - 1] = '\0';
    }
    
    if (doc.containsKey("cal")) {
        JsonObject cal = doc["cal"];
        if (cal.containsKey("scale")) {
            ioConfig.adcInputs[index].cal.scale = cal["scale"];
        }
        if (cal.containsKey("offset")) {
            ioConfig.adcInputs[index].cal.offset = cal["offset"];
        }
    }
    
    if (doc.containsKey("showOnDashboard")) {
        ioConfig.adcInputs[index].showOnDashboard = doc["showOnDashboard"];
    }
    
    saveIOConfig();
    
    IPC_ConfigAnalogInput_t cfg;
    cfg.transactionId = generateTransactionId();
    cfg.index = index;
    strncpy(cfg.unit, ioConfig.adcInputs[index].unit, sizeof(cfg.unit) - 1);
    cfg.unit[sizeof(cfg.unit) - 1] = '\0';
    cfg.calScale = ioConfig.adcInputs[index].cal.scale;
    cfg.calOffset = ioConfig.adcInputs[index].cal.offset;
    
    bool sent = ipc.sendPacket(IPC_MSG_CONFIG_ANALOG_INPUT, (uint8_t*)&cfg, sizeof(cfg));
    
    if (sent) {
        addPendingTransaction(cfg.transactionId, IPC_MSG_CONFIG_ANALOG_INPUT, IPC_MSG_CONTROL_ACK, 1, cfg.index);
        log(LOG_INFO, false, "Updated ADC[%d] config: %s, unit=%s, scale=%.4f, offset=%.4f\n",
            index, ioConfig.adcInputs[index].name, cfg.unit, cfg.calScale, cfg.calOffset);
        server.send(200, "application/json", "{\"success\":true}");
    } else {
        log(LOG_WARNING, false, "Failed to send ADC[%d] config to IO MCU\n", index);
        server.send(500, "application/json", "{\"success\":false,\"error\":\"Failed to update IO MCU\"}");
    }
}

// =============================================================================
// DAC Configuration Handlers
// =============================================================================

void handleGetDACConfig(uint8_t index) {
    if (index < 8 || index > 9) {
        server.send(400, "application/json", "{\"error\":\"Invalid DAC index\"}");
        return;
    }
    
    uint8_t dacIndex = index - 8;
    
    StaticJsonDocument<256> doc;
    doc["index"] = index;
    doc["name"] = ioConfig.dacOutputs[dacIndex].name;
    doc["unit"] = ioConfig.dacOutputs[dacIndex].unit;
    doc["enabled"] = ioConfig.dacOutputs[dacIndex].enabled;
    doc["showOnDashboard"] = ioConfig.dacOutputs[dacIndex].showOnDashboard;
    
    JsonObject cal = doc.createNestedObject("cal");
    cal["scale"] = ioConfig.dacOutputs[dacIndex].cal.scale;
    cal["offset"] = ioConfig.dacOutputs[dacIndex].cal.offset;
    
    String response;
    serializeJson(doc, response);
    server.send(200, "application/json", response);
}

void handleSaveDACConfig(uint8_t index) {
    if (index < 8 || index > 9) {
        server.send(400, "application/json", "{\"error\":\"Invalid DAC index\"}");
        return;
    }
    
    uint8_t dacIndex = index - 8;
    
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
    
    if (doc.containsKey("name")) {
        strncpy(ioConfig.dacOutputs[dacIndex].name, doc["name"], sizeof(ioConfig.dacOutputs[dacIndex].name) - 1);
        ioConfig.dacOutputs[dacIndex].name[sizeof(ioConfig.dacOutputs[dacIndex].name) - 1] = '\0';
    }
    
    if (doc.containsKey("cal")) {
        JsonObject cal = doc["cal"];
        if (cal.containsKey("scale")) {
            ioConfig.dacOutputs[dacIndex].cal.scale = cal["scale"];
        }
        if (cal.containsKey("offset")) {
            ioConfig.dacOutputs[dacIndex].cal.offset = cal["offset"];
        }
    }
    
    if (doc.containsKey("showOnDashboard")) {
        ioConfig.dacOutputs[dacIndex].showOnDashboard = doc["showOnDashboard"];
    }
    
    saveIOConfig();
    
    IPC_ConfigAnalogOutput_t cfg;
    cfg.transactionId = generateTransactionId();
    cfg.index = index;
    strncpy(cfg.unit, ioConfig.dacOutputs[dacIndex].unit, sizeof(cfg.unit) - 1);
    cfg.unit[sizeof(cfg.unit) - 1] = '\0';
    cfg.calScale = ioConfig.dacOutputs[dacIndex].cal.scale;
    cfg.calOffset = ioConfig.dacOutputs[dacIndex].cal.offset;
    
    bool sent = ipc.sendPacket(IPC_MSG_CONFIG_ANALOG_OUTPUT, (uint8_t*)&cfg, sizeof(cfg));
    
    if (sent) {
        addPendingTransaction(cfg.transactionId, IPC_MSG_CONFIG_ANALOG_OUTPUT, IPC_MSG_CONTROL_ACK, 1, cfg.index);
        log(LOG_INFO, false, "Updated DAC[%d] config: %s, unit=%s, scale=%.4f, offset=%.4f\n",
            index, ioConfig.dacOutputs[dacIndex].name, cfg.unit, cfg.calScale, cfg.calOffset);
        server.send(200, "application/json", "{\"success\":true}");
    } else {
        log(LOG_WARNING, false, "Failed to send DAC[%d] config to IO MCU\n", index);
        server.send(500, "application/json", "{\"success\":false,\"error\":\"Failed to update IO MCU\"}");
    }
}

// =============================================================================
// RTD Configuration Handlers
// =============================================================================

void handleGetRTDConfig(uint8_t index) {
    if (index < 10 || index >= 10 + MAX_RTD_SENSORS) {
        server.send(400, "application/json", "{\"error\":\"Invalid RTD index\"}");
        return;
    }
    
    uint8_t rtdIndex = index - 10;
    StaticJsonDocument<256> doc;
    doc["index"] = index;
    doc["name"] = ioConfig.rtdSensors[rtdIndex].name;
    doc["unit"] = ioConfig.rtdSensors[rtdIndex].unit;
    doc["wires"] = ioConfig.rtdSensors[rtdIndex].wireConfig;
    doc["type"] = ioConfig.rtdSensors[rtdIndex].nominalOhms;
    doc["showOnDashboard"] = ioConfig.rtdSensors[rtdIndex].showOnDashboard;
    
    JsonObject cal = doc.createNestedObject("cal");
    cal["scale"] = ioConfig.rtdSensors[rtdIndex].cal.scale;
    cal["offset"] = ioConfig.rtdSensors[rtdIndex].cal.offset;
    
    String response;
    serializeJson(doc, response);
    server.send(200, "application/json", response);
}

void handleSaveRTDConfig(uint8_t index) {
    if (index < 10 || index >= 10 + MAX_RTD_SENSORS) {
        server.send(400, "application/json", "{\"error\":\"Invalid RTD index\"}");
        return;
    }
    
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
    
    uint8_t rtdIndex = index - 10;
    
    if (doc.containsKey("name")) {
        strncpy(ioConfig.rtdSensors[rtdIndex].name, doc["name"], sizeof(ioConfig.rtdSensors[rtdIndex].name) - 1);
        ioConfig.rtdSensors[rtdIndex].name[sizeof(ioConfig.rtdSensors[rtdIndex].name) - 1] = '\0';
    }
    
    if (doc.containsKey("unit")) {
        strncpy(ioConfig.rtdSensors[rtdIndex].unit, doc["unit"], sizeof(ioConfig.rtdSensors[rtdIndex].unit) - 1);
        ioConfig.rtdSensors[rtdIndex].unit[sizeof(ioConfig.rtdSensors[rtdIndex].unit) - 1] = '\0';
    }
    
    if (doc.containsKey("wires")) {
        ioConfig.rtdSensors[rtdIndex].wireConfig = doc["wires"];
    }
    
    if (doc.containsKey("type")) {
        ioConfig.rtdSensors[rtdIndex].nominalOhms = doc["type"];
    }
    
    if (doc.containsKey("cal")) {
        JsonObject cal = doc["cal"];
        if (cal.containsKey("scale")) {
            ioConfig.rtdSensors[rtdIndex].cal.scale = cal["scale"];
        }
        if (cal.containsKey("offset")) {
            ioConfig.rtdSensors[rtdIndex].cal.offset = cal["offset"];
        }
    }
    
    if (doc.containsKey("showOnDashboard")) {
        ioConfig.rtdSensors[rtdIndex].showOnDashboard = doc["showOnDashboard"];
    }
    
    saveIOConfig();
    
    IPC_ConfigRTD_t cfg;
    cfg.transactionId = generateTransactionId();
    cfg.index = index;
    strncpy(cfg.unit, ioConfig.rtdSensors[rtdIndex].unit, sizeof(cfg.unit) - 1);
    cfg.unit[sizeof(cfg.unit) - 1] = '\0';
    cfg.calScale = ioConfig.rtdSensors[rtdIndex].cal.scale;
    cfg.calOffset = ioConfig.rtdSensors[rtdIndex].cal.offset;
    cfg.wireConfig = ioConfig.rtdSensors[rtdIndex].wireConfig;
    cfg.nominalOhms = ioConfig.rtdSensors[rtdIndex].nominalOhms;
    
    bool sent = ipc.sendPacket(IPC_MSG_CONFIG_RTD, (uint8_t*)&cfg, sizeof(cfg));
    
    if (sent) {
        addPendingTransaction(cfg.transactionId, IPC_MSG_CONFIG_RTD, IPC_MSG_CONTROL_ACK, 1, cfg.index);
        log(LOG_INFO, false, "Updated RTD[%d] config: %s, unit=%s, %d-wire PT%d, scale=%.4f, offset=%.4f\n",
            index, ioConfig.rtdSensors[rtdIndex].name, cfg.unit, cfg.wireConfig, cfg.nominalOhms, 
            cfg.calScale, cfg.calOffset);
        server.send(200, "application/json", "{\"success\":true}");
    } else {
        log(LOG_WARNING, false, "Failed to send RTD[%d] config to IO MCU\n", index);
        server.send(500, "application/json", "{\"success\":false,\"error\":\"Failed to update IO MCU\"}");
    }
}

// =============================================================================
// GPIO Configuration Handlers
// =============================================================================

void handleGetGPIOConfig(uint8_t index) {
    if (index < 13 || index >= 13 + MAX_GPIO) {
        server.send(400, "application/json", "{\"error\":\"Invalid GPIO index\"}");
        return;
    }
    
    uint8_t gpioIndex = index - 13;
    
    StaticJsonDocument<256> doc;
    doc["index"] = index;
    doc["name"] = ioConfig.gpio[gpioIndex].name;
    doc["pullMode"] = (uint8_t)ioConfig.gpio[gpioIndex].pullMode;
    doc["enabled"] = ioConfig.gpio[gpioIndex].enabled;
    doc["showOnDashboard"] = ioConfig.gpio[gpioIndex].showOnDashboard;
    
    String response;
    serializeJson(doc, response);
    server.send(200, "application/json", response);
}

void handleSaveGPIOConfig(uint8_t index) {
    if (index < 13 || index >= 13 + MAX_GPIO) {
        server.send(400, "application/json", "{\"error\":\"Invalid GPIO index\"}");
        return;
    }
    
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
    
    uint8_t gpioIndex = index - 13;
    
    if (doc.containsKey("name")) {
        strlcpy(ioConfig.gpio[gpioIndex].name, doc["name"] | "", 
                sizeof(ioConfig.gpio[gpioIndex].name));
    }
    
    if (doc.containsKey("pullMode")) {
        ioConfig.gpio[gpioIndex].pullMode = (GPIOPullMode)(doc["pullMode"] | GPIO_PULL_UP);
    }
    
    if (doc.containsKey("enabled")) {
        ioConfig.gpio[gpioIndex].enabled = doc["enabled"] | true;
    }
    
    if (doc.containsKey("showOnDashboard")) {
        ioConfig.gpio[gpioIndex].showOnDashboard = doc["showOnDashboard"];
    }
    
    saveIOConfig();
    
    IPC_ConfigGPIO_t cfg;
    cfg.transactionId = generateTransactionId();
    cfg.index = index;
    strlcpy(cfg.name, ioConfig.gpio[gpioIndex].name, sizeof(cfg.name));
    cfg.pullMode = (uint8_t)ioConfig.gpio[gpioIndex].pullMode;
    cfg.enabled = ioConfig.gpio[gpioIndex].enabled;
    
    bool sent = ipc.sendPacket(IPC_MSG_CONFIG_GPIO, (uint8_t*)&cfg, sizeof(cfg));
    
    if (sent) {
        addPendingTransaction(cfg.transactionId, IPC_MSG_CONFIG_GPIO, IPC_MSG_CONTROL_ACK, 1, cfg.index);
        log(LOG_INFO, false, "Updated GPIO[%d] config: %s, pullMode=%d, enabled=%d\n",
            index, cfg.name, cfg.pullMode, cfg.enabled);
        server.send(200, "application/json", "{\"success\":true}");
    } else {
        log(LOG_WARNING, false, "Failed to send GPIO[%d] config to IO MCU\n", index);
        server.send(500, "application/json", "{\"success\":false,\"error\":\"Failed to update IO MCU\"}");
    }
}

// =============================================================================
// Energy Sensor Configuration Handlers
// =============================================================================

void handleGetEnergySensorConfig(uint8_t index) {
    if (index < 31 || index >= 31 + MAX_ENERGY_SENSORS) {
        server.send(400, "application/json", "{\"error\":\"Invalid energy sensor index\"}");
        return;
    }
    
    uint8_t sensorIndex = index - 31;
    StaticJsonDocument<256> doc;
    doc["index"] = index;
    doc["name"] = ioConfig.energySensors[sensorIndex].name;
    doc["showOnDashboard"] = ioConfig.energySensors[sensorIndex].showOnDashboard;
    
    String response;
    serializeJson(doc, response);
    server.send(200, "application/json", response);
}

void handleSaveEnergySensorConfig(uint8_t index) {
    if (index < 31 || index >= 31 + MAX_ENERGY_SENSORS) {
        server.send(400, "application/json", "{\"error\":\"Invalid energy sensor index\"}");
        return;
    }
    
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
    
    uint8_t sensorIndex = index - 31;
    
    if (doc.containsKey("name")) {
        strncpy(ioConfig.energySensors[sensorIndex].name, doc["name"], 
                sizeof(ioConfig.energySensors[sensorIndex].name) - 1);
        ioConfig.energySensors[sensorIndex].name[sizeof(ioConfig.energySensors[sensorIndex].name) - 1] = '\0';
    }
    
    if (doc.containsKey("showOnDashboard")) {
        ioConfig.energySensors[sensorIndex].showOnDashboard = doc["showOnDashboard"];
    }
    
    saveIOConfig();
    
    log(LOG_INFO, false, "Updated Energy Sensor[%d] config: %s, dashboard=%d\n",
        index, ioConfig.energySensors[sensorIndex].name, 
        ioConfig.energySensors[sensorIndex].showOnDashboard);
    
    server.send(200, "application/json", "{\"success\":true}");
}

// =============================================================================
// Device Sensor Configuration Handlers
// =============================================================================

void handleGetDeviceSensorConfig(uint8_t index) {
    if (index < 70 || index >= 70 + MAX_DEVICE_SENSORS) {
        server.send(400, "application/json", "{\"error\":\"Invalid device sensor index\"}");
        return;
    }
    
    uint8_t sensorIndex = index - 70;
    
    StaticJsonDocument<256> doc;
    doc["index"] = index;
    doc["name"] = ioConfig.deviceSensors[sensorIndex].name;
    doc["showOnDashboard"] = ioConfig.deviceSensors[sensorIndex].showOnDashboard;
    doc["nameOverridden"] = ioConfig.deviceSensors[sensorIndex].nameOverridden;
    
    String response;
    serializeJson(doc, response);
    server.send(200, "application/json", response);
}

void handleSaveDeviceSensorConfig(uint8_t index) {
    if (index < 70 || index >= 70 + MAX_DEVICE_SENSORS) {
        server.send(400, "application/json", "{\"error\":\"Invalid device sensor index\"}");
        return;
    }
    
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
    
    uint8_t sensorIndex = index - 70;
    
    if (doc.containsKey("name")) {
        strncpy(ioConfig.deviceSensors[sensorIndex].name, doc["name"], 
                sizeof(ioConfig.deviceSensors[sensorIndex].name) - 1);
        ioConfig.deviceSensors[sensorIndex].name[sizeof(ioConfig.deviceSensors[sensorIndex].name) - 1] = '\0';
        ioConfig.deviceSensors[sensorIndex].nameOverridden = (strlen(ioConfig.deviceSensors[sensorIndex].name) > 0);
    }
    
    if (doc.containsKey("showOnDashboard")) {
        ioConfig.deviceSensors[sensorIndex].showOnDashboard = doc["showOnDashboard"];
    }
    
    saveIOConfig();
    
    log(LOG_INFO, false, "Updated device sensor[%d] config: name='%s', showOnDashboard=%d\n", 
        index, ioConfig.deviceSensors[sensorIndex].name,
        ioConfig.deviceSensors[sensorIndex].showOnDashboard);
    
    server.send(200, "application/json", "{\"success\":true}");
}

// =============================================================================
// COM Port Configuration Handlers
// =============================================================================

void handleGetComPortConfig(uint8_t index) {
    if (index >= MAX_COM_PORTS) {
        server.send(400, "application/json", "{\"error\":\"Invalid COM port index\"}");
        return;
    }
    
    StaticJsonDocument<256> doc;
    doc["index"] = index;
    doc["name"] = ioConfig.comPorts[index].name;
    doc["baudRate"] = ioConfig.comPorts[index].baudRate;
    doc["dataBits"] = ioConfig.comPorts[index].dataBits;
    doc["stopBits"] = ioConfig.comPorts[index].stopBits;
    doc["parity"] = ioConfig.comPorts[index].parity;
    doc["showOnDashboard"] = ioConfig.comPorts[index].showOnDashboard;
    
    String response;
    serializeJson(doc, response);
    server.send(200, "application/json", response);
}

void handleSaveComPortConfig(uint8_t index) {
    if (index >= MAX_COM_PORTS) {
        server.send(400, "application/json", "{\"error\":\"Invalid COM port index\"}");
        return;
    }
    
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
    
    if (doc.containsKey("name")) {
        strncpy(ioConfig.comPorts[index].name, doc["name"], sizeof(ioConfig.comPorts[index].name) - 1);
        ioConfig.comPorts[index].name[sizeof(ioConfig.comPorts[index].name) - 1] = '\0';
    }
    
    if (doc.containsKey("baudRate")) {
        ioConfig.comPorts[index].baudRate = doc["baudRate"];
    }
    
    if (doc.containsKey("dataBits")) {
        ioConfig.comPorts[index].dataBits = doc["dataBits"];
    }
    
    if (doc.containsKey("stopBits")) {
        ioConfig.comPorts[index].stopBits = doc["stopBits"];
    }
    
    if (doc.containsKey("parity")) {
        ioConfig.comPorts[index].parity = doc["parity"];
    }
    
    if (doc.containsKey("showOnDashboard")) {
        ioConfig.comPorts[index].showOnDashboard = doc["showOnDashboard"];
    }
    
    saveIOConfig();
    
    IPC_ConfigComPort_t cfg;
    cfg.transactionId = generateTransactionId();
    cfg.index = index;
    cfg.baudRate = ioConfig.comPorts[index].baudRate;
    cfg.dataBits = ioConfig.comPorts[index].dataBits;
    cfg.stopBits = ioConfig.comPorts[index].stopBits;
    cfg.parity = ioConfig.comPorts[index].parity;
    
    bool sent = ipc.sendPacket(IPC_MSG_CONFIG_COMPORT, (uint8_t*)&cfg, sizeof(cfg));
    
    if (sent) {
        addPendingTransaction(cfg.transactionId, IPC_MSG_CONFIG_COMPORT, IPC_MSG_CONTROL_ACK, 1, cfg.index);
        log(LOG_INFO, false, "Updated COM port %d config: baud=%d, parity=%d, stop=%.1f\n",
            index, cfg.baudRate, cfg.parity, cfg.stopBits);
        server.send(200, "application/json", "{\"success\":true}");
    } else {
        log(LOG_WARNING, false, "Failed to send COM port %d config to IO MCU\n", index);
        server.send(500, "application/json", "{\"success\":false,\"error\":\"Failed to update IO MCU\"}");
    }
}

void handleGetComPorts() {
    StaticJsonDocument<1024> doc;
    
    JsonArray ports = doc.createNestedArray("ports");
    for (int i = 0; i < MAX_COM_PORTS; i++) {
        JsonObject port = ports.createNestedObject();
        port["index"] = i;
        port["name"] = ioConfig.comPorts[i].name;
        port["baud"] = ioConfig.comPorts[i].baudRate;
        port["dataBits"] = ioConfig.comPorts[i].dataBits;
        port["parity"] = ioConfig.comPorts[i].parity;
        port["stopBits"] = ioConfig.comPorts[i].stopBits;
        port["d"] = ioConfig.comPorts[i].showOnDashboard;
        port["error"] = false;  // TODO: Get actual status from object cache
    }
    
    String response;
    serializeJson(doc, response);
    server.send(200, "application/json", response);
}
