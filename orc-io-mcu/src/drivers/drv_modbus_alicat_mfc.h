#pragma once

#include "sys_init.h"

// Forward declarations to avoid circular includes
struct ModbusDriver_t;

struct ModbusAlicatMFC_t {
    ModbusDriver_t *modbusDriver;
    uint8_t slaveID;
    FlowSensor_t flowSensor;
    PressureSensor_t pressureSensor;
    float setpoint;
};

extern ModbusAlicatMFC_t modbusAlicatMFCprobe;

void init_modbusAlicatMFCDriver(ModbusDriver_t *modbusDriver, uint8_t slaveID);
void modbusAlicatMFC_manage();
void modbusAlicatMFC_writeSP(float setpoint);