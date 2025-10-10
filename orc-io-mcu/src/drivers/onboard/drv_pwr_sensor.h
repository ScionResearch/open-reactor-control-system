#pragma once

#include "sys_init.h"

#include "INA260.h"

struct PowerSensorDriver_t {
    INA260 *sensor;
    float updateInterval;
};

extern VoltageSensor_t pwr_voltage[2];
extern CurrentSensor_t pwr_current[2];
extern PowerSensor_t pwr_power[2];
extern PowerSensorDriver_t pwr_interface[2];

bool pwrSensor_init(void);
bool pwrSensor_update(void);