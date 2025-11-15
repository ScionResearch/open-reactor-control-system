#pragma once

#include "sys_init.h"

#define CAL_DAC_PTR             0
#define CAL_ADC_PTR             2
#define CAL_RTD_PTR             10

// Compile time contants
constexpr size_t CALIBRATE_OBJ_SIZE = sizeof(Calibrate_t);
constexpr int NUM_CAL_TABLE_ENTRIES = 20;

extern Calibrate_t calTable[NUM_CAL_TABLE_ENTRIES];

bool calibrate_calc(float expected_1, float expected_2, float actual_1, float actual_2, Calibrate_t *calObj);