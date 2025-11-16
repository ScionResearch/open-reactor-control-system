#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include "../utils/logger.h"

// LittleFS configuration
#define IO_CONFIG_FILENAME "/io_config.json"
#define IO_CONFIG_MAGIC_NUMBER 0xA5
#define IO_CONFIG_VERSION 1

// Maximum counts for each object type
#define MAX_ADC_INPUTS 8
#define MAX_DAC_OUTPUTS 2
#define MAX_RTD_SENSORS 3
#define MAX_GPIO 8
#define MAX_DIGITAL_OUTPUTS 5  // Indices 21-25 (open drain 1-4, high current)
#define MAX_STEPPER_MOTORS 1   // Index 26
#define MAX_DC_MOTORS 4        // Indices 27-30
#define MAX_ENERGY_SENSORS 2   // Indices 31-32 (Main, Heater)
#define MAX_CONTROLLERS 10     // Indices 40-49 (Control loops)
#define MAX_TEMP_CONTROLLERS 3  // Indices 40-42 (Temperature Control loops)
#define MAX_PH_CONTROLLERS 1   // Indices 43 (pH controller)
#define MAX_DO_CONTROLLERS 1   // Indices 44 (Dissolved Oxygen controller)

/**
 * @brief Calibration structure for analog inputs/outputs
 */
struct CalibrationConfig {
    float scale;
    float offset;
};

/**
 * @brief Configuration for ADC analog inputs (indices 0-7)
 */
struct ADCInputConfig {
    char name[32];          // User-defined name
    char unit[8];           // Unit of measurement (mV, V, mA, uV)
    CalibrationConfig cal;  // Calibration scale and offset
    bool enabled;           // Enable/disable this input
    bool showOnDashboard;   // Show on main dashboard
};

/**
 * @brief Configuration for DAC analog outputs (indices 8-9)
 */
struct DACOutputConfig {
    char name[32];
    char unit[8];           // Usually mV or V
    CalibrationConfig cal;
    bool enabled;
    bool showOnDashboard;   // Show on main dashboard
};

/**
 * @brief Configuration for RTD temperature sensors (indices 10-12)
 */
struct RTDSensorConfig {
    char name[32];
    char unit[8];           // C, F, or K
    CalibrationConfig cal;  // Calibration (scale and offset)
    uint8_t wireConfig;     // 2, 3, or 4 wire configuration
    uint16_t nominalOhms;   // 100 (PT100) or 1000 (PT1000)
    bool enabled;
    bool showOnDashboard;   // Show on main dashboard
};

/**
 * @brief Pull resistor mode for GPIO inputs
 */
enum GPIOPullMode : uint8_t {
    GPIO_PULL_NONE = 0,    // High-Z (floating)
    GPIO_PULL_UP = 1,      // Internal pull-up
    GPIO_PULL_DOWN = 2     // Internal pull-down
};

/**
 * @brief Configuration for digital GPIO inputs (indices 13-20)
 */
struct GPIOConfig {
    char name[32];
    GPIOPullMode pullMode;  // Pull resistor configuration
    bool enabled;
    bool showOnDashboard;   // Show on main dashboard
};

/**
 * @brief Output mode for digital outputs
 */
enum OutputMode : uint8_t {
    OUTPUT_MODE_ON_OFF = 0,  // Simple on/off control
    OUTPUT_MODE_PWM = 1      // PWM control (0-100%)
};

/**
 * @brief Configuration for open drain outputs (indices 21-24) and high current output (25)
 */
struct DigitalOutputConfig {
    char name[32];
    OutputMode mode;         // On/Off or PWM
    bool enabled;
    bool showOnDashboard;
};

/**
 * @brief Configuration for stepper motor (index 26)
 */
struct StepperMotorConfig {
    char name[32];
    uint16_t stepsPerRev;    // 100-1000, default 200
    uint16_t maxRPM;         // 100-1000, default 500
    uint16_t holdCurrent_mA; // 10-1000 mA
    uint16_t runCurrent_mA;  // 10-1800 mA
    uint16_t acceleration;   // RPM/s
    bool invertDirection;
    bool enabled;
    bool showOnDashboard;
    
    // TMC5130 advanced features
    bool stealthChopEnabled;     // Enable StealthChop mode
    bool coolStepEnabled;        // Enable CoolStep mode
    bool fullStepEnabled;        // Enable FullStep mode
    float stealthChopMaxRPM;     // RPM threshold for StealthChop (default 100)
    float coolStepMinRPM;        // RPM threshold for CoolStep (default 200)
    float fullStepMinRPM;        // RPM threshold for FullStep (default 300)
};

