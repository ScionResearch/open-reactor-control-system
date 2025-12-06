#pragma once

/**
 * @file apiDashboard.h
 * @brief Dashboard API endpoints for real-time monitoring and global controls
 * 
 * Handles:
 * - /api/dashboard - Get all dashboard-visible objects with live data
 * - /api/dashboard/layout - Get/save tile layout configuration
 * - /api/dashboard/alarms - Get active alarms and fault summary
 * - /api/dashboard/enable-all - Global enable for outputs/controllers (returns RTC timestamp)
 * - /api/dashboard/pause - Pause non-temperature controllers (pH, flow, DO)
 * - /api/dashboard/disable-all - Global disable for outputs/controllers
 * - /api/dashboard/clear-volumes - Clear all cumulative dosing volumes
 * 
 * Layout persistence is intentional (user-triggered save) to protect flash write cycles.
 */

#include <Arduino.h>

// =============================================================================
// API Setup
// =============================================================================

/**
 * @brief Register all dashboard API endpoints with the web server
 */
void setupDashboardAPI(void);

// =============================================================================
// Dashboard Data Endpoints
// =============================================================================

/**
 * @brief GET /api/dashboard - Get all dashboard-visible objects
 * 
 * Returns objects where showOnDashboard=true with current live values.
 * Response includes object type, index, name, value, unit, status flags.
 */
void handleGetDashboard(void);

/**
 * @brief GET /api/dashboard/alarms - Get active alarm summary
 * 
 * Returns count and list of objects with fault flags set.
 */
void handleGetAlarms(void);

// =============================================================================
// Layout Configuration Endpoints
// =============================================================================

/**
 * @brief GET /api/dashboard/layout - Get current tile layout
 * 
 * Returns ordered array of tile positions with object references.
 */
void handleGetDashboardLayout(void);

/**
 * @brief POST /api/dashboard/layout - Save tile layout
 * 
 * Saves user-arranged tile order to flash. Intentional action only.
 * Body: { "tiles": [{ "type": "adc", "index": 0 }, ...] }
 */
void handleSaveDashboardLayout(void);

// =============================================================================
// Global Control Endpoints
// =============================================================================

/**
 * @brief POST /api/dashboard/enable-all - Enable all outputs and controllers
 * 
 * Sends enable commands to all dashboard-visible outputs and controllers.
 * Returns current RTC timestamp for run timer tracking.
 */
void handleEnableAll(void);

/**
 * @brief POST /api/dashboard/pause - Pause non-temperature controllers
 * 
 * Disables pH, flow, and DO controllers while keeping temperature control active.
 * Used to pause a run without losing temperature regulation.
 */
void handlePauseControllers(void);

/**
 * @brief POST /api/dashboard/disable-all - Disable all outputs and controllers
 * 
 * Sends disable commands to all dashboard-visible outputs and controllers.
 */
void handleDisableAll(void);

/**
 * @brief POST /api/dashboard/clear-volumes - Clear all cumulative volumes
 * 
 * Resets cumulative dosing volumes on all flow controllers.
 */
void handleClearVolumes(void);
