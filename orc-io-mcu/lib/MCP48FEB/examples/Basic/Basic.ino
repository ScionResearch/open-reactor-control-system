/*
 * MCP48FEB Basic Example
 * 
 * This example demonstrates the basic functionality of the MCP48FEB library
 * for controlling the MCP48FEBxx family of dual-channel 12-bit DACs.
 * 
 * The example shows:
 * - Initializing the DAC
 * - Setting configuration (VREF, Power-down mode, Gain)
 * - Writing and reading DAC values
 * - Using non-volatile (EEPROM) functions
 * - Generating a simple sine wave on both DAC channels
 * 
 * Hardware Connections:
 * - Connect the CS pin to the pin specified in the dac_cs_pin variable
 * - Connect the LAT pin to the pin specified in the dac_lat_pin variable (optional)
 * - Connect SPI pins (MOSI, MISO, SCK) to the corresponding Arduino pins
 * 
 * Written by Your Name, March 2025
 */

#include <MCP48FEB.h>

// Define pins
const int dac_cs_pin = 10;  // Chip Select pin

// Create MCP48FEBxx object
MCP48FEBxx dac(dac_cs_pin);

// Variables for sine wave generation
const int numPoints = 100;
float angle = 0.0;
const float angleStep = 2.0 * PI / numPoints;

void setup() {
  Serial.begin(115200);
  delay(1000);  // Give the serial monitor time to open
  
  Serial.println(F("MCP48FEB Basic Example"));
  Serial.println(F("------------------------"));
  
  // Initialize the DAC
  Serial.print(F("Initializing DAC... "));
  if (dac.begin()) {
    Serial.println(F("Success!"));
  } else {
    Serial.println(F("Failed!"));
    while (1);  // Halt if initialization fails
  }
  
  // Check Power-On-Reset status
  Serial.print(F("Power-On-Reset status: "));
  Serial.println(dac.getPORStatus() ? F("Reset occurred") : F("No reset"));
  
  // Configure DAC channel 0
  Serial.println(F("\nConfiguring DAC Channel 0:"));
  
  // Set VREF to VDD (using supply voltage as reference)
  Serial.print(F("  Setting VREF to VDD... "));
  if (dac.setVREF(0, VREF_VDD)) {
    Serial.println(F("Success!"));
  } else {
    Serial.println(F("Failed!"));
  }
  
  // Set normal operation mode (not powered down)
  Serial.print(F("  Setting normal operation mode... "));
  if (dac.setPD(0, PD_NORMAL)) {
    Serial.println(F("Success!"));
  } else {
    Serial.println(F("Failed!"));
  }
  
  // Set gain to 1x
  Serial.print(F("  Setting gain to 1x... "));
  if (dac.setGain(0, GAIN_1X)) {
    Serial.println(F("Success!"));
  } else {
    Serial.println(F("Failed!"));
  }
  
  // Configure DAC channel 1
  Serial.println(F("\nConfiguring DAC Channel 1:"));
  
  // Set VREF to internal bandgap (1.22V)
  Serial.print(F("  Setting VREF to internal bandgap... "));
  if (dac.setVREF(1, VREF_BANDGAP)) {
    Serial.println(F("Success!"));
  } else {
    Serial.println(F("Failed!"));
  }
  
  // Set normal operation mode
  Serial.print(F("  Setting normal operation mode... "));
  if (dac.setPD(1, PD_NORMAL)) {
    Serial.println(F("Success!"));
  } else {
    Serial.println(F("Failed!"));
  }
  
  // Set gain to 2x
  Serial.print(F("  Setting gain to 2x... "));
  if (dac.setGain(1, GAIN_2X)) {
    Serial.println(F("Success!"));
  } else {
    Serial.println(F("Failed!"));
  }
  
  // Write initial values to both DAC channels
  Serial.println(F("\nWriting initial values to DAC channels:"));
  
  // Write 2048 (mid-scale) to DAC0
  Serial.print(F("  Writing 2048 to DAC0... "));
  if (dac.writeDAC(0, 2048)) {
    Serial.println(F("Success!"));
  } else {
    Serial.println(F("Failed!"));
  }
  
  // Write 1000 to DAC1
  Serial.print(F("  Writing 1000 to DAC1... "));
  if (dac.writeDAC(1, 1000)) {
    Serial.println(F("Success!"));
  } else {
    Serial.println(F("Failed!"));
  }
  
  // Read back values from DAC channels
  Serial.println(F("\nReading back DAC channel values:"));
  
  // Read DAC0
  Serial.print(F("  DAC0 value: "));
  Serial.println(dac.readDAC(0));
  
  // Read DAC1
  Serial.print(F("  DAC1 value: "));
  Serial.println(dac.readDAC(1));
  
  // Demonstrate non-volatile (EEPROM) functions
  Serial.println(F("\nDemonstrating non-volatile (EEPROM) functions:"));
  
  // Save current DAC values to EEPROM
  Serial.print(F("  Saving all registers to EEPROM... "));
  int result = dac.saveRegistersToEEPROM();
  if (result > 0) {
    Serial.println(F("Success!"));
  } else {
    Serial.print(F("Failed with code: "));
    Serial.println(result);
  }
  
  Serial.println(F("\nStarting sine wave generation on both channels..."));
}

void loop() {
  // Generate sine wave on both channels with phase difference
  uint16_t dac0Value = 2048 + sin(angle) * 2047;  // Sine wave centered at 2048 with amplitude 2047
  uint16_t dac1Value = 2048 + sin(angle + PI) * 2047;  // Inverted sine wave (180° phase shift)
  
  // Write values to DAC channels
  dac.writeDAC(0, dac0Value);
  dac.writeDAC(1, dac1Value);
  
  // Increment angle for next iteration
  angle += angleStep;
  if (angle >= 2 * PI) {
    angle = 0;
  }
  
  // Print values to serial every 20 iterations
  static int counter = 0;
  if (counter++ % 20 == 0) {
    Serial.print(F("DAC0: "));
    Serial.print(dac0Value);
    Serial.print(F("  DAC1: "));
    Serial.println(dac1Value);
  }
  
  // Small delay to control the frequency of the sine wave
  delay(10);
}
