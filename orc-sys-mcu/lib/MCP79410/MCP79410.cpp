#include "MCP79410.h"

MCP79410::MCP79410(TwoWire &wire) : _wire(wire) {}

bool MCP79410::begin() {
    _wire.begin();
    
    // Enable the battery backup first
    enableBatteryBackup(true);
    delay(10); // Give time for battery backup to stabilize
    
    // Verify battery backup is enabled
    uint8_t wkday = read_register(REG_RTCWKDAY);
    if (!(wkday & 0x08)) {
        return false;
    }
    
    // Start the oscillator if it's not running
    if (!isRunning()) {
        enableOscillator(true);
        delay(10); // Give the oscillator time to start
    }
    
    // Verify the oscillator is now running
    if (!isRunning()) {
        return false;
    }
    
    return true;
}

bool MCP79410::setTime(uint8_t hour, uint8_t minute, uint8_t second) {
    if (hour > 23 || minute > 59 || second > 59) return false;
    
    // Read current seconds register to preserve ST bit
    uint8_t currentSec = read_register(REG_RTCSEC);
    uint8_t stBit = currentSec & 0x80; // Preserve ST bit
    
    // Write new values while preserving ST bit for seconds
    write_register(REG_RTCSEC, dec2bcd(second) | stBit);
    write_register(REG_RTCMIN, dec2bcd(minute));
    write_register(REG_RTCHOUR, dec2bcd(hour));
    
    // Ensure oscillator is running
    if (!isRunning()) {
        enableOscillator(true);
    }
    
    return true;
}

bool MCP79410::setDate(uint16_t year, uint8_t month, uint8_t day) {
    if (year < 2000 || year > 2099 || month < 1 || month > 12 || day < 1 || day > 31) return false;
    
    uint8_t yearReg = dec2bcd(year - 2000);
    write_register(REG_RTCYEAR, yearReg);
    write_register(REG_RTCMTH, dec2bcd(month));
    write_register(REG_RTCDATE, dec2bcd(day));
    return true;
}

bool MCP79410::setDateTime(uint16_t year, uint8_t month, uint8_t day, 
                          uint8_t hour, uint8_t minute, uint8_t second) {
    return setDate(year, month, day) && setTime(hour, minute, second);
}

bool MCP79410::setDateTime(const DateTime& dateTime) {
    // Validate input values
    if (dateTime.year < 2000 || dateTime.year > 2099 ||
        dateTime.month < 1 || dateTime.month > 12 ||
        dateTime.day < 1 || dateTime.day > 31 ||
        dateTime.hour > 23 || dateTime.minute > 59 || dateTime.second > 59) {
        return false;
    }

    // Verify battery backup and oscillator are enabled
    uint8_t wkday = read_register(REG_RTCWKDAY);
    if (!(wkday & 0x08)) {
        enableBatteryBackup(true);
        delay(10);
    }

    if (!isRunning()) {
        enableOscillator(true);
        delay(10);
    }

    // Read current seconds register to preserve ST bit
    uint8_t currentSec = read_register(REG_RTCSEC);
    uint8_t stBit = currentSec & 0x80; // Preserve ST bit
    
    // Start a single I2C transaction for all registers
    _wire.beginTransmission(MCP79410_ADDRESS);
    _wire.write(REG_RTCSEC);
    _wire.write(dec2bcd(dateTime.second) | stBit);  // Seconds with ST bit
    _wire.write(dec2bcd(dateTime.minute));          // Minutes
    _wire.write(dec2bcd(dateTime.hour));            // Hours
    _wire.write(wkday | 0x08);                      // Weekday with VBATEN bit set
    _wire.write(dec2bcd(dateTime.day));             // Day
    _wire.write(dec2bcd(dateTime.month));           // Month
    _wire.write(dec2bcd(dateTime.year - 2000));     // Year
    
    return (_wire.endTransmission() == 0);
}

bool MCP79410::getTime(uint8_t &hour, uint8_t &minute, uint8_t &second) {
    uint8_t sec = read_register(REG_RTCSEC);
    uint8_t min = read_register(REG_RTCMIN);
    uint8_t hr = read_register(REG_RTCHOUR);
    
    // Check if any read failed (I2C returns 0xFF on failure)
    if (sec == 0xFF || min == 0xFF || hr == 0xFF) {
        return false;
    }
    
    second = bcd2dec(sec & 0x7F);
    minute = bcd2dec(min & 0x7F);
    hour = bcd2dec(hr & 0x3F);
    return true;
}

bool MCP79410::getDate(uint16_t &year, uint8_t &month, uint8_t &day) {
    uint8_t yr = read_register(REG_RTCYEAR);
    uint8_t mth = read_register(REG_RTCMTH);
    uint8_t dt = read_register(REG_RTCDATE);
    
    // Check if any read failed
    if (yr == 0xFF || mth == 0xFF || dt == 0xFF) {
        return false;
    }
    
    year = bcd2dec(yr) + 2000;
    month = bcd2dec(mth & 0x1F);
    day = bcd2dec(dt & 0x3F);
    return true;
}

