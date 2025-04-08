#include "TMC5130.h"

// System variables - all exposed to user
TMC5130::config_t config;
TMC5130::status_t status;
TMC5130::register_t reg;

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

    // Write library defaults to TMC5130 registers
    writeRegister(TMC5130_REG_GCONF, reg.GCONF);
    writeRegister(TMC5130_REG_RAMPMODE, reg.RAMPMODE);
    writeRegister(TMC5130_REG_VSTART, reg.VSTART);
    writeRegister(TMC5130_REG_TZEROWAIT, reg.TZEROWAIT);
    writeRegister(TMC5130_REG_CHOPCONF, reg.CHOPCONF);

    // CoolStep setup temp
    writeRegister(TMC5130_REG_THIGH, 0x00000033);

    // Get register contents (readable registers)
    readRegister(TMC5130_REG_GCONF, &reg.GCONF);
    readRegister(TMC5130_REG_GSTAT, &reg.GSTAT);
    readRegister(TMC5130_REG_IFCNT, &reg.IFCNT);
    readRegister(TMC5130_REG_IOIN, &reg.IOIN);
    readRegister(TMC5130_REG_TSTEP, &reg.TSTEP);
    readRegister(TMC5130_REG_RAMPMODE, &reg.RAMPMODE);
    readRegister(TMC5130_REG_XACTUAL, &reg.XACTUAL);
    readRegister(TMC5130_REG_VACTUAL, &reg.VACTUAL);
    readRegister(TMC5130_REG_XTARGET, &reg.XTARGET);
    readRegister(TMC5130_REG_SW_MODE, &reg.SW_MODE);
    readRegister(TMC5130_REG_RAMP_STAT, &reg.RAMP_STAT);
    readRegister(TMC5130_REG_XLATCH, &reg.XLATCH);
    readRegister(TMC5130_REG_ENCMODE, &reg.ENCMODE);
    readRegister(TMC5130_REG_X_ENC, &reg.X_ENC);
    readRegister(TMC5130_REG_ENC_STATUS, &reg.ENC_STATUS);
    readRegister(TMC5130_REG_ENC_LATCH, &reg.ENC_LATCH);
    readRegister(TMC5130_REG_MSCNT, &reg.MSCNT);
    readRegister(TMC5130_REG_MSCURACT, &reg.MSCURACT);
    readRegister(TMC5130_REG_CHOPCONF, &reg.CHOPCONF);
    readRegister(TMC5130_REG_DRV_STATUS, &reg.DRV_STATUS);
    readRegister(TMC5130_REG_PWM_SCALE, &reg.PWM_SCALE);
    readRegister(TMC5130_REG_LOST_STEPS, &reg.LOST_STEPS);

    return true;
}

// Configuration functions
bool TMC5130::setStepsPerRev(uint32_t steps) {
    if (steps == 0 || steps > 2000) return false;
    config.steps_per_rev = steps;
    return true;
}

bool TMC5130::setMaxRPM(float rpm) {
    float v_val = TMC5130_V_STEP * config.steps_per_rev * (rpm/60);
    Serial.printf("Max RPM %f V value: %f\n", rpm, v_val);

    // check for V value greater than 2^20 (max)
    if (v_val > 0x000FFFFF) {
        Serial.printf("Max RPM %f V value: %f (greater than 2^20)\n", rpm, v_val); 
        return false;
    }
    config.max_rpm = rpm;
    
    // Set THIGH to 3x TSTEP max value (Coolstep will operate above this value, ie at 1/3rd max RPM and below)
    uint32_t thigh = RPMtoTSTEP(rpm) * 3;
    // Set TPWMTHRS to 2x TSTEP
    uint32_t tpwmthrs = RPMtoTSTEP(rpm) * 2;
    if (!writeRegister(TMC5130_REG_TPWMTHRS, tpwmthrs)) {
        Serial.println("Failed to write TPWMTHRS");
        return false;
    }
    if (!writeRegister(TMC5130_REG_TCOOLTHRS, thigh)) {
        Serial.println("Failed to write TCOOLTHRS");
        return false;
    }
    if (!writeRegister(TMC5130_REG_THIGH, thigh)) {
        Serial.println("Failed to write THIGH");
        return false;
    }
    Serial.printf("THIGH: %d\n", thigh);

    return true;
}

bool TMC5130::setIrun(uint16_t rms_mA) {
    if (rms_mA == 0 || rms_mA > 1800) {
        //Serial.printf("Invalid rms_mA: %d\n", rms_mA);
        return false;
    }
    config.irun = rms_mA;
    uint8_t vsense = (rms_mA > 1060) ? 0 : 1;   // vsense flag set for less than 1.06A, current monitoring higher sensitivety
    
    if (vsense == 1) reg.CHOPCONF |= 1 << TMC5130_CHOPCONF_VSENSE_bp;
    else reg.CHOPCONF &= ~(1 << TMC5130_CHOPCONF_VSENSE_bp);

    if (!writeRegister(TMC5130_REG_CHOPCONF, reg.CHOPCONF)) return false;

    uint8_t irun = ImAtoIRUN_IHOLD(rms_mA, vsense);
    uint8_t ihold = ImAtoIRUN_IHOLD(config.ihold, vsense);      // Re-calculate IHOLD in case vsense changed
    reg.IHOLD_IRUN &= ~(0xFFFF);                                // Clear IRUN & IHOLD bits
    reg.IHOLD_IRUN |= (irun << TMC5130_IHOLD_IRUN_IRUN_bp) | (ihold << TMC5130_IHOLD_IRUN_IHOLD_bp);       // Set IRUN & IHOLD bits


    //Serial.printf("Set IRUN to %d mA, value: %d. IHOLD_IRUN: 0x%08X, VSENSE: %d\n", rms_mA, irun, reg.IHOLD_IRUN, vsense);
    if (!writeRegister(TMC5130_REG_IHOLD_IRUN, reg.IHOLD_IRUN)) return false;

    return true;
}

