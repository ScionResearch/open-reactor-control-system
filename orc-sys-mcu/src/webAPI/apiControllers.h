#pragma once

/**
 * @file apiControllers.h
 * @brief Controller configuration and control API endpoints
 * 
 * Handles:
 * - /api/controllers - Get all controllers status
 * - Temperature controllers (indices 40-42)
 * - pH controller (index 43)
 * - Flow controllers (indices 44-47)
 * - DO controller (index 48)
 * - DO profiles (indices 0-2)
 */

#include <Arduino.h>

void setupControllersAPI(void);

// Dynamic route handler for REST-style controller endpoints
void handleDynamicControllerRoute(void);

// Controller status
void handleGetControllers(void);

// Temperature controller handlers
void handleGetTempControllerConfig(uint8_t index);
void handleSaveTempControllerConfig(uint8_t index);
void handleUpdateControllerSetpoint(uint8_t index);
void handleEnableController(uint8_t index);
void handleDisableController(uint8_t index);
void handleStartController(uint8_t index);
void handleStopController(uint8_t index);
void handleStartAutotune(uint8_t index);
void handleDeleteController(uint8_t index);

// pH controller handlers
void handleGetpHControllerConfig(void);
void handleSavepHControllerConfig(void);
void handleDeletepHController(void);
void handleUpdatepHSetpoint(void);
void handleEnablepHController(void);
void handleDisablepHController(void);
void handleManualpHAcidDose(void);
void handleManualpHAlkalineDose(void);
void handleResetpHAcidVolume(void);
void handleResetpHAlkalineVolume(void);
void handleDosepHAcid(void);
void handleDosepHAlkaline(void);

// Flow controller handlers
void handleGetFlowControllerConfig(uint8_t index);
void handleSaveFlowControllerConfig(uint8_t index);
void handleDeleteFlowController(uint8_t index);
void handleSetFlowRate(uint8_t index);
void handleEnableFlowController(uint8_t index);
void handleDisableFlowController(uint8_t index);
void handleManualFlowDose(uint8_t index);
void handleResetFlowVolume(uint8_t index);

// DO controller handlers
void handleGetDOControllerConfig(void);
void handleSaveDOControllerConfig(void);
void handleDeleteDOController(void);
void handleSetDOSetpoint(void);
void handleEnableDOController(void);
void handleDisableDOController(void);

// DO profile handlers
void handleGetAllDOProfiles(void);
void handleGetDOProfile(uint8_t index);
void handleSaveDOProfile(uint8_t index);
void handleDeleteDOProfile(uint8_t index);
