#pragma once

#include "sys_init.h"

/**
 * @brief Dissolved Oxygen Controller Class
 * 
 * Profile-based DO control using linear interpolation.
 * Reads DO sensor, calculates error, interpolates profile curve,
 * and outputs to stirrer (DC motor or stepper) and/or MFC.
 * 
 * Control loop runs at 1 Hz (once per second).
 */
class DOController {
public:
    /**
     * @brief Construct a new DO Controller object
     * @param control Pointer to the DissolvedOxygenControl_t structure
     */
    DOController(DissolvedOxygenControl_t* control);
    
    /**
     * @brief Destroy the DO Controller object
     */
    ~DOController();
    
    /**
     * @brief Update controller (call every second)
     * Reads sensor, calculates outputs, applies to actuators
     */
    void update();
    
    /**
     * @brief Set the setpoint
     * @param setpoint_mg_L Target DO in mg/L
     */
    void setSetpoint(float setpoint_mg_L);
    
    /**
     * @brief Set the profile curve
     * @param numPoints Number of points in profile (0-20)
     * @param points Array of profile points (must be sorted by error_mg_L)
     */
    void setProfile(uint8_t numPoints, const DOProfilePoint* points);
    
private:
    DissolvedOxygenControl_t* _control;  ///< Pointer to control structure
    uint32_t _lastUpdateTime;            ///< Last update timestamp (millis)
    
    /**
     * @brief Read DO sensor value
     * Scans object index for DO sensor (type OBJ_T_DISSOLVED_OXYGEN_SENSOR)
     * @return float DO value in mg/L, or NAN if sensor not found
     */
    float _readDOSensor();
    
    /**
     * @brief Calculate control outputs from profile interpolation
     * Updates _control->currentStirrerOutput and _control->currentMFCOutput
     */
    void _calculateOutputs();
    
    /**
     * @brief Apply stirrer output
     * @param output Stirrer command (% for DC motor, RPM for stepper)
     */
    void _applyStirrerOutput(float output);
    
    /**
     * @brief Apply MFC output
     * @param output_mL_min MFC flow rate in mL/min
     */
    void _applyMFCOutput(float output_mL_min);
    
    /**
     * @brief Interpolate profile curve for given error
     * @param error Error value (setpoint - current DO)
     * @param forStirrer true=interpolate stirrer curve, false=interpolate MFC curve
     * @return float Interpolated output value
     */
    float _interpolateProfile(float error, bool forStirrer);
    
    /**
     * @brief Sort profile points by error value (ascending)
     * Ensures profile is sorted for binary search / interpolation
     */
    void _sortProfile();
};
