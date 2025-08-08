#include "drv_modbus.h"

ModbusDriver_t modbusDriver[4];

bool modbus_init(void) {
        // Only Serial and Serial1 are available
        HardwareSerial *serial[2] = {&Serial, &Serial1};
        int pins[2] = {-1, PIN_RS485_DE_1};
        for (int i = 0; i < 2; i++) {
            modbusDriver[i].serial = serial[i];
            modbusDriver[i].baud = 9600;
            modbusDriver[i].stopBits = 1;
            modbusDriver[i].parity = 0;
            modbusDriver[i].dataBits = 8;
            modbusDriver[i].dePin = pins[i];
            modbusDriver[i].newMessage = false;
            modbusDriver[i].message[0] = '\0';
        }

        char msg[64];
        for (int i = 0; i < 2; i++) {
            uint16_t config = modbus_getSerialConfig(modbusDriver[i].stopBits, modbusDriver[i].parity, modbusDriver[i].dataBits);
            if (!modbusDriver[i].modbus.begin(modbusDriver[i].serial, modbusDriver[i].baud, config, modbusDriver[i].dePin)) {
                sprintf(msg, "Failed to initialize Modbus driver %d\n", i);
                Serial.print(msg);
                return false;
            } else {
                sprintf(msg, "Modbus driver %d initialized\n", i);
                Serial.print(msg);
            }
        }
        return true;
}

void modbus_manage(void) {
    for (int i = 0; i < 2; i++) {
        modbusDriver[i].modbus.manage();
    }
}

// Returns Serial configuration value
uint16_t modbus_getSerialConfig(float stopBits, uint8_t parity, uint8_t dataBits) {
    // Use Arduino SAMD serial config macros
    uint16_t config = SERIAL_8N1;
    // Parity
    if (parity == 0) config |= SERIAL_PARITY_NONE;
    else if (parity == 1) config |= SERIAL_PARITY_ODD;
    else if (parity == 2) config |= SERIAL_PARITY_EVEN;
    // Stop bits and data bits are included in SERIAL_8N1
    return config;
}