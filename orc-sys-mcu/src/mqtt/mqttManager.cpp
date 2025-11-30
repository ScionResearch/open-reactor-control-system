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
static bool clientConfigured = false; // Track if client parameters have been set

// Forward declaration
static void reconnect();
static void mqttPublishAllSensorData();
static void mqttPublishIPCSensors();
static void ensureTopicPrefix();
static void mqttCallback(char* topic, byte* payload, unsigned int length);

// MQTT callback function (required by PubSubClient even if we don't subscribe to anything)
static void mqttCallback(char* topic, byte* payload, unsigned int length) {
    // Currently we only publish, but this callback is required by the library
    // Future: Handle incoming commands here
    log(LOG_INFO, false, "MQTT message received on topic: %s\n", topic);
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
        // TODO: Add subscriptions for commands here in the future
        // mqttClient.subscribe("orcs/system/command");
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
    if (strlen(networkConfig.mqttDevicePrefix) > 0) {
        strlcpy(deviceTopicPrefix, networkConfig.mqttDevicePrefix, sizeof(deviceTopicPrefix));
        return;
    }
    uint8_t mac[6];
    eth.macAddress(mac);
    snprintf(deviceTopicPrefix, sizeof(deviceTopicPrefix), "orcs/dev/%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
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

        // Create JSON payload
        StaticJsonDocument<256> doc;
        doc["timestamp"] = timestamp;
        doc["value"] = obj->value;
        doc["unit"] = obj->unit;
        
        // Get name from ioConfig (names are stored on SYS MCU, not transmitted via IPC)
        const char* name = getObjectNameByIndex(obj->index);
        doc["name"] = (name && name[0] != '\0') ? name : obj->name;
        
        // Add flags
        if (obj->flags & IPC_SENSOR_FLAG_FAULT) {
            doc["fault"] = true;
        }
        if (obj->flags & IPC_SENSOR_FLAG_RUNNING) {
            doc["running"] = true;
        }
        if (obj->flags & IPC_SENSOR_FLAG_DIRECTION) {
            doc["direction"] = "forward";
        } else if (obj->objectType == OBJ_T_STEPPER_MOTOR || obj->objectType == OBJ_T_BDC_MOTOR) {
            doc["direction"] = "reverse";
        }
        
        // For controllers, primary value is process value, additional values are setpoint & output
        bool isController = (obj->objectType >= OBJ_T_TEMPERATURE_CONTROL && obj->objectType <= OBJ_T_DEVICE_CONTROL);
        if (isController && obj->valueCount >= 2) {
            doc["processValue"] = obj->value;
            doc["setpoint"] = obj->additionalValues[0];
            doc["output"] = obj->additionalValues[1];
            // Remove generic "value" field for controllers
            doc.remove("value");
        }
        
        // Add message if present
        if ((obj->flags & IPC_SENSOR_FLAG_NEW_MSG) && strlen(obj->message) > 0) {
            doc["message"] = obj->message;
        }

        // Add additional values if present (e.g., energy monitor) - skip for controllers (already added above)
        if (obj->valueCount > 0 && !isController) {
            JsonArray additionalValues = doc.createNestedArray("additionalValues");
            JsonArray additionalUnits = doc.createNestedArray("additionalUnits");
            for (uint8_t j = 0; j < obj->valueCount && j < 4; j++) {
                additionalValues.add(obj->additionalValues[j]);
                additionalUnits.add(obj->additionalUnits[j]);
            }
        }

        // Serialize and publish
        char payload[256];
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

