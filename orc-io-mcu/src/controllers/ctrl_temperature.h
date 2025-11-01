#pragma once

#include <Arduino.h>
#include "../drivers/objects.h"

/**
 * @brief PID Temperature Controller with Auto-Tune
 * 
 * Implements a PID control loop for temperature regulation using index-based
 * references to temperature sensors and analog outputs in the object index.
 * 
 * Features:
 * - Standard PID control with anti-windup
 * - Relay-based auto-tune for automatic PID parameter determination
 * - Setpoint limits and output clamping
 * - Fault detection and handling
 * - Scheduler-compatible (call update() periodically)
 */
class TemperatureController {
public:
    TemperatureController();
    ~TemperatureController();
    
    // ========================================================================
    // INITIALIZATION
    // ========================================================================
    
    /**
     * @brief Initialize controller with a control structure
     * @param control Pointer to TemperatureControl_t structure
     * @return true if successful
     */
    bool begin(TemperatureControl_t* control);
    
    /**
     * @brief Assign temperature sensor by object index
     * @param sensorIndex Index in objIndex[] (must be temperature sensor type)
     * @return true if valid sensor index
     */
    bool assignSensor(uint16_t sensorIndex);
    
    /**
     * @brief Assign output by object index
     * @param outputIndex Index in objIndex[] (digital outputs 21-25 only)
     * @return true if valid output index
     */
    bool assignOutput(uint16_t outputIndex);
    
    // ========================================================================
    // CONTROL LOOP
    // ========================================================================
    
    /**
     * @brief Main control loop - call from scheduler
     * Updates PID calculation or auto-tune state
     */
    void update();
    
    /**
     * @brief Enable temperature control
     */
    void enable();
    
    /**
     * @brief Disable temperature control (output set to 0)
     */
    void disable();
    
    /**
     * @brief Check if controller is enabled
     * @return true if enabled
     */
    bool isEnabled();
    
    // ========================================================================
    // SETPOINT MANAGEMENT
    // ========================================================================
    
    /**
     * @brief Set target temperature setpoint
     * @param setpoint Target temperature (will be clamped to min/max limits)
     * @return true if setpoint accepted
     */
    bool setSetpoint(float setpoint);
    
    /**
     * @brief Get current setpoint
     * @return Current setpoint temperature
     */
    float getSetpoint();
    
    /**
     * @brief Set setpoint limits
     * @param minSetpoint Minimum allowed setpoint
     * @param maxSetpoint Maximum allowed setpoint
     */
    void setSetpointLimits(float minSetpoint, float maxSetpoint);
    
    // ========================================================================
    // PID TUNING
    // ========================================================================
    
    /**
     * @brief Set PID gains manually
     * @param kp Proportional gain
     * @param ki Integral gain
     * @param kd Derivative gain
     */
    void setPIDGains(float kp, float ki, float kd);
    
    /**
     * @brief Get current PID gains
     * @param kp Pointer to store Kp
     * @param ki Pointer to store Ki
     * @param kd Pointer to store Kd
     */
    void getPIDGains(float* kp, float* ki, float* kd);
    
    /**
     * @brief Set output limits
     * @param minOutput Minimum output value (%)
     * @param maxOutput Maximum output value (%)
     */
    void setOutputLimits(float minOutput, float maxOutput);
    
    // ========================================================================
    // AUTO-TUNE
    // ========================================================================
    
    /**
     * @brief Start auto-tune procedure using relay method
     * @param targetSetpoint Target setpoint for auto-tune
     * @param outputStep Output step size for relay (default 100%)
     * @return true if auto-tune started successfully
     */
    bool startAutotune(float targetSetpoint, float outputStep = 100.0);
    
    /**
     * @brief Stop auto-tune procedure
     */
    void stopAutotune();
    
    /**
     * @brief Check if auto-tune is running
     * @return true if auto-tuning in progress
     */
    bool isAutotuning();
    
    /**
     * @brief Get auto-tune progress
     * @return Progress percentage (0-100)
     */
    float getAutotuneProgress();
    
    // ========================================================================
    // STATUS
    // ========================================================================
    
    /**
     * @brief Get current control error
     * @return Error (setpoint - current temperature)
     */
    float getCurrentError();
    
    /**
     * @brief Get current output value
     * @return Output percentage (0-100)
     */
    float getCurrentOutput();
    
    /**
     * @brief Get current temperature reading
     * @return Current temperature
     */
    float getCurrentTemperature();
    
    /**
     * @brief Check if controller has a fault
     * @return true if fault condition exists
     */
    bool hasFault();
    
    /**
     * @brief Get fault/status message
     * @return Pointer to message string
     */
    const char* getMessage();
    
private:
    // Reference to control structure
    TemperatureControl_t* _control;
    
    // PID state variables
    float _integral;                    // Integral accumulator
    float _lastError;                   // Previous error for derivative
    unsigned long _lastUpdateTime;      // Last update timestamp (ms)
    
    // Auto-tune state machine
    enum AutotuneState {
        AUTOTUNE_OFF,
        AUTOTUNE_WAITING_STABILIZE,
        AUTOTUNE_RELAY_HIGH,
        AUTOTUNE_RELAY_LOW,
        AUTOTUNE_ANALYZING,
        AUTOTUNE_COMPLETE,
        AUTOTUNE_FAILED
    };
    
    AutotuneState _autotuneState;
    float _autotuneOutputHigh;          // High relay output
    float _autotuneOutputLow;           // Low relay output
    float _autotunePeaks[10];           // Store peak/valley values
    unsigned long _autotunePeakTimes[10]; // Peak/valley timestamps
    uint8_t _autotunePeakCount;         // Number of peaks detected
    unsigned long _autotuneStartTime;   // Auto-tune start time
    float _autotuneSetpoint;            // Target for auto-tune
    bool _autotuneLastCrossDirection;   // Last crossing direction (up/down)
    float _autotuneLastTemp;            // Previous temperature reading
    bool _autotuneAutoEnabled;          // Track if we auto-enabled controller for autotune
    bool _autotuneLookingForPeak;       // True = looking for peak, False = looking for valley
    float _autotuneExtreme;             // Current max (for peak) or min (for valley) since last crossing
    bool _autotuneJustCrossed;          // True right after crossing setpoint
    
    // ========================================================================
    // PRIVATE HELPER METHODS
    // ========================================================================
    
    /**
     * @brief Read temperature from assigned sensor
     * @return Temperature value, or NAN if fault
     */
    float _readSensor();
    
    /**
     * @brief Write output value to assigned output
     * @param value Output value (will be clamped to limits)
     * @return true if successful
     */
    bool _writeOutput(float value);
    
    /**
     * @brief Main PID computation
     */
    void _computePID();
    
    /**
     * @brief Update auto-tune state machine
     */
    void _updateAutotune();
    
    /**
     * @brief Calculate ON/OFF output with hysteresis
     * @param error Current error value
     * @return Output value (0 or 100)
     */
    float _calculateOnOffOutput(float error);
    
    /**
     * @brief Calculate PID output
     * @param error Current error value
     * @param dt Time delta (seconds)
     * @return Calculated output value
     */
    float _calculatePIDOutput(float error, float dt);
    
    /**
     * @brief Analyze auto-tune results and calculate PID gains
     */
    void _analyzeAutotuneResults();
    
    /**
     * @brief Reset PID state variables
     */
    void _resetPIDState();
    
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
