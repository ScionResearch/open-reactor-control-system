#include "DRV8235.h"

// Constructors -------------------------------------------------------------|
DRV8235::DRV8235(uint8_t I2C_dev_address, TwoWire *wire) {
    _I2C_dev_address = I2C_dev_address;
    _wire = wire;
    _faultPin = -1;
    _currentPin = -1;
    _initialised = false;
}

DRV8235::DRV8235(uint8_t I2C_dev_address, TwoWire *wire, int faultPin, int currentPin) {
    _I2C_dev_address = I2C_dev_address;
    _wire = wire;
    _faultPin = faultPin;
    _currentPin = currentPin;
    _initialised = false;
}

// Public methods -----------------------------------------------------------|

bool DRV8235::begin(void)
{

    analogReference(AR_INTERNAL2V0);
    analogReadResolution(12);

    if (drv8235_debug) Serial.println(F("DRV8235 Wire begin"));
    _wire->begin();
    uint8_t config_data = 0;

    // Config 0 - disable motor and clear PoR flag
    if (drv8235_debug) Serial.println(F("Config 0 - disable motor and clear PoR flag"));
    config_data = (1 << DRV8235_CLR_FLT_bp);
    _write_byte(DRV8325_CONFIG0, config_data);

    //Regulator Control 2 (Prog Duty)
    if (drv8235_debug) Serial.println(F("Regulator Control 2 (Prog Duty)"));
    config_data = 0;
    if (!_write_byte(DRV8325_REG_CTRL2, config_data)) return false;

    // NOTE: To set direction IN1 and IN2 should be complementary

    // Config 4
    if (drv8235_debug) Serial.println(F("Config 4"));
    config_data = (0 << DRV8235_PMODE_bp) | (1 << DRV8235_I2C_BC_bp) | (1 << DRV8235_I2C_EN_IN1_bp) | (0 << DRV8235_I2C_PH_IN2_bp);
    if (!_write_byte(DRV8325_CONFIG4, config_data)) return false;

    // Regulator Control 0
    if (drv8235_debug) Serial.println(F("Regulator Control 0"));
    config_data = DRV8235_VOLTAGE_REG_bm | (0 << DRV8235_PWM_FREQ_bp);
    if (!_write_byte(DRV8325_REG_CTRL0, config_data)) return false;

    // Regulator Control 1 (WSET VSET)
    if (drv8235_debug) Serial.println(F("Regulator Control 1 (WSET VSET)"));
    config_data = 0;
    if (!_write_byte(DRV8325_REG_CTRL1, config_data)) return false;

    _initialised = true;
    if (drv8235_debug) Serial.println(F("DRV8235 initialised"));

    return true;
}

void DRV8235::set_fault_cb(void (*cb)()) {
    _fault_cb = cb;
    if (_faultPin >= 0) {
        pinMode(_faultPin, INPUT_PULLUP);
        attachInterrupt(_faultPin, _fault_cb, FALLING);
        _fault_cb_set = true;
    }
}

uint8_t DRV8235::read_status(void) {
    uint8_t flt_reg = _read_byte(DRV8325_FAULT_STATUS);
    if (flt_reg > 0) {
        faultActive = true;
        fault = (flt_reg >> DRV8235_FAULT_bp) & 1;
        stall = (flt_reg >> DRV8235_STALL_bp) & 1;
        overCurrent = (flt_reg >> DRV8235_OCP_bp) & 1;
        overVoltage = (flt_reg >> DRV8235_OVP_bp) & 1;
        overTemperature = (flt_reg >> DRV8235_TSD_bp) & 1;
        powerOnReset = (flt_reg >> DRV8235_NPOR_bp) & 1;

        // Reset fault flag
        uint8_t config_data = _read_byte(DRV8325_CONFIG0);
        config_data |= (1 << DRV8235_CLR_FLT_bp);
        _write_byte(DRV8325_CONFIG0, config_data);
    }
    return flt_reg;
}

void DRV8235::manage(void) {
    if (!_fault_cb_set) read_status();   // Read status manually if the fault pin interrupt is not used
    if (_currentPin < 0) return;

    _current_sample[_current_sample_ptr] = analogRead(_currentPin);
    float movingAverage = 0;
    for (uint8_t i = 0; i < 100; i++) movingAverage += _current_sample[i];
    _motor_current = static_cast<uint16_t>(movingAverage * (2000.0 / 409600.0));
    _current_sample_ptr++;
    if (_current_sample_ptr >= 100) _current_sample_ptr = 0;  // Wrap around
}

uint16_t DRV8235::motorCurrent(void) {
    return _motor_current;
}

uint8_t DRV8235::motorCurrentIC(void) {
    return _read_byte(DRV8325_REG_STATUS2);
}

uint8_t DRV8235::motorVoltageIC(void) {
    return _read_byte(DRV8325_REG_STATUS1);
}

uint8_t DRV8235::motorSpeedIC(void) {
    return _read_byte(DRV8325_RC_STATUS1);
}

bool DRV8235::setSpeed(uint8_t speed) {
    uint8_t config_data = 0;
    if (speed > 100) speed = 100;
    
    config_data = static_cast<uint8_t>(lround(speed * DRV8235_VSET_PERCENT_MUTIPLIER));
    if (!_write_byte(DRV8325_REG_CTRL1, config_data)) return false;
    return true;
}

bool DRV8235::setVoltage(float voltage) {
    uint8_t config_data = 0;
    if (voltage > 38) voltage = 38;
    
    config_data = static_cast<uint8_t>(lround(voltage / DRV8235_V_LSB));
    if (!_write_byte(DRV8325_REG_CTRL1, config_data)) return false;
    return true;
}

bool DRV8235::run(void) {
    uint8_t config_data = _read_byte(DRV8325_CONFIG0);
    config_data |= (1 << DRV8235_EN_OUT_bp);
    if (!_write_byte(DRV8325_CONFIG0, config_data)) return false;
    return true;
}

bool DRV8235::stop(void) {
    uint8_t config_data = _read_byte(DRV8325_CONFIG0);
    config_data &= ~(1 << DRV8235_EN_OUT_bp);
    if (!_write_byte(DRV8325_CONFIG0, config_data)) return false;
    return true;
}

bool DRV8235::direction(bool reverse) {
    uint8_t config_data = _read_byte(DRV8325_CONFIG4);
    bool dir = config_data & 1;
    if (dir != reverse) {
        config_data ^= 1;
        if (!_write_byte(DRV8325_CONFIG4, config_data)) return false;
    }
    return true;
}

// Private methods ----------------------------------------------------------|
bool DRV8235::_write_byte(uint8_t reg_addr, uint8_t data) {
    _wire->beginTransmission(_I2C_dev_address);
    _wire->write(reg_addr);
    _wire->write(data);
    if (_wire->endTransmission(true)) return false;
    return true;
}
 uint8_t DRV8235::_read_byte(uint8_t reg_addr) {
    _wire->beginTransmission(_I2C_dev_address);
    _wire->write(reg_addr);
    if (_wire->endTransmission(true)) return 0xFF;
    _wire->requestFrom(_I2C_dev_address, 1);
    return _wire->read();
}