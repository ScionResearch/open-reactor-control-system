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
#define MAX_TEMP_CONTROLLERS 3  // Indices 40-42 (Temperature Control loops)

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
