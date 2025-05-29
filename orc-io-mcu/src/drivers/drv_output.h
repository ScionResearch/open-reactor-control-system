#pragma once

#include "sys_init.h"

// Slow PWM for hetaer output - set period to get 1Hz: PER = (120MHz / 1024 / 1Hz) - 1 = 117187
#define HEATER_PWM_PERIOD 117187

struct OutputDriver_t {
    DigitalOutput_t *outputObj[5];
    int pin[5];
};

extern OutputDriver_t outputDriver;
extern DigitalOutput_t digitalOutput[4];
extern DigitalOutput_t heaterOutput[1];

void output_init(void);
void output_update(void);

