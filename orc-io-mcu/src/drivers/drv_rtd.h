#pragma once

#include "sys_init.h"

#define NUM_MAX31865_INTERFACES 3

// Driver file for MAX31865 RTD sensor interfaces over SPI

enum RtdSensorType {
    PT100,
    PT1000 
};

struct RtdSensor_t {
    TemperatureSensor_t *temperatureObj;
    int cs_pin;
    Adafruit_MAX31865 *sensor;
    max31865_numwires_t wires;
    RtdSensorType sensorType;
};

extern TemperatureSensor_t rtd_sensor[3];
extern RtdSensor_t rtd_interface[3];

bool init_rtdDriver(void);                                // Initialises the CS pins and temperature objects
bool initTemperatureSensor(RtdSensor_t *sensorObj);       // Initialises the RTD sensor - requires the library instance and the temperarure object
bool readRtdSensors(void);                                // Loop function for periodic sensor reading
bool readRtdSensor(RtdSensor_t *sensorObj);               // Loop function for periodic sensor reading (individual sensor)
bool setRtdSensorType(RtdSensor_t *sensorObj, RtdSensorType sensorType);  // PT100 or PT1000
bool setRtdWires(RtdSensor_t *sensorObj, max31865_numwires_t wires);      // MAX31865_2WIRE, MAX31865_3WIRE or MAX31865_4WIRE