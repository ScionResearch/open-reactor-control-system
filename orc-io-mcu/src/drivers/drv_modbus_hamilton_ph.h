#pragma once

#include "sys_init.h"

// Forward declarations to avoid circular includes
struct ModbusDriver_t;

struct ModbusHamiltonPH_t {
    ModbusDriver_t *modbusDriver;
    uint8_t slaveID;
    PhSensor_t phSensor;
    TemperatureSensor_t temperatureSensor;
};

extern ModbusHamiltonPH_t modbusHamiltonPHprobe;

void init_modbusHamiltonPHDriver(ModbusDriver_t *modbusDriver, uint8_t slaveID);
void modbusHamiltonPH_manage();