#include "drv_do_sensor.h"
#include <ModbusRTUMasterUtils.h> // For float conversion

// Define the global driver and device instances
DoSensorDriver_t doSensorDriver;
DissolvedOxygenSensor_t doSensorDevice; // Assuming one global instance for now

// --- Modbus Specific Definitions (Based on Hamilton DO Arc Sensor) ---
// From sensor_modbus_map.md & assuming similar structure to pH:
// - DO (PMC1) starts at 0-based address 2089 (0x0829), 2 registers for float
// - Temperature (PMC6) starts at 0-based address 2409 (0x0969), 2 registers for float
#define DO_SENSOR_DEFAULT_ADDRESS 1 // Placeholder: Default Modbus slave address (Check sensor manual)
#define REG_DO_BLOCK_START 0x0829   // Starting register for DO data block (2089 decimal)
#define REG_TEMP_BLOCK_START 0x0969 // Starting register for Temperature data block (2409 decimal)
#define NUM_REGISTERS_PER_BLOCK 2  // Number of registers for a float value
#define FLOAT_VALUE_OFFSET 0        // Offset within the block for the float value (assuming first 2 registers)

// Initialization function - Accepts the shared Modbus master node
bool do_sensor_init(ModbusRTUMaster* masterNode, HardwareSerial* port, long baud, int8_t rtsPin, uint32_t readInterval) {
    if (!masterNode) {
        // Cannot initialize without a valid master node
        doSensorDriver.fault = true;
        doSensorDriver.newMessage = true;
        strcpy(doSensorDriver.message, "Invalid Modbus master provided");
        return false;
    }

    doSensorDriver.node = masterNode; // Use the provided shared master node
    doSensorDriver.device = &doSensorDevice; // Link driver to the data object
    doSensorDriver.serial_port = port; // Store for reference/debugging if needed
    doSensorDriver.baud_rate = baud;   // Store for reference/debugging if needed
    doSensorDriver.rts_pin = rtsPin;   // Store for reference/debugging if needed
    doSensorDriver.read_interval_ms = readInterval;
    doSensorDriver.ready = false;
    doSensorDriver.fault = false;
    doSensorDriver.newMessage = false;
    doSensorDriver.last_read_time = 0;
    doSensorDriver.message[0] = '\0'; // Initialize message buffer

    // --- Configure the device defaults ---
    doSensorDevice.modbusAddress = DO_SENSOR_DEFAULT_ADDRESS; // Set default address
    doSensorDevice.enabled = true; // Enable by default
    doSensorDevice.fault = false;
    doSensorDevice.newMessage = false;
    doSensorDevice.message[0] = '\0';

    // --- Modbus Master Initialization is now done externally ---
    // No need to: new ModbusRTUMaster(...)
    // No need to: node->useRts(...)
    // No need to: pinMode/digitalWrite for RTS
    // No need to: serial_port->begin(...)
    // No need to: node->begin()

    doSensorDriver.ready = true;
    doSensorDriver.newMessage = true;
    strcpy(doSensorDriver.message, "DO Sensor driver initialized (shared Modbus)");
    return true;
}

// Helper function to read a block and extract float value
// Returns true on success, false on failure
bool read_do_sensor_float_block(uint16_t startReg, float &value) {
    // Ensure the node pointer is valid before using
    if (!doSensorDriver.node) {
        doSensorDevice.fault = true;
        doSensorDevice.newMessage = true;
        strcpy(doSensorDevice.message, "Modbus master node is null");
        return false;
    }

    uint16_t buffer[NUM_REGISTERS_PER_BLOCK]; // Buffer to hold the read registers

    // Read the required number of registers (typically 2 for a float)
    if (doSensorDriver.node->readHoldingRegisters(
            doSensorDevice.modbusAddress,
            startReg,
            buffer, // Pass the buffer
            NUM_REGISTERS_PER_BLOCK // Read only the registers needed for the float
        )) {
        // Use ModbusRTUMasterUtils::bufferToFloatBE or LE depending on sensor byte order
        // Assuming Big-Endian (check sensor manual)
        value = ModbusRTUMasterUtils::bufferToFloatBE(buffer);

        // Clear communication fault if present
        if (doSensorDevice.fault) {
            doSensorDevice.fault = false;
            doSensorDevice.newMessage = true;
            strcpy(doSensorDevice.message, "Communication restored");
        }
        return true;
    } else {
        doSensorDevice.fault = true;
        doSensorDevice.newMessage = true;
        snprintf(doSensorDevice.message, sizeof(doSensorDevice.message),
                 "Modbus read failed, addr: %d, reg: 0x%04X, err: 0x%02X",
                 doSensorDevice.modbusAddress, startReg, doSensorDriver.node->getExceptionResponse());
        // Consider adding timeout check: doSensorDriver.node->getTimeoutFlag()
        return false;
    }
}

// Update function (call this periodically from the main loop)
bool do_sensor_update(void) {
    if (!doSensorDriver.ready || doSensorDriver.fault || !doSensorDevice.enabled) {
        return false; // Don't update if not ready, driver faulted, or device disabled
    }

    uint32_t current_time = millis();
    if (current_time - doSensorDriver.last_read_time >= doSensorDriver.read_interval_ms) {
        doSensorDriver.last_read_time = current_time;

        bool read_ok = true;

        // Read DO value
        if (!read_do_sensor_float_block(REG_DO_BLOCK_START + FLOAT_VALUE_OFFSET, doSensorDevice.dissolvedOxygen)) {
            read_ok = false;
            // Error message already set in read_do_sensor_float_block
        }

        // Read Temperature value
        if (!read_do_sensor_float_block(REG_TEMP_BLOCK_START + FLOAT_VALUE_OFFSET, doSensorDevice.temperature)) {
            read_ok = false;
            // Error message might overwrite previous one if both fail
        }

        return read_ok;
    }
    return true; // No update needed this cycle
}

