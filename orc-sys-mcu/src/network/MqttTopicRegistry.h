// src/network/MqttTopicRegistry.h
#pragma once

#include "IPCDataStructs.h"
#include <map>

// A map to associate IPC message IDs with their corresponding MQTT topic strings.
// The object ID from the IPC message will be appended to the topic.
// e.g., for MSG_TEMPERATURE_SENSOR with objId 0, the topic will be "orcs/sensors/temperature/0"
const std::map<MessageTypes, const char*> MqttTopicRegistry = {
    {MSG_POWER_SENSOR, "orcs/sensors/power"},
    {MSG_TEMPERATURE_SENSOR, "orcs/sensors/temperature"},
    {MSG_PH_SENSOR, "orcs/sensors/ph"},
    {MSG_DO_SENSOR, "orcs/sensors/do"},
    {MSG_OD_SENSOR, "orcs/sensors/od"},
    {MSG_GAS_FLOW_SENSOR, "orcs/sensors/gasflow"},
    {MSG_PRESSURE_SENSOR, "orcs/sensors/pressure"},
    {MSG_STIRRER_SPEED_SENSOR, "orcs/sensors/stirrer"},
    {MSG_WEIGHT_SENSOR, "orcs/sensors/weight"}
};