/**
 * @brief Configuration for DC motors (indices 27-30)
 */
struct DCMotorConfig {
    char name[32];
    bool invertDirection;
    bool enabled;
    bool showOnDashboard;
};

/**
 * @brief Configuration for Energy Monitors (indices 31-32)
 * INA260 sensors measuring voltage, current, and power
 */
struct EnergySensorConfig {
    char name[32];
    bool showOnDashboard;
};

/**
 * @brief Control method for temperature controllers
 */
enum ControlMethod {
    CONTROL_METHOD_ON_OFF = 0,
    CONTROL_METHOD_PID = 1
};

/**
 * @brief Configuration for Temperature Controllers (indices 40-49)
 */
struct TemperatureControllerConfig {
    bool isActive;                  // Controller slot in use
    char name[40];                  // User-defined name
    bool enabled;                   // Enable/disable controller
    bool showOnDashboard;           // Dashboard visibility
    char unit[8];                   // Temperature unit: "C", "F", "K"
    
    // Input/Output Connections
    uint8_t pvSourceIndex;          // Process value source (sensor index)
    uint8_t outputIndex;            // Control output (21-25 digital, 8-9 DAC)
    
    // Control Configuration
    ControlMethod controlMethod;    // 0=On/Off, 1=PID
    float setpoint;                 // Target value
    
    // On/Off Parameters
    float hysteresis;               // Deadband width (only used when controlMethod = ON_OFF)
    
    // PID Parameters
    float kP;                       // Proportional gain
    float kI;                       // Integral gain
    float kD;                       // Derivative gain
    float integralWindup;           // Anti-windup limit
    float outputMin;                // Output clamp min (0-100%)
    float outputMax;                // Output clamp max (0-100%)
};

/**
 * @brief Configuration for Device Sensors (indices 70-99)
 * These are sensor objects created by dynamic devices.
 * Each device can create multiple sensors (e.g., pH probe creates pH + Temperature)
 */
#define MAX_DEVICE_SENSORS 30  // Matches DYNAMIC_INDEX_END - DYNAMIC_INDEX_START + 1

struct DeviceSensorConfig {
    char name[33];           // User-defined name (max 32 characters + null terminator)
    bool showOnDashboard;    // Show on main dashboard
    bool nameOverridden;     // True if user has set custom name
};

/**
 * @brief Dosing configuration for acid or alkaline
 */
struct pHDosingConfig {
    bool enabled;                    // Is this dosing direction enabled?
    uint8_t outputType;              // 0=Digital Output, 1=DC Motor
    uint8_t outputIndex;             // Digital output (21-25) or DC motor (27-30)
    uint8_t motorPower;              // Power level if motor (0-100%), ignored for digital
    uint16_t dosingTime_ms;          // How long to activate output (milliseconds)
    uint32_t dosingInterval_ms;      // Minimum time between doses (milliseconds)
    float volumePerDose_mL;          // Volume dispensed per dose (mL) - for user monitoring
};

/**
 * @brief Configuration for pH Controller (index 43)
 */
struct pHControllerConfig {
    bool isActive;                   // Controller slot in use
    char name[40];                   // User-defined name
    bool enabled;                    // Enable/disable controller (runtime only, not saved)
    bool showOnDashboard;            // Dashboard visibility
    
    // Input Connection
    uint8_t pvSourceIndex;           // pH sensor index (typically 70-99 for Hamilton pH probes)
    
    // Control Configuration
    float setpoint;                  // Target pH
    float deadband;                  // Hysteresis around setpoint to prevent oscillation
    
    // Dosing Configuration
    pHDosingConfig acidDosing;       // Acid dosing (when pH too high)
    pHDosingConfig alkalineDosing;   // Alkaline dosing (when pH too low)
};

/**
 * @brief Configuration for Flow Controllers (indices 44-47)
 * 3 Feed pumps + 1 Waste pump
 * Open-loop flow control using timed dosing cycles
 */
#define MAX_FLOW_CONTROLLERS 4

struct FlowControllerConfig {
    bool isActive;                          // Controller slot in use
    char name[40];                          // User-defined name (e.g., "Feed Pump 1", "Waste Pump")
    bool enabled;                           // Enable/disable controller (runtime only, not saved)
    bool showOnDashboard;                   // Dashboard visibility
    
    // Flow Control Configuration
    float flowRate_mL_min;                  // Target flow rate in mL/min (THE SETPOINT)
    
