# Modified MAX31865 Library for Non-Blocking Applications

This is a modified version of the Adafruit MAX31865 Arduino Library optimized for non-blocking, high-performance applications.

## Key Improvements Over Original Adafruit Library

**Performance Enhancements:**
- **Dramatically faster read times**: Reduced from ~75ms to as low as 54µs (at 4MHz SPI) or 96µs (at 1MHz SPI)
- **Non-blocking operation**: Eliminates processor blocking during sensor reads
- **Higher refresh rates**: Use DRDY pin to determine when data is ready, allowing for higher refresh rates (60Hz or 50Hz in 50Hz filter mode)
- **Removed BusIO dependency**: Reduced complexity in implementation, easier to modify on the fly

**New Features:**
- **Auto-convert mode**: Continuous conversion capability with `autoConvert()` method
- **Get temperature with one method**: temperature() decides which data aquisition method to use based on the conversion mode, either the original blocking 1-shot method, or the non-blocking auto-convert method

**Important Considerations:**
- **Self-heating in auto-convert mode**: Bias current remains on during continuous operation, causing sensor heating
- **Thermal effects**: Small PT100 sensors (~0.5°C/mW thermal conductivity) may experience ~0.25°C self-heating error at room temperature
- **Temperature dependency**: Self-heating effects vary inversely with ambient temperature

## Original Adafruit Library Information

This is based on the Adafruit MAX31865 Arduino Library 

Tested and works great with the Adafruit Thermocouple Breakout w/MAX31865
   * http://www.adafruit.com/products/3328

These sensors use SPI to communicate, 4 pins are required to  
interface

Adafruit invests time and resources providing this open source code, 
please support Adafruit and open-source hardware by purchasing 
products from Adafruit!

Written by Limor Fried/Ladyada  for Adafruit Industries.  
BSD license, check license.txt for more information
All text above must be included in any redistribution
