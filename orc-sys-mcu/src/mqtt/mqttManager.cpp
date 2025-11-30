/**
 * MQTT Manager
 *
 * Responsibilities:
 *  - Maintain a resilient MQTT connection (with LWT and backoff)
 *  - Publish local and IPC sensor data using stable topic schema
 *  - Expose a compact API for event-driven publishing (from IPC)
 *  - Remain responsive on Core 0 alongside the Web server
 */

#include "mqttManager.h"

// Global MQTT client - using WiFiClient for lwIP w5500
// Use Wiznet lwIP TCP client under the hood via lwIPClient compatible type
WiFiClient mqttNetClient; // Provided by W5500lwIP integration
PubSubClient mqttClient(mqttNetClient);

static unsigned long lastMqttReconnectAttempt = 0;
static unsigned long lastMqttPublishTime = 0;
static bool lwtConfigured = false;
static char deviceTopicPrefix[64] = {0}; // e.g., "orcs/dev/AA:BB:CC:DD:EE:FF"
static char controlTopicPrefix[64] = {0}; // e.g., "orc.AA:BB:CC:DD:EE:FF.control"
static bool clientConfigured = false; // Track if client parameters have been set
static bool controlSubscribed = false; // Track if we've subscribed to control topics

// Forward declaration
static void reconnect();
static void mqttPublishAllSensorData();
static void mqttPublishIPCSensors();
static void ensureTopicPrefix();
static void mqttCallback(char* topic, byte* payload, unsigned int length);
static void subscribeToControlTopics();
static void handleControlMessage(const char* topic, const char* payload);
static void publishControlAck(const char* controlTopic, bool success, const char* message);

// Control message handlers (reuse existing IPC command functions from ipcManager)
static void handleOutputControl(uint8_t index, JsonDocument& doc);
static void handleDeviceControl(uint8_t index, JsonDocument& doc);
static void handleStepperControl(JsonDocument& doc);
static void handleDCMotorControl(uint8_t index, JsonDocument& doc);
static void handleTempControllerControl(uint8_t index, JsonDocument& doc);
static void handlepHControllerControl(JsonDocument& doc);
static void handleFlowControllerControl(uint8_t index, JsonDocument& doc);
static void handleDOControllerControl(JsonDocument& doc);

// MQTT callback function - handles incoming control messages
static void mqttCallback(char* topic, byte* payload, unsigned int length) {
    // Ignore our own ack messages to prevent infinite loop
    if (strstr(topic, "/ack") != nullptr) {
        return;
    }
    
    // Convert payload to null-terminated string
    char payloadStr[512];
    size_t copyLen = (length < sizeof(payloadStr) - 1) ? length : sizeof(payloadStr) - 1;
    memcpy(payloadStr, payload, copyLen);
    payloadStr[copyLen] = '\0';
    
    log(LOG_INFO, false, "MQTT RX [%s]: %s\n", topic, payloadStr);
    
    // Check if this is a control topic (orc/{MAC}/control/...)
    if (strstr(topic, "/control/") != nullptr) {
        handleControlMessage(topic, payloadStr);
    }
}

// Apply current config and attempt reconnect (call after API changes)
void mqttApplyConfigAndReconnect() {
    // Update server details
    if (strlen(networkConfig.mqttBroker) > 0) {
        mqttClient.setServer(networkConfig.mqttBroker, networkConfig.mqttPort);
    }
    // Reset topic prefix cache so changes to mqttDevicePrefix take effect
    deviceTopicPrefix[0] = '\0';
    // If currently connected, disconnect to force a clean reconnect with new LWT
    if (mqttClient.connected()) {
        mqttClient.disconnect();
        if (!statusLocked) { statusLocked = true; status.mqttConnected = false; status.updated = true; statusLocked = false; }
    }
    // Trigger immediate reconnect on next manageMqtt() tick
    lastMqttReconnectAttempt = 0;
}

// --- Diagnostics ---
bool mqttIsConnected() { return mqttClient.connected(); }
int mqttGetState() { return mqttClient.state(); }
const char* mqttGetDeviceTopicPrefix() { ensureTopicPrefix(); return deviceTopicPrefix; }

// Reconnect to MQTT broker
static void reconnect() {
    if (strlen(networkConfig.mqttBroker) == 0) {
        return;
    }
    
    log(LOG_INFO, true, "Attempting MQTT connection to %s:%d...", networkConfig.mqttBroker, networkConfig.mqttPort);
    
    char clientId[24];
    uint8_t mac[6];
    eth.macAddress(mac);
    snprintf(clientId, sizeof(clientId), "ORCS-%02X%02X%02X", mac[3], mac[4], mac[5]);
    
    log(LOG_INFO, false, "MQTT Client ID: %s\n", clientId);
    
    // Try simple connection first without LWT to debug
    bool connected = false;
    if (strlen(networkConfig.mqttUsername) > 0) {
        log(LOG_INFO, false, "MQTT connecting with username: %s\n", networkConfig.mqttUsername);
        connected = mqttClient.connect(clientId, networkConfig.mqttUsername, networkConfig.mqttPassword);
    } else {
        log(LOG_INFO, false, "MQTT connecting without credentials\n");
        connected = mqttClient.connect(clientId);
    }
    
    if (connected) {
        log(LOG_INFO, true, "MQTT connected successfully! Client state: %d\n", mqttClient.state());
        // Don't publish anything immediately after connecting - let the connection stabilize
        // The LWT will handle offline status automatically
        
        // Subscribe to control topics
        subscribeToControlTopics();
    } else {
        int state = mqttClient.state();
        const char* stateStr = "Unknown";
        switch(state) {
            case -4: stateStr = "MQTT_CONNECTION_TIMEOUT"; break;
            case -3: stateStr = "MQTT_CONNECTION_LOST"; break;
            case -2: stateStr = "MQTT_CONNECT_FAILED (TCP)"; break;
            case -1: stateStr = "MQTT_DISCONNECTED"; break;
            case 1: stateStr = "MQTT_CONNECT_BAD_PROTOCOL"; break;
            case 2: stateStr = "MQTT_CONNECT_BAD_CLIENT_ID"; break;
            case 3: stateStr = "MQTT_CONNECT_UNAVAILABLE"; break;
            case 4: stateStr = "MQTT_CONNECT_BAD_CREDENTIALS"; break;
            case 5: stateStr = "MQTT_CONNECT_UNAUTHORIZED"; break;
        }
        log(LOG_WARNING, true, "MQTT connection failed, rc=%d (%s). Will retry in %d seconds.\n", 
            state, stateStr, MQTT_RECONNECT_INTERVAL / 1000);
    }
}

// --- Topic Registry ---
typedef float (*SensorValueGetter)();
struct MqttTopicEntry {
    const char* topic;        // Topic suffix (relative). Full path will be deviceTopicPrefix + "/" + topic
    SensorValueGetter getter; // Getter reads from StatusVariables
    const char* description;  // Human description
};


// Forward declarations for getter functions
float getVpsu();
float getV20();
float getV5();
float getPsuOK();
float get20vOK();
float get5vOK();
float getSdCardOK();
float getIpcOK();
float getIpcConnected();
float getIpcTimeout();
float getRtcOK();
float getModbusConfigured();
float getModbusConnected();
float getModbusFault();
float getWebserverUp();
float getWebserverBusy();
float getMqttConnected();
float getMqttBusy();

