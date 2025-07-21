#include "controlManager.h"
#include "../utils/statusManager.h"
#include "../utils/logger.h"
#include "../utils/ipcManager.h"

void init_controlManager() {
    log(LOG_INFO, false, "Control Manager initialized.\n");
}

/**
 * @brief Updates temperature control settings and sends an IPC message.
 * @param config A JsonObject containing "setpoint" and "enabled".
 * @return True on success, false on failure.
 */
bool updateTemperatureControl(const JsonObject& config) {
    if (!config.containsKey("setpoint") || !config.containsKey("enabled")) {
        log(LOG_WARNING, true, "API: Invalid temperature control payload.\n");
        return false;
    }

    float setpoint = config["setpoint"];
    bool enabled = config["enabled"];

    // 1. Prepare IPC message
    Message msg;
    msg.msgId = MSG_TEMPERATURE_CONTROL;
    msg.objId = 0; // Assuming one temperature controller
    
    TemperatureControl data;
    data.sp_celcius = setpoint;
    data.enabled = enabled;
    // NOTE: PID values are not currently exposed to the UI, so we use defaults or existing values.
    // For now, we'll just send the setpoint and enabled status.
    // data.kp = status.temperatureControl.kp; // Example of using existing value
    // data.ki = status.temperatureControl.ki;
    // data.kd = status.temperatureControl.kd;

    msg.dataLength = sizeof(data);
    memcpy(msg.data, &data, msg.dataLength);
    
    // 2. Send IPC message
    if (ipc.sendMessage(msg)) {
        log(LOG_INFO, true, "IPC: Sent TemperatureControl update (Setpoint: %.2f, Enabled: %d)\n", setpoint, enabled);
        
        // 3. Update global status struct
        if (!statusLocked) {
            statusLocked = true;
            status.temperatureControl.sp_celcius = setpoint;
            status.temperatureControl.enabled = enabled;
            status.updated = true;
            statusLocked = false;
        }
        return true;
    } else {
        log(LOG_ERROR, true, "IPC: Failed to send TemperatureControl update.\n");
        return false;
    }
}

/**
 * @brief Updates pH control settings and sends an IPC message.
 * @param config A JsonObject containing "setpoint" and "enabled".
 * @return True on success, false on failure.
 */
bool updatePhControl(const JsonObject& config) {
    if (!config.containsKey("setpoint") || !config.containsKey("enabled")) {
        log(LOG_WARNING, true, "API: Invalid pH control payload.\n");
        return false;
    }

    float setpoint = config["setpoint"];
    bool enabled = config["enabled"];

    Message msg;
    msg.msgId = MSG_PH_CONTROL;
    msg.objId = 0;

    PHControl data;
    data.sp_pH = setpoint;
    data.enabled = enabled;
    // Other PHControl params are not set from UI yet.

    msg.dataLength = sizeof(data);
    memcpy(msg.data, &data, msg.dataLength);

    if (ipc.sendMessage(msg)) {
        log(LOG_INFO, true, "IPC: Sent PHControl update (Setpoint: %.2f, Enabled: %d)\n", setpoint, enabled);
        if (!statusLocked) {
            statusLocked = true;
            status.phControl.sp_pH = setpoint;
            status.phControl.enabled = enabled;
            status.updated = true;
            statusLocked = false;
        }
        return true;
    } else {
        log(LOG_ERROR, true, "IPC: Failed to send PHControl update.\n");
        return false;
    }
}
