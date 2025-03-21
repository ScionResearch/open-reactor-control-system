/*
 * MCP48FEB Full Functional Test
 * 
 * This example demonstrates comprehensive testing of the MCP48FEB library
 * for controlling the MCP48FEBxx family of dual-channel 12-bit DACs.
 * 
 * The example tests:
 * - All initialization methods
 * - All configuration options (VREF, Power-down modes, Gain settings)
 * - All DAC read/write operations (volatile and non-volatile)
 * - Status monitoring
 * - Different waveform generation
 * 
 * Hardware Connections:
 * - Connect the CS pin to the pin specified in the dac_cs_pin variable
 * - Connect the LAT pin to the pin specified in the dac_lat_pin variable
 * - Connect SPI pins (MOSI, MISO, SCK) to the corresponding Arduino pins
 * - Optionally connect an oscilloscope to the DAC outputs to visualize waveforms
 * 
 * Written by Your Name, March 2025
 */

#include <MCP48FEB.h>

// Define pins
const int dac_cs_pin = 10;  // Chip Select pin
const int dac_lat_pin = 9;  // Latch/sync pin for synchronized updates
 
// Create MCP48FEBxx object
MCP48FEBxx dac(dac_cs_pin, dac_lat_pin, &SPI);

// Test status variables
int testsRun = 0;
int testsPassed = 0;

// Function prototypes
void runTest(const char* testName, bool (*testFunction)());
bool testInitialization();
bool testVREFConfiguration();
bool testPowerDownConfiguration();
bool testGainConfiguration();
bool testDACOperations();
bool testNonVolatileOperations();
bool testWaveformGeneration();

void setup() {
  Serial.begin(115200);
  delay(1000);  // Give the serial monitor time to open
  
  Serial.println(F("MCP48FEB Full Functional Test"));
  Serial.println(F("-----------------------------"));
  
  // Run all tests
  runTest("Initialization", testInitialization);
  runTest("VREF Configuration", testVREFConfiguration);
  runTest("Power-down Configuration", testPowerDownConfiguration);
  runTest("Gain Configuration", testGainConfiguration);
  runTest("DAC Operations", testDACOperations);
  runTest("Non-volatile Operations", testNonVolatileOperations);
  
  // Print test summary
  Serial.println(F("\n--- Test Summary ---"));
  Serial.print(F("Total tests: "));
  Serial.println(testsRun);
  Serial.print(F("Tests passed: "));
  Serial.println(testsPassed);
  Serial.print(F("Success rate: "));
  Serial.print((testsPassed * 100.0) / testsRun);
  Serial.println(F("%"));
  
  if (testsPassed == testsRun) {
    Serial.println(F("\nAll tests passed! Starting waveform generation demo..."));
    testWaveformGeneration(); // This is not a test but a demo
  } else {
    Serial.println(F("\nSome tests failed. Check the output above for details."));
  }
}

void loop() {
  // Generate different waveforms on the two DAC channels
  static uint32_t lastChangeTime = 0;
  static int waveformType = 0;
  static uint32_t counter = 0;
  
  // Change waveform type every 5 seconds
  if (millis() - lastChangeTime > 5000) {
    waveformType = (waveformType + 1) % 4;
    lastChangeTime = millis();
    
    Serial.print(F("Switching to waveform type: "));
    switch (waveformType) {
      case 0: Serial.println(F("Sine wave")); break;
      case 1: Serial.println(F("Triangle wave")); break;
      case 2: Serial.println(F("Sawtooth wave")); break;
      case 3: Serial.println(F("Square wave")); break;
    }
  }
  
  // Generate selected waveform
  counter++;
  uint16_t value0 = 0;
  uint16_t value1 = 0;
  
  switch (waveformType) {
    case 0: // Sine wave
      value0 = 2048 + sin(counter * 0.05) * 2047;
      value1 = 2048 + sin(counter * 0.05 + PI) * 2047;
      break;
      
    case 1: // Triangle wave
      {
        uint16_t period = 100;
        uint16_t position = counter % period;
        if (position < period/2) {
          value0 = map(position, 0, period/2, 0, 4095);
        } else {
          value0 = map(position, period/2, period, 4095, 0);
        }
        value1 = 4095 - value0; // Inverted triangle on channel 1
      }
      break;
      
    case 2: // Sawtooth wave
      {
        uint16_t period = 100;
        uint16_t position = counter % period;
        value0 = map(position, 0, period, 0, 4095);
        value1 = 4095 - value0; // Inverted sawtooth on channel 1
      }
      break;
      
    case 3: // Square wave
      {
        uint16_t period = 100;
        uint16_t position = counter % period;
        value0 = (position < period/2) ? 4095 : 0;
        value1 = (position < period/2) ? 0 : 4095; // Inverted square on channel 1
      }
      break;
  }
  
  // Write values to DAC channels
  dac.writeDAC(0, value0);
  dac.writeDAC(1, value1);
  
  // Small delay to control the frequency of the waveforms
  delay(10);
}

