#ifndef MCP79410_H
#define MCP79410_H

#include <Arduino.h>
#include <Wire.h>

// Device address (7-bit)
#define MCP79410_ADDRESS 0x6F

// Register addresses
#define REG_RTCSEC   0x00
#define REG_RTCMIN   0x01
#define REG_RTCHOUR  0x02
#define REG_RTCWKDAY 0x03
#define REG_RTCDATE  0x04
#define REG_RTCMTH   0x05
#define REG_RTCYEAR  0x06
#define REG_CONTROL  0x07
#define REG_OSCTRIM  0x08
#define REG_SRAM_START 0x20
#define REG_SRAM_END   0x5F

struct DateTime {
    uint16_t year;
    uint8_t month;
    uint8_t day;
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
};

class MCP79410 {
public:
    // Constructor
    MCP79410(TwoWire &wire = Wire);
    
    // Initialization
    bool begin();
    
    // Time setting and reading
    bool setTime(uint8_t hour, uint8_t minute, uint8_t second);
    bool setDate(uint16_t year, uint8_t month, uint8_t day);
    bool setDateTime(uint16_t year, uint8_t month, uint8_t day,
                    uint8_t hour, uint8_t minute, uint8_t second);
    bool setDateTime(const DateTime& dateTime);
    
    // Time reading
    bool getTime(uint8_t &hour, uint8_t &minute, uint8_t &second);
    bool getDate(uint16_t &year, uint8_t &month, uint8_t &day);
    bool getDateTime(uint16_t &year, uint8_t &month, uint8_t &day,
                    uint8_t &hour, uint8_t &minute, uint8_t &second);
    bool getDateTime(DateTime* dateTime);
    
    // SRAM operations
    bool writeSRAM(uint8_t address, uint8_t data);
    uint8_t readSRAM(uint8_t address);
    bool writeSRAMBurst(uint8_t startAddress, uint8_t *data, uint8_t length);
    bool readSRAMBurst(uint8_t startAddress, uint8_t *data, uint8_t length);
    
    // Utility functions
    bool isRunning();
    void enableOscillator(bool enable);
    void enableBatteryBackup(bool enable);

private:
    TwoWire &_wire;
    uint8_t bcd2dec(uint8_t bcd);
    uint8_t dec2bcd(uint8_t dec);
    bool write_register(uint8_t reg, uint8_t value);
    uint8_t read_register(uint8_t reg);
};

#endif