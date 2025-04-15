#include "drv_do_sensor.h"

// Define the global driver and device instances
DoSensorDriver_t doSensorDriver;
DissolvedOxygenSensor_t doSensorDevice; // Assuming one global instance for now

// --- Modbus Specific Definitions ---
// These need to be defined based on the Hamilton DO sensor's Modbus documentation
#define DO_SENSOR_DEFAULT_ADDRESS 1 // Default Modbus slave address (Check sensor manual)
#define REG_DO_VALUE 0x0829         // Modbus register address for DO reading (2089 in hex)
#define REG_TEMPERATURE 0x0969      // Modbus register address for temperature (2409 in hex)
#define NUM_REGISTERS_TO_READ 10     // Example: Number of registers to read in one go

// Initialization function
bool do_sensor_init(HardwareSerial* port, long baud, int8_t rtsPin, uint32_t readInterval) {
    doSensorDriver.device = &doSensorDevice; // Link driver to the data object
    doSensorDriver.serial_port = port;
    doSensorDriver.baud_rate = baud;
    doSensorDriver.rts_pin = rtsPin;
    doSensorDriver.read_interval_ms = readInterval;
    doSensorDriver.ready = false;
    doSensorDriver.fault = false;
    doSensorDriver.newMessage = false;
    doSensorDriver.last_read_time = 0;
    message[0] = '\0';

    // --- Configure the device defaults ---
    doSensorDevice.modbusAddress = DO_SENSOR_DEFAULT_ADDRESS; // Set default address
    // Set other defaults for doSensorDevice if needed (units, enabled status, etc.)
    doSensorDevice.enabled = true; // Enable by default?

    // --- Initialize Modbus Master ---
    doSensorDriver.node = new ModbusRTUMaster(doSensorDriver.serial_port);
    if (!doSensorDriver.node) {
        doSensorDriver.fault = true;
        doSensorDriver.newMessage = true;
        strcpy(doSensorDriver.message, "Failed to allocate ModbusRTUMaster");
        return false;
    }

    // Configure RTS pin if used for RS485 direction control
    if (doSensorDriver.rts_pin >= 0) {
        doSensorDriver.node->useRts(true, doSensorDriver.rts_pin);
        pinMode(doSensorDriver.rts_pin, OUTPUT);
        digitalWrite(doSensorDriver.rts_pin, LOW); // Default state for RTS
    }

    // Initialize the serial port for Modbus
    doSensorDriver.serial_port->begin(doSensorDriver.baud_rate);
    // Add serial configuration if needed (e.g., SERIAL_8N1)

    // Start the Modbus node
    if (!doSensorDriver.node->begin()) {
        doSensorDriver.fault = true;
        doSensorDriver.newMessage = true;
        strcpy(doSensorDriver.message, "ModbusRTUMaster begin failed");
        // Consider deleting node if begin fails and allocated with new
        // delete doSensorDriver.node;
        // doSensorDriver.node = nullptr;
        return false;
    }

    doSensorDriver.ready = true;
    doSensorDriver.newMessage = true;
    strcpy(doSensorDriver.message, "DO Sensor Modbus driver initialized");
    return true;
}

// Update function (call this periodically from the main loop)
bool do_sensor_update(void) {
    if (!doSensorDriver.ready || doSensorDriver.fault || !doSensorDevice.enabled) {
        return false; // Don't update if not ready, faulted, or disabled
    }

    uint32_t current_time = millis();
    if (current_time - doSensorDriver.last_read_time >= doSensorDriver.read_interval_ms) {
        doSensorDriver.last_read_time = current_time;

        // --- Perform Modbus Read ---
        // Use the correct function code (e.g., READ_HOLDING_REGISTERS, READ_INPUT_REGISTERS)
        // and register addresses based on the sensor manual.
        // This example assumes reading holding registers.
        uint8_t result = doSensorDriver.node->readHoldingRegisters(
            doSensorDevice.modbusAddress,
            REG_DO_VALUE, // Starting register address
            NUM_REGISTERS_TO_READ // Number of registers to read
        );

        if (result == ModbusRTUMaster::MODBUS_SUCCESS) {
            // --- Process Successful Read ---
            // Get the data from the Modbus response buffer
            // Data format (float, int, scaling) depends on the sensor! Check manual.
            // Example: Assuming registers hold 16-bit integers that need scaling
            uint16_t raw_do = doSensorDriver.node->getResponseBuffer(0); // Index relative to start of read
            uint16_t raw_temp = doSensorDriver.node->getResponseBuffer(1);

            // --- Convert raw data to actual values (NEEDS ADJUSTMENT) ---
            // This conversion is highly dependent on the sensor's output format.
            // You might need to combine registers, apply scaling factors, handle different data types (float, int).
            // Placeholder conversion:
            doSensorDevice.dissolvedOxygen = (float)raw_do / 100.0; // Example scaling
            doSensorDevice.temperature = (float)raw_temp / 10.0;   // Example scaling

            // Clear any previous fault state related to communication
            if (doSensorDevice.fault) {
                 doSensorDevice.fault = false;
                 doSensorDevice.newMessage = true;
                 strcpy(doSensorDevice.message, "Communication restored");
            }

        } else {
            // --- Handle Modbus Error ---
            doSensorDevice.fault = true;
            doSensorDevice.newMessage = true;
            snprintf(doSensorDevice.message, sizeof(doSensorDevice.message),
                     "Modbus read failed, addr: %d, err: 0x%02X",
                     doSensorDevice.modbusAddress, result);
            // Consider adding more specific error handling based on 'result' code
            return false;
        }
    }
    return true;
}

