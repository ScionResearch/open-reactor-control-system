#include "drv_modbus.h"

ModbusDriver_t modbusDriver[4];

bool modbus_init(void) {
    HardwareSerial *serial[4] = {&Serial2, &Serial3, &Serial4, &Serial5};
    int pins[4] = {-1, -1, PIN_RS485_DE_1, PIN_RS485_DE_2};
    for (int i = 0; i < 4; i++) {
        modbusDriver[i].serial = serial[i];
        modbusDriver[i].baud = 9600;
        modbusDriver[i].stopBits = 1;
        modbusDriver[i].parity = 0;
        modbusDriver[i].dataBits = 8;
        modbusDriver[i].dePin = pins[i];
        modbusDriver[i].newMessage = false;
        modbusDriver[i].message[0] = '\0';
    }

    for (int i = 0; i < 4; i++) {
        uint16_t config = modbus_getSerialConfig(modbusDriver[i].stopBits, modbusDriver[i].parity, modbusDriver[i].dataBits);
        if (!modbusDriver[i].modbus.begin(modbusDriver[i].serial, modbusDriver[i].baud, config, modbusDriver[i].dePin)) {
            Serial.printf("Failed to initialize Modbus driver %d\n", i);
            return false;
        } else {
            Serial.printf("Modbus driver %d initialized\n", i);
        }
    }
    return true;
}

void modbus_manage(void) {
    for (int i = 0; i < 4; i++) {
        modbusDriver[i].modbus.manage();
    }
}

// Returns Serial configuration value
uint16_t modbus_getSerialConfig(float stopBits, uint8_t parity, uint8_t dataBits) {
    uint16_t config = 0;
    if (parity == 0) config = HARDSER_PARITY_NONE;
    else if (parity == 1) config = HARDSER_PARITY_ODD;
    else if (parity == 2) config = HARDSER_PARITY_EVEN;
    else return 0;

    if (stopBits == 1) config |= HARDSER_STOP_BIT_1;
    else if (stopBits == 1.5) config |= HARDSER_STOP_BIT_1_5;
    else if (stopBits == 2) config |= HARDSER_STOP_BIT_2;
    else return 0;

    if (dataBits == 5) config |= HARDSER_DATA_5;
    else if (dataBits == 6) config |= HARDSER_DATA_6;
    else if (dataBits == 7) config |= HARDSER_DATA_7;
    else if (dataBits == 8) config |= HARDSER_DATA_8;
    else return 0;

    return config;
}