MqttTopicEntry mqttTopics[] = {
    {"sensors/power/voltage", getVpsu, "Main PSU voltage (V)"},
    {"sensors/power/20v", getV20, "20V rail voltage (V)"},
    {"sensors/power/5v", getV5, "5V rail voltage (V)"},
    {"status/psu_ok", getPsuOK, "PSU OK status (1=OK, 0=Fault)"},
    {"status/20v_ok", get20vOK, "20V rail OK status (1=OK, 0=Fault)"},
    {"status/5v_ok", get5vOK, "5V rail OK status (1=OK, 0=Fault)"},
    {"status/sdcard_ok", getSdCardOK, "SD card OK status (1=OK, 0=Fault)"},
    {"status/ipc_ok", getIpcOK, "IPC OK status (1=OK, 0=Fault)"},
    {"status/ipc_connected", getIpcConnected, "IPC connected (1=Connected, 0=Disconnected)"},
    {"status/ipc_timeout", getIpcTimeout, "IPC timeout (1=Timeout, 0=OK)"},
    {"status/rtc_ok", getRtcOK, "RTC OK status (1=OK, 0=Fault)"},
    {"status/modbus_configured", getModbusConfigured, "Modbus configured (1=Devices configured, 0=None)"},
    {"status/modbus_connected", getModbusConnected, "Modbus connected (1=All connected, 0=Not)"},
    {"status/modbus_fault", getModbusFault, "Modbus fault (1=Fault, 0=OK)"},
    {"status/webserver_up", getWebserverUp, "Webserver up (1=Up, 0=Down)"},
    {"status/webserver_busy", getWebserverBusy, "Webserver busy (1=Busy, 0=Idle)"},
    {"status/mqtt_connected", getMqttConnected, "MQTT connected (1=Connected, 0=Not)"},
    {"status/mqtt_busy", getMqttBusy, "MQTT busy (1=Busy, 0=Idle)"},
};
const size_t mqttTopicCount = sizeof(mqttTopics) / sizeof(mqttTopics[0]);


// Getter implementations for all MQTT topics
float getVpsu() { return status.Vpsu; }
float getV20() { return status.V20; }
float getV5() { return status.V5; }
float getPsuOK() { return status.psuOK ? 1.0f : 0.0f; }
float get20vOK() { return status.V20OK ? 1.0f : 0.0f; }
float get5vOK() { return status.V5OK ? 1.0f : 0.0f; }
float getSdCardOK() { return status.sdCardOK ? 1.0f : 0.0f; }
float getIpcOK() { return status.ipcOK ? 1.0f : 0.0f; }
float getIpcConnected() { return status.ipcConnected ? 1.0f : 0.0f; }
float getIpcTimeout() { return status.ipcTimeout ? 1.0f : 0.0f; }
float getRtcOK() { return status.rtcOK ? 1.0f : 0.0f; }
float getModbusConfigured() { return status.modbusConfigured ? 1.0f : 0.0f; }
float getModbusConnected() { return status.modbusConnected ? 1.0f : 0.0f; }
float getModbusFault() { return status.modbusFault ? 1.0f : 0.0f; }
float getWebserverUp() { return status.webserverUp ? 1.0f : 0.0f; }
float getWebserverBusy() { return status.webserverBusy ? 1.0f : 0.0f; }
float getMqttConnected() { return status.mqttConnected ? 1.0f : 0.0f; }
float getMqttBusy() { return status.mqttBusy ? 1.0f : 0.0f; }


void init_mqttManager() {
    // Configure client parameters ONCE during initialization
    mqttClient.setBufferSize(512);  // Reduced from 1024 - we're not publishing large payloads now
    mqttClient.setKeepAlive(30);    // 30 seconds - balance between responsiveness and overhead
    mqttClient.setSocketTimeout(10); // 10 seconds - enough time for DNS resolution
    mqttClient.setCallback(mqttCallback); // Set callback (required by PubSubClient)
    clientConfigured = true;
    
    if (strlen(networkConfig.mqttBroker) > 0) {
        mqttClient.setServer(networkConfig.mqttBroker, networkConfig.mqttPort);
        log(LOG_INFO, false, "MQTT Manager initialized for broker %s:%d\n", networkConfig.mqttBroker, networkConfig.mqttPort);
        log(LOG_INFO, false, "MQTT config: keepAlive=30s, bufferSize=512, socketTimeout=10s\n");
    } else {
        log(LOG_INFO, false, "MQTT broker not configured. MQTT Manager will remain idle.\n");
    }
}

void manageMqtt() {
    // Skip MQTT management if not enabled or no broker configured
    if (!ethernetConnected || !networkConfig.mqttEnabled || strlen(networkConfig.mqttBroker) == 0) {
        if (status.mqttConnected) {
            if (!statusLocked) {
                statusLocked = true;
                status.mqttConnected = false;
                status.updated = true;
                statusLocked = false;
            }
        }
        return;
    }

    // Always call loop() to maintain connection, even when checking connection status
    mqttClient.loop();

    if (!mqttClient.connected()) {
        if (status.mqttConnected) { // Update status if we just disconnected
            log(LOG_WARNING, true, "MQTT disconnected unexpectedly (state=%d)\n", mqttClient.state());
            if (!statusLocked) {
                statusLocked = true;
                status.mqttConnected = false;
                status.updated = true;
                statusLocked = false;
            }
        }
        // Check if it's time to try reconnecting
        if (millis() - lastMqttReconnectAttempt > MQTT_RECONNECT_INTERVAL) {
            lastMqttReconnectAttempt = millis();
            reconnect();
        }
    } else {
        if (!status.mqttConnected) { // Update status if we just connected
            if (!statusLocked) {
                statusLocked = true;
                status.mqttConnected = true;
                status.updated = true;
                statusLocked = false;
            }
        }

        // Publish sensor data periodically
        uint32_t publishInterval = (networkConfig.mqttPublishIntervalMs > 0) ? networkConfig.mqttPublishIntervalMs : MQTT_PUBLISH_INTERVAL;
        if (millis() - lastMqttPublishTime > publishInterval) {
            lastMqttPublishTime = millis();
            mqttPublishAllSensorData();  // System status sensors
            mqttPublishIPCSensors();     // IPC sensors from object cache
        }
    }
}

/**
 * @brief Publishes all registered sensor data to their respective MQTT topics.
 *
 * For each sensor:
 *   - Publishes the value as a float to its individual topic (e.g., "orcs/system/power/voltage": 24.15)
 *   - Adds an entry to the consolidated JSON payload under "orcs/system/sensors" with both value and timestamp (ISO8601 UTC):
 *     {
 *       "sensors": {
 *         "orcs/system/power/voltage": { "value": 24.15, "timestamp": "2025-07-18T14:23:45Z" },
 *         ...
 *       }
 *     }
 *
 * @note The timestamp is an ISO8601 UTC string (e.g., "2025-07-18T14:23:45Z"), generated from the system RTC.
 */
static void ensureTopicPrefix() {
    if (deviceTopicPrefix[0] != '\0') return;
    
    uint8_t mac[6];
    eth.macAddress(mac);
    char macStr[18];
    snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    
    // Data topic prefix: orc/{MAC}/data or custom prefix
    if (strlen(networkConfig.mqttDevicePrefix) > 0) {
        strlcpy(deviceTopicPrefix, networkConfig.mqttDevicePrefix, sizeof(deviceTopicPrefix));
    } else {
        snprintf(deviceTopicPrefix, sizeof(deviceTopicPrefix), "orc/%s/data", macStr);
    }
    
    // Control topic prefix: orc/{MAC}/control (always uses MAC for consistency)
    snprintf(controlTopicPrefix, sizeof(controlTopicPrefix), "orc/%s/control", macStr);
    
    log(LOG_INFO, false, "MQTT topics: data=%s, control=%s\n", deviceTopicPrefix, controlTopicPrefix);
}

