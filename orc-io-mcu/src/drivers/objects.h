#pragma once

#include <Arduino.h>

// Placeholder struct for Cascade_Param_Config_t
typedef struct {
    float min;
    float max;
    float param1;
    float param2;
} Cascade_Param_Config_t;

// Placeholder struct for PID_Params_t
typedef struct {
    float kp;
    float ki;
    float kd;
    float min_output;
    float max_output;
    float setpoint;
    float sample_time;
} PID_Params_t;

// Placeholder struct for DissolvedOxygenSensor_t
typedef struct {
    float value;
    bool enabled;
    bool fault;
    bool newMessage;
    char message[100];
} DissolvedOxygenSensor_t;

#define MAX_NUM_OBJECTS 80

// Object index------------------------------------------>|
// Using tagged union with metadata for heterogeneous objects

// Object types
enum ObjectType {
    // Sensors
    OBJ_T_ANALOG_INPUT,             // x8
    OBJ_T_DIGITAL_INPUT,            // x8
    OBJ_T_TEMPERATURE_SENSOR,       // x3
    OBJ_T_PH_SENSOR,                // x?
    OBJ_T_DISSOLVED_OXYGEN_SENSOR,  // x?
    OBJ_T_OPTICAL_DENSITY_SENSOR,   // x?
    OBJ_T_FLOW_SENSOR,              // x?
    OBJ_T_PRESSURE_SENSOR,          // x?
    OBJ_T_POWER_SENSOR,             // x2
    // Outputs
    OBJ_T_ANALOG_OUTPUT,            // x2
    OBJ_T_DIGITAL_OUTPUT,           // x5
    // Motion drivers
    OBJ_T_STEPPER_MOTOR,            // x1
    OBJ_T_BDC_MOTOR,                // x4
    // Control objects
    OBJ_T_TEMPERATURE_CONTROL,      // x3
    OBJ_T_PH_CONTROL,               // x?
    OBJ_T_DISSOLVED_OXYGEN_CONTROL, // x?
    OBJ_T_OPTICAL_DENSITY_CONTROL,  // x?
    OBJ_T_GAS_FLOW_CONTROL,         // x?
    OBJ_T_STIRRER_CONTROL,          // x1
    OBJ_T_PUMP_CONTROL,             // x4
    OBJ_T_FEED_CONTROL,             // x1
    OBJ_T_WASTE_CONTROL,            // x1
    // Comm ports
    OBJ_T_SERIAL_RS232_PORT,        // x2
    OBJ_T_SERIAL_RS485_PORT         // x2
};

// Object index structure
struct ObjectIndex_t {
    ObjectType type;    // Object type tag
    void* obj;          // Pointer to sensor/output/control object
    char name[40];      // Text name of object
    bool valid;         // True if object is configured
};

// Object index array
extern ObjectIndex_t objIndex[MAX_NUM_OBJECTS];
extern int numObjects;

// Object index contains types and pointers to all objects which need to be accessed from
// the system MCU. The first ~40 objects are reserved for on-board fixed sensors, outputs
// and control objects. The remainder are dynamic and can be created by the user.

// Object index<------------------------------------------|

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

struct DigitalIO_t {
    bool pullup;
    bool output;
    bool state;
};
struct TemperatureSensor_t {
    float temperature;
    char unit[5];
    bool fault;
    bool newMessage;
    char message[100];
};

// Structure to hold data for a pH Sensor
typedef struct {
    uint8_t modbusAddress; // Modbus address of the sensor
    float pH;              // Current pH reading
    float temperature;     // Current temperature reading from the sensor
    bool enabled;          // Is the sensor reading enabled?
    bool fault;            // Is the sensor in a fault state?
    bool newMessage;       // Flag for new status messages
    char message[100];     // Buffer for status messages
} PhSensor_t;

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

struct PowerSensor_t {
    float voltage;
    float current;
    float power;
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
    bool pwmEnabled;
    float pwmDuty;  // Duty cycle in percent (0-100)
    bool fault;
    bool newMessage;
    char message[100];
};

// Device objects
struct StepperDevice_t {
    float rpm;
    float maxRPM;
    float acceleration;
    float load;
    bool inverted;
    bool running;
    bool enabled;
    bool stealthChop;
    uint16_t stepsPerRev;
    uint16_t holdCurrent;
    uint16_t runCurrent;
};

struct MotorDevice_t {
    float power;
        // Removed duplicate 'float rpm;'
    bool direction;
    bool inverted;
    bool running;
    bool enabled;
    uint16_t runCurrent;
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
    DissolvedOxygenSensor_t sensor; // Associated DO sensor
    bool enabled;                   // Enable/disable control loop
    float setpoint;                 // Target DO value

    // --- PID/Cascade Configuration (replaces LUTs) ---
    PID_Params_t pid_params;        // PID tuning parameters
    Cascade_Param_Config_t stir_config; // Cascade config for stirrer
    Cascade_Param_Config_t gas_flow_config; // Cascade config for gas flow
    Cascade_Param_Config_t o2_conc_config;  // Cascade config for O2 concentration
    float sample_time_s;            // Control loop sample time

    // --- Removed LUTs ---
    // float stirrerLUT[2][10];
    // float gasLUT[2][10];

    // Default constructor (optional, adjust as needed)
    DissolvedOxygenControl_t() : enabled(false), setpoint(0.0f), sample_time_s(1.0f) {
        // Initialize PID/Cascade params to defaults if desired
        pid_params = {1.0f, 0.1f, 0.01f, -100.0f, 100.0f, 0.0f, 100.0f};
        stir_config = {0.0f, 33.0f, 50.0f, 500.0f}; // Example: Stirrer active in lower 1/3rd CV
        gas_flow_config = {30.0f, 66.0f, 0.1f, 2.0f}; // Example: Gas flow in middle 1/3rd CV
        o2_conc_config = {60.0f, 100.0f, 21.0f, 100.0f}; // Example: O2 conc in upper 1/3rd CV
    }
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

// Communication port objects
struct SerialRS232_t {
    bool enabled;
    bool newMessage;
    char message[100];
};

struct SerialRS485_t {
    bool enabled;
    bool newMessage;
    char message[100];
};