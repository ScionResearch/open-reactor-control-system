#include "MCP48FEB.h"

// Constructor
MCP48FEBxx::MCP48FEBxx(int cs_pin) {
    _dac_cs_pin = cs_pin;
    _dac_lat_pin = -1;
    _dac_spi = &SPI;
    _initialised = false;
}

MCP48FEBxx::MCP48FEBxx(int cs_pin, int lat_pin, SPIClass *spi) {
    _dac_cs_pin = cs_pin;
    _dac_lat_pin = lat_pin;
    _dac_spi = spi;
    _initialised = false;
}

// Initialisation
bool MCP48FEBxx::begin(void) {
    pinMode(_dac_cs_pin, OUTPUT);
    digitalWrite(_dac_cs_pin, HIGH);
    if (_dac_lat_pin != -1) {
        pinMode(_dac_lat_pin, OUTPUT);
        digitalWrite(_dac_lat_pin, LOW);    // Write new DAC values immediately
    }
    _dac_spi->begin();
    uint16_t data = 0;
    _initialised = true;
    bool valid = readRegister(MCP48FEBxx_REG_GAIN_STATUS, &data);
    if (!valid) _initialised = false;
    return valid;
}

// Configuration functions
bool MCP48FEBxx::setVREF(uint8_t channel, MCP48FEBxx_VREF vref) {
    if (channel > 1) return false;
    uint16_t data = 0;
    if (!readRegister(MCP48FEBxx_REG_VREF, &data)) return false;
    data &= ~(3 << (channel * 2));
    data |= vref << (channel * 2);
    return writeRegister(MCP48FEBxx_REG_VREF, data);
}

bool MCP48FEBxx::setPD(uint8_t channel, MCP48FEBxx_PD pd) {
    if (channel > 1) return false;
    uint16_t data = 0;
    if (!readRegister(MCP48FEBxx_REG_POWERDOWN, &data)) return false;
    data &= ~(3 << (channel * 2));
    data |= pd << (channel * 2);
    return writeRegister(MCP48FEBxx_REG_POWERDOWN, data);
}

bool MCP48FEBxx::setGain(uint8_t channel, MCP48FEBxx_GAIN gain) {
    if (channel > 1) return false;
    uint16_t data = 0;
    if (!readRegister(MCP48FEBxx_REG_GAIN_STATUS, &data)) return false;
    data &= ~(1 << (channel + MCP48FEBxx_GAIN_0_bp));
    data |= gain << (channel + MCP48FEBxx_GAIN_0_bp);
    return writeRegister(MCP48FEBxx_REG_GAIN_STATUS, data);
}

// Status functions
bool MCP48FEBxx::getPORStatus(void) {
    uint16_t data = 0;
    if (!readRegister(MCP48FEBxx_REG_GAIN_STATUS, &data)) return false;
    return ((data >> MCP48FEBxx_POR_bp) & 1);
}

bool MCP48FEBxx::getEEWAStatus(void) {
    uint16_t data = 0;
    if (!readRegister(MCP48FEBxx_REG_GAIN_STATUS, &data)) return false;
    return ((data >> MCP48FEBxx_EEWA_bp) & 1);
}

// DAC functions
bool MCP48FEBxx::writeDAC(uint8_t channel, uint16_t value) {
    if (channel > 1) return false;
    return writeRegister(channel, value);
}


uint16_t MCP48FEBxx::readDAC(uint8_t channel) {
    if (channel > 1) return false;
    uint16_t data = 0;
    readRegister(channel, &data);
    return data;
}

// Non-volatile config functions
bool MCP48FEBxx::setVREF_EEPROM(uint8_t channel, MCP48FEBxx_VREF vref) {
    if (channel > 1) return false;
    if (getEEWAStatus()) return false;  // EEPROM write is in progress
    return writeRegister(MCP48FEBxx_REG_NV_VREF, vref << (channel * 2));
}

bool MCP48FEBxx::setPD_EEPROM(uint8_t channel, MCP48FEBxx_PD pd) {
    if (channel > 1) return false;
    if (getEEWAStatus()) return false;  // EEPROM write is in progress
    return writeRegister(MCP48FEBxx_REG_NV_POWERDOWN, pd << (channel * 2));
}