static void mqttPublishAllSensorData() {
    if (!mqttClient.connected()) {
        return;
    }

    // Get current timestamp
    String isoTimestamp = getISO8601Timestamp(100);
    if (isoTimestamp.length() == 0) {
        isoTimestamp = "1970-01-01T00:00:00Z"; // fallback
    }

    // Create JSON payload for all sensor data, each with its own ISO8601 timestamp
    DynamicJsonDocument doc(2048);
    ensureTopicPrefix();

    for (size_t i = 0; i < mqttTopicCount; i++) {
        float value = mqttTopics[i].getter();
        
        // Add value and timestamp for consolidated payload
        char fullKey[160];
        snprintf(fullKey, sizeof(fullKey), "%s/%s", deviceTopicPrefix, mqttTopics[i].topic);
        JsonObject sensorObj = doc["sensors"].createNestedObject(fullKey);
        sensorObj["value"] = value;
        sensorObj["timestamp"] = isoTimestamp;

        // Publish individual topics with JSON payload (matching documented format)
        StaticJsonDocument<128> individualDoc;
        individualDoc["value"] = value;
        individualDoc["online"] = true; // System status sensors are always online
        individualDoc["timestamp"] = isoTimestamp;
        
        char payload[128];
        serializeJson(individualDoc, payload, sizeof(payload));
        char topicBuf[160];
        snprintf(topicBuf, sizeof(topicBuf), "%s/%s", deviceTopicPrefix, mqttTopics[i].topic);
        mqttClient.publish(topicBuf, payload);
    }

    // Publish consolidated sensor data
    String jsonString;
    serializeJson(doc, jsonString);
    char consolidatedTopic[96];
    snprintf(consolidatedTopic, sizeof(consolidatedTopic), "%s/%s", deviceTopicPrefix, "sensors/all");
    mqttClient.publish(consolidatedTopic, jsonString.c_str());

    //log(LOG_INFO, false, "Published MQTT sensor data with ISO8601 timestamps\n");
}

/**
 * @brief Publishes IPC sensor data from object cache
 * 
 * Iterates through the object cache and publishes valid sensor readings
 * to their respective MQTT topics. Rate-limited to prevent flooding.
 */
