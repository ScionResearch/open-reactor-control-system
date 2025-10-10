#pragma once

#include "sys_init.h"
#include "modbus-rtu-master.h"

struct ModbusDriver_t {
    ModbusRTUMaster modbus;     // Modbus RTU master instance
    SerialCom_t *portObj;       // Pointer to port object (user-accessible)
    HardwareSerial *serial;     // Hardware serial instance (fixed to PCB)
    int8_t dePin;               // DE/RE pin for RS-485 (-1 for RS-232)
    bool configChanged = false; // Flag to trigger port reconfiguration
};

extern ModbusDriver_t modbusDriver[4];
extern SerialCom_t modbusPort[4];

bool modbus_init(void);
void modbus_manage(void);
uint16_t modbus_getSerialConfig(float stopBits, uint8_t parity, uint8_t dataBits);