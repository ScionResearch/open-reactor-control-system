#pragma once

#include "sys_init.h"

#define CAL_FLASH_VERSION_ADDR  0
#define CAL_FLASH_VERSION       1   // Increment to force re-initialise when structure changes
#define CAL_DAC_PTR             0
#define CAL_ADC_PTR             2
#define CAL_RTD_PTR             10
#define CAL_MOT_PTR             13

// Compile time contants
constexpr size_t CALIBRATE_OBJ_SIZE = sizeof(Calibrate_t);
constexpr int NUM_CAL_TABLE_ENTRIES = 20;
constexpr int CALIBRATE_FLASH_ADDR = 0x0010;

extern Calibrate_t calTable[NUM_CAL_TABLE_ENTRIES];

bool calibrate_init(void);
bool calibrate_calc(float expected_1, float expected_2, float actual_1, float actual_2, Calibrate_t *calObj);
bool calibrate_save(void);
bool calibrate_load(void);