static void mqttPublishIPCSensors() {
    // Debug
    uint32_t ts = millis();
    if (!mqttClient.connected()) {
        return;
    }

    ensureTopicPrefix();
    String timestamp = getISO8601Timestamp();
    if (timestamp.length() == 0) {
        timestamp = "1970-01-01T00:00:00Z";
    }

    uint16_t publishCount = 0;
    const uint16_t MAX_PUBLISHES_PER_CYCLE = 90; // Limit to prevent flooding (controllers + sensors + devices)

    // Iterate through all cached objects (0-89)
    for (uint8_t i = 0; i < MAX_CACHED_OBJECTS && publishCount < MAX_PUBLISHES_PER_CYCLE; i++) {
        ObjectCache::CachedObject* obj = objectCache.getObject(i);
        
        // Skip invalid or stale objects
        if (!obj || !obj->valid || objectCache.isStale(i)) {
            continue;
        }

        // Map object type to topic path
        const char* topicPath = nullptr;
        switch (obj->objectType) {
            // Input sensors
            case OBJ_T_TEMPERATURE_SENSOR:
                topicPath = "sensors/temperature";
                break;
            case OBJ_T_PH_SENSOR:
                topicPath = "sensors/ph";
                break;
            case OBJ_T_DISSOLVED_OXYGEN_SENSOR:
                topicPath = "sensors/do";
                break;
            case OBJ_T_OPTICAL_DENSITY_SENSOR:
                topicPath = "sensors/od";
                break;
            case OBJ_T_FLOW_SENSOR:
                topicPath = "sensors/flow";
                break;
            case OBJ_T_PRESSURE_SENSOR:
                topicPath = "sensors/pressure";
                break;
            case OBJ_T_POWER_SENSOR:
                topicPath = "sensors/power";
                break;
            case OBJ_T_ENERGY_SENSOR:
                topicPath = "sensors/energy";
                break;
            case OBJ_T_ANALOG_INPUT:
                topicPath = "sensors/analog";
                break;
            case OBJ_T_DIGITAL_INPUT:
                topicPath = "sensors/digital";
                break;
            
            // Outputs
            case OBJ_T_DIGITAL_OUTPUT:
                topicPath = "actuators/digital";
                break;
            case OBJ_T_ANALOG_OUTPUT:
                topicPath = "actuators/analog";
                break;
            
            // Motors
            case OBJ_T_STEPPER_MOTOR:
                topicPath = "actuators/stepper";
                break;
            case OBJ_T_BDC_MOTOR:
                topicPath = "actuators/dcmotor";
                break;
            
            // Controllers (indices 40-49)
            case OBJ_T_TEMPERATURE_CONTROL:
                topicPath = "controllers/temperature";
                break;
            case OBJ_T_PH_CONTROL:
                topicPath = "controllers/ph";
                break;
            case OBJ_T_FLOW_CONTROL:
                topicPath = "controllers/flow";
                break;
            case OBJ_T_DISSOLVED_OXYGEN_CONTROL:
                topicPath = "controllers/do";
                break;
            case OBJ_T_OPTICAL_DENSITY_CONTROL:
                topicPath = "controllers/od";
                break;
            case OBJ_T_GAS_FLOW_CONTROL:
                topicPath = "controllers/gasflow";
                break;
            case OBJ_T_STIRRER_CONTROL:
                topicPath = "controllers/stirrer";
                break;
            case OBJ_T_PUMP_CONTROL:
                topicPath = "controllers/pump";
                break;
            case OBJ_T_DEVICE_CONTROL:
                topicPath = "controllers/device";
                break;
            
            // External device sensors (indices 70-89)
            case OBJ_T_HAMILTON_PH_PROBE:
                topicPath = "devices/hamilton_ph";
                break;
            case OBJ_T_HAMILTON_DO_PROBE:
                topicPath = "devices/hamilton_do";
                break;
            case OBJ_T_HAMILTON_OD_PROBE:
                topicPath = "devices/hamilton_od";
                break;
            case OBJ_T_ALICAT_MFC:
                topicPath = "devices/alicat_mfc";
                break;
            
            default:
                // Skip unmapped types
                continue;
        }

        // Construct topic
        char fullTopic[192];
        snprintf(fullTopic, sizeof(fullTopic), "%s/%s/%d", deviceTopicPrefix, topicPath, obj->index);

        // Create JSON payload - use larger buffer for complex objects
        StaticJsonDocument<384> doc;
        doc["timestamp"] = timestamp;
        
        // Get name from ioConfig (names are stored on SYS MCU, not transmitted via IPC)
        const char* name = getObjectNameByIndex(obj->index);
        doc["name"] = (name && name[0] != '\0') ? name : obj->name;
        
        // Add fault flag if present
        if (obj->flags & IPC_SENSOR_FLAG_FAULT) {
            doc["fault"] = true;
        }
        
        // Add message if present
        if ((obj->flags & IPC_SENSOR_FLAG_NEW_MSG) && strlen(obj->message) > 0) {
            doc["message"] = obj->message;
        }
        
        // Type-specific payload formatting
        switch (obj->objectType) {
            // ================================================================
            // ENERGY MONITORS - Separate named fields for V, A, W
            // ================================================================
            case OBJ_T_ENERGY_SENSOR:
                doc["voltage"] = obj->value;
                doc["voltageUnit"] = obj->unit;
                if (obj->valueCount >= 2) {
                    doc["current"] = obj->additionalValues[0];
                    doc["currentUnit"] = obj->additionalUnits[0];
                    doc["power"] = obj->additionalValues[1];
                    doc["powerUnit"] = obj->additionalUnits[1];
                }
                break;
            
            // ================================================================
            // DC MOTORS - Named current field + running status
            // ================================================================
            case OBJ_T_BDC_MOTOR:
                doc["power"] = obj->value;
                doc["powerUnit"] = obj->unit;
                doc["running"] = (obj->flags & IPC_SENSOR_FLAG_RUNNING) ? true : false;
                doc["direction"] = (obj->flags & IPC_SENSOR_FLAG_DIRECTION) ? "forward" : "reverse";
                if (obj->valueCount >= 1) {
                    doc["current"] = obj->additionalValues[0];
                    doc["currentUnit"] = obj->additionalUnits[0];
                }
                break;
            
            // ================================================================
            // STEPPER MOTOR - Add running status
            // ================================================================
            case OBJ_T_STEPPER_MOTOR:
                doc["value"] = obj->value;
                doc["unit"] = obj->unit;
                doc["running"] = (obj->flags & IPC_SENSOR_FLAG_RUNNING) ? true : false;
                doc["direction"] = (obj->flags & IPC_SENSOR_FLAG_DIRECTION) ? "forward" : "reverse";
                break;
            
            // ================================================================
            // TEMPERATURE CONTROLLER - additionalValues: [output, kp, ki, kd]
            // Setpoint comes from ioConfig (not transmitted via IPC to save bandwidth)
            // ================================================================
            case OBJ_T_TEMPERATURE_CONTROL: {
                int ctrlIdx = obj->index - 40;
                doc["processValue"] = obj->value;
                doc["unit"] = obj->unit;
                doc["running"] = (obj->flags & IPC_SENSOR_FLAG_RUNNING) ? true : false;
                doc["tuning"] = (obj->flags & 0x10) ? true : false;
                
                if (ctrlIdx >= 0 && ctrlIdx < MAX_TEMP_CONTROLLERS) {
                    doc["setpoint"] = ioConfig.tempControllers[ctrlIdx].setpoint;
                    doc["controlMethod"] = (uint8_t)ioConfig.tempControllers[ctrlIdx].controlMethod;
                    
                    // For PID mode (1), include PID values
                    if (ioConfig.tempControllers[ctrlIdx].controlMethod == CONTROL_METHOD_PID) {
                        if (obj->valueCount >= 4) {
                            doc["output"] = obj->additionalValues[0];
                            doc["kp"] = obj->additionalValues[1];
                            doc["ki"] = obj->additionalValues[2];
                            doc["kd"] = obj->additionalValues[3];
                        }
                        doc["integralWindup"] = ioConfig.tempControllers[ctrlIdx].integralWindup;
                    } else {
                        // For On/Off mode (0), include hysteresis
                        if (obj->valueCount >= 1) {
                            doc["output"] = obj->additionalValues[0];
                        }
                        doc["hysteresis"] = ioConfig.tempControllers[ctrlIdx].hysteresis;
                    }
                }
                break;
            }
            
            // ================================================================
            // pH CONTROLLER - additionalValues: [output, acidVol, alkalineVol]
            // Setpoint comes from ioConfig
            // ================================================================
            case OBJ_T_PH_CONTROL:
                doc["processValue"] = obj->value;
                doc["unit"] = obj->unit;
                doc["running"] = (obj->flags & IPC_SENSOR_FLAG_RUNNING) ? true : false;
                doc["setpoint"] = ioConfig.phController.setpoint;
                doc["deadband"] = ioConfig.phController.deadband;
                if (obj->valueCount >= 3) {
                    doc["output"] = (int)obj->additionalValues[0];  // 0=off, 1=acid, 2=base
                    doc["acidDosed"] = obj->additionalValues[1];
                    doc["baseDosed"] = obj->additionalValues[2];
                    doc["dosedUnit"] = "mL";
                }
                break;
            
            // ================================================================
            // FLOW CONTROLLER - primary=setpoint, additionalValues: [output, interval, totalVol]
            // ================================================================
            case OBJ_T_FLOW_CONTROL:
                doc["setpoint"] = obj->value;  // Flow rate is the setpoint
                doc["unit"] = obj->unit;
                doc["running"] = (obj->flags & IPC_SENSOR_FLAG_RUNNING) ? true : false;
                if (obj->valueCount >= 3) {
                    doc["output"] = (int)obj->additionalValues[0];  // 0=off, 1=dosing
                    doc["pumpInterval"] = obj->additionalValues[1];
                    doc["totalDosed"] = obj->additionalValues[2];
                    doc["totalDosedUnit"] = "mL";
                }
                break;
            
            // ================================================================
            // DO CONTROLLER - additionalValues: [stirrerOut, mfcOut, error, setpoint]
            // ================================================================
            case OBJ_T_DISSOLVED_OXYGEN_CONTROL:
                doc["processValue"] = obj->value;
                doc["unit"] = obj->unit;
                doc["running"] = (obj->flags & IPC_SENSOR_FLAG_RUNNING) ? true : false;
                if (obj->valueCount >= 4) {
                    doc["stirrerOutput"] = obj->additionalValues[0];
                    doc["mfcOutput"] = obj->additionalValues[1];
                    doc["error"] = obj->additionalValues[2];
                    doc["setpoint"] = obj->additionalValues[3];
                }
                break;
            
            // ================================================================
            // DEVICES (MFC, pH probe, DO probe, etc) - Add status field
            // ================================================================
            case OBJ_T_HAMILTON_PH_PROBE:
            case OBJ_T_HAMILTON_DO_PROBE:
            case OBJ_T_HAMILTON_OD_PROBE:
            case OBJ_T_ALICAT_MFC:
            case OBJ_T_DEVICE_CONTROL:
                doc["value"] = obj->value;
                doc["unit"] = obj->unit;
                // Determine status from flags
                if (obj->flags & IPC_SENSOR_FLAG_FAULT) {
                    doc["status"] = "fault";
                } else if (obj->flags & IPC_SENSOR_FLAG_CONNECTED) {
                    doc["status"] = "connected";
                } else {
                    doc["status"] = "disconnected";
                }
                break;
            
            // ================================================================
            // DEFAULT - Standard sensor format with additionalValues if present
            // ================================================================
            default:
                doc["value"] = obj->value;
                doc["unit"] = obj->unit;
                // Add running/direction for motors
                if (obj->flags & IPC_SENSOR_FLAG_RUNNING) {
                    doc["running"] = true;
                }
                if (obj->objectType == OBJ_T_STEPPER_MOTOR || obj->objectType == OBJ_T_BDC_MOTOR) {
                    doc["direction"] = (obj->flags & IPC_SENSOR_FLAG_DIRECTION) ? "forward" : "reverse";
                }
                // Add additional values array for other types
                if (obj->valueCount > 0) {
                    JsonArray additionalValues = doc.createNestedArray("additionalValues");
                    JsonArray additionalUnits = doc.createNestedArray("additionalUnits");
                    for (uint8_t j = 0; j < obj->valueCount && j < 4; j++) {
                        additionalValues.add(obj->additionalValues[j]);
                        additionalUnits.add(obj->additionalUnits[j]);
                    }
                }
                break;
        }

        // Serialize and publish
        char payload[384];
        serializeJson(doc, payload, sizeof(payload));
        
        if (mqttClient.publish(fullTopic, payload)) {
            publishCount++;
        }
    }
    // Debug
    uint32_t te = millis();

    /*if (publishCount > 0) {
        log(LOG_INFO, false, "Published %d IPC sensor readings to MQTT in %dms\n", publishCount, te-ts);
    }*/
}

    /**
     * @brief Publishes a single sensor reading received from the I/O controller.
     *
     * This function is called by IPC callbacks. It decodes the IPC message,
     * constructs a JSON payload, and publishes it to the appropriate MQTT topic.
     *
     * @param msg The IPC message containing the sensor data.
     */
    void publishSensorData(const Message& msg) {
        if (!mqttClient.connected()) {
            return;
        }

        // 1. Find the base topic from the registry
        auto it = MqttTopicRegistry.find((MessageTypes)msg.msgId);
        if (it == MqttTopicRegistry.end()) {
            log(LOG_WARNING, false, "MQTT: No topic registered for MSG ID %d\n", msg.msgId);
            return;
        }
    
        // 2. Construct the full topic with object ID
        ensureTopicPrefix();
        char fullTopic[192];
        snprintf(fullTopic, sizeof(fullTopic), "%s/%s/%d", deviceTopicPrefix, it->second, msg.objId);

        // 3. Get a timestamp
        String timestamp = getISO8601Timestamp();
        if (timestamp.length() == 0) {
            log(LOG_WARNING, true, "MQTT: Could not get timestamp for publishing.\n");
            return; // Can't publish without a timestamp
        }

        // 4. Create JSON payload based on message type
        StaticJsonDocument<256> doc;
        doc["timestamp"] = timestamp;
        
        // Get name from ioConfig (names are stored on SYS MCU, not transmitted via IPC)
        const char* name = getObjectNameByIndex(msg.objId);
        if (name && name[0] != '\0') {
            doc["name"] = name;
        }

        // Decode the data payload based on the message type
        switch (msg.msgId) {
            case MSG_TEMPERATURE_SENSOR: {
                TemperatureSensor data;
                memcpy(&data, msg.data, sizeof(data));
                doc["value"] = data.celcius;
                doc["online"] = data.online;
                break;
            }
            case MSG_PH_SENSOR: {
                PHSensor data;
                memcpy(&data, msg.data, sizeof(data));
                doc["value"] = data.pH;
                doc["online"] = data.online;
                break;
            }
            case MSG_DO_SENSOR: {
                DissolvedOxygenSensor data;
                memcpy(&data, msg.data, sizeof(data));
                doc["value"] = data.oxygen;
                doc["online"] = data.online;
                break;
            }
            case MSG_OD_SENSOR: {
                OpticalDensitySensor data;
                memcpy(&data, msg.data, sizeof(data));
                doc["value"] = data.OD;
                doc["online"] = data.online;
                break;
            }
            case MSG_GAS_FLOW_SENSOR: {
                GasFlowSensor data;
                memcpy(&data, msg.data, sizeof(data));
                doc["value"] = data.mlPerMinute;
                doc["online"] = data.online;
                break;
            }
            case MSG_PRESSURE_SENSOR: {
                PressureSensor data;
                memcpy(&data, msg.data, sizeof(data));
                doc["value"] = data.kPa;
                doc["online"] = data.online;
                break;
            }
            case MSG_STIRRER_SPEED_SENSOR: {
                StirrerSpeedSensor data;
                memcpy(&data, msg.data, sizeof(data));
                doc["value"] = data.rpm;
                doc["online"] = data.online;
                break;
            }
            // ... ADD CASES FOR ALL OTHER SENSOR TYPES FROM IPCDataStructs.h HERE ...
            case MSG_WEIGHT_SENSOR: {
                WeightSensor data;
                memcpy(&data, msg.data, sizeof(data));
                doc["value"] = data.grams;
                doc["online"] = data.online;
                break;
            }
            default:
                log(LOG_WARNING, false, "MQTT: Unknown sensor type %d for publishing\n", msg.msgId);
                return;
        }

        char payload[256];
        serializeJson(doc, payload, sizeof(payload));

        // 5. Publish the message
        if (mqttClient.publish(fullTopic, payload)) {
            log(LOG_INFO, false, "MQTT Published [%s]: %s\n", fullTopic, payload);
        } else {
            log(LOG_WARNING, true, "MQTT publish failed for topic: %s\n", fullTopic);
        }
    }

    /**
     * @brief Publishes sensor data received via new IPC protocol
     * 
     * This function handles IPC_SensorData_t structures from the new IPC protocol.
     * It constructs a JSON payload and publishes it to the appropriate MQTT topic.
     * 
     * @param data Pointer to IPC_SensorData_t structure
     */
    void publishSensorDataIPC(const IPC_SensorData_t* data) {
        if (!mqttClient.connected() || data == nullptr) {
            return;
        }

        // Map object type to topic path
        const char* topicPath = nullptr;
        switch (data->objectType) {
            case OBJ_T_TEMPERATURE_SENSOR:
                topicPath = "sensors/temperature";
                break;
            case OBJ_T_PH_SENSOR:
                topicPath = "sensors/ph";
                break;
            case OBJ_T_DISSOLVED_OXYGEN_SENSOR:
                topicPath = "sensors/do";
                break;
            case OBJ_T_OPTICAL_DENSITY_SENSOR:
                topicPath = "sensors/od";
                break;
            case OBJ_T_FLOW_SENSOR:
                topicPath = "sensors/flow";
                break;
            case OBJ_T_PRESSURE_SENSOR:
                topicPath = "sensors/pressure";
                break;
            case OBJ_T_POWER_SENSOR:
                topicPath = "sensors/power";
                break;
            case OBJ_T_ANALOG_INPUT:
                topicPath = "sensors/analog";
                break;
            default:
                // Silently ignore unmapped types (DACs, GPIOs, motors, etc.)
                // These are not sensors and don't need MQTT publishing
                return;
        }

        // Construct the full topic with object index
        ensureTopicPrefix();
        char fullTopic[192];
        snprintf(fullTopic, sizeof(fullTopic), "%s/%s/%d", deviceTopicPrefix, topicPath, data->index);

        // Get timestamp - use IPC timestamp if available, otherwise get current time
        String timestamp;
        if (data->timestamp > 0) {
            // Convert milliseconds timestamp to ISO8601
            // TODO: Implement proper timestamp conversion from IPC timestamp
            timestamp = getISO8601Timestamp(); // For now, use current time
        } else {
            timestamp = getISO8601Timestamp();
        }

        if (timestamp.length() == 0) {
            log(LOG_WARNING, true, "MQTT: Could not get timestamp for publishing.\n");
            return;
        }

        // Create JSON payload
        StaticJsonDocument<256> doc;
        doc["timestamp"] = timestamp;
        doc["value"] = data->value;
        doc["unit"] = data->unit;
        doc["status"] = data->flags;
        
        // Get name from ioConfig (names are stored on SYS MCU, not transmitted via IPC)
        const char* name = getObjectNameByIndex(data->index);
        if (name && name[0] != '\0') {
            doc["name"] = name;
        }
        
        // Add fault flag if present
        if (data->flags & IPC_SENSOR_FLAG_FAULT) {
            doc["fault"] = true;
        }
        
        // Add message if present
        if ((data->flags & IPC_SENSOR_FLAG_NEW_MSG) && strlen(data->message) > 0) {
            doc["message"] = data->message;
        }

        char payload[256];
        serializeJson(doc, payload, sizeof(payload));

        // Publish the message (suppress per-message logging to reduce spam)
        if (!mqttClient.publish(fullTopic, payload)) {
            log(LOG_WARNING, true, "MQTT publish failed for topic: %s\n", fullTopic);
        }
    }

