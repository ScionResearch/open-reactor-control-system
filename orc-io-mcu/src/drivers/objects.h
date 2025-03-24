#pragma once

#include "sys_init.h"

// Constants----------------------------------------------->|
#define MAX_TEMPERATURE_SENSORS 20
#define MAX_PH_SENSORS 20
#define MAX_DISSOLVED_OXYGEN_SENSORS 20
#define MAX_OPTICAL_DENSITY_SENSORS 20
#define MAX_FLOW_SENSORS 20
#define MAX_PRESSURE_SENSORS 20

#define MAX_TEMPERATURE_CONTROLLERS 20
#define MAX_PH_CONTROLLERS 20
#define MAX_DISSOLVED_OXYGEN_CONTROLLERS 20
#define MAX_STIRRER_CONTROLLERS 20
#define MAX_FLOW_CONTROLLERS 20
#define MAX_PUMP_CONTROLLERS 20
#define MAX_FEED_CONTROLLERS 20
#define MAX_WASTE_CONTROLLERS 20

// Constants<----------------------------------------------|

// Object struct definitions----------------------------->|

// Calibration object
struct Calibrate_t {
    float scale;            // Multiplier, 1 = no scaling
    float offset;           // Offset value
    uint32_t timestamp;     // Timestamp of last calibration
    // Default constructor
    Calibrate_t() : scale(1.0), offset(0.0), timestamp(1735689600) {}   // 01/01/2025
};

// Sensor objects
struct AnalogInput_t {
    float value;
    char unit[5];
    bool enabled;
    bool fault;
    bool newMessage;
    char message[100];
    Calibrate_t *cal;
};
struct TemperatureSensor_t {
    float temperature;
    char unit[5];
    bool fault;
    bool newMessage;
    char message[100];
};

struct PhSensor_t {
    float ph;
    char unit[5];
    bool fault;
    bool newMessage;
    char message[100];
};

struct DissolvedOxygenSensor_t {
    float dissolvedOxygen;
    char unit[5];
    bool fault;
    bool newMessage;
    char message[100];
};

struct OpticalDensitySensor_t {
    float opticalDensity;
    char unit[5];
    bool fault;
    bool newMessage;
    char message[100];
};

struct FlowSensor_t {
    float flow;
    char unit[5];
    bool fault;
    bool newMessage;
    char message[100];
};

struct PressureSensor_t {
    float pressure;
    char unit[5];
    bool fault;
    bool newMessage;
    char message[100];
};

// Output objects
struct AnalogOutput_t {
    float value;
    char unit[5];
    bool enabled;
    bool fault;
    bool newMessage;
    char message[100];
    Calibrate_t *cal;
};

struct DigitalOutput_t {
    bool state;
    bool fault;
    bool newMessage;
    char message[100];
};

// Control objects
struct TemperatureControl_t {
    TemperatureSensor_t sensor;
    bool enabled;
    float setpoint;
    float kp;
    float ki;
    float kd;
};

struct PhControl_t {
    PhSensor_t sensor;
    bool enabled;
    float setpoint;
    float interval;
    float maxDoseTime;
};

struct DissolvedOxygenControl_t {
    DissolvedOxygenSensor_t sensor;
    bool enabled;
    float setpoint;
    float stirrerLUT[2][10];
    float gasLUT[2][10];
};

struct gasFlowControl_t {
    float setpoint;
    bool enabled;
    float kp;
    float ki;
    float kd;
};

struct stirrerControl_t {
    float setpoint;
    bool enabled;
    float kp;
    float ki;
    float kd;
};

struct pumpControl_t {
    float percent;
    bool enabled;
};

struct feedControl_t {
    bool enabled;
    float interval;
    float duty;
};

struct wasteControl_t {
    bool enabled;
    float interval;
    float duty;
};