bool MCP48FEBxx::setGain_EEROM(uint8_t channel, MCP48FEBxx_GAIN gain) {
    if (channel > 1) return false;
    if (getEEWAStatus()) return false;  // EEPROM write is in progress
    return writeRegister(MCP48FEBxx_REG_NV_GAIN, gain << (channel + MCP48FEBxx_GAIN_0_bp));
}

bool MCP48FEBxx::writeDAC_EEPROM(uint8_t channel, uint16_t value) {
    if (channel > 1) return false;
    if (getEEWAStatus()) return false;  // EEPROM write is in progress
    return writeRegister(channel + MCP48FEBxx_REG_NV_DAC0, value);
}

int MCP48FEBxx::saveRegistersToEEPROM(void) {
    uint16_t data = 0;
    // DAC channel 0 VM read -> NVM write
    if (!readRegister(MCP48FEBxx_REG_DAC0, &data)) return -1;
    if (!waitForEEWA()) return -2;
    if (!writeRegister(MCP48FEBxx_REG_NV_DAC0, data)) return -3;

    // DAC channel 1 VM read -> NVM write
    if (!readRegister(MCP48FEBxx_REG_DAC1, &data)) return -4;
    if (!waitForEEWA()) return -5;
    if (!writeRegister(MCP48FEBxx_REG_NV_DAC1, data)) return -6;

    // Powerdown read -> NVM write
    if (!readRegister(MCP48FEBxx_REG_POWERDOWN, &data)) return -7;
    if (!waitForEEWA()) return -8;
    if (!writeRegister(MCP48FEBxx_REG_NV_POWERDOWN, data)) return -9;

    // Gain read -> NVM write
    if (!readRegister(MCP48FEBxx_REG_GAIN_STATUS, &data)) return -10;
    if (!waitForEEWA()) return -11;
    if (!writeRegister(MCP48FEBxx_REG_NV_GAIN, data)) return -12;

    // VREF read -> NVM write
    if (!readRegister(MCP48FEBxx_REG_VREF, &data)) return -13;
    if (!waitForEEWA()) return -14;
    if (!writeRegister(MCP48FEBxx_REG_NV_VREF, data)) return -15;

    return 1;
}

bool MCP48FEBxx:: readRegister(uint8_t reg, uint16_t *data) {
    if (!_initialised) return false;
    uint8_t cmd_valid = 0;
    uint8_t cmd = reg << MCP48FEBxx_REG_ADDRESS_bp | MCP48FEBxx_CMD_READ << MCP48FEBxx_CMD_bp;
    _dac_spi->beginTransaction(SPISettings(MCP48FEBxx_SPI_SPEED, MSBFIRST, SPI_MODE0));
    digitalWrite(_dac_cs_pin, LOW);
    cmd_valid = _dac_spi->transfer(cmd);
    cmd_valid &= 1;
    if (cmd_valid) *data = _dac_spi->transfer16(0);
    digitalWrite(_dac_cs_pin, HIGH);
    _dac_spi->endTransaction();
    return (bool)cmd_valid;
}

bool MCP48FEBxx:: writeRegister(uint8_t reg, uint16_t data) {
    if (!_initialised) return false;
    uint8_t cmd_valid = 0;
    uint8_t cmd = reg << MCP48FEBxx_REG_ADDRESS_bp | MCP48FEBxx_CMD_WRITE << MCP48FEBxx_CMD_bp;
    _dac_spi->beginTransaction(SPISettings(MCP48FEBxx_SPI_SPEED, MSBFIRST, SPI_MODE0));
    digitalWrite(_dac_cs_pin, LOW);
    cmd_valid = _dac_spi->transfer(cmd);
    cmd_valid &= 1;
    if (cmd_valid) _dac_spi->transfer16(data);
    digitalWrite(_dac_cs_pin, HIGH);
    _dac_spi->endTransaction();
    return (bool)cmd_valid;
}

// Private functions
bool MCP48FEBxx::waitForEEWA(void) {
    bool busy = true;
    uint32_t start = millis();
    while (busy && (millis() - start) < MCP48FEBxx_EEPROM_MAX_WAIT_ms) {
        busy = getEEWAStatus();
        delayMicroseconds(100);
    }
    return !busy;
}