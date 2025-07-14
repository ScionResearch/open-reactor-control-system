#include "mqttManager.h"

// Use the base WiFiClient for PubSubClient, even with Ethernet
WiFiClient ethClient; 
PubSubClient mqttClient(ethClient);

unsigned long lastMqttReconnectAttempt = 0;
unsigned long lastMqttPublishTime = 0;

/**
 * @brief Attempts to reconnect to the MQTT broker.
 */
void reconnect() {
    if (strlen(networkConfig.mqttBroker) == 0) {
        return; // Don't attempt to connect if broker is not configured
    }

    log(LOG_INFO, true, "Attempting MQTT connection to %s...", networkConfig.mqttBroker);
    
    // Create a unique client ID from the MAC address
    char clientId[24];
    uint8_t mac[6];
    eth.macAddress(mac);
    snprintf(clientId, sizeof(clientId), "ORCS-%02X%02X%02X", mac[3], mac[4], mac[5]);

    // Attempt to connect
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

/**
 * @brief Publishes the main PSU voltage to the MQTT broker.
 */
void publishVoltageData() {
    if (!mqttClient.connected()) {
        return;
    }

    // Create JSON payload
    StaticJsonDocument<128> doc;
    doc["value"] = status.Vpsu;

    // Add timestamp
    DateTime now;
    if (getGlobalDateTime(now, 100)) {
        char timeStr[32];
        snprintf(timeStr, sizeof(timeStr), "%04d-%02d-%02dT%02d:%02d:%02dZ", 
                 now.year, now.month, now.day, now.hour, now.minute, now.second);
        doc["timestamp"] = timeStr;
    }

    char payload[128];
    serializeJson(doc, payload);

    // Define topic
    const char* topic = "orcs/system/power/voltage";

    // Publish
    if (mqttClient.publish(topic, payload)) {
        log(LOG_DEBUG, false, "MQTT Published [%s]: %s\n", topic, payload);
    } else {
        log(LOG_WARNING, true, "MQTT publish failed for topic: %s\n", topic);
    }
}

void init_mqttManager() {
    if (strlen(networkConfig.mqttBroker) > 0) {
        mqttClient.setServer(networkConfig.mqttBroker, networkConfig.mqttPort);
        log(LOG_INFO, false, "MQTT Manager initialized for broker %s:%d\n", networkConfig.mqttBroker, networkConfig.mqttPort);
    } else {
        log(LOG_INFO, false, "MQTT broker not configured. MQTT Manager will remain idle.\n");
    }
}

void manageMqtt() {
    if (!ethernetConnected || strlen(networkConfig.mqttBroker) == 0) {
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
        mqttClient.loop();

        // Check if it's time to publish data
        if (millis() - lastMqttPublishTime > MQTT_PUBLISH_INTERVAL) {
            lastMqttPublishTime = millis();
            publishVoltageData();
        }
    }
}

void mqttPublishSensorData() {
    publishVoltageData();
}
