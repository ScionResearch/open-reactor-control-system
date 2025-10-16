#pragma once

#include <Arduino.h>

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
    OBJ_T_VOLTAGE_SENSOR,           // x2
    OBJ_T_CURRENT_SENSOR,           // x2
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
    char unit[8];
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
    bool fault;
    bool newMessage;
    char message[100];
};
struct TemperatureSensor_t {
    float temperature;
    char unit[8];
    bool fault;
    bool newMessage;
    char message[100];
};

struct PhSensor_t {
    float ph;
    char unit[8];
    bool fault;
    bool newMessage;
    char message[100];
};

struct DissolvedOxygenSensor_t {
    float dissolvedOxygen;
    char unit[8];
    bool fault;
    bool newMessage;
    char message[100];
};

struct OpticalDensitySensor_t {
    float opticalDensity;
    char unit[8];
    bool fault;
    bool newMessage;
    char message[100];
};

struct FlowSensor_t {
    float flow;
    char unit[8];
    bool fault;
    bool newMessage;
    char message[100];
};

struct PressureSensor_t {
    float pressure;
    char unit[8];
    bool fault;
    bool newMessage;
    char message[100];
};

struct VoltageSensor_t {
    float voltage;
    char unit[8];
    bool fault;
    bool newMessage;
    char message[100];
};

struct CurrentSensor_t {
    float current;
    char unit[8];
    bool fault;
    bool newMessage;
    char message[100];
};

struct PowerSensor_t {
    float power;
    char unit[8];
    bool fault;
    bool newMessage;
    char message[100];
};

// Output objects
struct AnalogOutput_t {
    float value;
    char unit[8];
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
    bool direction;
    bool inverted;
    bool running;
    bool enabled;
    bool stealthChop;
    uint16_t stepsPerRev;
    uint16_t holdCurrent;
    uint16_t runCurrent;
    char unit[8];           // "rpm"
    bool fault;
    bool newMessage;
    char message[100];
};

struct MotorDevice_t {
    float power;
    bool direction;
    bool inverted;
    bool running;
    bool enabled;
    uint16_t runCurrent;
    char unit[8];           // "%"
    bool fault;
    bool newMessage;
    char message[100];
};

// Control objects
struct TemperatureControl_t {
    // References to existing objects (by index)
    uint16_t sensorIndex;       // Index in objIndex[] for temperature sensor
    uint16_t outputIndex;       // Index in objIndex[] for analog output
    
    // Control state
    bool enabled;
    bool autotuning;
    
    // Setpoint & limits
    float setpoint;             // Target temperature
    float setpointMin;          // Minimum allowed setpoint
    float setpointMax;          // Maximum allowed setpoint
    
    // PID parameters
    float kp;                   // Proportional gain
    float ki;                   // Integral gain  
    float kd;                   // Derivative gain
    
    // Output limits
    float outputMin;            // Minimum output (typically 0%)
    float outputMax;            // Maximum output (typically 100%)
    bool outputInverted;        // true for cooling, false for heating
    
    // Status
    float currentTemp;          // Last measured temperature
    float currentOutput;        // Last computed output
    float processError;         // Current error (setpoint - currentTemp)
    
    // Fault handling
    bool fault;
    bool newMessage;
    char message[100];
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
    FlowSensor_t sensor;
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
struct SerialCom_t {
    uint8_t portNumber;      // Port number (1-4)
    uint32_t baudRate;       // Baud rate (e.g., 9600, 19200, 115200)
    uint8_t dataBits;        // Data bits (5, 6, 7, or 8)
    float stopBits;          // Stop bits (1, 1.5, or 2)
    uint8_t parity;          // Parity: 0=none, 1=odd, 2=even
    bool enabled;            // Port enabled
    uint8_t slaveCount;      // Number of Modbus slaves on this port
    bool fault;              // Fault flag
    bool newMessage;         // New message flag
    char message[100];       // Status/error message
};