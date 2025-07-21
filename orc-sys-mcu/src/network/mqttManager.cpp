#include "../utils/timeManager.h" // For globalDateTime

// Helper to format DateTime as ISO8601 string
String dateTimeToISO8601(const DateTime& dt) {
    char buf[25];
    snprintf(buf, sizeof(buf), "%04u-%02u-%02uT%02u:%02u:%02uZ",
        dt.year, dt.month, dt.day, dt.hour, dt.minute, dt.second);
    return String(buf);
}
#include "../utils/logger.h"
#include "mqttManager.h"
#include "MqttTopicRegistry.h"
#include "network.h" // for NetworkConfig and networkConfig
#include "../utils/statusManager.h" // for status and statusLocked
#include <Arduino.h>
#include <ArduinoJson.h>
#include <PubSubClient.h>
#include <WiFi.h>
#include <cstring>   // for strlen
#include <cstdint>   // for uint8_t
#include <cstdio>    // for snprintf

// Global MQTT client - using WiFiClient for lwIP w5500
WiFiClient mqttNetClient;
PubSubClient mqttClient(mqttNetClient);

unsigned long lastMqttReconnectAttempt = 0;
unsigned long lastMqttPublishTime = 0;

// Forward declaration
void reconnect();
void mqttPublishAllSensorData();

// Reconnect to MQTT broker
void reconnect() {
    if (strlen(networkConfig.mqttBroker) == 0) {
        return;
    }
    log(LOG_INFO, true, "Attempting MQTT connection to %s...", networkConfig.mqttBroker);
    char clientId[24];
    uint8_t mac[6];
    eth.macAddress(mac);
    snprintf(clientId, sizeof(clientId), "ORCS-%02X%02X%02X", mac[3], mac[4], mac[5]);
    bool connected = false;
    if (strlen(networkConfig.mqttUsername) > 0) {
        connected = mqttClient.connect(clientId, networkConfig.mqttUsername, networkConfig.mqttPassword);
    } else {
        connected = mqttClient.connect(clientId);
    }
    if (connected) {
        log(LOG_INFO, true, "MQTT connected successfully!\n");
        // TODO: Add subscriptions for commands here in the future
        // mqttClient.subscribe("orcs/system/command");
    } else {
        log(LOG_WARNING, true, "MQTT connection failed, rc=%d. Will retry in %d seconds.\n", mqttClient.state(), MQTT_RECONNECT_INTERVAL / 1000);
    }
}