bool MCP79410::getDateTime(uint16_t &year, uint8_t &month, uint8_t &day,
                          uint8_t &hour, uint8_t &minute, uint8_t &second) {
    bool dateOk = getDate(year, month, day);
    bool timeOk = getTime(hour, minute, second);
    return dateOk && timeOk;
}

bool MCP79410::getDateTime(DateTime* dateTime) {
    if (dateTime == nullptr) {
        return false;
    }
    
    // Read all registers in one sequence to ensure time consistency
    _wire.beginTransmission(MCP79410_ADDRESS);
    _wire.write(REG_RTCSEC);
    if (_wire.endTransmission() != 0) {
        return false;
    }
    
    _wire.requestFrom(MCP79410_ADDRESS, (uint8_t)7); // Read all time registers at once
    if (_wire.available() < 7) {
        return false;
    }
    
    uint8_t sec = _wire.read();
    uint8_t min = _wire.read();
    uint8_t hr = _wire.read();
    _wire.read(); // Skip weekday
    uint8_t day = _wire.read();
    uint8_t month = _wire.read();
    uint8_t year = _wire.read();
    
    // Convert from BCD and store in struct
    dateTime->second = bcd2dec(sec & 0x7F);
    dateTime->minute = bcd2dec(min & 0x7F);
    dateTime->hour = bcd2dec(hr & 0x3F);
    dateTime->day = bcd2dec(day & 0x3F);
    dateTime->month = bcd2dec(month & 0x1F);
    dateTime->year = bcd2dec(year) + 2000;
    
    return true;
}

bool MCP79410::writeSRAM(uint8_t address, uint8_t data) {
    if (address < REG_SRAM_START || address > REG_SRAM_END) return false;
    return write_register(address, data);
}

uint8_t MCP79410::readSRAM(uint8_t address) {
    if (address < REG_SRAM_START || address > REG_SRAM_END) return 0;
    return read_register(address);
}

bool MCP79410::writeSRAMBurst(uint8_t startAddress, uint8_t *data, uint8_t length) {
    if (startAddress < REG_SRAM_START || 
        startAddress + length > REG_SRAM_END + 1) return false;

    _wire.beginTransmission(MCP79410_ADDRESS);
    _wire.write(startAddress);
    for (uint8_t i = 0; i < length; i++) {
        _wire.write(data[i]);
    }
    return (_wire.endTransmission() == 0);
}

bool MCP79410::readSRAMBurst(uint8_t startAddress, uint8_t *data, uint8_t length) {
    if (startAddress < REG_SRAM_START || 
        startAddress + length > REG_SRAM_END + 1) return false;

    _wire.beginTransmission(MCP79410_ADDRESS);
    _wire.write(startAddress);
    _wire.endTransmission();
    
    _wire.requestFrom(MCP79410_ADDRESS, length);
    for (uint8_t i = 0; i < length && _wire.available(); i++) {
        data[i] = _wire.read();
    }
    return true;
}

bool MCP79410::isRunning() {
    return (read_register(REG_RTCSEC) & 0x80) == 0x80;
}

void MCP79410::enableOscillator(bool enable) {
    uint8_t sec = read_register(REG_RTCSEC);
    if (enable) {
        sec |= 0x80;  // Set ST bit
    } else {
        sec &= ~0x80; // Clear ST bit
    }
    write_register(REG_RTCSEC, sec);
}

void MCP79410::enableBatteryBackup(bool enable) {
    uint8_t day = read_register(REG_RTCWKDAY);
    if (enable) {
        day |= 0x08;  // Set VBATEN bit
    } else {
        day &= ~0x08; // Clear VBATEN bit
    }
    write_register(REG_RTCWKDAY, day);
}

uint8_t MCP79410::bcd2dec(uint8_t bcd) {
    return ((bcd / 16) * 10) + (bcd % 16);
}

uint8_t MCP79410::dec2bcd(uint8_t dec) {
    return ((dec / 10) * 16) + (dec % 10);
}

bool MCP79410::write_register(uint8_t reg, uint8_t value) {
    _wire.beginTransmission(MCP79410_ADDRESS);
    _wire.write(reg);
    _wire.write(value);
    return (_wire.endTransmission() == 0);
}

uint8_t MCP79410::read_register(uint8_t reg) {
    _wire.beginTransmission(MCP79410_ADDRESS);
    _wire.write(reg);
    _wire.endTransmission();
    
    _wire.requestFrom(MCP79410_ADDRESS, (uint8_t)1);
    return _wire.read();
}