#pragma once

#include "sys_init.h"
#include "modbus-rtu-master.h"

struct ModbusDriver_t {
    ModbusRTUMaster modbus;
    HardwareSerial *serial;
    uint32_t baud;
    uint8_t stopBits;
    float parity;
    uint8_t dataBits;
    uint8_t dePin;
    bool newMessage;
    char message[100];
};

extern ModbusDriver_t modbusDriver[4];

bool modbus_init(void);
void modbus_manage(void);
uint16_t modbus_getSerialConfig(float stopBits, uint8_t parity, uint8_t dataBits);