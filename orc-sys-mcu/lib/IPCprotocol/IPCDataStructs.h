#ifndef IPC_DATA_STRUCTS_H
#define IPC_DATA_STRUCTS_H

#include <stdint.h>

// Sensor Data Structures
struct PowerSensor {
    float volts;
    float amps;
    float watts;
    bool online;
};

struct TemperatureSensor {
    float celcius;
    bool online;
};

struct PHSensor {
    float pH;
    bool online;
};

struct DissolvedOxygenSensor {
    float oxygen;
    bool online;
};

struct OpticalDensitySensor {
    float OD;
    bool online;
};

struct GasFlowSensor {
    float mlPerMinute;
    bool online;
};

struct PressureSensor {
    float kPa;
    bool online;
};

struct StirrerSpeedSensor {
    float rpm;
    bool online;
};

struct WeightSensor {
    float grams;
    bool online;
};

// Control Object Structures
struct TemperatureControl {
    float sp_celcius;
    bool enabled;
    float kp;
    float ki;
    float kd;
};

struct PHControl {
    float sp_pH;
    bool enabled;
    float period;
    float max_dose_time;
};

struct DissolvedOxygenControl {
    float sp_oxygen;
    bool enabled;
    float DOstirrerLUT[2][10];  // Lookup table for stirrer control
    float DOgasLUT[2][10];      // Lookup table for gas control
};

struct GasFlowControl {
    float sp_mlPerMinute;
    bool enabled;
    float kp;
    float ki;
    float kd;
};

struct StirrerSpeedControl {
    float sp_rpm;
    bool enabled;
    float kp;
    float ki;
    float kd;
};

struct PumpSpeedControl {
    float percent;
};

struct FeedControl {
    float period;
    float duty;
    bool enabled;
};

struct WasteControl {
    float period;
    float duty;
    bool enabled;
};

// Message IDs for each type of data
enum MessageTypes {
    // Sensor message IDs (1-50)
    MSG_POWER_SENSOR = 1,
    MSG_TEMPERATURE_SENSOR = 2,
    MSG_PH_SENSOR = 3,
    MSG_DO_SENSOR = 4,
    MSG_OD_SENSOR = 5,
    MSG_GAS_FLOW_SENSOR = 6,
    MSG_PRESSURE_SENSOR = 7,
    MSG_STIRRER_SPEED_SENSOR = 8,
    MSG_WEIGHT_SENSOR = 9,

    // Control message IDs (51-100)
    MSG_TEMPERATURE_CONTROL = 51,
    MSG_PH_CONTROL = 52,
    MSG_DO_CONTROL = 53,
    MSG_GAS_FLOW_CONTROL = 54,
    MSG_STIRRER_SPEED_CONTROL = 55,
    MSG_PUMP_SPEED_CONTROL = 56,
    MSG_FEED_CONTROL = 57,
    MSG_WASTE_CONTROL = 58
};

#endif /* IPC_DATA_STRUCTS_H */
