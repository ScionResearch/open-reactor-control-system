#pragma once

#include <Arduino.h>
#include <SPI.h>
#include "TMC5130_reg.h"

// SPI default speed 4MHz
#define TMC5130_SPI_SPEED 4000000

class TMC5130 {
    public:
        // Constructor
        TMC5130(int cs_pin);
        TMC5130(int cs_pin, SPIClass *spi);

        // Initialisation
        bool begin(void);

        // Read write functions
        uint8_t readRegister(uint8_t reg, uint32_t *data);
        uint8_t writeRegister(uint8_t reg, uint32_t data);

    private:
        int _cs_pin;
        SPIClass *_spi;
        bool _initialised;
};