# MCP79410 RTC Library

An Arduino library for the MCP79410 Real-Time Clock/Calendar with SRAM and Battery Backup.

## Features

- Full RTC functionality with battery backup
- Easy-to-use time and date setting/reading
- SRAM read/write operations (64 bytes)
- Automatic oscillator management
- Error checking and validation
- Efficient struct-based DateTime operations

## Installation

1. Download this library to your Arduino/PlatformIO project's library folder
2. Include the header in your project:
```cpp
#include "MCP79410.h"
```

## Hardware Connection

The MCP79410 uses I2C for communication. Connect your device as follows:

- VCC → 3.3V or 5V
- GND → Ground
- SCL → Arduino SCL pin
- SDA → Arduino SDA pin
- VBAT → Battery backup (optional, connect to 3V battery)

## Usage

### Basic Initialization

```cpp
#include "MCP79410.h"

MCP79410 rtc;  // Create RTC object
// MCP79410 rtc(Wire1);  // or, create RTC object with specified I2C instance

void setup() {
    if (!rtc.begin()) {
        Serial.println("RTC initialization failed!");
        return;
    }
    Serial.println("RTC initialized successfully!");
}
```

### Setting Time and Date

There are two ways to set the date and time:

1. Using individual components:
```cpp
// Set time to 14:11:14
rtc.setTime(14, 11, 14);

// Set date to December 13, 2024
rtc.setDate(2024, 12, 13);

// Or set both at once
rtc.setDateTime(2024, 12, 13, 14, 11, 14);
```

2. Using DateTime struct (recommended):
```cpp
DateTime dt = {
    .year = 2024,
    .month = 12,
    .day = 13,
    .hour = 14,
    .minute = 11,
    .second = 14
};

if (rtc.setDateTime(&dt)) {
    Serial.println("Time set successfully!");
} else {
    Serial.println("Failed to set time!");
}
```

### Reading Time and Date

Similarly, there are two ways to read the time:

1. Using individual components:
```cpp
uint16_t year;
uint8_t month, day, hour, minute, second;

if (rtc.getDateTime(year, month, day, hour, minute, second)) {
    Serial.printf("%04d-%02d-%02d %02d:%02d:%02d\n",
                  year, month, day, hour, minute, second);
} else {
    Serial.println("Failed to read time!");
}
```

2. Using DateTime struct (recommended):
```cpp
DateTime dt;
if (rtc.getDateTime(&dt)) {
    Serial.printf("%04d-%02d-%02d %02d:%02d:%02d\n",
                  dt.year, dt.month, dt.day,
                  dt.hour, dt.minute, dt.second);
} else {
    Serial.println("Failed to read time!");
}
```

### SRAM Operations

The MCP79410 includes 64 bytes of SRAM that can be used to store user data:

```cpp
// Write a single byte
rtc.writeSRAM(0x20, 0x42);

// Read a single byte
uint8_t value = rtc.readSRAM(0x20);

// Write multiple bytes
uint8_t data[] = {0x01, 0x02, 0x03, 0x04};
rtc.writeSRAMBurst(0x20, data, sizeof(data));

// Read multiple bytes
uint8_t buffer[4];
rtc.readSRAMBurst(0x20, buffer, sizeof(buffer));
```

### Battery Backup

The library automatically enables battery backup during initialization. You can manually control it:

```cpp
rtc.enableBatteryBackup(true);  // Enable battery backup
rtc.enableBatteryBackup(false); // Disable battery backup
```

## Error Handling

All set/get operations return a boolean indicating success or failure. Always check these return values:

```cpp
if (!rtc.setTime(25, 0, 0)) {  // Invalid hour (25)
    Serial.println("Failed to set time - invalid value!");
}
```

## Technical Details

- I2C Address: 0x6F
- Valid year range: 2000-2099
- SRAM range: 0x20-0x5F (64 bytes)
- Battery backup supported
- Automatic oscillator management

## License

This library is released under the MIT License.

## Contributing

Contributions are welcome! Please feel free to submit a Pull Request.
