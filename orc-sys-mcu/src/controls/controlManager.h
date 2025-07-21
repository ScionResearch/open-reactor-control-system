#pragma once

#include "../sys_init.h"

// Initialize the control manager
void init_controlManager();

// Functions to update control parameters based on API requests
bool updateTemperatureControl(const JsonObject& config);
bool updatePhControl(const JsonObject& config);
// Add other control update functions here as needed, e.g.:
// bool updateStirrerControl(const JsonObject& config);