// Helper function to run a test and report results
void runTest(const char* testName, bool (*testFunction)()) {
  Serial.print(F("\n--- Testing: "));
  Serial.print(testName);
  Serial.println(F(" ---"));
  
  bool result = testFunction();
  testsRun++;
  
  Serial.print(F("Result: "));
  if (result) {
    Serial.println(F("PASSED"));
    testsPassed++;
  } else {
    Serial.println(F("FAILED"));
  }
}

// Test initialization
bool testInitialization() {
  Serial.println(F("Initializing DAC..."));
  bool initResult = dac.begin();
  
  if (initResult) {
    Serial.println(F("DAC initialization successful"));
    
    // Check Power-On-Reset status
    bool porStatus = dac.getPORStatus();
    Serial.print(F("Power-On-Reset status: "));
    Serial.println(porStatus ? F("Reset occurred") : F("No reset"));
    
    // Check EEWA status
    bool eewaStatus = dac.getEEWAStatus();
    Serial.print(F("EEPROM Write Access status: "));
    Serial.println(eewaStatus ? F("Write in progress") : F("Ready"));
  } else {
    Serial.println(F("DAC initialization failed"));
  }
  
  return initResult;
}

// Test VREF configuration
bool testVREFConfiguration() {
  bool allTests = true;
  Serial.println(F("Testing all VREF configurations for both channels"));
  
  // Array of VREF options to test
  MCP48FEBxx_VREF vrefOptions[] = {
    VREF_VDD,
    VREF_BANDGAP,
    VREF_EXTERNAL,
    VREF_EXT_BUFFERED
  };
  
  // Test each VREF option on both channels
  for (int channel = 0; channel <= 1; channel++) {
    for (int i = 0; i < 4; i++) {
      Serial.print(F("Channel "));
      Serial.print(channel);
      Serial.print(F(", Setting VREF to "));
      
      switch (vrefOptions[i]) {
        case VREF_VDD: Serial.print(F("VDD")); break;
        case VREF_BANDGAP: Serial.print(F("Bandgap (1.22V)")); break;
        case VREF_EXTERNAL: Serial.print(F("External VREF pin")); break;
        case VREF_EXT_BUFFERED: Serial.print(F("External Buffered (1.25V)")); break;
      }
      
      bool result = dac.setVREF(channel, vrefOptions[i]);
      Serial.print(F("... "));
      Serial.println(result ? F("Success") : F("Failed"));
      
      allTests &= result;
      delay(50);  // Small delay between operations
    }
  }
  
  return allTests;
}

// Test Power-down configuration
bool testPowerDownConfiguration() {
  bool allTests = true;
  Serial.println(F("Testing all Power-down configurations for both channels"));
  
  // Array of Power-down options to test
  MCP48FEBxx_PD pdOptions[] = {
    PD_NORMAL,
    PD_1K_PULLDOWN,
    PD_100K_PULLDOWN,
    PD_HIGH_IMPEDANCE
  };
  
  // Test each Power-down option on both channels
  for (int channel = 0; channel <= 1; channel++) {
    for (int i = 0; i < 4; i++) {
      Serial.print(F("Channel "));
      Serial.print(channel);
      Serial.print(F(", Setting Power-down to "));
      
      switch (pdOptions[i]) {
        case PD_NORMAL: Serial.print(F("Normal operation")); break;
        case PD_1K_PULLDOWN: Serial.print(F("1kΩ pull-down")); break;
        case PD_100K_PULLDOWN: Serial.print(F("100kΩ pull-down")); break;
        case PD_HIGH_IMPEDANCE: Serial.print(F("High-impedance")); break;
      }
      
      bool result = dac.setPD(channel, pdOptions[i]);
      Serial.print(F("... "));
      Serial.println(result ? F("Success") : F("Failed"));
      
      allTests &= result;
      delay(50);  // Small delay between operations
    }
  }
  
  // Return to normal operation for further tests
  dac.setPD(0, PD_NORMAL);
  dac.setPD(1, PD_NORMAL);
  
  return allTests;
}

// Test Gain configuration
bool testGainConfiguration() {
  bool allTests = true;
  Serial.println(F("Testing all Gain configurations for both channels"));
  
  // Array of Gain options to test
  MCP48FEBxx_GAIN gainOptions[] = {
    GAIN_1X,
    GAIN_2X
  };
  
  // Test each Gain option on both channels
  for (int channel = 0; channel <= 1; channel++) {
    for (int i = 0; i < 2; i++) {
      Serial.print(F("Channel "));
      Serial.print(channel);
      Serial.print(F(", Setting Gain to "));
      
      switch (gainOptions[i]) {
        case GAIN_1X: Serial.print(F("1x")); break;
        case GAIN_2X: Serial.print(F("2x")); break;
      }
      
      bool result = dac.setGain(channel, gainOptions[i]);
      Serial.print(F("... "));
      Serial.println(result ? F("Success") : F("Failed"));
      
      allTests &= result;
      delay(50);  // Small delay between operations
    }
  }
  
  return allTests;
}

