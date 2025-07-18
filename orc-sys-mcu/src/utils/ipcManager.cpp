#include "ipcManager.h"

#include "network/mqttManager.h" // For publishSensorData

// Inter-processor communication
void init_ipcManager(void) {
  Serial1.setRX(PIN_SI_RX);
  Serial1.setTX(PIN_SI_TX);
  ipc.begin(115200);
  // Add in handshaking checks here...
  log(LOG_INFO, false, "Inter-processor communication setup complete\n");
  if (statusLocked) return;
  statusLocked = true;
  status.ipcOK = true;
  status.updated = true;
  statusLocked = false;
}

void manageIPC(void) {
  // Check for incoming messages
  ipc.update();

  // Set status flag
  /*if (statusLocked) return;
  statusLocked = true;
  status.ipcOK = true;
  status.updated = true;
  statusLocked = false;*/
}


// Generic handler that forwards any valid sensor message to the MQTT manager
void forwardSensorDataToMqtt(const Message& msg) {
  publishSensorData(msg);
}

/**
 * @brief Registers callback functions for all known sensor IPC messages.
 * Each callback will forward the message to the MQTT publishing system.
 */
void registerIpcCallbacks(void) {
  log(LOG_INFO, false, "Registering IPC callbacks for MQTT...\n");

  // Register a single generic handler for all sensor types
  ipc.registerCallback(MSG_TEMPERATURE_SENSOR, forwardSensorDataToMqtt);
  ipc.registerCallback(MSG_PH_SENSOR, forwardSensorDataToMqtt);
  ipc.registerCallback(MSG_DO_SENSOR, forwardSensorDataToMqtt);
  ipc.registerCallback(MSG_OD_SENSOR, forwardSensorDataToMqtt);
  ipc.registerCallback(MSG_GAS_FLOW_SENSOR, forwardSensorDataToMqtt);
  ipc.registerCallback(MSG_PRESSURE_SENSOR, forwardSensorDataToMqtt);
  ipc.registerCallback(MSG_STIRRER_SPEED_SENSOR, forwardSensorDataToMqtt);
  ipc.registerCallback(MSG_WEIGHT_SENSOR, forwardSensorDataToMqtt);
  // Add other sensors here as they are implemented on the I/O controller

  log(LOG_INFO, false, "IPC callbacks registered.\n");
}