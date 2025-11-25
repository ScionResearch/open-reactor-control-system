#pragma once

#include "sys_init.h"

#define HAMILTON_PMC_REG_SIZE   10      // Number of registers that must be read when reading PMCs
#define HAMILTON_PMC_1_ADDR     2089    // Primary Measurement Channel 1 register block
#define HAMILTON_PMC_6_ADDR     2409    // Primary Measurement Channel 6 register block

// Hamilton units array - array of string pointers (static to avoid multiple definition)
static const char* hamiltonUnits[32] = {
    "none", "K", "°C", "°F", "%-vol", "%-sat", "ug/l ppb", "mg/l", "g/l",
    "uS/cm", "mS/cm", "1/cm", "pH", "mV/pH", "kOhm", "MOhm",
    "pA", "nA", "uA", "mA", "uV", "mV", "V", "mbar", "Pa", "Ohm", "%/°C", "°",
    "", "", "", "SPECIAL"
};

/**
 * @brief Get Hamilton unit string from unit code
 * 
 * @param unitCode Unit code as a bit field (first set bit determines unit)
 * @return const char* Pointer to unit string, or "unknown" if invalid
 */
inline const char* getHamiltonUnit(uint32_t unitCode) {
    for (uint8_t unitIndex = 0; unitIndex < 32; unitIndex++) {
        if (unitCode & (1 << unitIndex)) {
            return hamiltonUnits[unitIndex];
        }
    }
    return "unknown";  // Default if no bit is set
}
