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
        gpio[i].pullMode = 1;  // Default to pull-up
        pinMode(gpioDriver.pin[i], INPUT_PULLUP);
        gpio[i].output = false;
        gpio[i].state = false;
        gpio[i].fault = false;
        gpio[i].newMessage = false;
        gpio[i].message[0] = '\0';
        
        // Add to object index (fixed indices 13-20)
        objIndex[13 + i].type = OBJ_T_DIGITAL_INPUT;
        objIndex[13 + i].obj = &gpio[i];
        sprintf(objIndex[13 + i].name, "Input %d", i + 1);  // Match board labels (1-8)
        objIndex[13 + i].valid = true;
    }
    
    // Initialize spare GPIO (not indexed)
    for (int i = 0; i < 15; i++) {
        gpioDriver.gpioExpObj[i] = &gpioExp[i];
        gpioDriver.expPin[i] = pinsSpare[i];
        gpioExp[i].pullMode = 1;  // Default to pull-up
        pinMode(gpioDriver.expPin[i], INPUT_PULLUP);
        gpioExp[i].output = false;
        gpioExp[i].state = false;
        gpioExp[i].fault = false;
        gpioExp[i].newMessage = false;
        gpioExp[i].message[0] = '\0';
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
            if (gpioDriver.gpioObj[i]->output) {
                pinMode(gpioDriver.pin[i], OUTPUT);
            } else {
                // Set pull mode: 0=None, 1=Pull-up, 2=Pull-down
                if (gpioDriver.gpioObj[i]->pullMode == 1) {
                    pinMode(gpioDriver.pin[i], INPUT_PULLUP);
                } else if (gpioDriver.gpioObj[i]->pullMode == 2) {
                    pinMode(gpioDriver.pin[i], INPUT_PULLDOWN);
                } else {
                    pinMode(gpioDriver.pin[i], INPUT);  // High-Z
                }
            }
        }
        for (int i = 0; i < 15; i++) {
            if (gpioDriver.gpioExpObj[i]->output) {
                pinMode(gpioDriver.expPin[i], OUTPUT);
            } else {
                if (gpioDriver.gpioExpObj[i]->pullMode == 1) {
                    pinMode(gpioDriver.expPin[i], INPUT_PULLUP);
                } else if (gpioDriver.gpioExpObj[i]->pullMode == 2) {
                    pinMode(gpioDriver.expPin[i], INPUT_PULLDOWN);
                } else {
                    pinMode(gpioDriver.expPin[i], INPUT);
                }
            }
        }
    }
}

/**
 * @brief Configure a GPIO input with name and pull mode
 * @param index Object index (13-20 for main GPIO)
 * @param name Custom name for the input
 * @param pullMode 0=None (High-Z), 1=Pull-up, 2=Pull-down
 */
void gpio_configure(uint8_t index, const char* name, uint8_t pullMode) {
    // Validate index (13-20 for main GPIO)
    if (index < 13 || index >= 21) {
        Serial.printf("[GPIO] Invalid index %d for configuration\n", index);
        return;
    }
    
    uint8_t gpioIndex = index - 13;  // Convert to array index (0-7)
    
    // Update name in object index
    if (name != nullptr && name[0] != '\0') {
        strncpy(objIndex[index].name, name, sizeof(objIndex[index].name) - 1);
        objIndex[index].name[sizeof(objIndex[index].name) - 1] = '\0';
    }
    
    // Update pull mode if changed
    if (gpio[gpioIndex].pullMode != pullMode) {
        gpio[gpioIndex].pullMode = pullMode;
        gpioDriver.configChanged = true;  // Trigger pin reconfiguration
        
        const char* modeStr = (pullMode == 1) ? "PULL-UP" :
                              (pullMode == 2) ? "PULL-DOWN" : "HIGH-Z";
        Serial.printf("[GPIO] Input %d (%s) configured: %s\n", 
                      gpioIndex + 1, objIndex[index].name, modeStr);
    }
}