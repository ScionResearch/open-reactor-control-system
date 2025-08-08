// src/network/MqttTopicRegistry.h
#pragma once

#include "IPCDataStructs.h"
#include <map>

/**
 * @brief Associates IPC message types with relative MQTT topic paths.
 *
 * Full topic becomes: <devicePrefix>/<mappedPath>/<objId>
 * - devicePrefix defaults to "orcs/dev/<MAC>" unless overridden via config
 * - mappedPath examples: "orcs/sensors/temperature" (relative path)
 */
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
