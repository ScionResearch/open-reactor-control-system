// filepath: c:\Users\vanderwt\OneDrive - Scion\Documents\PROJECTS\Smart Manufacturing\Digital Twin\open-reactor-control-system\orc-io-mcu\src\sys_init.cpp
#include "sys_init.h"

// Define the global Modbus master pointer
ModbusRTUMaster* modbusMaster1 = nullptr;

// Initialize the shared Modbus master for RS485 bus 1
bool modbus_init(HardwareSerial* port, long baud, int8_t rtsPin) {
    if (modbusMaster1 != nullptr) {
        // Already initialized
        return true;
    }

    modbusMaster1 = new ModbusRTUMaster(port);
    if (!modbusMaster1) {
        Serial.println("Failed to allocate ModbusRTUMaster");
        return false;
    }

    // Configure RTS pin if used
    if (rtsPin >= 0) {
        modbusMaster1->useRts(true, rtsPin);
        pinMode(rtsPin, OUTPUT);
        digitalWrite(rtsPin, LOW); // Default state for RTS
    }

    // Initialize the serial port (only once)
    port->begin(baud);
    // Add serial configuration if needed (e.g., SERIAL_8N1)

    // Start the Modbus node (only once)
    if (!modbusMaster1->begin()) { // Note: begin() here doesn't take baud/config, it's set in the constructor/port->begin
        Serial.println("ModbusRTUMaster begin failed");
        delete modbusMaster1;
        modbusMaster1 = nullptr;
        return false;
    }

    Serial.println("Shared Modbus Master (RS485-1) initialized.");
    return true;
}
