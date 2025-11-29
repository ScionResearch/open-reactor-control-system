#pragma once

/**
 * @file apiDevices.h
 * @brief Device management API endpoints
 * 
 * Handles:
 * - /api/devices - List/Create devices
 * - /api/devices/{index} - Get/Update/Delete device
 * - Device control commands
 */

#include <Arduino.h>

void setupDevicesAPI(void);

// Dynamic route handler for device endpoints
void handleDynamicDeviceRoute(void);
void handleDynamicDeviceControlRoute(void);

// Device CRUD handlers
void handleGetDevices(void);
void handleGetDevice(void);
void handleCreateDevice(void);
void handleUpdateDevice(void);
void handleDeleteDevice(void);

// Device control handlers
void handleSetDeviceSetpoint(uint16_t controlIndex);
