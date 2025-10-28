#include "drv_analogue_pressure.h"
#include "../onboard/drv_dac.h"
#include "../../sys_init.h"

// Unit conversion factors to Pascals
#define PA_TO_PA    1.0f
#define KPA_TO_PA   1000.0f
#define BAR_TO_PA   100000.0f
#define PSI_TO_PA   6894.757f
#define ATM_TO_PA   101325.0f
#define MBAR_TO_PA  100.0f

// Constructor
AnaloguePressureController::AnaloguePressureController(uint8_t dacIndex)
    : _dacIndex(dacIndex) {
    
    // Initialize calibration to defaults (will be set via setCalibration())
    _calibration.scale = 100.0f;   // 100 Pa/mV (0-10V = 0-1000000 Pa = 0-10 bar)
    _calibration.offset = 0.0f;
    _calibration.timestamp = 0;
    
    // Default unit
    strncpy(_unit, "bar", sizeof(_unit) - 1);
    _unit[sizeof(_unit) - 1] = '\0';
    
    // Set control object metadata
    strncpy(_controlObj.setpointUnit, _unit, sizeof(_controlObj.setpointUnit) - 1);
    _controlObj.setpointUnit[sizeof(_controlObj.setpointUnit) - 1] = '\0';
    
    // Initialize control object
    memset(&_controlObj, 0, sizeof(DeviceControl_t));
    
    _controlObj.deviceType = IPC_DEV_PRESSURE_CTRL;
    _controlObj.connected = false;
    _controlObj.fault = false;
    _controlObj.setpoint = 0.0f;
    _controlObj.actualValue = 0.0f;  // Will be populated from sensor object
    _controlObj.sensorCount = 1;     // 1 sensor (actual pressure feedback)
    
    // Initialize sensor object (actual DAC feedback)
    memset(&_pressureSensor, 0, sizeof(PressureSensor_t));
    strncpy(_pressureSensor.unit, _unit, sizeof(_pressureSensor.unit) - 1);
    _pressureSensor.pressure = 0.0f;
    _pressureSensor.fault = false;
}

// Initialize
bool AnaloguePressureController::init() {
    Serial.printf("[PRESSURE] Initializing controller: DAC=%d, scale=%.6f, offset=%.2f\n",
                 _dacIndex, _calibration.scale, _calibration.offset);
    
    // Validate DAC index
    if (_dacIndex != 8 && _dacIndex != 9) {
        _setFault("Invalid DAC index");
        Serial.printf("[PRESSURE] ERROR: Invalid DAC index %d (must be 8 or 9)\n", _dacIndex);
        return false;
    }
    
    // Initialize to zero output
    if (!_writeDACMillivolts(0)) {
        _setFault("Failed to initialize DAC");
        Serial.println("[PRESSURE] ERROR: Failed to write to DAC during init");
        return false;
    }
    
    _controlObj.connected = true;
    _clearFault();
    
    Serial.printf("[PRESSURE] Initialized successfully, unit=%s\n", _unit);
    
    return true;
}

// Update (minimal - no I/O needed)
bool AnaloguePressureController::update() {
    // Update sensor object with actual DAC output
    _updateActualValue();
    
    return true;
}

// Set calibration
void AnaloguePressureController::setCalibration(float scale, float offset, const char* unit) {
    _calibration.scale = scale;
    _calibration.offset = offset;
    _calibration.timestamp = millis();
    
    strncpy(_unit, unit, sizeof(_unit) - 1);
    _unit[sizeof(_unit) - 1] = '\0';
    
    // Update control object unit
    strncpy(_controlObj.setpointUnit, _unit, sizeof(_controlObj.setpointUnit) - 1);
    _controlObj.setpointUnit[sizeof(_controlObj.setpointUnit) - 1] = '\0';
    
    // Update sensor object unit so Sensors tab shows correct unit
    strncpy(_pressureSensor.unit, _unit, sizeof(_pressureSensor.unit) - 1);
    _pressureSensor.unit[sizeof(_pressureSensor.unit) - 1] = '\0';
    
    Serial.printf("[PRESSURE] Calibration updated: scale=%.6f, offset=%.2f, unit=%s\n",
                 _calibration.scale, _calibration.offset, _unit);
}

// Write setpoint
bool AnaloguePressureController::writeSetpoint(float pressure) {
    // Convert to Pascals
    float pressure_Pa = _pressureToPascals(pressure);
    
    // Convert to millivolts using inverse calibration
    uint16_t mV = _pressureToMillivolts(pressure_Pa);
    
    // Write to DAC
    if (!_writeDACMillivolts(mV)) {
        _setFault("Failed to write setpoint to DAC");
        Serial.printf("[PRESSURE] ERROR: Failed to write %.2f %s (%.0f Pa, %d mV)\n",
                     pressure, _unit, pressure_Pa, mV);
        return false;
    }
    
    // Update control object
    _controlObj.setpoint = pressure;
    
    // Update actual value from DAC feedback
    _updateActualValue();
    
    _clearFault();
    
    Serial.printf("[PRESSURE] Setpoint written: %.2f %s → %.0f Pa → %d mV\n",
                 pressure, _unit, pressure_Pa, mV);
    
    return true;
}

