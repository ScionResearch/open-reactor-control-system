// filepath: c:\Users\vanderwt\OneDrive - Scion\Documents\PROJECTS\Smart Manufacturing\Digital Twin\open-reactor-control-system\orc-io-mcu\src\drivers\drv_ph_sensor.cpp
#include "drv_ph_sensor.h"
#include <ModbusRTUMasterUtils.h> // For float conversion

// Define the global driver and device instances
PhSensorDriver_t phSensorDriver;
PhSensor_t phSensorDevice; // Assuming one global instance for now

// --- Modbus Specific Definitions (Based on Hamilton pH Arc Sensor) ---
// From sensor_modbus_map.md:
// - pH (PMC1) starts at 0-based address 2089 (0x0829), 10 registers block
// - Temperature (PMC6) starts at 0-based address 2409 (0x0969), 10 registers block
// - Default Modbus address needs confirmation from sensor/manual (using 1 as placeholder)
#define PH_SENSOR_DEFAULT_ADDRESS 1 // Placeholder: Default Modbus slave address (Check sensor manual)
#define REG_PH_BLOCK_START 0x0829   // Starting register for pH data block (2089 decimal)
#define REG_TEMP_BLOCK_START 0x0969 // Starting register for Temperature data block (2409 decimal)
#define NUM_REGISTERS_PER_BLOCK 10  // Number of registers per measurement block
#define FLOAT_VALUE_OFFSET 0        // Offset within the block for the float value (assuming first 2 registers)

// Initialization function
bool ph_sensor_init(HardwareSerial* port, long baud, int8_t rtsPin, uint32_t readInterval) {
    phSensorDriver.device = &phSensorDevice; // Link driver to the data object
    phSensorDriver.serial_port = port;
    phSensorDriver.baud_rate = baud;
    phSensorDriver.rts_pin = rtsPin;
    phSensorDriver.read_interval_ms = readInterval;
    phSensorDriver.ready = false;
    phSensorDriver.fault = false;
    phSensorDriver.newMessage = false;
    phSensorDriver.last_read_time = 0;
    // phSensorDriver.message[0] = '\0'; // Initialize message buffer if needed

    // --- Configure the device defaults ---
    phSensorDevice.modbusAddress = PH_SENSOR_DEFAULT_ADDRESS; // Set default address
    phSensorDevice.enabled = true; // Enable by default
    phSensorDevice.fault = false;
    phSensorDevice.newMessage = false;
    // phSensorDevice.message[0] = '\0';

    // --- Initialize Modbus Master --- 
    // IMPORTANT: This creates a NEW master. For shared bus, this needs refactoring.
    // A single ModbusRTUMaster instance should be shared between drivers on the same port.
    phSensorDriver.node = new ModbusRTUMaster(phSensorDriver.serial_port);
    if (!phSensorDriver.node) {
        phSensorDriver.fault = true;
        phSensorDriver.newMessage = true;
        // strcpy(phSensorDriver.message, "Failed to allocate ModbusRTUMaster");
        return false;
    }

    // Configure RTS pin if used for RS485 direction control
    if (phSensorDriver.rts_pin >= 0) {
        phSensorDriver.node->useRts(true, phSensorDriver.rts_pin);
        pinMode(phSensorDriver.rts_pin, OUTPUT);
        digitalWrite(phSensorDriver.rts_pin, LOW); // Default state for RTS
    }

    // Initialize the serial port for Modbus
    // Note: If shared, this might be done elsewhere
    phSensorDriver.serial_port->begin(phSensorDriver.baud_rate);

    // Start the Modbus node
    if (!phSensorDriver.node->begin()) {
        phSensorDriver.fault = true;
        phSensorDriver.newMessage = true;
        // strcpy(phSensorDriver.message, "ModbusRTUMaster begin failed");
        delete phSensorDriver.node; // Clean up allocated memory
        phSensorDriver.node = nullptr;
        return false;
    }

    phSensorDriver.ready = true;
    phSensorDriver.newMessage = true;
    // strcpy(phSensorDriver.message, "pH Sensor Modbus driver initialized (shared bus needs refactor)");
    return true;
}

// Helper function to read a block and extract float value
// Returns true on success, false on failure
bool read_sensor_float_block(uint16_t startReg, float &value) {
    uint8_t result = phSensorDriver.node->readHoldingRegisters(
        phSensorDevice.modbusAddress,
        startReg,
        NUM_REGISTERS_PER_BLOCK
    );

    if (result == ModbusRTUMaster::MODBUS_SUCCESS) {
        // Assuming the float value is in the first two registers of the block
        // Use ModbusRTUMasterUtils::bufferToFloatBE or LE depending on sensor byte order
        // Assuming Big-Endian (check sensor manual)
        value = ModbusRTUMasterUtils::bufferToFloatBE(phSensorDriver.node->getResponseBuffer(FLOAT_VALUE_OFFSET));
        return true;
    } else {
        phSensorDevice.fault = true;
        phSensorDevice.newMessage = true;
        // snprintf(phSensorDevice.message, sizeof(phSensorDevice.message),
        //          "Modbus read failed, addr: %d, reg: 0x%04X, err: 0x%02X",
        //          phSensorDevice.modbusAddress, startReg, result);
        return false;
    }
}

// Update function (call this periodically from the main loop)
bool ph_sensor_update(void) {
    if (!phSensorDriver.ready || phSensorDriver.fault || !phSensorDevice.enabled) {
        return false; // Don't update if not ready, faulted, or disabled
    }

    uint32_t current_time = millis();
    if (current_time - phSensorDriver.last_read_time >= phSensorDriver.read_interval_ms) {
        phSensorDriver.last_read_time = current_time;

        bool read_ok = true;
        float temp_ph = 0.0;
        float temp_temp = 0.0;

        // Read pH value block
        if (!read_sensor_float_block(REG_PH_BLOCK_START, temp_ph)) {
            read_ok = false; // Error message handled in helper function
        }

        // Read Temperature value block (only if pH read was okay or if we want to try anyway)
        if (read_ok) { // Optionally, attempt temp read even if pH failed: if (!read_sensor_float_block(...)) { read_ok = false; }
           if (!read_sensor_float_block(REG_TEMP_BLOCK_START, temp_temp)) {
               read_ok = false; // Error message handled in helper function
           }
        }

        if (read_ok) {
            // --- Process Successful Reads ---
            phSensorDevice.pH = temp_ph;
            phSensorDevice.temperature = temp_temp;

            // Clear any previous fault state related to communication
            if (phSensorDevice.fault) {
                 phSensorDevice.fault = false;
                 phSensorDevice.newMessage = true;
                 // strcpy(phSensorDevice.message, "Communication restored");
            }
        } else {
            // Fault flags and messages are set within read_sensor_float_block on error
            return false; // Indicate update failed
        }
    }
    return true; // Indicate update cycle completed (or wasn't time yet)
}