    // Output Configuration
    uint8_t outputType;                     // 0=Digital output, 1=DC motor
    uint8_t outputIndex;                    // Digital output (21-25) or DC motor (27-30)
    uint8_t motorPower;                     // Motor power level (0-100%), ignored if digital
    
    // Flow Calibration (user-provided)
    uint16_t calibrationDoseTime_ms;        // Dose time used during calibration
    uint8_t calibrationMotorPower;          // Motor power during calibration (0-100%)
    float calibrationVolume_mL;             // Volume delivered at calibration settings
    
    // Safety Limits
    uint32_t minDosingInterval_ms;          // Minimum time between doses (safety)
    uint16_t maxDosingTime_ms;              // Maximum dose time per cycle (safety)
};

/**
 * @brief DO Profile Point - one point in the control curve
 * Stored on SYS MCU, sent to IO MCU when profile is activated
 */
struct DOProfilePoint {
    float error_mg_L;           // X: setpoint - current DO (mg/L)
    float stirrerOutput;        // Y1: stirrer speed (% for DC motor, RPM for stepper)
    float mfcOutput_mL_min;     // Y2: MFC flow rate (mL/min)
};

/**
 * @brief DO Profile Configuration
 * Maximum of 3 user-defined profiles
 * Each profile can have 10-20 points
 */
#define MAX_DO_PROFILES 3
#define MAX_DO_PROFILE_POINTS 20

struct DOProfileConfig {
    bool isActive;              // Profile slot in use
    char name[40];              // User-defined profile name
    uint8_t numPoints;          // Number of points in profile (10-20)
    DOProfilePoint points[MAX_DO_PROFILE_POINTS];  // Profile curve points
};

/**
 * @brief Configuration for DO Controller (index 48)
 * Single controller for dissolved oxygen control
 * Uses profile-based control with linear interpolation
 */
struct DOControllerConfig {
    bool isActive;              // Controller in use
    char name[40];              // User-defined name
    bool enabled;               // Enable/disable controller (runtime only, not saved)
    bool showOnDashboard;       // Dashboard visibility
    float setpoint_mg_L;        // Target DO in mg/L
    
    // Active profile (0-2, references doProfiles array)
    uint8_t activeProfileIndex; // Index of active profile (0-2)
    
    // Stirrer output configuration
    bool stirrerEnabled;
    uint8_t stirrerType;        // 0=DC motor, 1=stepper
    uint8_t stirrerIndex;       // Motor index: 27-30 for DC, 26 for stepper
    float stirrerMaxRPM;        // For stepper: maximum RPM (ignored for DC motor)
    
    // MFC output configuration
    bool mfcEnabled;
    uint8_t mfcDeviceIndex;     // Device index (50-69) of Alicat MFC
};

/**
 * @brief Configuration for COM ports (serial communication)
 * Ports: 0-1 = RS-232, 2-3 = RS-485
 */
#define MAX_COM_PORTS 4

struct ComPortConfig {
    char name[32];
    uint32_t baudRate;       // 1200, 2400, 4800, 9600, 19200, 38400, 57600, 115200
    uint8_t dataBits;        // Fixed to 8 for Modbus
    float stopBits;          // 1 or 2
    uint8_t parity;          // 0=none, 1=odd, 2=even
    bool enabled;
    bool showOnDashboard;
};

/**
 * @brief Device interface types
 */
enum DeviceInterfaceType : uint8_t {
    DEVICE_INTERFACE_MODBUS_RTU = 0,
    DEVICE_INTERFACE_ANALOGUE_IO = 1,
    DEVICE_INTERFACE_MOTOR_DRIVEN = 2
};

/**
 * @brief Device driver types
 */
enum DeviceDriverType : uint8_t {
    // Modbus drivers (0-9)
    DEVICE_DRIVER_HAMILTON_PH = 0,
    DEVICE_DRIVER_HAMILTON_DO = 1,
    DEVICE_DRIVER_HAMILTON_OD = 2,
    DEVICE_DRIVER_ALICAT_MFC = 3,
    
    // Analogue IO drivers (10-19)
    DEVICE_DRIVER_PRESSURE_CONTROLLER = 10,
    
    // Motor driven drivers (20-29)
    DEVICE_DRIVER_STIRRER = 20,
    DEVICE_DRIVER_PUMP = 21
};

/**
 * @brief Configuration for peripheral devices
 * Dynamic Device Configuration
 * Maximum of 20 user-created devices
 * Sensor indices: 70-99 (30 slots)
 * Control indices: 50-69 (20 slots, calculated as sensorIndex - 20)
 */
