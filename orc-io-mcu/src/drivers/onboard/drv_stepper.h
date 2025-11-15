#pragma once

#include "sys_init.h"

#include "TMC5130.h"

struct StepperDriver_t {
    TMC5130 *stepper;
    StepperDevice_t *device;
    bool ready;
    bool fault;
    bool newMessage;
    char message[100];
};

extern StepperDriver_t stepperDriver;
extern StepperDevice_t stepperDevice;

bool stepper_init(void);
void stepper_update(void);
bool stepper_update_cfg(bool setParams);
