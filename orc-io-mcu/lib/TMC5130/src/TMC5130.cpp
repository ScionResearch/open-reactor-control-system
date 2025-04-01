#include "TMC5130.h"

// Constructor
TMC5130::TMC5130(int cs_pin) {
    _cs_pin = cs_pin;
    _spi = &SPI;
    _initialised = false;
}

TMC5130::TMC5130(int cs_pin, SPIClass *spi) {
    _cs_pin = cs_pin;
    _spi = spi;
    _initialised = false;
}

// Initialisation
bool TMC5130::begin(void) {
    pinMode(_cs_pin, OUTPUT);
    digitalWrite(_cs_pin, HIGH);
    _spi->begin();
    _initialised = true;
    return true;
}

// Read write functions
uint8_t TMC5130::readRegister(uint8_t reg, uint32_t *data) {
    if (!_initialised) return 0;
    uint8_t status = 0;
    uint8_t buf[4] = {0, 0, 0, 0};
    _spi->beginTransaction(SPISettings(TMC5130_SPI_SPEED, MSBFIRST, SPI_MODE3));

    // Send read datagram (40-bit)
    digitalWrite(_cs_pin, LOW);
    _spi->transfer(reg);
    _spi->transfer(buf, 4);
    digitalWrite(_cs_pin, HIGH);
    delayMicroseconds(1);

    // Get data
    digitalWrite(_cs_pin, LOW);
    status = _spi->transfer(reg);
    for (int i = 0; i < 4; i++) buf[i] = _spi->transfer(0);
    digitalWrite(_cs_pin, HIGH);
    _spi->endTransaction();
    *data = (uint32_t)buf[0] << 24 | (uint32_t)buf[1] << 16 | (uint32_t)buf[2] << 8 | (uint32_t)buf[3];
    return status;
}

uint8_t TMC5130::writeRegister(uint8_t reg, uint32_t data) {
    if (!_initialised) return 0;
    reg |= 0x80; // Set write bit
    uint8_t status = 0;
    uint8_t buf[4];
    buf[0] = data >> 24;
    buf[1] = data >> 16;
    buf[2] = data >> 8;
    buf[3] = data;
    Serial.printf("Write to register 0x%02X data: 0x%02X%02X%02X%02X\n", reg, buf[0], buf[1], buf[2], buf[3]);
    _spi->beginTransaction(SPISettings(TMC5130_SPI_SPEED, MSBFIRST, SPI_MODE3));

    // Send write datagram (40-bit)
    digitalWrite(_cs_pin, LOW);
    status = _spi->transfer(reg);
    _spi->transfer(buf, 4);
    digitalWrite(_cs_pin, HIGH);
    delayMicroseconds(10);
    _spi->endTransaction();
    return status;
}