bool TMC5130::setIhold(uint16_t rms_mA) {
    if (rms_mA == 0 || rms_mA > 1000) {
        //Serial.printf("Invalid rms_mA: %d\n", rms_mA);
        return false;
    }
    config.ihold = rms_mA;
    uint8_t vsense = (reg.CHOPCONF >> TMC5130_CHOPCONF_VSENSE_bp) & 1;   // vsense flag dictated by setIrun function
    uint8_t ihold = ImAtoIRUN_IHOLD(rms_mA, vsense);
    reg.IHOLD_IRUN &= ~(0xFF);                                           // Clear IHOLD bits
    reg.IHOLD_IRUN |= (ihold << TMC5130_IHOLD_IRUN_IHOLD_bp);            // Set IHOLD bits

    //Serial.printf("Set IHOLD to %d mA, value: %d. IHOLD_IRUN: 0x%08X, VSENSE: %d\n", rms_mA, ihold, reg.IHOLD_IRUN, vsense);
    if (!writeRegister(TMC5130_REG_IHOLD_IRUN, reg.IHOLD_IRUN)) return false;

    return true;
}

bool TMC5130::setRPM(float rpm) {
    if (rpm > config.max_rpm) return false;
    config.rpm = rpm;
    reg.VMAX = lround(TMC5130_V_STEP * config.steps_per_rev * (rpm/60));
    if (status.running) {
        if (!writeRegister(TMC5130_REG_VMAX, reg.VMAX)) return false;
    }
    return true;
}

bool TMC5130::setAcceleration(float rpm_per_s) {
    if (rpm_per_s > config.max_rpm) return false;
    config.accelleration = rpm_per_s;
    reg.AMAX = lround((rpm_per_s/60) / (TMC5130_A_STEP / config.steps_per_rev));
    Serial.printf("RPM/s²: %f, calculated AMAX: %d\n", rpm_per_s, reg.AMAX);
    if(!writeRegister(TMC5130_REG_AMAX, reg.AMAX)) return false;
    return true;
}

bool TMC5130::setStealthChop(bool enable) {
    config.stealth_chop = enable;
    if (status.running) stop();     // PWM mode cannot be enabled while motor is running
    if (enable) reg.GCONF |= TMC5130_GCONF_EN_PWM_MODE_bm;
    else reg.GCONF &= ~(TMC5130_GCONF_EN_PWM_MODE_bm);
    return writeRegister(TMC5130_REG_GCONF, reg.GCONF);
}

bool TMC5130::setDirection(bool forward) {
    if (forward) reg.RAMPMODE = 1;
    else reg.RAMPMODE = 2;
    return writeRegister(TMC5130_REG_RAMPMODE, reg.RAMPMODE);
}

bool TMC5130::invertDirection(bool invert) {
    if (invert) reg.GCONF |= TMC5130_GCONF_REVERSE_SHAFT_bm;
    else reg.GCONF &= ~(TMC5130_GCONF_REVERSE_SHAFT_bm);
    return writeRegister(TMC5130_REG_GCONF, reg.GCONF);
}

bool TMC5130::run(void) {
    reg.CHOPCONF &= 0xFFFFFFF0;         // Clear TOFF time
    reg.CHOPCONF |= 0x00000005;         // TOFF time set to 5 to enable the driver
    if (!writeRegister(TMC5130_REG_CHOPCONF, reg.CHOPCONF)) return false;
    status.running = true;
    setRPM(config.rpm);
    return true;
}

bool TMC5130::stop(void) {
    reg.CHOPCONF &= 0xFFFFFFF0;      // Clear TOFF time
    if (!writeRegister(TMC5130_REG_VMAX, 0)) return false;
    if (!writeRegister(TMC5130_REG_CHOPCONF, reg.CHOPCONF)) return false;
    status.running = false;
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
    //Serial.printf("Write to register 0x%02X data: 0x%02X%02X%02X%02X\n", reg, buf[0], buf[1], buf[2], buf[3]);
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

// Private functions

uint8_t TMC5130::ImAtoIRUN_IHOLD(uint16_t mAval, bool vsense) {
    uint8_t i_divider = (vsense == 0) ? TMC5130_mA_PER_BIT_LOW_SENSITIVITY : TMC5130_mA_PER_BIT_HIGH_SENSITIVITY;
    float i_val = (float)mAval / (float)i_divider;
    return (uint8_t)lround(i_val);
}

// Returns the number of clock cycles per step for TSTEP threshold values given RPM
uint32_t TMC5130::RPMtoTSTEP(float rpm) {
    return lround(12400000 / ((rpm/60) * 256 * config.steps_per_rev));
}