// --- Topic Registry ---
typedef float (*SensorValueGetter)();
struct MqttTopicEntry {
    const char* topic;
    SensorValueGetter getter;
    const char* description;
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
float getRtcOK();
float getModbusConnected();
float getModbusBusy();
float getWebserverUp();
float getWebserverBusy();
float getMqttConnected();
float getMqttBusy();

MqttTopicEntry mqttTopics[] = {
    {"orcs/system/power/voltage", getVpsu, "Main PSU voltage (V)"},
    {"orcs/system/power/20v", getV20, "20V rail voltage (V)"},
    {"orcs/system/power/5v", getV5, "5V rail voltage (V)"},
    {"orcs/system/status/psu_ok", getPsuOK, "PSU OK status (1=OK, 0=Fault)"},
    {"orcs/system/status/20v_ok", get20vOK, "20V rail OK status (1=OK, 0=Fault)"},
    {"orcs/system/status/5v_ok", get5vOK, "5V rail OK status (1=OK, 0=Fault)"},
    {"orcs/system/status/sdcard_ok", getSdCardOK, "SD card OK status (1=OK, 0=Fault)"},
    {"orcs/system/status/ipc_ok", getIpcOK, "IPC OK status (1=OK, 0=Fault)"},
    {"orcs/system/status/rtc_ok", getRtcOK, "RTC OK status (1=OK, 0=Fault)"},
    {"orcs/system/status/modbus_connected", getModbusConnected, "Modbus connected (1=Connected, 0=Not)"},
    {"orcs/system/status/modbus_busy", getModbusBusy, "Modbus busy (1=Busy, 0=Idle)"},
    {"orcs/system/status/webserver_up", getWebserverUp, "Webserver up (1=Up, 0=Down)"},
    {"orcs/system/status/webserver_busy", getWebserverBusy, "Webserver busy (1=Busy, 0=Idle)"},
    {"orcs/system/status/mqtt_connected", getMqttConnected, "MQTT connected (1=Connected, 0=Not)"},
    {"orcs/system/status/mqtt_busy", getMqttBusy, "MQTT busy (1=Busy, 0=Idle)"},
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
float getRtcOK() { return status.rtcOK ? 1.0f : 0.0f; }
float getModbusConnected() { return status.modbusConnected ? 1.0f : 0.0f; }
float getModbusBusy() { return status.modbusBusy ? 1.0f : 0.0f; }
float getWebserverUp() { return status.webserverUp ? 1.0f : 0.0f; }
float getWebserverBusy() { return status.webserverBusy ? 1.0f : 0.0f; }
float getMqttConnected() { return status.mqttConnected ? 1.0f : 0.0f; }
float getMqttBusy() { return status.mqttBusy ? 1.0f : 0.0f; }


void init_mqttManager() {
    log(LOG_DEBUG, false, "[Core0] init_mqttManager() start\n");
    if (strlen(networkConfig.mqttBroker) > 0) {
        mqttClient.setServer(networkConfig.mqttBroker, networkConfig.mqttPort);
        log(LOG_INFO, false, "MQTT Manager initialized for broker %s:%d\n", networkConfig.mqttBroker, networkConfig.mqttPort);
    } else {
        log(LOG_INFO, false, "MQTT broker not configured. MQTT Manager will remain idle.\n");
    }
}

void manageMqtt() {
    log(LOG_DEBUG, false, "[MQTT] manageMqtt start\n");
    if (!ethernetConnected || strlen(networkConfig.mqttBroker) == 0) {
        log(LOG_DEBUG, false, "[MQTT] not connected or broker not set\n");
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

    if (!mqttClient.connected()) {
        log(LOG_DEBUG, false, "[MQTT] not connected, will try reconnect\n");
        if (status.mqttConnected) { // Update status if we just disconnected
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
        // Process MQTT messages and keep-alives
        log(LOG_DEBUG, false, "[MQTT] calling mqttClient.loop\n");
        mqttClient.loop();

        // Check if it's time to publish data
        if (millis() - lastMqttPublishTime > MQTT_PUBLISH_INTERVAL) {
            lastMqttPublishTime = millis();
            log(LOG_DEBUG, false, "[MQTT] publishing all sensor data\n");
            mqttPublishAllSensorData();
        }
    }
    log(LOG_DEBUG, false, "[MQTT] manageMqtt end\n");
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
void mqttPublishAllSensorData() {
    if (!mqttClient.connected()) {
        return;
    }

    // Get current timestamp
    DateTime now;
    if (!dateTimeLocked) {
        dateTimeLocked = true;
        memcpy(&now, &globalDateTime, sizeof(DateTime));
        dateTimeLocked = false;
    } else {
        // Fallback: try to get RTC time directly
        rtc.getDateTime(&now);
    }

    String isoTimestamp = dateTimeToISO8601(now);

    // Create JSON payload for all sensor data, each with its own ISO8601 timestamp
    DynamicJsonDocument doc(2048);

    for (size_t i = 0; i < mqttTopicCount; i++) {
        float value = mqttTopics[i].getter();
        
        // Add value and timestamp for consolidated payload
        JsonObject sensorObj = doc["sensors"].createNestedObject(mqttTopics[i].topic);
        sensorObj["value"] = value;
        sensorObj["timestamp"] = isoTimestamp;

        // Publish individual topics with JSON payload (matching documented format)
        StaticJsonDocument<128> individualDoc;
        individualDoc["value"] = value;
        individualDoc["online"] = true; // System status sensors are always online
        individualDoc["timestamp"] = isoTimestamp;
        
        char payload[128];
        serializeJson(individualDoc, payload, sizeof(payload));
        mqttClient.publish(mqttTopics[i].topic, payload);
    }

    // Publish consolidated sensor data
    String jsonString;
    serializeJson(doc, jsonString);
    mqttClient.publish("orcs/system/sensors", jsonString.c_str());

    log(LOG_INFO, false, "Published MQTT sensor data with ISO8601 timestamps\n");
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
        char fullTopic[128];
        snprintf(fullTopic, sizeof(fullTopic), "%s/%d", it->second, msg.objId);

        // 3. Get a timestamp
        String timestamp = getISO8601Timestamp();
        if (timestamp.length() == 0) {
            log(LOG_WARNING, true, "MQTT: Could not get timestamp for publishing.\n");
            return; // Can't publish without a timestamp
        }

        // 4. Create JSON payload based on message type
        StaticJsonDocument<256> doc;
        doc["timestamp"] = timestamp;

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
            log(LOG_DEBUG, false, "MQTT Published [%s]: %s\n", fullTopic, payload);
        } else {
            log(LOG_WARNING, true, "MQTT publish failed for topic: %s\n", fullTopic);
        }
    }

