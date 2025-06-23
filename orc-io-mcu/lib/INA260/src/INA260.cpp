#include "INA260.h"

// Constructors -------------------------------------------------------------|
INA260::INA260(uint8_t I2C_dev_address, TwoWire *wire) {
    _I2C_dev_address = I2C_dev_address;
    _wire = wire;
    _irqPin = -1;
}

INA260::INA260(uint8_t I2C_dev_address, TwoWire *wire, int irqPin) {
    _I2C_dev_address = I2C_dev_address;
    _wire = wire;
    _irqPin = irqPin;
}

// Public methods -----------------------------------------------------------|

bool INA260::begin() {
    _wire->begin();

    uint16_t manufacturer_id = _read_16(INA260_REG_MANUFACTURER_ID, 100);
    if (_ina260_debug) Serial.printf("INA260 Manufacturer ID: 0x%04X\n", manufacturer_id);
    if (manufacturer_id != INA260_MANUFACTURER_ID) return false;

    uint16_t device_id = _read_16(INA260_REG_DEVICE_ID, 100);
    if (_ina260_debug) Serial.printf("INA260 Device ID: 0x%04X\n", device_id);
    if (device_id != INA260_DEVICE_ID) return false;

    _initialised = true;
    return true;
}

bool INA260::reset(void) {
    if (!_write_16(INA260_REG_CONFIG, INA260_RESET_bm)) return false;
    delay(10);
    return begin();
}

void INA260::set_irq_cb(void (*cb)()) {
    if (_irqPin < 0) return;
    _irq_cb = cb;
    pinMode(_irqPin, INPUT_PULLUP);
    attachInterrupt(_irqPin, _irq_cb, FALLING);
}

bool INA260::setMode(INA260_MODE mode) {
    uint16_t config_reg = _read_16(INA260_REG_CONFIG, 100);
    config_reg &= ~INA260_MODE_MASK;
    config_reg |= static_cast<uint16_t>(mode);
    return _write_16(INA260_REG_CONFIG, config_reg);
}

bool INA260::setAverage(INA260_AVERAGE avg) {
    uint16_t config_reg = _read_16(INA260_REG_CONFIG, 100);
    config_reg &= ~INA260_AVG_MASK;
    config_reg |= static_cast<uint16_t>(avg);
    return _write_16(INA260_REG_CONFIG, config_reg);
}

bool INA260::setVoltageConversionTime(INA260_V_CONV_TIME busTime) {
    uint16_t config_reg = _read_16(INA260_REG_CONFIG, 100);
    config_reg &= ~INA260_VBUSCT_MASK;
    config_reg |= static_cast<uint16_t>(busTime);
    return _write_16(INA260_REG_CONFIG, config_reg);
}

bool INA260::setCurrentConversionTime(INA260_I_CONV_TIME shuntTime) {
    uint16_t config_reg = _read_16(INA260_REG_CONFIG, 100);
    config_reg &= ~INA260_ISHCT_MASK;
    config_reg |= static_cast<uint16_t>(shuntTime);
    return _write_16(INA260_REG_CONFIG, config_reg);
}

bool INA260::setOverCurrentLimit(uint16_t mA) {
    uint16_t alert_val = mA / INA260_LSB_CURRENT_mA;
    
    if (!_write_16(INA260_REG_MASK_ENABLE, INA260_OCL_bm)) return false;
    if (!_write_16(INA260_REG_ALERT_LIMIT, alert_val)) return false;
    return true;
}

bool INA260::setUnderCurrentLimit(uint16_t mA) {
    uint16_t alert_val = mA / INA260_LSB_CURRENT_mA;
    
    if (!_write_16(INA260_REG_MASK_ENABLE, INA260_UCL_bm)) return false;
    if (!_write_16(INA260_REG_ALERT_LIMIT, alert_val)) return false;
    return true;
}

bool INA260::setOverVoltLimit(uint16_t mV) {
    uint16_t alert_val = mV / INA260_LSB_VOLTAGE_mV;
    
    if (!_write_16(INA260_REG_MASK_ENABLE, INA260_BOL_bm)) return false;
    if (!_write_16(INA260_REG_ALERT_LIMIT, alert_val)) return false;
    return true;
}

bool INA260::setUnderVoltLimit(uint16_t mV) {
    uint16_t alert_val = mV / INA260_LSB_VOLTAGE_mV;
    
    if (!_write_16(INA260_REG_MASK_ENABLE, INA260_BUL_bm)) return false;
    if (!_write_16(INA260_REG_ALERT_LIMIT, alert_val)) return false;
    return true;
}

bool INA260::setOverPowerLimit(uint16_t mW) {
    uint16_t alert_val = mW / INA260_LSB_POWER_mW;
    
    if (!_write_16(INA260_REG_MASK_ENABLE, INA260_POL_bm)) return false;
    if (!_write_16(INA260_REG_ALERT_LIMIT, alert_val)) return false;
    return true;
}

bool INA260::setConversionReadyFlag() {
    if (!_write_16(INA260_REG_MASK_ENABLE, INA260_CNVR_bm)) return false;
    return true;
}

float INA260::amps(void) {
    return (_read_16(INA260_REG_CURRENT, 100) * INA260_LSB_CURRENT_mA) / 1000.0;
}

float INA260::volts(void) {
    return (_read_16(INA260_REG_VOLTAGE, 100) * INA260_LSB_VOLTAGE_mV) / 1000.0;
}

float INA260::watts(void) {
    return (_read_16(INA260_REG_POWER, 100) * INA260_LSB_POWER_mW) / 1000.0;
}

float INA260::milliamps(void) {
    return _read_16(INA260_REG_CURRENT, 100) * INA260_LSB_CURRENT_mA;
}

float INA260::millivolts(void) {
    return _read_16(INA260_REG_VOLTAGE, 100) * INA260_LSB_VOLTAGE_mV;
}

float INA260::milliwatts(void) {
    return _read_16(INA260_REG_POWER, 100) * INA260_LSB_POWER_mW;
}

// Private methods ----------------------------------------------------------|
bool INA260::_write_16(uint8_t reg_addr, uint16_t data) {
    uint8_t buf[2] = {
        static_cast<uint8_t>(data >> 8),
        static_cast<uint8_t>(data & 0xFF)
    };
    _wire->beginTransmission(_I2C_dev_address);
    _wire->write(reg_addr);
    _wire->write(buf, 2);
    if (_wire->endTransmission(true)) return false;
    return true;
}
uint16_t INA260::_read_16(uint8_t reg_addr, uint16_t timeout_ms) {
  Wire.beginTransmission(_I2C_dev_address);
  Wire.write(reg_addr);
  if (Wire.endTransmission(false) != 0) {
    return 0xFFFF; // Error: NACK or bus fault
  }

  Wire.requestFrom(_I2C_dev_address, (uint8_t)2);

  uint32_t start = millis();
  while (Wire.available() < 2) {
    if ((millis() - start) > timeout_ms) {
      return 0xFFFF; // Error: Timeout
    }
  }

  uint8_t msb = Wire.read();
  uint8_t lsb = Wire.read();

  return ((uint16_t)msb << 8) | lsb;
}