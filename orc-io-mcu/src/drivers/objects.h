#pragma once

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

// Sensor objects
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

// Object struct definitions<--------------------------------|

// Object arrays------------------------------------------->|

// Sensor objects
/*TemperatureSensor_t temperatureSensor[MAX_TEMPERATURE_SENSORS];
PhSensor_t phSensor[MAX_PH_SENSORS];
DissolvedOxygenSensor_t dissolvedOxygenSensor[MAX_DISSOLVED_OXYGEN_SENSORS];
OpticalDensitySensor_t opticalDensitySensor[MAX_OPTICAL_DENSITY_SENSORS];
FlowSensor_t flowSensor[MAX_FLOW_SENSORS];
PressureSensor_t pressureSensor[MAX_PRESSURE_SENSORS];

// Control objects
TemperatureControl_t temperatureControl[MAX_TEMPERATURE_CONTROLLERS];
PhControl_t phControl[MAX_PH_CONTROLLERS];
DissolvedOxygenControl_t dissolvedOxygenControl[MAX_DISSOLVED_OXYGEN_CONTROLLERS];
gasFlowControl_t gasFlowControl[MAX_FLOW_CONTROLLERS];
stirrerControl_t stirrerControl[MAX_STIRRER_CONTROLLERS];
pumpControl_t pumpControl[MAX_PUMP_CONTROLLERS];
feedControl_t feedControl[MAX_FEED_CONTROLLERS];
wasteControl_t wasteControl[MAX_WASTE_CONTROLLERS];*/

// Object arrays<------------------------------------------|