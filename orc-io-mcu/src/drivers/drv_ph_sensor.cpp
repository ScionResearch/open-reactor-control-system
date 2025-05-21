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
#define NUM_REGISTERS_PER_BLOCK 2  // Number of registers for a float value
#define FLOAT_VALUE_OFFSET 0        // Offset within the block for the float value (assuming first 2 registers)

// Initialization function - Accepts the shared Modbus master node
bool ph_sensor_init(ModbusRTUMaster* masterNode, HardwareSerial* port, long baud, int8_t rtsPin, uint32_t readInterval) {
    if (!masterNode) {
        // Cannot initialize without a valid master node
        phSensorDriver.fault = true;
        phSensorDriver.newMessage = true;
        strcpy(phSensorDriver.message, "Invalid Modbus master provided");
        return false;
    }

    phSensorDriver.node = masterNode; // Use the provided shared master node
    phSensorDriver.device = &phSensorDevice; // Link driver to the data object
    phSensorDriver.serial_port = port; // Store for reference/debugging if needed
    phSensorDriver.baud_rate = baud;   // Store for reference/debugging if needed
    phSensorDriver.rts_pin = rtsPin;   // Store for reference/debugging if needed
    phSensorDriver.read_interval_ms = readInterval;
    phSensorDriver.ready = false;
    phSensorDriver.fault = false;
    phSensorDriver.newMessage = false;
    phSensorDriver.last_read_time = 0;
    phSensorDriver.message[0] = '\0'; // Initialize message buffer

    // --- Configure the device defaults ---
    phSensorDevice.modbusAddress = PH_SENSOR_DEFAULT_ADDRESS; // Set default address
    phSensorDevice.enabled = true; // Enable by default
    phSensorDevice.fault = false;
    phSensorDevice.newMessage = false;
    phSensorDevice.message[0] = '\0';

    // --- Modbus Master Initialization is now done externally ---
    // No need to: new ModbusRTUMaster(...)
    // No need to: node->useRts(...)
    // No need to: pinMode/digitalWrite for RTS
    // No need to: serial_port->begin(...)
    // No need to: node->begin()

    phSensorDriver.ready = true;
    phSensorDriver.newMessage = true;
    strcpy(phSensorDriver.message, "pH Sensor driver initialized (shared Modbus)");
    return true;
}

// Helper function to read a block and extract float value
// Returns true on success, false on failure
bool read_sensor_float_block(uint16_t startReg, float &value) {
    // Ensure the node pointer is valid before using
    if (!phSensorDriver.node) {
        phSensorDevice.fault = true;
        phSensorDevice.newMessage = true;
        strcpy(phSensorDevice.message, "Modbus master node is null");
        return false;
    }

    uint16_t buffer[NUM_REGISTERS_PER_BLOCK]; // Buffer to hold the read registers

    // Read the required number of registers (typically 2 for a float)
    if (phSensorDriver.node->readHoldingRegisters(
            phSensorDevice.modbusAddress,
            startReg,
            buffer, // Pass the buffer
            NUM_REGISTERS_PER_BLOCK // Read only the registers needed for the float
        )) {
        // Use ModbusRTUMasterUtils::bufferToFloatBE or LE depending on sensor byte order
        // Assuming Big-Endian (check sensor manual)
        value = ModbusRTUMasterUtils::bufferToFloatBE(buffer);

        // Clear communication fault if present
        if (phSensorDevice.fault) {
            phSensorDevice.fault = false;
            phSensorDevice.newMessage = true;
            strcpy(phSensorDevice.message, "Communication restored");
        }
        return true;
    } else {
        phSensorDevice.fault = true;
        phSensorDevice.newMessage = true;
        snprintf(phSensorDevice.message, sizeof(phSensorDevice.message),
                 "Modbus read failed, addr: %d, reg: 0x%04X, err: 0x%02X",
                 phSensorDevice.modbusAddress, startReg, phSensorDriver.node->getExceptionResponse());
        // Consider adding timeout check: phSensorDriver.node->getTimeoutFlag()
        return false;
    }
}

// Update function (call this periodically from the main loop)
bool ph_sensor_update(void) {
    if (!phSensorDriver.ready || phSensorDriver.fault || !phSensorDevice.enabled) {
        return false; // Don't update if not ready, driver faulted, or device disabled
    }

    uint32_t current_time = millis();
    if (current_time - phSensorDriver.last_read_time >= phSensorDriver.read_interval_ms) {
        phSensorDriver.last_read_time = current_time;

        bool read_ok = true;

        // Read pH value
        if (!read_sensor_float_block(REG_PH_BLOCK_START + FLOAT_VALUE_OFFSET, phSensorDevice.pH)) {
            read_ok = false;
            // Error message already set in read_sensor_float_block
        }

        // Read Temperature value (only proceed if pH read was okay or if independent reads are desired)
        // If one read fails, should we attempt the next? Depends on requirements.
        // Assuming we attempt both reads regardless.
        if (!read_sensor_float_block(REG_TEMP_BLOCK_START + FLOAT_VALUE_OFFSET, phSensorDevice.temperature)) {
            read_ok = false;
            // Error message already set in read_sensor_float_block
            // Note: This might overwrite the pH read error message if both fail.
        }

        return read_ok;
    }
    return true; // No update needed this cycle
}