// Convert pressure to Pascals
float AnaloguePressureController::_pressureToPascals(float pressure) {
    // Determine conversion factor based on unit
    float factor = PA_TO_PA;  // Default
    
    if (strcmp(_unit, "Pa") == 0) {
        factor = PA_TO_PA;
    } else if (strcmp(_unit, "kPa") == 0) {
        factor = KPA_TO_PA;
    } else if (strcmp(_unit, "bar") == 0) {
        factor = BAR_TO_PA;
    } else if (strcmp(_unit, "psi") == 0 || strcmp(_unit, "PSI") == 0) {
        factor = PSI_TO_PA;
    } else if (strcmp(_unit, "atm") == 0) {
        factor = ATM_TO_PA;
    } else if (strcmp(_unit, "mbar") == 0) {
        factor = MBAR_TO_PA;
    }
    
    return pressure * factor;
}

// Convert pressure to mV using inverse calibration
uint16_t AnaloguePressureController::_pressureToMillivolts(float pressure_Pa) {
    // Inverse calibration formula:
    // Forward:  pressure_Pa = scale * voltage_mV + offset
    // Inverse:  voltage_mV = (pressure_Pa - offset) / scale
    
    if (_calibration.scale == 0.0f) {
        Serial.printf("[PRESSURE] ERROR: Calibration scale is zero\n");
        return 0;
    }
    
    float mV_float = (pressure_Pa - _calibration.offset) / _calibration.scale;
    
    // Clamp to DAC output range (0-10000 mV)
    if (mV_float < 0.0f) {
        mV_float = 0.0f;
    } else if (mV_float > 10000.0f) {
        mV_float = 10000.0f;
    }
    
    return (uint16_t)mV_float;
}

// Write to DAC
bool AnaloguePressureController::_writeDACMillivolts(uint16_t mV) {
    // _dacIndex is already the DAC channel (0 or 1), not object index
    uint8_t dacChannel = _dacIndex;
    
    if (dacChannel > 1) {
        _controlObj.fault = true;
        strncpy(_controlObj.message, "Invalid DAC channel", sizeof(_controlObj.message) - 1);
        Serial.printf("[PRESSURE] ERROR: Invalid DAC channel %d\n", dacChannel);
        return false;
    }
    
    // Clamp to 0-10240 mV range
    if (mV > 10240) mV = 10240;
    
    // Access DAC output directly (will be applied in DAC_update())
    extern AnalogOutput_t dacOutput[2];
    
    Serial.printf("[PRESSURE] Writing to DAC channel %d: %.1f mV (enabled=%d, current value=%.1f)\n", 
                 dacChannel, (float)mV, dacOutput[dacChannel].enabled, dacOutput[dacChannel].value);
    
    dacOutput[dacChannel].value = (float)mV;
    
    Serial.printf("[PRESSURE] DAC channel %d value updated to %.1f mV\n", 
                 dacChannel, dacOutput[dacChannel].value);
    
    return true;
}

// Set fault
void AnaloguePressureController::_setFault(const char* msg) {
    _controlObj.fault = true;
    _controlObj.newMessage = true;
    strncpy(_controlObj.message, msg, sizeof(_controlObj.message) - 1);
    _controlObj.message[sizeof(_controlObj.message) - 1] = '\0';
}

// Clear fault
void AnaloguePressureController::_clearFault() {
    _controlObj.fault = false;
    _controlObj.newMessage = false;
    _controlObj.message[0] = '\0';
}

// Update actual value from DAC feedback
void AnaloguePressureController::_updateActualValue() {
    // Access DAC output directly to read back actual value
    extern AnalogOutput_t dacOutput[2];
    
    uint8_t dacChannel = _dacIndex;
    if (dacChannel > 1) {
        return;  // Invalid channel
    }
    
    // Read actual DAC output in millivolts
    uint16_t actualMv = (uint16_t)dacOutput[dacChannel].value;
    
    // Convert back to pressure in user units
    float actualPressure = _millivoltsToPressure(actualMv);
    
    // Update sensor object
    _pressureSensor.pressure = actualPressure;
    
    // Also update control object's actualValue for convenience
    _controlObj.actualValue = actualPressure;
}

// Convert millivolts to pressure in user units
float AnaloguePressureController::_millivoltsToPressure(uint16_t mV) {
    // Forward calibration formula: pressure_Pa = scale * voltage_mV + offset
    float pressure_Pa = (_calibration.scale * (float)mV) + _calibration.offset;
    
    // Convert from Pascals to user units
    float factor = PA_TO_PA;  // Default
    
    if (strcmp(_unit, "Pa") == 0) {
        factor = PA_TO_PA;
    } else if (strcmp(_unit, "kPa") == 0) {
        factor = KPA_TO_PA;
    } else if (strcmp(_unit, "bar") == 0) {
        factor = BAR_TO_PA;
    } else if (strcmp(_unit, "psi") == 0 || strcmp(_unit, "PSI") == 0) {
        factor = PSI_TO_PA;
    } else if (strcmp(_unit, "atm") == 0) {
        factor = ATM_TO_PA;
    } else if (strcmp(_unit, "mbar") == 0) {
        factor = MBAR_TO_PA;
    }
    
    return pressure_Pa / factor;
}
