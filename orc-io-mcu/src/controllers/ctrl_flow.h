#pragma once

#include <Arduino.h>
#include "../drivers/objects.h"

/**
 * @brief Flow Controller
 * 
 * Open-loop flow controller for feed/waste pumps.
 * Controls pumps using timed dosing cycles based on flow rate setpoint and calibration data.
 * No sensor feedback - timing is calculated from user-provided calibration.
 * 
 * Flow Calculation:
 * - User calibrates: "At dose time X ms and power Y%, pump delivers Z mL"
 * - User sets flow rate: "I want R mL/min"
 * - Controller calculates: "Need to dose every (Z / R * 60000) ms"
 * 
 * Features:
 * - Flow rate setpoint (mL/min)
 * - Automatic interval calculation from calibration
 * - Support for digital outputs or DC motors
 * - Cumulative volume tracking
 * - Safety limits (min interval, max dose time)
 * - Manual dosing
 */
class FlowController {
public:
    /**
     * @brief Construct a new Flow Controller
     * @param control Pointer to FlowControl_t structure in object index
     */
    FlowController(FlowControl_t* control);
    
    /**
     * @brief Destroy the Flow Controller
     */
    ~FlowController();
    
    /**
     * @brief Update controller (called periodically)
     * Manages automatic dosing timing, checks if dose is due
     */
    void update();
    
    /**
     * @brief Set flow rate setpoint
     * @param flowRate_mL_min Target flow rate in mL/min
     */
    void setFlowRate(float flowRate_mL_min);
    
    /**
     * @brief Manual dose (one cycle)
     * @return true if dose started successfully
     */
    bool manualDose();
    
    /**
     * @brief Reset cumulative volume to zero
     */
    void resetVolume();
    
    /**
     * @brief Recalculate dosing parameters from current calibration and flow rate
     * Called after configuration changes
     */
    void calculateDosingParameters();
    
    /**
     * @brief Get current flow rate setpoint
     * @return float Flow rate in mL/min
     */
    float getFlowRate() { return _control->flowRate_mL_min; }
    
    /**
     * @brief Get current output state
     * @return uint8_t 0=off, 1=dosing
     */
    uint8_t getCurrentOutput() { return _control->currentOutput; }
    
    /**
     * @brief Get cumulative volume pumped
     * @return float Volume in mL
     */
    float getCumulativeVolume() { return _control->cumulativeVolume_mL; }

private:
    FlowControl_t* _control;    // Pointer to control structure
    
    uint32_t _doseStartTime;    // When current dose started (millis())
    bool _dosing;               // Currently dosing
    
    /**
     * @brief Check if dosing is allowed (interval timing)
     * @return true if interval has elapsed and dose is due
     */
    bool _canDose();
    
    /**
     * @brief Start a dose cycle
     * @return true if started successfully
     */
    bool _startDose();
    
    /**
     * @brief Activate dosing output
     * @return true if activated successfully
     */
    bool _activateOutput();
    
    /**
     * @brief Stop dosing output
     */
    void _stopOutput();
    
    /**
     * @brief Update dosing timeout (stop after duration elapsed)
     * Checks if current dose has finished, stops output and updates volume
     */
    void _updateDosingTimeout();
};
