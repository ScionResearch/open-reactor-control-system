#pragma once

#include "sys_init.h"

#include <MAX31865.h>

#define NUM_MAX31865_INTERFACES 3

// Driver file for MAX31865 RTD sensor interfaces over SPI

enum RtdSensorType {
    PT100,
    PT1000 
};

// Driver struct - contains interface parameters and sensor object
struct RTDDriver_t {
    TemperatureSensor_t *temperatureObj;
    Calibrate_t *cal;
    int cs_pin;
    int drdy_pin;
    MAX31865 *sensor;
    max31865_numwires_t wires;
    RtdSensorType sensorType;
};

extern TemperatureSensor_t rtd_sensor[3];
extern RTDDriver_t rtd_interface[3];

bool init_rtdDriver(void);                                // Initialises the CS pins and temperature objects
bool initTemperatureSensor(RTDDriver_t *sensorObj);       // Initialises the RTD sensor - requires the library instance and the temperarure object
bool readRtdSensors(void);                                // Loop function for periodic sensor reading
bool readRtdSensor(RTDDriver_t *sensorObj);               // Loop function for periodic sensor reading (individual sensor)
bool setRtdSensorType(RTDDriver_t *sensorObj, RtdSensorType sensorType);  // PT100 or PT1000
bool setRtdWires(RTDDriver_t *sensorObj, max31865_numwires_t wires);      // MAX31865_2WIRE, MAX31865_3WIRE or MAX31865_4WIRE

void RTD_manage(void);