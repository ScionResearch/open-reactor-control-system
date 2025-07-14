#pragma once

#include "../sys_init.h"
#include <PubSubClient.h>

#define MQTT_PUBLISH_INTERVAL 10000 // Publish data every 10 seconds
#define MQTT_RECONNECT_INTERVAL 5000 // Attempt to reconnect every 5 seconds

/**
 * @brief Initializes the MQTT client with server details from config.
 */
void init_mqttManager();

/**
 * @brief Manages the MQTT connection and periodic data publishing.
 *        Should be called repeatedly in the network loop (Core 0).
 */
void manageMqtt();

/**
 * @brief Publishes sensor data to the MQTT broker.
 */
void mqttPublishSensorData();
