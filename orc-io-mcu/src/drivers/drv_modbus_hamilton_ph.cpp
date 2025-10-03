#include "drv_modbus_hamilton_ph.h"

ModbusHamiltonPH_t modbusHamiltonPHprobe;

void init_modbusHamiltonPHDriver(ModbusDriver_t *modbusDriver, uint8_t slaveID) {
    modbusHamiltonPHprobe.modbusDriver = modbusDriver;
    modbusHamiltonPHprobe.slaveID = slaveID;
}

void phResponseHandler(bool valid, uint16_t *data) {
  if (!valid) {
    //Serial.println("Invalid ph probe data.");
    return;
  }
  float pH;
  memcpy(&pH, &data[2], sizeof(float));
  //Serial.printf("PH probe data: pH: %0.2f\n", pH);
  modbusHamiltonPHprobe.phSensor.ph = pH;
}

void temperatureResponseHandler(bool valid, uint16_t *data) {
  if (!valid) {
    //Serial.println("Invalid ph probe temperature data.");
    return;
  }
  float temperature;
  memcpy(&temperature, &data[2], sizeof(float));
  //Serial.printf("PH probe temperature: %0.2f\n", temperature);
  modbusHamiltonPHprobe.temperatureSensor.temperature = temperature;
}

void modbusHamiltonPH_manage() {
    //Serial.println("Queuing pH probe modbus requests");
    uint8_t functionCode = 3;
    uint16_t address = 2089;
    static uint16_t data[10];
    if (!modbusHamiltonPHprobe.modbusDriver->modbus.pushRequest(modbusHamiltonPHprobe.slaveID, functionCode, address, data, 10, phResponseHandler)) {
        //Serial.println("ERROR - queue full");
        return;
    }
    address = 2409;
    if (!modbusHamiltonPHprobe.modbusDriver->modbus.pushRequest(modbusHamiltonPHprobe.slaveID, functionCode, address, data, 10, temperatureResponseHandler)) {
        //Serial.println("ERROR - queue full");
        return;
    }

    //Serial.printf("Current queue size: %d\n", modbusHamiltonPHprobe.modbusDriver->modbus.getQueueCount());
}