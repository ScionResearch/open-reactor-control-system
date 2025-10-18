#pragma once

#include "sys_init.h"

struct GpioDriver_t {
    DigitalIO_t *gpioObj[8];
    DigitalIO_t *gpioExpObj[15];
    int pin[8];
    int expPin[15];
    bool configChanged = false;
};

extern GpioDriver_t gpioDriver;
extern DigitalIO_t gpio[8];
extern DigitalIO_t gpioExp[15];

// Manages 8 main GPIO plus expansion GPIO
void gpio_init(void);
void gpio_update(void);
void gpio_configure(uint8_t index, const char* name, uint8_t pullMode);