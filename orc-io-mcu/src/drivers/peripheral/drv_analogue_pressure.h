#ifndef DRV_ANALOGUE_PRESSURE_H
#define DRV_ANALOGUE_PRESSURE_H

#include <Arduino.h>
#include "../objects.h"

/**
 * @brief Analogue Pressure Controller Driver
 * 
 * Interface: Analogue Output (0-10V DAC)
 * Control: Set pressure → convert to mV → write to DAC
 * 
 * Features:
 * - Unit-agnostic calibration (stores as Pascals internally)
 * - Linear interpolation: pressure → mV
 * - No feedback sensor (write-only control)
 * - Direct DAC access (no polling/communication overhead)
 * 
 * Calibration:
 * - minPressure_Pa: Pressure at 0V output
 * - maxPressure_Pa: Pressure at 10V output
 * - User specifies in any units, driver converts to Pa
 */
class AnaloguePressureController {
public:
    /**
     * @brief Construct a new Analogue Pressure Controller object
     * 
     * @param dacIndex DAC output index (8 or 9)
     */
    AnaloguePressureController(uint8_t dacIndex);
    
    /**
     * @brief Initialize the controller
     * 
     * Verifies DAC exists and is accessible.
     * 
     * @return true if initialization successful
     */
    bool init();
    
    /**
     * @brief Periodic update function
     * 
     * Minimal implementation - no I/O needed since DAC write is immediate.
     * Included for consistency with device driver interface.
     * 
     * @return true always (no operations to fail)
     */
    bool update();
    
    /**
     * @brief Set calibration parameters
     * 
     * @param scale Calibration scale factor (Pa/mV)
     * @param offset Calibration offset (Pa)
     * @param unit Pressure unit for display
     */
    void setCalibration(float scale, float offset, const char* unit);
    
    /**
     * @brief Write pressure setpoint
     * 
     * Converts pressure (in user units) → Pascals → mV (via inverse calibration) → DAC output
     * Inverse formula: voltage_mV = (pressure_Pa - offset) / scale
     * 
     * @param pressure Desired pressure setpoint in configured unit
     * @return true if setpoint written successfully
     */
    bool writeSetpoint(float pressure);
    
    /**
     * @brief Get control object pointer
     * 
     * @return DeviceControl_t* Pointer to internal control object
     */
    DeviceControl_t* getControlObject() { return &_controlObj; }
    
    /**
     * @brief Get sensor object for actual DAC feedback
     * 
     * @param index Sensor index (0 = actual pressure feedback)
     * @return PressureSensor_t* Pointer to sensor object, or nullptr if index invalid
     * @note Returns actual DAC output converted back to pressure units
     */
    void* getSensorObject(uint8_t index) { 
        return (index == 0) ? (void*)&_pressureSensor : nullptr; 
    }
    
    /**
     * @brief Get sensor count
     * 
     * @return uint8_t Always returns 1 (actual pressure feedback)
     */
    uint8_t getSensorCount() const { return 1; }
    
private:
    // Configuration
    uint8_t _dacIndex;           // DAC channel (8 or 9)
    Calibrate_t _calibration;    // Scale/offset calibration
    char _unit[8];               // User-facing pressure unit
    
    // Control object
    DeviceControl_t _controlObj;
    
    // Sensor object (actual DAC feedback)
    PressureSensor_t _pressureSensor;
    
    // Helper functions
    
    /**
     * @brief Convert user pressure to Pascals
     * 
     * @param pressure Pressure in display units
     * @return float Pressure in Pascals
     */
    float _pressureToPascals(float pressure);
    
    /**
     * @brief Convert pressure in Pascals to millivolts using inverse calibration
     * 
     * @param pressure_Pa Pressure in Pascals
     * @return uint16_t DAC output in millivolts (0-10000)
     */
    uint16_t _pressureToMillivolts(float pressure_Pa);
    
    /**
     * @brief Write millivolts to DAC output
     * 
     * @param mV Output voltage in millivolts (0-10000)
     * @return true if write successful
     */
    bool _writeDACMillivolts(uint16_t mV);
    
    /**
     * @brief Set fault condition
     * 
     * @param msg Fault message
     */
    void _setFault(const char* msg);
    
    /**
     * @brief Clear fault condition
     */
    void _clearFault();
    
    /**
     * @brief Update sensor object with actual DAC output
     * 
     * Reads back the actual DAC value and converts to pressure units
     * for feedback/diagnostic purposes.
     */
    void _updateActualValue();
    
    /**
     * @brief Convert millivolts to pressure in user units
     * 
     * @param mV DAC output in millivolts
     * @return float Pressure in user units
     */
    float _millivoltsToPressure(uint16_t mV);
};

#endif
