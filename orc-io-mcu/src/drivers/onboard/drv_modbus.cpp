#include "drv_modbus.h"

ModbusDriver_t modbusDriver[4];
SerialCom_t modbusPort[4];

bool modbus_init(void) {
    HardwareSerial *serial[4] = {&Serial2, &Serial3, &Serial4, &Serial5};
    int8_t pins[4] = {-1, -1, PIN_RS485_DE_1, PIN_RS485_DE_2};
    
    for (int i = 0; i < 4; i++) {
        // Initialize port object (user-accessible)
        modbusPort[i].portNumber = i + 1;
        modbusPort[i].baudRate = 9600;      // Default baud
        modbusPort[i].dataBits = 8;         // Default 8 data bits
        modbusPort[i].stopBits = 1;         // Default 1 stop bit
        modbusPort[i].parity = 0;           // Default no parity
        modbusPort[i].enabled = true;
        modbusPort[i].slaveCount = 0;       // No devices yet
        modbusPort[i].fault = false;
        modbusPort[i].newMessage = false;
        modbusPort[i].message[0] = '\0';
        
        // Initialize driver (hardware-specific)
        modbusDriver[i].portObj = &modbusPort[i];
        modbusDriver[i].serial = serial[i];
        modbusDriver[i].dePin = pins[i];
        modbusDriver[i].configChanged = false;
        
        // Add to object index (indices 33-36, shifted from 37-40)
        ObjectType portType = (i < 2) ? OBJ_T_SERIAL_RS232_PORT : OBJ_T_SERIAL_RS485_PORT;
        objIndex[33 + i].type = portType;
        objIndex[33 + i].obj = &modbusPort[i];
        sprintf(objIndex[33 + i].name, "Modbus Port %d (%s)", 
                i + 1, (i < 2) ? "RS-232" : "RS-485");
        objIndex[33 + i].valid = true;
    }

    // Initialize hardware
    for (int i = 0; i < 4; i++) {
        uint16_t config = modbus_getSerialConfig(modbusPort[i].stopBits, 
                                                  modbusPort[i].parity, 
                                                  modbusPort[i].dataBits);
        if (!modbusDriver[i].modbus.begin(modbusDriver[i].serial, 
                                          modbusPort[i].baudRate, 
                                          config, 
                                          modbusDriver[i].dePin)) {
            Serial.printf("Failed to initialize Modbus driver %d\n", i + 1);
            modbusPort[i].fault = true;
            modbusPort[i].newMessage = true;
            sprintf(modbusPort[i].message, "Failed to init Modbus port %d", i + 1);
            return false;
        } else {
            Serial.printf("Modbus driver %d initialized\n", i + 1);
        }
    }
    return true;
}

void modbus_manage(void) {
    for (int i = 0; i < 4; i++) {
        // Check if user has changed port configuration via IPC
        if (modbusDriver[i].configChanged) {
            uint16_t config = modbus_getSerialConfig(modbusPort[i].stopBits, 
                                                      modbusPort[i].parity, 
                                                      modbusPort[i].dataBits);
            modbusDriver[i].modbus.setSerialConfig(modbusPort[i].baudRate, config);
            modbusDriver[i].configChanged = false;
            
            modbusPort[i].newMessage = true;
            sprintf(modbusPort[i].message, "Port config updated: %lu baud, %dN%d", 
                    modbusPort[i].baudRate, modbusPort[i].dataBits, 
                    (int)modbusPort[i].stopBits);
        }
        
        // Manage Modbus protocol
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