#include "drv_gpio.h"

int pinsGPIO[8] {
    PIN_GPIO_0,
    PIN_GPIO_1,
    PIN_GPIO_2,
    PIN_GPIO_3,
    PIN_GPIO_4,
    PIN_GPIO_5,
    PIN_GPIO_6,
    PIN_GPIO_7
};
int pinsSpare[15] {
    PIN_SP_IO_0,
    PIN_SP_IO_1,
    PIN_SP_IO_2,
    PIN_SP_IO_3,
    PIN_SP_IO_4,
    PIN_SP_IO_5,
    PIN_SP_IO_6,
    PIN_SP_IO_7,
    PIN_SP_IO_8,
    PIN_SP_IO_9,
    PIN_SP_IO_10,
    PIN_SP_IO_11,
    PIN_SP_IO_12,
    PIN_SP_IO_13,
    PIN_SP_IO_14
};

GpioDriver_t gpioDriver;
DigitalIO_t gpio[8];
DigitalIO_t gpioExp[15];

void gpio_init(void) {
    for (int i = 0; i < 8; i++) {
        gpioDriver.gpioObj[i] = &gpio[i];
        gpioDriver.pin[i] = pinsGPIO[i];
        pinMode(gpioDriver.pin[i], INPUT_PULLUP);
        gpio[i].output = false;
        gpio[i].state = false;
        gpio[i].pullup = true;
    }
    for (int i = 0; i < 15; i++) {
        gpioDriver.gpioExpObj[i] = &gpioExp[i];
        gpioDriver.expPin[i] = pinsSpare[i];
        pinMode(gpioDriver.expPin[i], INPUT_PULLUP);
        gpioExp[i].output = false;
        gpioExp[i].state = false;
        gpioExp[i].pullup = true;
    }
}

void gpio_update(void) {
    for (int i = 0; i < 8; i++) {
        if (gpioDriver.gpioObj[i]->output) {
            digitalWrite(gpioDriver.pin[i], gpioDriver.gpioObj[i]->state);
        } else {
            gpioDriver.gpioObj[i]->state = digitalRead(gpioDriver.pin[i]);
        }
    }

    for (int i = 0; i < 15; i++) {
        if (gpioDriver.gpioExpObj[i]->output) {
            digitalWrite(gpioDriver.expPin[i], gpioDriver.gpioExpObj[i]->state);
        } else {
            gpioDriver.gpioExpObj[i]->state = digitalRead(gpioDriver.expPin[i]);
        }
    }

    // Check if configuration has changed
    if (gpioDriver.configChanged) {
        gpioDriver.configChanged = false;
        for (int i = 0; i < 8; i++) {
            pinMode(gpioDriver.pin[i], gpioDriver.gpioObj[i]->output ? OUTPUT : gpioDriver.gpioObj[i]->pullup ? INPUT_PULLUP : INPUT);
        }
        for (int i = 0; i < 15; i++) {
            pinMode(gpioDriver.expPin[i], gpioDriver.gpioExpObj[i]->output ? OUTPUT : gpioDriver.gpioExpObj[i]->pullup ? INPUT_PULLUP : INPUT);
        }
    }
}