// =============================================================================
// MQTT CONTROL IMPLEMENTATION
// =============================================================================

/**
 * @brief Subscribe to control topics after MQTT connection
 * 
 * Subscribes to wildcard control topics:
 * - orc.{MAC}.control/output/#  - Output control (digital, DAC)
 * - orc.{MAC}.control/device/#  - Device control (MFC setpoint)
 * - orc.{MAC}.control/controller/# - Controller control (temp, pH, flow, DO)
 */
static void subscribeToControlTopics() {
    ensureTopicPrefix();
    
    // Subscribe to all control topics with wildcard
    char subTopic[96];
    snprintf(subTopic, sizeof(subTopic), "%s/#", controlTopicPrefix);
    
    if (mqttClient.subscribe(subTopic)) {
        log(LOG_INFO, true, "MQTT subscribed to: %s\n", subTopic);
        controlSubscribed = true;
    } else {
        log(LOG_WARNING, true, "MQTT subscription failed: %s\n", subTopic);
        controlSubscribed = false;
    }
}

/**
 * @brief Publish acknowledgment for a control command
 */
static void publishControlAck(const char* controlTopic, bool success, const char* message) {
    if (!mqttClient.connected()) return;
    
    // Build ack topic by appending /ack to the control topic
    char ackTopic[192];
    snprintf(ackTopic, sizeof(ackTopic), "%s/ack", controlTopic);
    
    StaticJsonDocument<256> doc;
    doc["success"] = success;
    doc["message"] = message;
    doc["timestamp"] = getISO8601Timestamp();
    
    char payload[256];
    serializeJson(doc, payload, sizeof(payload));
    
    mqttClient.publish(ackTopic, payload);
    log(LOG_INFO, false, "MQTT ACK [%s]: %s\n", ackTopic, success ? "OK" : "FAIL");
}

/**
 * @brief Parse incoming control message and route to appropriate handler
 * 
 * Topic format: orc.{MAC}.control/{category}/{index}
 * Categories: output, device, controller/temp, controller/ph, controller/flow, controller/do
 */
