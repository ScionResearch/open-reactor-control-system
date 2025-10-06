#pragma once

#include "../sys_init.h"
#include <PubSubClient.h>

// Default intervals (can be overridden via NetworkConfig.mqttPublishIntervalMs)
#ifndef MQTT_PUBLISH_INTERVAL
#define MQTT_PUBLISH_INTERVAL 10000 // Publish data every 10 seconds
#endif

#ifndef MQTT_RECONNECT_INTERVAL
#define MQTT_RECONNECT_INTERVAL 5000 // Attempt to reconnect every 5 seconds
#endif

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

/**
 * @brief Publishes a single sensor reading received from the I/O controller (legacy).
 * @param msg The IPC message containing the sensor data.
 */
void publishSensorData(const Message& msg);

/**
 * @brief Publishes sensor data received via new IPC protocol.
 * @param data Pointer to IPC_SensorData_t structure
 */
void publishSensorDataIPC(const IPC_SensorData_t* data);

/**
 * @brief Apply current NetworkConfig MQTT settings to the client and trigger a reconnect attempt.
 *        Use after updating config via the HTTP API so changes take effect without a reboot.
 */
void mqttApplyConfigAndReconnect();

// Diagnostics
bool mqttIsConnected();
int mqttGetState();
const char* mqttGetDeviceTopicPrefix();