// Test DAC operations
bool testDACOperations() {
  bool allTests = true;
  Serial.println(F("Testing DAC write and read operations"));
  
  // Test values to write and read back
  uint16_t testValues[] = {0, 1024, 2048, 3072, 4095};
  
  // Test each value on both channels
  for (int channel = 0; channel <= 1; channel++) {
    for (int i = 0; i < 5; i++) {
      uint16_t valueToWrite = testValues[i];
      
      Serial.print(F("Channel "));
      Serial.print(channel);
      Serial.print(F(", Writing value "));
      Serial.print(valueToWrite);
      
      bool writeResult = dac.writeDAC(channel, valueToWrite);
      Serial.print(F("... "));
      Serial.println(writeResult ? F("Success") : F("Failed"));
      
      delay(50);  // Small delay between operations
      
      uint16_t readValue = dac.readDAC(channel);
      Serial.print(F("  Read back value: "));
      Serial.println(readValue);
      
      bool readMatch = (readValue == valueToWrite);
      Serial.print(F("  Values match: "));
      Serial.println(readMatch ? F("Yes") : F("No"));
      
      allTests &= writeResult && readMatch;
    }
  }
  
  return allTests;
}

// Test non-volatile operations
bool testNonVolatileOperations() {
  bool allTests = true;
  Serial.println(F("Testing non-volatile (EEPROM) operations"));
  
  // Set test values for volatile registers
  Serial.println(F("Setting test values for registers..."));
  dac.writeDAC(0, 1000);
  dac.writeDAC(1, 3000);
  dac.setVREF(0, VREF_VDD);
  dac.setVREF(1, VREF_BANDGAP);
  dac.setPD(0, PD_NORMAL);
  dac.setPD(1, PD_NORMAL);
  dac.setGain(0, GAIN_1X);
  dac.setGain(1, GAIN_2X);
  
  // Save registers to EEPROM
  Serial.println(F("Saving all registers to EEPROM..."));
  int saveResult = dac.saveRegistersToEEPROM();
  bool saveSuccess = (saveResult > 0);
  
  Serial.print(F("Save result: "));
  if (saveSuccess) {
    Serial.println(F("Success"));
  } else {
    Serial.print(F("Failed with code: "));
    Serial.println(saveResult);
  }
  
  // Additional tests for individual EEPROM write functions
  Serial.println(F("Testing individual EEPROM write functions..."));
  
  bool vrefEepromResult = dac.setVREF_EEPROM(0, VREF_EXTERNAL);
  Serial.print(F("VREF EEPROM write: "));
  Serial.println(vrefEepromResult ? F("Success") : F("Failed"));
  
  bool pdEepromResult = dac.setPD_EEPROM(0, PD_NORMAL);
  Serial.print(F("Power-down EEPROM write: "));
  Serial.println(pdEepromResult ? F("Success") : F("Failed"));
  
  bool gainEepromResult = dac.setGain_EEROM(0, GAIN_1X);
  Serial.print(F("Gain EEPROM write: "));
  Serial.println(gainEepromResult ? F("Success") : F("Failed"));
  
  bool dacEepromResult = dac.writeDAC_EEPROM(0, 2000);
  Serial.print(F("DAC value EEPROM write: "));
  Serial.println(dacEepromResult ? F("Success") : F("Failed"));
  
  allTests &= saveSuccess && vrefEepromResult && pdEepromResult && 
             gainEepromResult && dacEepromResult;
  
  return allTests;
}

// Waveform generation demo
bool testWaveformGeneration() {
  Serial.println(F("Starting waveform generation test..."));
  
  // Set DAC configuration for waveform generation
  dac.setVREF(0, VREF_VDD);
  dac.setVREF(1, VREF_VDD);
  dac.setPD(0, PD_NORMAL);
  dac.setPD(1, PD_NORMAL);
  dac.setGain(0, GAIN_1X);
  dac.setGain(1, GAIN_1X);
  
  Serial.println(F("Generating short sine wave burst on both channels"));
  
  // Generate a short sine wave burst on both channels
  for (int i = 0; i < 100; i++) {
    uint16_t dac0Value = 2048 + sin(i * 0.1) * 2047;
    uint16_t dac1Value = 2048 + sin(i * 0.1 + PI) * 2047;
    
    dac.writeDAC(0, dac0Value);
    dac.writeDAC(1, dac1Value);
    
    if (i % 10 == 0) {
      Serial.print(F("DAC0: "));
      Serial.print(dac0Value);
      Serial.print(F("  DAC1: "));
      Serial.println(dac1Value);
    }
    
    delay(10);
  }
  
  Serial.println(F("Continuous waveform generation will now start in the main loop"));
  return true;
}
