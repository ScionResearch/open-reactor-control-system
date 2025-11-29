#pragma once

/**
 * @file apiInputs.h
 * @brief Input configuration API endpoints
 * 
 * Handles:
 * - /api/inputs - Get all inputs status
 * - ADC configuration (indices 0-7)
 * - DAC configuration (indices 8-9)
 * - RTD configuration (indices 10-12)
 * - GPIO configuration (indices 13-20)
 * - Energy sensor configuration (indices 31-32)
 * - Device sensor configuration (indices 70-99)
 * - COM port configuration (indices 0-3)
 */

#include <Arduino.h>

void setupInputsAPI(void);

// Input status
void handleGetInputs(void);

// ADC handlers
void handleGetADCConfig(uint8_t index);
void handleSaveADCConfig(uint8_t index);

// DAC handlers
void handleGetDACConfig(uint8_t index);
void handleSaveDACConfig(uint8_t index);

// RTD handlers
void handleGetRTDConfig(uint8_t index);
void handleSaveRTDConfig(uint8_t index);

// GPIO handlers
void handleGetGPIOConfig(uint8_t index);
void handleSaveGPIOConfig(uint8_t index);

// Energy sensor handlers
void handleGetEnergySensorConfig(uint8_t index);
void handleSaveEnergySensorConfig(uint8_t index);

// Device sensor handlers
void handleGetDeviceSensorConfig(uint8_t index);
void handleSaveDeviceSensorConfig(uint8_t index);

// COM port handlers
void handleGetComPortConfig(uint8_t index);
void handleSaveComPortConfig(uint8_t index);
void handleGetComPorts(void);
