/**
 * @file apiMqtt.cpp
 * @brief MQTT configuration API implementation
 */

#include "apiMqtt.h"
#include "../network/networkManager.h"
#include "../mqtt/mqttManager.h"
#include <ArduinoJson.h>

void setupMqttAPI()
{
    server.on("/api/mqtt", HTTP_GET, []() {
        StaticJsonDocument<512> doc;
        doc["mqttEnabled"] = networkConfig.mqttEnabled;
        doc["mqttBroker"] = networkConfig.mqttBroker;
        doc["mqttPort"] = networkConfig.mqttPort;
        doc["mqttUsername"] = networkConfig.mqttUsername;
        doc["mqttPassword"] = ""; // never return stored password
        doc["mqttPublishIntervalMs"] = networkConfig.mqttPublishIntervalMs;
        doc["mqttDevicePrefix"] = networkConfig.mqttDevicePrefix;
        String response;
        serializeJson(doc, response);
        server.send(200, "application/json", response);
    });
    
    server.on("/api/mqtt", HTTP_POST, []() {
        if (!server.hasArg("plain")) {
            server.send(400, "application/json", "{\"error\":\"No data received\"}");
            return;
        }
        StaticJsonDocument<512> doc;
        DeserializationError error = deserializeJson(doc, server.arg("plain"));
        if (error) {
            server.send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
            return;
        }
        
        // Parse enabled flag
        bool enabled = doc["mqttEnabled"] | false;
        const char* broker = doc["mqttBroker"] | "";
        
        // Validate: if enabled, broker address is required
        if (enabled && strlen(broker) == 0) {
            server.send(400, "application/json", "{\"error\":\"MQTT broker address is required when MQTT is enabled\"}");
            return;
        }
        
        networkConfig.mqttEnabled = enabled;
        strlcpy(networkConfig.mqttBroker, broker, sizeof(networkConfig.mqttBroker));
        networkConfig.mqttPort = doc["mqttPort"] | 1883;
        strlcpy(networkConfig.mqttUsername, doc["mqttUsername"] | "", sizeof(networkConfig.mqttUsername));
        const char* newPassword = doc["mqttPassword"] | "";
        if (strlen(newPassword) > 0) {
            strlcpy(networkConfig.mqttPassword, newPassword, sizeof(networkConfig.mqttPassword));
        }
        // Optional fields
        if (doc.containsKey("mqttPublishIntervalMs")) {
            networkConfig.mqttPublishIntervalMs = doc["mqttPublishIntervalMs"];
        }
        if (doc.containsKey("mqttDevicePrefix")) {
            strlcpy(networkConfig.mqttDevicePrefix, doc["mqttDevicePrefix"], sizeof(networkConfig.mqttDevicePrefix));
        }
        saveNetworkConfig();
        // Apply MQTT config immediately and attempt reconnect
        mqttApplyConfigAndReconnect();
        server.send(200, "application/json", "{\"status\":\"success\",\"message\":\"MQTT configuration applied\"}");
    });
    
    // Diagnostics endpoint
    server.on("/api/mqtt/diag", HTTP_GET, []() {
        StaticJsonDocument<256> doc;
        doc["broker"] = networkConfig.mqttBroker;
        doc["port"] = networkConfig.mqttPort;
        doc["connected"] = mqttIsConnected();
        doc["state"] = mqttGetState();
        doc["prefix"] = mqttGetDeviceTopicPrefix();
        String response;
        serializeJson(doc, response);
        server.send(200, "application/json", response);
    });
}