static void handleControlMessage(const char* topic, const char* payload) {
    // Parse JSON payload
    StaticJsonDocument<512> doc;
    DeserializationError error = deserializeJson(doc, payload);
    
    if (error) {
        log(LOG_WARNING, false, "MQTT control: Invalid JSON: %s\n", error.c_str());
        publishControlAck(topic, false, "Invalid JSON");
        return;
    }
    
    // Find the category and index in the topic
    // Expected: orc/XX:XX:XX:XX:XX:XX/control/output/21
    const char* controlPart = strstr(topic, "/control/");
    if (!controlPart) {
        log(LOG_WARNING, false, "MQTT control: Invalid topic format\n");
        publishControlAck(topic, false, "Invalid topic format");
        return;
    }
    
    // Skip past "/control/"
    const char* pathStart = controlPart + 9;
    
    // Parse path: category/index or category/subcategory/index
    char category[32] = {0};
    char subcategory[32] = {0};
    int index = -1;
    
    // Try to parse as category/index
    if (sscanf(pathStart, "%31[^/]/%d", category, &index) >= 1) {
        // Check if there's a subcategory (e.g., controller/temp/40)
        char* secondSlash = strchr(pathStart, '/');
        if (secondSlash) {
            char* thirdSlash = strchr(secondSlash + 1, '/');
            if (thirdSlash) {
                // Has subcategory: category/subcategory/index
                sscanf(pathStart, "%31[^/]/%31[^/]/%d", category, subcategory, &index);
            }
        }
    }
    
    log(LOG_INFO, false, "MQTT control: category=%s, subcategory=%s, index=%d\n", 
        category, subcategory, index);
    
    // Route to appropriate handler
    if (strcmp(category, "output") == 0 && index >= 0) {
        handleOutputControl((uint8_t)index, doc);
        publishControlAck(topic, true, "Output command sent");
    }
    else if (strcmp(category, "stepper") == 0) {
        handleStepperControl(doc);
        publishControlAck(topic, true, "Stepper command sent");
    }
    else if (strcmp(category, "motor") == 0 && index >= 27 && index <= 30) {
        handleDCMotorControl((uint8_t)index, doc);
        publishControlAck(topic, true, "DC motor command sent");
    }
    else if (strcmp(category, "device") == 0 && index >= 50 && index <= 69) {
        handleDeviceControl((uint8_t)index, doc);
        publishControlAck(topic, true, "Device command sent");
    }
    else if (strcmp(category, "controller") == 0) {
        if (strcmp(subcategory, "temp") == 0 && index >= 40 && index <= 42) {
            handleTempControllerControl((uint8_t)index, doc);
            publishControlAck(topic, true, "Temp controller command sent");
        }
        else if (strcmp(subcategory, "ph") == 0 || index == 43) {
            handlepHControllerControl(doc);
            publishControlAck(topic, true, "pH controller command sent");
        }
        else if (strcmp(subcategory, "flow") == 0 && index >= 44 && index <= 47) {
            handleFlowControllerControl((uint8_t)index, doc);
            publishControlAck(topic, true, "Flow controller command sent");
        }
        else if (strcmp(subcategory, "do") == 0 || index == 48) {
            handleDOControllerControl(doc);
            publishControlAck(topic, true, "DO controller command sent");
        }
        else {
            publishControlAck(topic, false, "Unknown controller type");
        }
    }
    else {
        publishControlAck(topic, false, "Unknown control category");
    }
}

/**
 * @brief Handle output control commands (digital outputs 21-25, DAC 8-9)
 * 
 * JSON payloads:
 * - {"state": true/false}  - Digital ON/OFF
 * - {"power": 0-100}       - PWM percentage
 * - {"mV": 0-10240}        - DAC millivolts
 */
static void handleOutputControl(uint8_t index, JsonDocument& doc) {
    bool sent = false;
    
    // Digital outputs (21-25)
    if (index >= 21 && index <= 25) {
        if (doc.containsKey("state")) {
            bool state = doc["state"];
            sent = sendDigitalOutputCommand(index, DOUT_CMD_SET_STATE, state, 0);
            log(LOG_INFO, false, "MQTT: Output %d state -> %s\n", index, state ? "ON" : "OFF");
        }
        else if (doc.containsKey("power")) {
            float power = doc["power"] | 0.0f;
            if (power >= 0 && power <= 100) {
                sent = sendDigitalOutputCommand(index, DOUT_CMD_SET_PWM, false, power);
                log(LOG_INFO, false, "MQTT: Output %d PWM -> %.1f%%\n", index, power);
            }
        }
    }
    // DAC outputs (8-9)
    else if (index >= 8 && index <= 9) {
        if (doc.containsKey("mV")) {
            float mV = doc["mV"] | 0.0f;
            if (mV >= 0 && mV <= 10240) {
                sent = sendAnalogOutputCommand(index, AOUT_CMD_SET_VALUE, mV);
                log(LOG_INFO, false, "MQTT: DAC %d -> %.1f mV\n", index, mV);
            }
        }
    }
    
    if (!sent) {
        log(LOG_WARNING, false, "MQTT: Output %d command failed or invalid\n", index);
    }
}

/**
 * @brief Handle device control commands (MFC setpoint)
 * 
 * JSON payload: {"setpoint": value}
 */
static void handleDeviceControl(uint8_t index, JsonDocument& doc) {
    // MFC devices are at indices 50-69
    if (index < 50 || index > 69) {
        log(LOG_WARNING, false, "MQTT: Invalid device index %d\n", index);
        return;
    }
    
    if (doc.containsKey("setpoint")) {
        float setpoint = doc["setpoint"];
        
        // Send device control command via IPC
        IPC_DeviceControlCmd_t cmd;
        cmd.transactionId = generateTransactionId();
        cmd.index = index;
        cmd.objectType = OBJ_T_DEVICE_CONTROL;
        cmd.command = DEV_CMD_SET_SETPOINT;
        cmd.setpoint = setpoint;
        memset(cmd.reserved, 0, sizeof(cmd.reserved));
        
        bool sent = ipc.sendPacket(IPC_MSG_DEVICE_CONTROL, (uint8_t*)&cmd, sizeof(cmd));
        
        if (sent) {
            addPendingTransaction(cmd.transactionId, IPC_MSG_DEVICE_CONTROL, IPC_MSG_CONTROL_ACK, 1, index);
            log(LOG_INFO, false, "MQTT: Device %d setpoint -> %.2f\n", index, setpoint);
        } else {
            log(LOG_WARNING, false, "MQTT: Device %d setpoint failed - IPC queue full\n", index);
        }
    }
}

/**
 * @brief Handle stepper motor control commands (index 26)
 * 
 * JSON payloads:
 * - {"start": true, "rpm": 100, "forward": true}  - Start motor
 * - {"stop": true}                                 - Stop motor
 * - {"rpm": 100}                                   - Set RPM (while running)
 * - {"forward": true/false}                        - Set direction
 */
static void handleStepperControl(JsonDocument& doc) {
    bool sent = false;
    
    if (doc.containsKey("stop") && doc["stop"].as<bool>()) {
        sent = sendStepperCommand(STEPPER_CMD_STOP, 0, false);
        log(LOG_INFO, false, "MQTT: Stepper stop\n");
    }
    else if (doc.containsKey("start") && doc["start"].as<bool>()) {
        float rpm = doc["rpm"] | 0.0f;
        bool forward = doc["forward"] | true;
        
        if (rpm > 0 && rpm <= ioConfig.stepperMotor.maxRPM) {
            sent = sendStepperCommand(STEPPER_CMD_START, rpm, forward);
            log(LOG_INFO, false, "MQTT: Stepper start RPM=%.1f, dir=%s\n", rpm, forward ? "FWD" : "REV");
        } else {
            log(LOG_WARNING, false, "MQTT: Stepper invalid RPM %.1f (max=%.1f)\n", rpm, ioConfig.stepperMotor.maxRPM);
        }
    }
    else if (doc.containsKey("rpm")) {
        float rpm = doc["rpm"] | 0.0f;
        if (rpm >= 0 && rpm <= ioConfig.stepperMotor.maxRPM) {
            sent = sendStepperCommand(STEPPER_CMD_SET_RPM, rpm, true);
            log(LOG_INFO, false, "MQTT: Stepper RPM -> %.1f\n", rpm);
        }
    }
    else if (doc.containsKey("forward")) {
        bool forward = doc["forward"];
        sent = sendStepperCommand(STEPPER_CMD_SET_DIR, 0, forward);
        log(LOG_INFO, false, "MQTT: Stepper direction -> %s\n", forward ? "FWD" : "REV");
    }
    
    if (!sent) {
        log(LOG_WARNING, false, "MQTT: Stepper command failed or invalid\n");
    }
}

