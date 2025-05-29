#pragma once

#include "sys_init.h"

struct OutputDriver_t {
    DigitalOutput_t *outputObj[4];
    int pin[4];
};

extern OutputDriver_t outputDriver;
extern DigitalOutput_t digitalOutput[4];

void output_init(void);
void output_update(void);

