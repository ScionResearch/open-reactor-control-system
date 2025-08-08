
#include "ipcManager.h"
#include "network/mqttManager.h" // For publishSensorData
#include "statusManager.h" // For status struct

// Inter-processor communication
void init_ipcManager(void) {
  Serial1.setRX(PIN_SI_RX);
  Serial1.setTX(PIN_SI_TX);
  ipc.begin(19200);
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



/**
 * @brief Generic handler that forwards any valid sensor message to MQTT and updates status.
 */
void processSensorData(const Message& msg) {
  // 1. Forward to MQTT
  publishSensorData(msg);

  // 2. Update the global status struct
  if (statusLocked) return;
  statusLocked = true;

  switch (msg.msgId) {
    case MSG_POWER_SENSOR: {
      memcpy(&status.powerSensor, msg.data, sizeof(PowerSensor));
      break;
    }
    case MSG_TEMPERATURE_SENSOR: {
      memcpy(&status.temperatureSensor, msg.data, sizeof(TemperatureSensor));
      break;
    }
    case MSG_PH_SENSOR: {
      memcpy(&status.phSensor, msg.data, sizeof(PHSensor));
      break;
    }
    case MSG_DO_SENSOR: {
      memcpy(&status.doSensor, msg.data, sizeof(DissolvedOxygenSensor));
      break;
    }
    case MSG_OD_SENSOR: {
      memcpy(&status.odSensor, msg.data, sizeof(OpticalDensitySensor));
      break;
    }
    case MSG_GAS_FLOW_SENSOR: {
      memcpy(&status.gasFlowSensor, msg.data, sizeof(GasFlowSensor));
      break;
    }
    case MSG_PRESSURE_SENSOR: {
      memcpy(&status.pressureSensor, msg.data, sizeof(PressureSensor));
      break;
    }
    case MSG_STIRRER_SPEED_SENSOR: {
      memcpy(&status.stirrerSpeedSensor, msg.data, sizeof(StirrerSpeedSensor));
      break;
    }
    case MSG_WEIGHT_SENSOR: {
      memcpy(&status.weightSensor, msg.data, sizeof(WeightSensor));
      break;
    }
    default:
      break; // No status update for unknown types
  }
  status.updated = true;
  statusLocked = false;
}

/**
 * @brief Registers callback functions for all known sensor IPC messages.
 * Each callback will forward the message to MQTT and update status.
 */
void registerIpcCallbacks(void) {
  log(LOG_INFO, false, "Registering IPC callbacks for MQTT and Status...\n");

  // Register a single generic handler for all sensor types
  ipc.registerCallback(MSG_POWER_SENSOR, processSensorData);
  ipc.registerCallback(MSG_TEMPERATURE_SENSOR, processSensorData);
  ipc.registerCallback(MSG_PH_SENSOR, processSensorData);
  ipc.registerCallback(MSG_DO_SENSOR, processSensorData);
  ipc.registerCallback(MSG_OD_SENSOR, processSensorData);
  ipc.registerCallback(MSG_GAS_FLOW_SENSOR, processSensorData);
  ipc.registerCallback(MSG_PRESSURE_SENSOR, processSensorData);
  ipc.registerCallback(MSG_STIRRER_SPEED_SENSOR, processSensorData);
  ipc.registerCallback(MSG_WEIGHT_SENSOR, processSensorData);

  log(LOG_INFO, false, "IPC callbacks registered.\n");
}