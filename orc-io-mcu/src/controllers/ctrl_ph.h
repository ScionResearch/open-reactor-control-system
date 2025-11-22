#pragma once

#include <Arduino.h>
#include "../drivers/objects.h"

/**
 * @brief pH Controller
 * 
 * Simple dosing controller for pH regulation using acid and/or alkaline dosing.
 * Controls pH by activating outputs (digital or motor-driven) for specified durations
 * when pH deviates from setpoint beyond the deadband.
 * 
 * Features:
 * - Dual dosing (acid when pH too high, alkaline when pH too low)
 * - Configurable deadband (hysteresis) to prevent oscillation
 * - Minimum interval between doses to prevent overshoot
 * - Support for digital outputs or DC motors
 * - Manual dosing commands
 */
class pHController {
public:
    /**
     * @brief Construct a new pH Controller
     * @param control Pointer to pHControl_t structure in object index
     */
    pHController(pHControl_t* control);
    
    /**
     * @brief Destroy the pH Controller
     */
    ~pHController();
    
    /**
     * @brief Update controller (called periodically)
     * Reads pH sensor, checks if dosing is needed, manages dosing timing
     */
    void update();
    
    /**
     * @brief Set pH setpoint
     * @param pH Target pH value
     */
    void setSetpoint(float pH);
    
    /**
     * @brief Manual acid dose
     * @return true if dose started successfully
     */
    bool doseAcid();
    
    /**
     * @brief Manual alkaline dose
     * @return true if dose started successfully
     */
    bool doseAlkaline();
    
    /**
     * @brief Reset acid cumulative volume to zero
     */
    void resetAcidVolume();
    
    /**
     * @brief Reset alkaline cumulative volume to zero
     */
    void resetAlkalineVolume();
    
    /**
     * @brief Get current pH reading
     * @return float Current pH
     */
    float getCurrentpH() { return _control->currentpH; }
    
    /**
     * @brief Get current output state
     * @return float 0=off, 1=dosing acid, 2=dosing alkaline
     */
    float getCurrentOutput() { return _control->currentOutput; }

private:
    pHControl_t* _control;      // Pointer to control structure
    
    uint32_t _doseStartTime;    // When current dose started (millis())
    bool _dosing;               // Currently dosing
    bool _dosingAcid;           // true=acid, false=alkaline
    
    /**
     * @brief Read pH from sensor
     * @return float pH value or NaN if error
     */
    float _readpH();
    
    /**
     * @brief Check if automatic dosing is needed
     */
    void _checkDosing();
    
    /**
     * @brief Check if acid dosing is allowed (interval timing)
     * @return true if allowed
     */
    bool _canDoseAcid();
    
    /**
     * @brief Check if alkaline dosing is allowed (interval timing)
     * @return true if allowed
     */
    bool _canDoseAlkaline();
    
    /**
     * @brief Activate dosing output
     * @param type 0=Digital, 1=Motor, 2=MFC
     * @param index Output index (21-25 digital, 27-30 motor, 50-69 MFC)
     * @param power Motor power (0-100%), ignored for digital/MFC
     * @param duration Dose duration (milliseconds)
     * @return true if activated successfully
     */
    bool _activateOutput(uint8_t type, uint8_t index, uint8_t power, uint16_t duration);
    
    /**
     * @brief Stop dosing output
     * @param type 0=Digital, 1=Motor, 2=MFC
     * @param index Output index (21-25 digital, 27-30 motor, 50-69 MFC)
     */
    void _stopOutput(uint8_t type, uint8_t index);
    
    /**
     * @brief Update dosing timeout (stop after duration elapsed)
     */
    void _updateDosingTimeout();

    /**
     * @brief Validate sensor and output indices
     * @return true if indices are valid
     */
    bool _validateIndices();
    
    /**
     * @brief Set fault condition
     * @param message Fault message
     */
    void _setFault(const char* message);
    
    /**
     * @brief Clear fault condition
     */
    void _clearFault();
};
