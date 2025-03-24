#include "calibrate.h"

Calibrate_t calTable[NUM_CAL_TABLE_ENTRIES];

bool calibrate_init(void) {
    EEPROM.setCommitASAP(false);
    uint8_t version = EEPROM.read(CAL_FLASH_VERSION_ADDR);
    if (version != CAL_FLASH_VERSION) {
        EEPROM.put(CALIBRATE_FLASH_ADDR, calTable);
        EEPROM.write(CAL_FLASH_VERSION_ADDR, CAL_FLASH_VERSION);
        EEPROM.commit();
        return false;
    }
    EEPROM.get(CALIBRATE_FLASH_ADDR, calTable);
    for (int i = 0; i < NUM_CAL_TABLE_ENTRIES; i++) {
        Serial.printf("Calibration table entry %d: scale = %f, offset = %f\n", i, calTable[i].scale, calTable[i].offset);
    }
    return true;
}

bool calibrate_calc(float expected_1, float expected_2, float actual_1, float actual_2, Calibrate_t *calObj) {
    if (expected_1 >= expected_2) return false;
    calObj->scale = (expected_2 - expected_1) / (actual_2 - actual_1);
    calObj->offset = expected_1 - (calObj->scale * actual_1);
    return true;
}

bool calibrate_save(void) {
    EEPROM.put(CALIBRATE_FLASH_ADDR, calTable);
    EEPROM.commit();
    return true;
}

bool calibrate_load(void) {
    EEPROM.get(CALIBRATE_FLASH_ADDR, calTable);
    return true;
}