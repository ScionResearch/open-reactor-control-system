#pragma once

#include "sys_init.h"

#include "INA260.h"

struct PowerSensorDriver_t {
    INA260 *sensor;
    float updateInterval;
};

extern EnergySensor_t pwr_energy[2];
extern PowerSensorDriver_t pwr_interface[2];

bool pwrSensor_init(void);
bool pwrSensor_update(void);