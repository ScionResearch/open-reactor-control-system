#include "calibrate.h"

Calibrate_t calTable[NUM_CAL_TABLE_ENTRIES];

bool calibrate_calc(float expected_1, float expected_2, float actual_1, float actual_2, Calibrate_t *calObj) {
    if (expected_1 >= expected_2) return false;
    calObj->scale = (expected_2 - expected_1) / (actual_2 - actual_1);
    calObj->offset = expected_1 - (calObj->scale * actual_1);
    return true;
}