#define MAX_DEVICES 20
#define DYNAMIC_INDEX_START 70
#define DYNAMIC_INDEX_END 99

struct DeviceConfig {
    bool isActive;                      // Is this slot in use?
    uint8_t dynamicIndex;               // Dynamic sensor index (70-99, 0xFF if not assigned)
    DeviceInterfaceType interfaceType;
    DeviceDriverType driverType;
    char name[40];                      // User-defined device name (matches IPC limit)
    
    // Interface-specific parameters
    union {
        // Modbus RTU device parameters
        struct {
            uint8_t portIndex;          // COM port index (0-3)
            uint8_t slaveID;            // Modbus slave ID (1-247)
        } modbus;
        
        // Analogue IO device parameters
        struct {
            uint8_t dacOutputIndex;     // DAC output index (0-1)
            char unit[8];               // Pressure unit (e.g., "bar", "kPa", "psi")
            float scale;                // Calibration scale factor (Pa/mV)
            float offset;               // Calibration offset (Pa)
        } analogueIO;
        
        // Motor driven device parameters
        struct {
            bool usesStepper;           // true = stepper motor (index 26), false = DC motor (27-30)
            uint8_t motorIndex;         // Motor index: 26 for stepper, 27-30 for DC
        } motorDriven;
    };
    
    // Device-specific parameters (outside union, applies to all devices)
    float maxFlowRate_mL_min;           // For Alicat MFC: maximum flow rate capability
};

/**
 * @brief Main IO configuration structure
 */
struct IOConfig {
    uint8_t magicNumber;
    uint8_t version;
    
    // Fixed hardware configurations
    ADCInputConfig adcInputs[MAX_ADC_INPUTS];
    DACOutputConfig dacOutputs[MAX_DAC_OUTPUTS];
    RTDSensorConfig rtdSensors[MAX_RTD_SENSORS];
    GPIOConfig gpio[MAX_GPIO];
    DigitalOutputConfig digitalOutputs[MAX_DIGITAL_OUTPUTS];
    StepperMotorConfig stepperMotor;  // Single stepper motor
    DCMotorConfig dcMotors[MAX_DC_MOTORS];
    EnergySensorConfig energySensors[MAX_ENERGY_SENSORS];  // Indices 31-32
    TemperatureControllerConfig tempControllers[MAX_TEMP_CONTROLLERS];  // Indices 40-42
    pHControllerConfig phController;  // Index 43 (single controller)
    FlowControllerConfig flowControllers[MAX_FLOW_CONTROLLERS];  // Indices 44-47 (3 feed + 1 waste)
    DOControllerConfig doController;  // Index 48 (single controller)
    DOProfileConfig doProfiles[MAX_DO_PROFILES];  // User-defined DO control profiles (3 max)
    ComPortConfig comPorts[MAX_COM_PORTS];  // RS-232 (0-1) and RS-485 (2-3)
    
    // Dynamic peripheral devices (sensor indices 70-99, control indices 50-69)
    DeviceConfig devices[MAX_DEVICES];
    
    // Device sensor object configurations (indices 70-99)
    DeviceSensorConfig deviceSensors[MAX_DEVICE_SENSORS];
};

// Function prototypes
bool loadIOConfig();
void saveIOConfig();
void setDefaultIOConfig();
void printIOConfig();
void pushIOConfigToIOmcu();  // Push config to IO MCU via IPC

// Device management helpers
int8_t allocateDynamicIndex(DeviceDriverType driverType); // Allocate consecutive indices for device type, returns -1 if not enough space
int8_t allocateDynamicIndex();           // Legacy: Allocate single index (70-99), returns -1 if full
void freeDynamicIndex(uint8_t index);    // Free a dynamic index (and all its sub-indices)
bool isDynamicIndexInUse(uint8_t index); // Check if an index is in use
int8_t findDeviceByIndex(uint8_t dynamicIndex); // Find device array position by index, returns -1 if not found

/**
 * @brief Get the control object index for a device
 * 
 * Calculates the correct control index based on device type and architecture:
 * - Control-only devices (e.g., Pressure Controller): dynamicIndex IS the control index (50-69)
 * - Sensor+Control devices (e.g., pH, DO, MFC): control index = dynamicIndex - 20 (70-99 → 50-69)
 * 
 * @param device Pointer to device configuration
 * @return Control object index (50-69), or dynamicIndex if device is NULL
 */
uint8_t getDeviceControlIndex(const DeviceConfig* device);

// Global configuration instance
extern IOConfig ioConfig;