/**
 * @brief Handle DC motor control commands (indices 27-30)
 * 
 * JSON payloads:
 * - {"start": true, "power": 50, "forward": true}  - Start motor
 * - {"stop": true}                                  - Stop motor
 * - {"power": 50}                                   - Set power (while running)
 * - {"forward": true/false}                         - Set direction
 */
static void handleDCMotorControl(uint8_t index, JsonDocument& doc) {
    if (index < 27 || index > 30) {
        log(LOG_WARNING, false, "MQTT: Invalid DC motor index %d\n", index);
        return;
    }
    
    bool sent = false;
    
    if (doc.containsKey("stop") && doc["stop"].as<bool>()) {
        sent = sendDCMotorCommand(index, DCMOTOR_CMD_STOP, 0, false);
        log(LOG_INFO, false, "MQTT: DC motor %d stop\n", index);
    }
    else if (doc.containsKey("start") && doc["start"].as<bool>()) {
        float power = doc["power"] | 0.0f;
        bool forward = doc["forward"] | true;
        
        if (power >= 0 && power <= 100) {
            sent = sendDCMotorCommand(index, DCMOTOR_CMD_START, power, forward);
            log(LOG_INFO, false, "MQTT: DC motor %d start power=%.1f%%, dir=%s\n", 
                index, power, forward ? "FWD" : "REV");
        } else {
            log(LOG_WARNING, false, "MQTT: DC motor %d invalid power %.1f\n", index, power);
        }
    }
    else if (doc.containsKey("power")) {
        float power = doc["power"] | 0.0f;
        if (power >= 0 && power <= 100) {
            sent = sendDCMotorCommand(index, DCMOTOR_CMD_SET_POWER, power, true);
            log(LOG_INFO, false, "MQTT: DC motor %d power -> %.1f%%\n", index, power);
        }
    }
    else if (doc.containsKey("forward")) {
        bool forward = doc["forward"];
        sent = sendDCMotorCommand(index, DCMOTOR_CMD_SET_DIR, 0, forward);
        log(LOG_INFO, false, "MQTT: DC motor %d direction -> %s\n", index, forward ? "FWD" : "REV");
    }
    
    if (!sent) {
        log(LOG_WARNING, false, "MQTT: DC motor %d command failed or invalid\n", index);
    }
}

/**
 * @brief Handle temperature controller commands
 * 
 * JSON payloads:
 * - {"enabled": true/false}
 * - {"setpoint": value}
 * - {"autotune": true}
 * - {"kp": value, "ki": value, "kd": value}
 * - {"hysteresis": value}
 */
static void handleTempControllerControl(uint8_t index, JsonDocument& doc) {
    int ctrlIdx = index - 40;
    if (ctrlIdx < 0 || ctrlIdx >= MAX_TEMP_CONTROLLERS) return;
    
    IPC_TempControllerControl_t cmd;
    memset(&cmd, 0, sizeof(cmd));
    cmd.transactionId = generateTransactionId();
    cmd.index = index;
    cmd.objectType = OBJ_T_TEMPERATURE_CONTROL;
    
    bool sent = false;
    
    if (doc.containsKey("enabled")) {
        cmd.command = doc["enabled"].as<bool>() ? TEMP_CTRL_CMD_ENABLE : TEMP_CTRL_CMD_DISABLE;
        sent = ipc.sendPacket(IPC_MSG_CONTROL_WRITE, (uint8_t*)&cmd, sizeof(cmd));
        log(LOG_INFO, false, "MQTT: Temp controller %d %s\n", index, 
            doc["enabled"].as<bool>() ? "enabled" : "disabled");
    }
    else if (doc.containsKey("setpoint")) {
        cmd.command = TEMP_CTRL_CMD_SET_SETPOINT;
        cmd.setpoint = doc["setpoint"];
        sent = ipc.sendPacket(IPC_MSG_CONTROL_WRITE, (uint8_t*)&cmd, sizeof(cmd));
        if (sent) {
            ioConfig.tempControllers[ctrlIdx].setpoint = cmd.setpoint;
        }
        log(LOG_INFO, false, "MQTT: Temp controller %d setpoint -> %.1f\n", index, cmd.setpoint);
    }
    else if (doc.containsKey("autotune") && doc["autotune"].as<bool>()) {
        cmd.command = TEMP_CTRL_CMD_START_AUTOTUNE;
        cmd.setpoint = ioConfig.tempControllers[ctrlIdx].setpoint;
        cmd.autotuneOutputStep = 100.0f;
        sent = ipc.sendPacket(IPC_MSG_CONTROL_WRITE, (uint8_t*)&cmd, sizeof(cmd));
        log(LOG_INFO, false, "MQTT: Temp controller %d autotune started\n", index);
    }
    else if (doc.containsKey("kp") || doc.containsKey("ki") || doc.containsKey("kd")) {
        // Update PID values - need to send full config
        if (doc.containsKey("kp")) ioConfig.tempControllers[ctrlIdx].kP = doc["kp"];
        if (doc.containsKey("ki")) ioConfig.tempControllers[ctrlIdx].kI = doc["ki"];
        if (doc.containsKey("kd")) ioConfig.tempControllers[ctrlIdx].kD = doc["kd"];
        
        // Send config update
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
        
        sent = ipc.sendPacket(IPC_MSG_CONFIG_TEMP_CONTROLLER, (uint8_t*)&cfg, sizeof(cfg));
        log(LOG_INFO, false, "MQTT: Temp controller %d PID updated: P=%.2f I=%.2f D=%.2f\n", 
            index, cfg.kP, cfg.kI, cfg.kD);
    }
    else if (doc.containsKey("hysteresis")) {
        ioConfig.tempControllers[ctrlIdx].hysteresis = doc["hysteresis"];
        // Send config update (similar to PID)
        log(LOG_INFO, false, "MQTT: Temp controller %d hysteresis -> %.2f\n", 
            index, ioConfig.tempControllers[ctrlIdx].hysteresis);
    }
    
    if (sent) {
        addPendingTransaction(cmd.transactionId, IPC_MSG_CONTROL_WRITE, IPC_MSG_CONTROL_ACK, 1, index);
    }
}

/**
 * @brief Handle pH controller commands
 * 
 * JSON payloads:
 * - {"enabled": true/false}
 * - {"setpoint": value}
 * - {"doseAcid": mL}
 * - {"doseAlkaline": mL}
 * - {"resetVolumes": true}
 */
