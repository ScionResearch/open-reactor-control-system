#pragma once

/**
 * @file apiOutputs.h
 * @brief Output control and configuration API endpoints
 * 
 * Handles:
 * - /api/outputs - Get all outputs status
 * - Digital output configuration and control (indices 21-25)
 * - DAC output control (indices 8-9)
 * - Stepper motor configuration and control (index 26)
 * - DC motor configuration and control (indices 27-30)
 */

#include <Arduino.h>

void setupOutputsAPI(void);

// Output status
void handleGetOutputs(void);

// Digital output handlers
void handleGetDigitalOutputConfig(uint8_t index);
void handleSaveDigitalOutputConfig(uint8_t index);
void handleSetOutputState(uint8_t index);
void handleSetOutputValue(uint8_t index);

// Analog output (DAC) handlers
void handleSetAnalogOutputValue(uint8_t index);

// Stepper motor handlers
void handleGetStepperConfig(void);
void handleSaveStepperConfig(void);
void handleSetStepperRPM(void);
void handleSetStepperDirection(void);
void handleStartStepper(void);
void handleStopStepper(void);

// DC motor handlers
void handleGetDCMotorConfig(uint8_t index);
void handleSaveDCMotorConfig(uint8_t index);
void handleSetDCMotorPower(uint8_t index);
void handleSetDCMotorDirection(uint8_t index);
void handleStartDCMotor(uint8_t index);
void handleStopDCMotor(uint8_t index);
