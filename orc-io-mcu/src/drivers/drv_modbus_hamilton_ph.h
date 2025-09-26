#pragma once

#include "sys_init.h"

struct ModbusHamiltonPH_t {
    ModbusDriver_t modbus;
    uint8_t slaveID;
    PhSensor_t phSensor;
    TemperatureSensor_t temperatureSensor;
};

bool init_modbusHamiltonPHDriver(ModbusHamiltonPH_t probe);
void modbusHamiltonPH_manage(ModbusHamiltonPH_t probe);