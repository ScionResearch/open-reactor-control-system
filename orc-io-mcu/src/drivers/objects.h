#pragma once

#include <Arduino.h>

#define MAX_NUM_OBJECTS 100  // Expanded from 80 to support more dynamic devices

// Object index allocation:
// 0-32:   Fixed onboard objects (ADC, DAC, RTD, GPIO, outputs, motors, energy)
// 33-37:  COM ports (5 slots)
// 38-39:  Reserved for onboard device feedback
// 40-49:  Controller objects (10 slots) - PID loops, sequencers, control algorithms
// 50-69:  Device control objects (20 slots) - Setpoints, commands for peripheral devices
// 70-99:  Device sensor objects (30 slots) - Sensor readings from peripheral devices

// Object types
enum ObjectType {
    // Sensors
    OBJ_T_ANALOG_INPUT,             // Indices 0-7
    OBJ_T_DIGITAL_INPUT,            // Indices 13-20
    OBJ_T_TEMPERATURE_SENSOR,       // Indices 10-12, 70+
    OBJ_T_PH_SENSOR,                // Indices 70+ (dynamic)
    OBJ_T_DISSOLVED_OXYGEN_SENSOR,  // Indices 70+ (dynamic)
    OBJ_T_OPTICAL_DENSITY_SENSOR,   // Indices 70+ (dynamic)
    OBJ_T_FLOW_SENSOR,              // Indices 70+ (dynamic)
    OBJ_T_PRESSURE_SENSOR,          // Indices 70+ (dynamic)
    OBJ_T_VOLTAGE_SENSOR,           // Reserved for future use
    OBJ_T_CURRENT_SENSOR,           // Reserved for future use
    OBJ_T_POWER_SENSOR,             // Reserved for future use
    OBJ_T_ENERGY_SENSOR,            // Indices 31-32 - Multi-value (voltage, current, power)
    // Outputs
    OBJ_T_ANALOG_OUTPUT,            // Indices 8-9
    OBJ_T_DIGITAL_OUTPUT,           // Indices 21-25
    // Motion drivers
    OBJ_T_STEPPER_MOTOR,            // Index 26
    OBJ_T_BDC_MOTOR,                // Indices 27-30
    // Controller objects (40-49)
    OBJ_T_TEMPERATURE_CONTROL,      // PID temperature control loops
    OBJ_T_PH_CONTROL,               // pH dosing control
    OBJ_T_DISSOLVED_OXYGEN_CONTROL, // DO control (gas mixing + stirrer)
    OBJ_T_OPTICAL_DENSITY_CONTROL,  // OD/biomass control
    OBJ_T_GAS_FLOW_CONTROL,         // MFC control loops
    OBJ_T_STIRRER_CONTROL,          // Stirrer speed control
    OBJ_T_PUMP_CONTROL,             // Peristaltic pump control
    OBJ_T_FEED_CONTROL,             // Nutrient feed sequencer
    OBJ_T_WASTE_CONTROL,            // Waste removal sequencer
    // Device control objects (50-69)
    OBJ_T_DEVICE_CONTROL,           // Control/status for peripheral devices
    // Comm ports (33-36)
    OBJ_T_SERIAL_RS232_PORT,        // RS-232 ports
    OBJ_T_SERIAL_RS485_PORT         // RS-485 ports
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
    uint8_t pullMode;    // 0=None (High-Z), 1=Pull-up, 2=Pull-down
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
    Calibrate_t *cal;
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

struct EnergySensor_t {
    float voltage;      // Volts
    float current;      // Amperes
    float power;        // Watts
    char unit[8];       // Primary unit (V)
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

// Device control object (indices 50-69)
// Provides control interface and status for peripheral devices (MFCs, pumps, etc.)
struct DeviceControl_t {
    float setpoint;             // Control setpoint (flow rate, pH target, etc.)
    float actualValue;          // Feedback value from associated sensor
    char setpointUnit[10];      // Unit string for setpoint
    bool connected;             // Device connection status (Modbus/I2C responding)
    bool fault;                 // Fault condition
    bool newMessage;            // Message available flag
    char message[100];          // Status/error message buffer
    uint8_t slaveID;            // Modbus slave ID or I2C address
    uint8_t deviceType;         // IPC_DeviceType enum value
    uint8_t startSensorIndex;   // First associated sensor index (70-99)
    uint8_t sensorCount;        // Number of associated sensor objects
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