static void handlepHControllerControl(JsonDocument& doc) {
    IPC_pHControllerControl_t cmd;
    memset(&cmd, 0, sizeof(cmd));
    cmd.transactionId = generateTransactionId();
    cmd.index = 43;
    cmd.objectType = OBJ_T_PH_CONTROL;
    
    bool sent = false;
    
    if (doc.containsKey("enabled")) {
        cmd.command = doc["enabled"].as<bool>() ? PH_CMD_ENABLE : PH_CMD_DISABLE;
        sent = ipc.sendPacket(IPC_MSG_CONTROL_WRITE, (uint8_t*)&cmd, sizeof(cmd));
        log(LOG_INFO, false, "MQTT: pH controller %s\n", doc["enabled"].as<bool>() ? "enabled" : "disabled");
    }
    else if (doc.containsKey("setpoint")) {
        cmd.command = PH_CMD_SET_SETPOINT;
        cmd.setpoint = doc["setpoint"];
        sent = ipc.sendPacket(IPC_MSG_CONTROL_WRITE, (uint8_t*)&cmd, sizeof(cmd));
        if (sent) {
            ioConfig.phController.setpoint = cmd.setpoint;
        }
        log(LOG_INFO, false, "MQTT: pH controller setpoint -> %.2f\n", cmd.setpoint);
    }
    else if (doc.containsKey("doseAcid") && doc["doseAcid"].as<bool>()) {
        // Trigger one dose at configured volume
        cmd.command = PH_CMD_DOSE_ACID;
        sent = ipc.sendPacket(IPC_MSG_CONTROL_WRITE, (uint8_t*)&cmd, sizeof(cmd));
        log(LOG_INFO, false, "MQTT: pH controller dose acid triggered\n");
    }
    else if (doc.containsKey("doseAlkaline") && doc["doseAlkaline"].as<bool>()) {
        // Trigger one dose at configured volume
        cmd.command = PH_CMD_DOSE_ALKALINE;
        sent = ipc.sendPacket(IPC_MSG_CONTROL_WRITE, (uint8_t*)&cmd, sizeof(cmd));
        log(LOG_INFO, false, "MQTT: pH controller dose alkaline triggered\n");
    }
    else if (doc.containsKey("resetVolumes") && doc["resetVolumes"].as<bool>()) {
        // Reset both acid and alkaline volumes
        cmd.command = PH_CMD_RESET_ACID_VOLUME;
        sent = ipc.sendPacket(IPC_MSG_CONTROL_WRITE, (uint8_t*)&cmd, sizeof(cmd));
        cmd.command = PH_CMD_RESET_BASE_VOLUME;
        sent = ipc.sendPacket(IPC_MSG_CONTROL_WRITE, (uint8_t*)&cmd, sizeof(cmd));
        log(LOG_INFO, false, "MQTT: pH controller reset volumes\n");
    }
    
    if (sent) {
        addPendingTransaction(cmd.transactionId, IPC_MSG_CONTROL_WRITE, IPC_MSG_CONTROL_ACK, 1, 43);
    }
}

/**
 * @brief Handle flow controller commands
 * 
 * JSON payloads:
 * - {"enabled": true/false}
 * - {"setpoint": mL/min}
 * - {"manualDose": mL}
 * - {"resetVolume": true}
 */
static void handleFlowControllerControl(uint8_t index, JsonDocument& doc) {
    int ctrlIdx = index - 44;
    if (ctrlIdx < 0 || ctrlIdx >= MAX_FLOW_CONTROLLERS) return;
    
    IPC_FlowControllerControl_t cmd;
    memset(&cmd, 0, sizeof(cmd));
    cmd.transactionId = generateTransactionId();
    cmd.index = index;
    cmd.objectType = OBJ_T_FLOW_CONTROL;
    
    bool sent = false;
    
    if (doc.containsKey("enabled")) {
        cmd.command = doc["enabled"].as<bool>() ? FLOW_CMD_ENABLE : FLOW_CMD_DISABLE;
        sent = ipc.sendPacket(IPC_MSG_CONTROL_WRITE, (uint8_t*)&cmd, sizeof(cmd));
        log(LOG_INFO, false, "MQTT: Flow controller %d %s\n", index, 
            doc["enabled"].as<bool>() ? "enabled" : "disabled");
    }
    else if (doc.containsKey("setpoint")) {
        cmd.command = FLOW_CMD_SET_FLOW_RATE;
        cmd.flowRate_mL_min = doc["setpoint"];
        sent = ipc.sendPacket(IPC_MSG_CONTROL_WRITE, (uint8_t*)&cmd, sizeof(cmd));
        if (sent) {
            ioConfig.flowControllers[ctrlIdx].flowRate_mL_min = cmd.flowRate_mL_min;
        }
        log(LOG_INFO, false, "MQTT: Flow controller %d setpoint -> %.2f mL/min\n", index, cmd.flowRate_mL_min);
    }
    else if (doc.containsKey("manualDose") && doc["manualDose"].as<bool>()) {
        // Trigger one dose cycle
        cmd.command = FLOW_CMD_MANUAL_DOSE;
        sent = ipc.sendPacket(IPC_MSG_CONTROL_WRITE, (uint8_t*)&cmd, sizeof(cmd));
        log(LOG_INFO, false, "MQTT: Flow controller %d manual dose triggered\n", index);
    }
    else if (doc.containsKey("resetVolume") && doc["resetVolume"].as<bool>()) {
        cmd.command = FLOW_CMD_RESET_VOLUME;
        sent = ipc.sendPacket(IPC_MSG_CONTROL_WRITE, (uint8_t*)&cmd, sizeof(cmd));
        log(LOG_INFO, false, "MQTT: Flow controller %d reset volume\n", index);
    }
    
    if (sent) {
        addPendingTransaction(cmd.transactionId, IPC_MSG_CONTROL_WRITE, IPC_MSG_CONTROL_ACK, 1, index);
    }
}

/**
 * @brief Handle DO controller commands
 * 
 * JSON payloads:
 * - {"enabled": true/false}
 * - {"setpoint": mg/L}
 * - {"activeProfile": "name"} or {"activeProfileIndex": 0-2}
 */
static void handleDOControllerControl(JsonDocument& doc) {
    IPC_DOControllerControl_t cmd;
    memset(&cmd, 0, sizeof(cmd));
    cmd.transactionId = generateTransactionId();
    cmd.index = 48;
    cmd.objectType = OBJ_T_DISSOLVED_OXYGEN_CONTROL;
    
    bool sent = false;
    
    if (doc.containsKey("enabled")) {
        cmd.command = doc["enabled"].as<bool>() ? DO_CMD_ENABLE : DO_CMD_DISABLE;
        sent = ipc.sendPacket(IPC_MSG_CONTROL_WRITE, (uint8_t*)&cmd, sizeof(cmd));
        log(LOG_INFO, false, "MQTT: DO controller %s\n", doc["enabled"].as<bool>() ? "enabled" : "disabled");
    }
    else if (doc.containsKey("setpoint")) {
        cmd.command = DO_CMD_SET_SETPOINT;
        cmd.setpoint_mg_L = doc["setpoint"];
        sent = ipc.sendPacket(IPC_MSG_CONTROL_WRITE, (uint8_t*)&cmd, sizeof(cmd));
        if (sent) {
            ioConfig.doController.setpoint_mg_L = cmd.setpoint_mg_L;
        }
        log(LOG_INFO, false, "MQTT: DO controller setpoint -> %.2f mg/L\n", cmd.setpoint_mg_L);
    }
    else if (doc.containsKey("activeProfileIndex")) {
        int8_t profileIdx = doc["activeProfileIndex"];
        if (profileIdx >= 0 && profileIdx < MAX_DO_PROFILES) {
            // Profile change requires sending full config with new profile
            ioConfig.doController.activeProfileIndex = profileIdx;
            log(LOG_INFO, false, "MQTT: DO controller profile -> %d\n", profileIdx);
            // TODO: Send full DO config to apply profile change
            sent = true;
        }
    }
    else if (doc.containsKey("activeProfile")) {
        // Find profile by name
        const char* profileName = doc["activeProfile"];
        for (int i = 0; i < MAX_DO_PROFILES; i++) {
            if (ioConfig.doProfiles[i].isActive && 
                strcmp(ioConfig.doProfiles[i].name, profileName) == 0) {
                ioConfig.doController.activeProfileIndex = i;
                log(LOG_INFO, false, "MQTT: DO controller profile -> %s (%d)\n", profileName, i);
                // TODO: Send full DO config to apply profile change
                sent = true;
                break;
            }
        }
    }
    
    if (sent) {
        addPendingTransaction(cmd.transactionId, IPC_MSG_CONTROL_WRITE, IPC_MSG_CONTROL_ACK, 1, 48);
    }
}
