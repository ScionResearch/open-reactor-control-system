#pragma once

#include "sys_init.h"
#include <stdint.h> // Ensure standard types are available
#include <stdbool.h> // Ensure bool is available

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

struct DissolvedOxygenSensor_t {
    float dissolvedOxygen; // Value read from sensor
    float temperature;     // Temperature reading from sensor (if available)
    char doUnit[10];       // e.g., "% O2", "mg/L"
    char tempUnit[5];      // e.g., "C"
    uint8_t modbusAddress; // Modbus slave address
    bool enabled;          // To enable/disable reading
    bool fault;
    bool newMessage;
    char message[100];
    // Default constructor
    DissolvedOxygenSensor_t() : dissolvedOxygen(0.0), temperature(0.0), modbusAddress(0), enabled(false), fault(false), newMessage(false) {
        strcpy(doUnit, "% O2"); // Default unit
        strcpy(tempUnit, "C");  // Default unit
        message[0] = '\\0';
    }
};

// --- Structures needed for DO Control Configuration ---
// Structure for PID controller parameters (moved here from do_control.h)
typedef struct {
    float kp;
    float ki;
    float kd;
    float integral_min; // Anti-windup limits
    float integral_max;
    float output_min;   // Overall PID output limits
    float output_max;
} PID_Params_t;

// Structure for configuring a single cascade parameter (moved here from do_control.h)
typedef struct {
    float cv_threshold_min; // Primary CV value to start activating this parameter
    float cv_threshold_max; // Primary CV value when this parameter reaches its max output
    float op_range_min;     // Minimum physical operating value (e.g., RPM, LPM, %)
    float op_range_max;     // Maximum physical operating value
} Cascade_Param_Config_t;

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

// Device objects
struct StepperDevice_t {
float rpm;
float rpm;
    float maxRPM;
    float acceleration;
    float load;
    bool direction;
    bool inverted;
    bool running;
    bool enabled;
    bool stealthChop;
    uint16_t stepsPerRev;
    uint16_t holdCurrent;
    uint16_t runCurrent;
};

struct MotorDevice_t {
    
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