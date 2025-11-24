// src/network/MqttTopicRegistry.h
#pragma once

#include "IPCDataStructs.h"
#include <map>

/**
 * @brief Associates IPC message types with relative MQTT topic paths.
 *
 * Full topic becomes: <devicePrefix>/<mappedPath>/<objId>
 * - devicePrefix defaults to "orcs/dev/<MAC>" unless overridden via config
 * - mappedPath examples: "sensors/temperature" (relative path, no leading slash)
 */
const std::map<MessageTypes, const char*> MqttTopicRegistry = {
     {MSG_POWER_SENSOR, "sensors/power"},
     {MSG_TEMPERATURE_SENSOR, "sensors/temperature"},
     {MSG_PH_SENSOR, "sensors/ph"},
     {MSG_DO_SENSOR, "sensors/do"},
     {MSG_OD_SENSOR, "sensors/od"},
     {MSG_GAS_FLOW_SENSOR, "sensors/gasflow"},
     {MSG_PRESSURE_SENSOR, "sensors/pressure"},
     {MSG_STIRRER_SPEED_SENSOR, "sensors/stirrer"},
     {MSG_WEIGHT_SENSOR, "sensors/weight"}
};
