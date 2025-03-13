#pragma once

#include "../sys_init.h"

// Voltage divider constants
#define V_PSU_MUL_V      0.01726436769
#define V_5V_MUL_V       0.00240673828

// Limits
#define V_PSU_MIN        22.0
#define V_PSU_MAX        29.0
#define V_20V_MIN        19.5
#define V_20V_MAX        20.5
#define V_5V_MIN         4.5
#define V_5V_MAX         5.5


void init_powerManager(void);
void handlePowerManager(void);