#pragma once

#include "sys_init.h"

#include "INA260.h"

struct PowerSensorDriver_t {
    PowerSensor_t *powerObj;
    INA260 *sensor;
    float updateInterval;
};

extern PowerSensor_t pwr_sensor[2];
extern PowerSensorDriver_t pwr_interface[2];

bool pwrSensor_init(void);
bool pwrSensor_update(void);