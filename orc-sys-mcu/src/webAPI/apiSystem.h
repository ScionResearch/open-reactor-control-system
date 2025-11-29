#pragma once

/**
 * @file apiSystem.h
 * @brief System status and control API endpoints
 * 
 * Handles:
 * - System status (/api/system/status)
 * - All status aggregation (/api/status/all)
 * - Sensor data (/api/sensors)
 * - System reboot
 * - Recording configuration
 */

#include <Arduino.h>

// =============================================================================
// Setup Function
// =============================================================================

/**
 * @brief Register all system API endpoints
 */
void setupSystemAPI(void);

// =============================================================================
// Handler Functions
// =============================================================================

void handleSystemStatus(void);

// Recording configuration
void handleGetRecordingConfig(void);
void handleSaveRecordingConfig(void);
