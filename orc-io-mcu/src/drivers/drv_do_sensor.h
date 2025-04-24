#pragma once

#include "sys_init.h" // Include common system definitions if needed
#include "objects.h"  // Include the definition for DissolvedOxygenSensor_t
#include <ModbusRTUMaster.h> // Include the Modbus library header

// Define the serial port used for Modbus RS485 communication
// You might need to define this elsewhere (e.g., pins.h) or configure it here
// Example: #define ModbusSerial Serial1

struct DoSensorDriver_t {
    ModbusRTUMaster *node;          // Pointer to the Modbus master instance
    DissolvedOxygenSensor_t *device; // Pointer to the specific DO sensor data object
    HardwareSerial *serial_port;    // Pointer to the hardware serial port used
    long baud_rate;                 // Baud rate for Modbus communication
    int8_t rts_pin;                 // RTS pin for RS485 direction control (-1 if not used)
    bool ready;                     // Flag indicating if the driver is initialized
    bool fault;                     // Flag indicating a fault condition
    bool newMessage;                // Flag for new status messages
    char message[100];              // Buffer for status messages
    uint32_t last_read_time;        // Timestamp of the last successful read
    uint32_t read_interval_ms;      // How often to read from the sensor
};

// Declare external instances (you'll define these in the .cpp file)
// Assuming one DO sensor for now
extern DoSensorDriver_t doSensorDriver;
extern DissolvedOxygenSensor_t doSensorDevice; // Defined in objects.h, but declare extern here

// Function prototypes
// Pass ModbusRTUMaster pointer, serial port pointer is now mainly for info/debugging
bool do_sensor_init(ModbusRTUMaster* masterNode, HardwareSerial* port, long baud, int8_t rtsPin = -1, uint32_t readInterval = 1000); // Initialize the driver
bool do_sensor_update(void); // Update function to read data periodically
