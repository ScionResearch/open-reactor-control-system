// filepath: c:\Users\vanderwt\OneDrive - Scion\Documents\PROJECTS\Smart Manufacturing\Digital Twin\open-reactor-control-system\orc-io-mcu\src\drivers\drv_ph_sensor.h
#pragma once

#include "sys_init.h" // Include common system definitions if needed
#include "objects.h"  // Include the definition for PhSensor_t
#include <ModbusRTUMaster.h> // Include the Modbus library header

// Define the serial port used for Modbus RS485 communication
// This should be the same as the DO sensor if on the same bus

struct PhSensorDriver_t {
    ModbusRTUMaster *node;          // Pointer to the Modbus master instance (needs sharing)
    PhSensor_t *device;             // Pointer to the specific pH sensor data object
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

// Declare external instances
extern PhSensorDriver_t phSensorDriver;
extern PhSensor_t phSensorDevice; // Defined in objects.h, but declare extern here

// Function prototypes
// Pass ModbusRTUMaster pointer, serial port pointer is now mainly for info/debugging
bool ph_sensor_init(ModbusRTUMaster* masterNode, HardwareSerial* port, long baud, int8_t rtsPin = -1, uint32_t readInterval = 1000); // Initialize the driver
bool ph_sensor_update(void); // Update function to read data periodically
