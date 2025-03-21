# MCP48FEB Arduino Library

An Arduino library for controlling the Microchip MCP48FEBxx family of dual-channel 12-bit Digital-to-Analog Converters (DACs) with SPI interface.

## Features

- Support for all MCP48FEBxx family devices (MCP48FEB01, MCP48FEB02, etc.)
- Volatile and non-volatile (EEPROM) register control
- Configuration of:
  - Reference voltage source (VREF)
  - Power-down modes
  - Gain settings (1x or 2x)
- DAC output control with 12-bit resolution
- Status monitoring (Power-On-Reset, EEPROM Write Access)
- Optional LDAC pin support for synchronized updates
- 10MHz SPI communication

## Supported Devices

This library supports the following MCP48FEBxx devices:
- MCP48FEB01: Dual 12-bit DAC with internal EEPROM and SPI interface
- MCP48FEB02: Dual 12-bit DAC with internal EEPROM and SPI interface
- Other variants in the MCP48FEBxx family

## Installation

### Arduino IDE

1. Download the library as a ZIP file
2. In the Arduino IDE, navigate to Sketch > Include Library > Add .ZIP Library...
3. Select the downloaded ZIP file
4. The library will be installed and available in the examples and library manager

### PlatformIO

1. Add the following to your `platformio.ini` file:
```ini
lib_deps =
    MCP48FEB
```

## Hardware Connections

Connect your Arduino or compatible microcontroller to the MCP48FEBxx DAC as follows:

| Arduino Pin | MCP48FEBxx Pin | Description |
|-------------|----------------|-------------|
| SPI MOSI    | SDI            | Serial Data Input |
| SPI MISO    | SDO            | Serial Data Output |
| SPI SCK     | SCK            | Serial Clock |
| Any Digital | CS             | Chip Select (Active Low) |
| Any Digital (optional) | LDAC | Load DAC (Active Low) |
| 3.3V or 5V  | VDD            | Power Supply |
| GND         | VSS            | Ground |

## Usage

### Basic Example

```cpp
#include <MCP48FEB.h>

// Define pins
const int dac_cs_pin = 10;  // Chip Select pin
const int dac_lat_pin = 9;  // Optional LDAC pin for synchronized updates

// Create MCP48FEBxx object
MCP48FEBxx dac(dac_cs_pin, dac_lat_pin, &SPI);

void setup() {
  Serial.begin(115200);
  
  // Initialize the DAC
  if (dac.begin()) {
    Serial.println("DAC initialized successfully");
  } else {
    Serial.println("DAC initialization failed");
    while (1);
  }
  
  // Configure DAC channel 0
  dac.setVREF(0, VREF_VDD);     // Use VDD as reference
  dac.setPD(0, PD_NORMAL);      // Normal operation mode
  dac.setGain(0, GAIN_1X);      // 1x gain
  
  // Configure DAC channel 1
  dac.setVREF(1, VREF_BANDGAP); // Use internal bandgap reference
  dac.setPD(1, PD_NORMAL);      // Normal operation mode
  dac.setGain(1, GAIN_2X);      // 2x gain
  
  // Write values to DAC channels
  dac.writeDAC(0, 2048);        // Mid-scale on channel 0
  dac.writeDAC(1, 1000);        // Approx 1/4 scale on channel 1
}

void loop() {
  // Your code here
}
```

### More Examples

Check the examples folder for more detailed examples demonstrating different features of the library:

- **Basic**: Shows initialization, configuration, and basic DAC operations
- **FullFunctionalTest**: Comprehensive test of all library features and waveform generation demo

## API Reference

### Constructor

```cpp
MCP48FEBxx(int cs_pin);
MCP48FEBxx(int cs_pin, int lat_pin, SPIClass *spi);
```
- `cs_pin`: Chip Select pin
- `lat_pin`: Optional LDAC pin for synchronized updates
- `spi`: Optional SPI interface (defaults to the default hardware SPI)

### Initialization

```cpp
bool begin(void);
```
Initializes the DAC and SPI interface. Returns `true` if successful.

### Configuration Functions

```cpp
bool setVREF(uint8_t channel, MCP48FEBxx_VREF vref);
bool setPD(uint8_t channel, MCP48FEBxx_PD pd);
bool setGain(uint8_t channel, MCP48FEBxx_GAIN gain);
```
- `channel`: DAC channel (0 or 1)
- `vref`: Reference voltage source (VREF_VDD, VREF_BANDGAP, VREF_EXTERNAL, VREF_EXT_BUFFERED)
- `pd`: Power-down mode (PD_NORMAL, PD_1K_PULLDOWN, PD_100K_PULLDOWN, PD_HIGH_IMPEDANCE)
- `gain`: Gain setting (GAIN_1X, GAIN_2X)

### Status Functions

```cpp
bool getPORStatus(void);
bool getEEWAStatus(void);
```
- `getPORStatus()`: Returns `true` if a Power-On-Reset has occurred
- `getEEWAStatus()`: Returns `true` if an EEPROM write is in progress

### DAC Functions

```cpp
bool writeDAC(uint8_t channel, uint16_t value);
uint16_t readDAC(uint8_t channel);
```
- `channel`: DAC channel (0 or 1)
- `value`: 12-bit DAC value (0-4095)

### Non-volatile Config Functions

```cpp
bool setVREF_EEPROM(uint8_t channel, MCP48FEBxx_VREF vref);
bool setPD_EEPROM(uint8_t channel, MCP48FEBxx_PD pd);
bool setGain_EEROM(uint8_t channel, MCP48FEBxx_GAIN gain);
bool writeDAC_EEPROM(uint8_t channel, uint16_t value);
int saveRegistersToEEPROM(void);
```
These functions write configuration to the non-volatile EEPROM memory. The settings will be retained even after power is removed.

## License

This library is released under the [MIT License](LICENSE).

## Author

Developed by the Open Reactor Control System team, 2025.
