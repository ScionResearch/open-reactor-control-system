#include "drv_modbus_hamilton_ph.h"

ModbusHamiltonPH_t modbusHamiltonPHprobe;

void init_modbusHamiltonPHDriver(ModbusDriver_t *modbusDriver, uint8_t slaveID) {
    modbusHamiltonPHprobe.modbusDriver = modbusDriver;
    modbusHamiltonPHprobe.slaveID = slaveID;
}

void phResponseHandler(bool valid, uint16_t *data) {
  if (!valid) {
    modbusHamiltonPHprobe.phSensor.fault = true;
    snprintf(modbusHamiltonPHprobe.phSensor.message, sizeof(modbusHamiltonPHprobe.phSensor.message), "Invalid pH data from pH probe.");
    modbusHamiltonPHprobe.phSensor.newMessage = true;
    return;
  }
  float pH;
  memcpy(&pH, &data[2], sizeof(float));
  modbusHamiltonPHprobe.phSensor.ph = pH;
}

void temperatureResponseHandler(bool valid, uint16_t *data) {
  if (!valid) {
    modbusHamiltonPHprobe.temperatureSensor.fault = true;
    snprintf(modbusHamiltonPHprobe.temperatureSensor.message, sizeof(modbusHamiltonPHprobe.temperatureSensor.message), "Invalid temperature data from pH probe.");
    modbusHamiltonPHprobe.temperatureSensor.newMessage = true;
    return;
  }
  float temperature;
  memcpy(&temperature, &data[2], sizeof(float));
  modbusHamiltonPHprobe.temperatureSensor.temperature = temperature;
}

void modbusHamiltonPH_manage() {
    uint8_t functionCode = 3;
    uint16_t address = 2089;
    static uint16_t data[10];
    if (!modbusHamiltonPHprobe.modbusDriver->modbus.pushRequest(modbusHamiltonPHprobe.slaveID, functionCode, address, data, 10, phResponseHandler)) {
        return;
    }
    address = 2409;
    if (!modbusHamiltonPHprobe.modbusDriver->modbus.pushRequest(modbusHamiltonPHprobe.slaveID, functionCode, address, data, 10, temperatureResponseHandler)) {
        return;
    }
}