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
};

// Function prototypes
bool loadIOConfig();
void saveIOConfig();
void setDefaultIOConfig();
void printIOConfig();
void pushIOConfigToIOmcu();  // Push config to IO MCU via IPC

// Global configuration instance
extern IOConfig ioConfig;
