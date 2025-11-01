#include "ctrl_temperature.h"
#include <math.h>

// External reference to object index
extern ObjectIndex_t objIndex[];
extern int numObjects;

// ============================================================================
// CONSTRUCTOR / DESTRUCTOR
// ============================================================================

TemperatureController::TemperatureController() :
    _control(nullptr),
    _integral(0.0),
    _lastError(0.0),
    _lastUpdateTime(0),
    _autotuneState(AUTOTUNE_OFF),
    _autotuneOutputHigh(0.0),
    _autotuneOutputLow(0.0),
    _autotunePeakCount(0),
    _autotuneStartTime(0),
    _autotuneSetpoint(0.0),
    _autotuneLastCrossDirection(false),
    _autotuneLastTemp(0.0)
{
    memset(_autotunePeaks, 0, sizeof(_autotunePeaks));
    memset(_autotunePeakTimes, 0, sizeof(_autotunePeakTimes));
}

TemperatureController::~TemperatureController() {
    // Nothing to clean up
}

// ============================================================================
// INITIALIZATION
// ============================================================================

bool TemperatureController::begin(TemperatureControl_t* control) {
    if (control == nullptr) {
        return false;
    }
    
    _control = control;
    
    // Initialize control structure defaults if needed
    if (_control->setpointMin == 0.0 && _control->setpointMax == 0.0) {
        _control->setpointMin = 0.0;
        _control->setpointMax = 100.0;
    }
    
    if (_control->outputMin == 0.0 && _control->outputMax == 0.0) {
        _control->outputMin = 0.0;
        _control->outputMax = 100.0;
    }
    
    // Reset state
    _resetPIDState();
    _control->enabled = false;
    _control->autotuning = false;
    _control->fault = false;
    strcpy(_control->message, "Controller initialized");
    
    return true;
}

bool TemperatureController::assignSensor(uint16_t sensorIndex) {
    if (sensorIndex >= MAX_NUM_OBJECTS) {
        _setFault("Invalid sensor index");
        return false;
    }
    
    if (!objIndex[sensorIndex].valid) {
        _setFault("Sensor index not enrolled");
        return false;
    }
    
    if (objIndex[sensorIndex].type != OBJ_T_TEMPERATURE_SENSOR) {
        _setFault("Object is not a temperature sensor");
        return false;
    }
    
    _control->sensorIndex = sensorIndex;
    return true;
}

bool TemperatureController::assignOutput(uint16_t outputIndex) {
    if (outputIndex >= MAX_NUM_OBJECTS) {
        _setFault("Invalid output index");
        return false;
    }
    
    if (!objIndex[outputIndex].valid) {
        _setFault("Output index not enrolled");
        return false;
    }
    
    // Only accept digital outputs (21-25) for temperature control
    // DAC outputs (8-9) are not suitable for heater control
    if (objIndex[outputIndex].type != OBJ_T_DIGITAL_OUTPUT) {
        _setFault("Output must be digital output (21-25)");
        return false;
    }
    
    _control->outputIndex = outputIndex;
    return true;
}

// ============================================================================
// CONTROL LOOP
// ============================================================================

void TemperatureController::update() {
    if (!_control || !_control->enabled) {
        return;
    }
    
    // Validate indices
    if (!_validateIndices()) {
        return;
    }
    
    // Update based on mode
    if (_control->autotuning) {
        _updateAutotune();
    } else {
        _computePID();
    }
}

void TemperatureController::enable() {
    if (!_control) return;
    
    if (!_validateIndices()) {
        _setFault("Cannot enable: invalid sensor/output indices");
        return;
    }
    
    _resetPIDState();
    _control->enabled = true;
    _clearFault();
}

void TemperatureController::disable() {
    if (!_control) return;
    
    _control->enabled = false;
    _control->autotuning = false;
    _writeOutput(0.0);  // Turn off output
    _autotuneState = AUTOTUNE_OFF;
}

bool TemperatureController::isEnabled() {
    return _control && _control->enabled;
}

// ============================================================================
// SETPOINT MANAGEMENT
// ============================================================================

bool TemperatureController::setSetpoint(float setpoint) {
    if (!_control) return false;
    
    // Clamp to limits
    if (setpoint < _control->setpointMin) {
        setpoint = _control->setpointMin;
    }
    if (setpoint > _control->setpointMax) {
        setpoint = _control->setpointMax;
    }
    
    _control->setpoint = setpoint;
    
    // Reset integral when setpoint changes significantly
    if (fabs(_lastError - (_control->setpoint - _control->currentTemp)) > 5.0) {
        _integral = 0.0;
    }
    
    return true;
}

float TemperatureController::getSetpoint() {
    return _control ? _control->setpoint : 0.0;
}

void TemperatureController::setSetpointLimits(float minSetpoint, float maxSetpoint) {
    if (!_control) return;
    
    _control->setpointMin = minSetpoint;
    _control->setpointMax = maxSetpoint;
    
    // Clamp current setpoint if needed
    if (_control->setpoint < minSetpoint) {
        _control->setpoint = minSetpoint;
    }
    if (_control->setpoint > maxSetpoint) {
        _control->setpoint = maxSetpoint;
    }
}

// ============================================================================
// PID TUNING
// ============================================================================

void TemperatureController::setPIDGains(float kp, float ki, float kd) {
    if (!_control) return;
    
    _control->kp = kp;
    _control->ki = ki;
    _control->kd = kd;
    
    // Reset integral to avoid bumps
    _integral = 0.0;
}

void TemperatureController::getPIDGains(float* kp, float* ki, float* kd) {
    if (!_control) return;
    
    if (kp) *kp = _control->kp;
    if (ki) *ki = _control->ki;
    if (kd) *kd = _control->kd;
}

void TemperatureController::setOutputLimits(float minOutput, float maxOutput) {
    if (!_control) return;
    
    _control->outputMin = minOutput;
    _control->outputMax = maxOutput;
}

// ============================================================================
// AUTO-TUNE
// ============================================================================

bool TemperatureController::startAutotune(float targetSetpoint, float outputStep) {
    if (!_control) {
        _setFault("Cannot start autotune: invalid controller");
        return false;
    }
    
    if (!_validateIndices()) {
        _setFault("Cannot start autotune: invalid indices");
        return false;
    }
    
    // Validate parameters
    if (outputStep <= 0.0 || outputStep > 100.0) {
        _setFault("Invalid output step for autotune");
        return false;
    }
    
    // Read current temperature
    float currentTemp = _readSensor();
    if (isnan(currentTemp)) {
        _setFault("Cannot start autotune: sensor fault");
        return false;
    }
    
    // Verify we're starting below target (need room to heat up)
    if (currentTemp >= targetSetpoint - 2.0) {
        _setFault("Start temp must be at least 2°C below target for autotune");
        return false;
    }
    
    // Auto-enable controller if not already enabled
    _autotuneAutoEnabled = false;
    if (!_control->enabled) {
        _control->enabled = true;
        _autotuneAutoEnabled = true;
        Serial.println("[TempCtrl] Auto-enabled controller for autotune");
    }
    
    // Initialize auto-tune
    _autotuneState = AUTOTUNE_RELAY_HIGH;  // Start heating immediately
    _control->autotuning = true;
    _autotuneSetpoint = targetSetpoint;
    _control->setpoint = targetSetpoint;
    _autotunePeakCount = 0;
    _autotuneStartTime = millis();
    
    // Calculate relay outputs for full range (0% to 100% by default)
    _autotuneOutputHigh = outputStep;  // e.g., 100%
    _autotuneOutputLow = 0.0;          // 0%
    
    // Clamp to output limits
    if (_autotuneOutputHigh > _control->outputMax) _autotuneOutputHigh = _control->outputMax;
    if (_autotuneOutputLow < _control->outputMin) _autotuneOutputLow = _control->outputMin;
    
    // Start with HIGH output to heat up quickly from below setpoint
    _writeOutput(_autotuneOutputHigh);
    _autotuneLastTemp = currentTemp;
    
    // Initialize peak/valley tracking
    _autotuneLookingForPeak = true;   // Start looking for peak (we're heating up)
    _autotuneExtreme = currentTemp;    // Track max/min temp
    _autotuneJustCrossed = false;      // Haven't crossed setpoint yet
    
    Serial.printf("[TempCtrl] Auto-tune started: setpoint=%.1f, step=%.1f%% (%.0f%% to %.0f%%)\n",
                  targetSetpoint, outputStep, _autotuneOutputLow, _autotuneOutputHigh);
    sprintf(_control->message, "Auto-tune in progress");
    
    return true;
}

void TemperatureController::stopAutotune() {
    if (!_control) return;
    
    _control->autotuning = false;
    _autotuneState = AUTOTUNE_OFF;
    _integral = 0.0;  // Reset integral
    
    // Auto-disable controller if we auto-enabled it
    if (_autotuneAutoEnabled) {
        _control->enabled = false;
        _autotuneAutoEnabled = false;
        Serial.println("[TempCtrl] Auto-disabled controller after autotune");
    }
    
    Serial.println("[TempCtrl] Auto-tune stopped");
}

bool TemperatureController::isAutotuning() {
    return _control && _control->autotuning;
}

float TemperatureController::getAutotuneProgress() {
    if (!_control || !_control->autotuning) return 0.0;
    
    // Progress based on peaks detected (need at least 6 for good results)
    float progress = (_autotunePeakCount / 6.0) * 100.0;
    if (progress > 100.0) progress = 100.0;
    
    return progress;
}

// ============================================================================
// STATUS
// ============================================================================

float TemperatureController::getCurrentError() {
    return _control ? _control->processError : 0.0;
}

float TemperatureController::getCurrentOutput() {
    return _control ? _control->currentOutput : 0.0;
}

float TemperatureController::getCurrentTemperature() {
    return _control ? _control->currentTemp : 0.0;
}

bool TemperatureController::hasFault() {
    return _control && _control->fault;
}

const char* TemperatureController::getMessage() {
    return _control ? _control->message : "No controller";
}

// ============================================================================
// PRIVATE HELPER METHODS
// ============================================================================

float TemperatureController::_readSensor() {
    if (!_control || !_validateIndices()) {
        return NAN;
    }
    
    TemperatureSensor_t* sensor = (TemperatureSensor_t*)objIndex[_control->sensorIndex].obj;
    
    if (sensor->fault) {
        _setFault("Sensor fault detected");
        return NAN;
    }
    
    return sensor->temperature;
}

bool TemperatureController::_writeOutput(float value) {
    if (!_control || !_validateIndices()) {
        return false;
    }
    
    // Clamp to output limits
    if (value < _control->outputMin) value = _control->outputMin;
    if (value > _control->outputMax) value = _control->outputMax;
    
    // Apply inversion if needed
    if (_control->outputInverted) {
        value = 100.0 - value;
    }
    
    // Write to digital output object
    DigitalOutput_t* output = (DigitalOutput_t*)objIndex[_control->outputIndex].obj;
    
    if (_control->controlMethod == 0) {
        // ON/OFF mode: Set state based on threshold
        output->pwmEnabled = false;
        output->state = (value > 0.0f);
    } else {
        // PID mode: Use PWM
        output->pwmEnabled = true;
        output->pwmDuty = value;
    }
    
    // Update control structure
    _control->currentOutput = value;
    
    return true;
}

void TemperatureController::_computePID() {
    unsigned long currentTime = millis();
    
    // Read current temperature
    float currentTemp = _readSensor();
    if (isnan(currentTemp)) {
        _writeOutput(0.0);  // Safety: turn off output on sensor fault
        return;
    }
    
    _control->currentTemp = currentTemp;
    
    // Calculate error
    float error = _control->setpoint - currentTemp;
    _control->processError = error;
    
    // Calculate time delta
    float dt = (currentTime - _lastUpdateTime) / 1000.0;  // Convert to seconds
    
    // Prevent huge dt on first run
    if (_lastUpdateTime == 0 || dt > 10.0) {
        dt = 0.1;  // Default to 100ms
    }
    
    float output;
    
    // Check control method
    if (_control->controlMethod == 0) {
        // ON/OFF control with hysteresis
        output = _calculateOnOffOutput(error);
    } else {
        // PID control
        output = _calculatePIDOutput(error, dt);
    }
    
    // Write output
    _writeOutput(output);
    
    // Update state
    _lastError = error;
    _lastUpdateTime = currentTime;
}

float TemperatureController::_calculateOnOffOutput(float error) {
    // ON/OFF control with hysteresis (deadband)
    // Error = Setpoint - CurrentTemp
    // Positive error = too cold, need heating
    // Negative error = too hot, stop heating
    
    float currentOutput = _control->currentOutput;
    float halfHysteresis = _control->hysteresis / 2.0;
    
    if (error > halfHysteresis) {
        // Too cold - turn ON
        return 100.0;
    } else if (error < -halfHysteresis) {
        // Too hot - turn OFF
        return 0.0;
    } else {
        // Within deadband - maintain current state
        return currentOutput;
    }
}

float TemperatureController::_calculatePIDOutput(float error, float dt) {
    // Proportional term
    float pTerm = _control->kp * error;
    
    // Integral term with anti-windup
    _integral += error * dt;
    
    // Anti-windup: clamp integral
    float maxIntegral = 50.0 / (_control->ki + 0.001);  // Prevent divide by zero
    if (_integral > maxIntegral) _integral = maxIntegral;
    if (_integral < -maxIntegral) _integral = -maxIntegral;
    
    float iTerm = _control->ki * _integral;
    
    // Derivative term (with filtering to reduce noise)
    float derivative = (error - _lastError) / dt;
    float dTerm = _control->kd * derivative;
    
    // Calculate total output
    float output = pTerm + iTerm + dTerm;
    
    // Clamp to output limits
    if (output < _control->outputMin) output = _control->outputMin;
    if (output > _control->outputMax) output = _control->outputMax;
    
    return output;
}

void TemperatureController::_updateAutotune() {
    float currentTemp = _readSensor();
    if (isnan(currentTemp)) {
        _setFault("Sensor fault during autotune");
        stopAutotune();
        return;
    }
    
    _control->currentTemp = currentTemp;
    unsigned long currentTime = millis();
    
    // Timeout check (30 minutes max)
    if (currentTime - _autotuneStartTime > 1800000) {
        _setFault("Auto-tune timeout");
        stopAutotune();
        return;
    }
    
    switch (_autotuneState) {
        case AUTOTUNE_RELAY_HIGH:
        case AUTOTUNE_RELAY_LOW:
        {
            // Detect setpoint crossings to switch relay
            bool crossedUp = (_autotuneLastTemp < _autotuneSetpoint && currentTemp >= _autotuneSetpoint);
            bool crossedDown = (_autotuneLastTemp > _autotuneSetpoint && currentTemp <= _autotuneSetpoint);
            
            if (crossedUp) {
                // Crossed setpoint going UP - switch to LOW output and start looking for PEAK
                _autotuneState = AUTOTUNE_RELAY_LOW;
                _writeOutput(_autotuneOutputLow);
                _autotuneLookingForPeak = true;   // Now looking for peak
                _autotuneExtreme = currentTemp;    // Reset peak tracker
                _autotuneJustCrossed = true;       // Mark that we just crossed
                Serial.printf("[TempCtrl] Autotune: Crossed UP at %.2f°C, switching to LOW output\n", currentTemp);
                
            } else if (crossedDown) {
                // Crossed setpoint going DOWN - switch to HIGH output and start looking for VALLEY
                _autotuneState = AUTOTUNE_RELAY_HIGH;
                _writeOutput(_autotuneOutputHigh);
                _autotuneLookingForPeak = false;   // Now looking for valley
                _autotuneExtreme = currentTemp;    // Reset valley tracker
                _autotuneJustCrossed = true;       // Mark that we just crossed
                Serial.printf("[TempCtrl] Autotune: Crossed DOWN at %.2f°C, switching to HIGH output\n", currentTemp);
            }
            
            // Track peak/valley detection (only after we've crossed setpoint at least once)
            if (!_autotuneJustCrossed && _autotunePeakCount < 10) {
                if (_autotuneLookingForPeak) {
                    // Looking for peak - track maximum temperature
                    if (currentTemp > _autotuneExtreme) {
                        // Still rising, update peak
                        _autotuneExtreme = currentTemp;
                    } else if ((currentTemp < _autotuneExtreme - 0.1) && (currentTemp > _autotuneSetpoint)) {
                        // Temperature dropped significantly - we found the peak!
                        _autotunePeaks[_autotunePeakCount] = _autotuneExtreme;
                        _autotunePeakTimes[_autotunePeakCount] = currentTime;
                        Serial.printf("[TempCtrl] Autotune: PEAK %d detected = %.2f°C\n", 
                                      _autotunePeakCount + 1, _autotuneExtreme);
                        _autotunePeakCount++;
                        _autotuneLookingForPeak = false;  // Stop looking until next crossing
                        
                        // Check if we have enough data
                        if (_autotunePeakCount >= 6) {
                            Serial.println("[TempCtrl] Autotune: Sufficient data collected, analyzing...");
                            _autotuneState = AUTOTUNE_ANALYZING;
                        }
                    }
                } else {
                    // Looking for valley - track minimum temperature
                    if (currentTemp < _autotuneExtreme) {
                        // Still falling, update valley
                        _autotuneExtreme = currentTemp;
                    } else if ((currentTemp > _autotuneExtreme + 0.1) && (currentTemp < _autotuneSetpoint)) {
                        // Temperature rose significantly - we found the valley!
                        _autotunePeaks[_autotunePeakCount] = _autotuneExtreme;
                        _autotunePeakTimes[_autotunePeakCount] = currentTime;
                        Serial.printf("[TempCtrl] Autotune: VALLEY %d detected = %.2f°C\n", 
                                      _autotunePeakCount + 1, _autotuneExtreme);
                        _autotunePeakCount++;
                        _autotuneLookingForPeak = true;  // Stop looking until next crossing
                        
                        // Check if we have enough data
                        if (_autotunePeakCount >= 6) {
                            Serial.println("[TempCtrl] Autotune: Sufficient data collected, analyzing...");
                            _autotuneState = AUTOTUNE_ANALYZING;
                        }
                    }
                }
            }
            
            // Clear the "just crossed" flag after first iteration
            if (_autotuneJustCrossed) {
                _autotuneJustCrossed = false;
            }
            
            break;
        }
            
        case AUTOTUNE_ANALYZING:
            _analyzeAutotuneResults();
            _autotuneState = AUTOTUNE_COMPLETE;
            _control->autotuning = false;
            _resetPIDState();
            
            // Auto-disable controller if we auto-enabled it
            if (_autotuneAutoEnabled) {
                _control->enabled = false;
                _autotuneAutoEnabled = false;
                _writeOutput(0);   // Ensure output is off
                Serial.println("[TempCtrl] Auto-disabled controller after autotune completion");
            }
            
            Serial.println("[TempCtrl] Auto-tune complete");
            sprintf(_control->message, "Autotune complete: Kp=%.2f Ki=%.2f Kd=%.2f",
                    _control->kp, _control->ki, _control->kd);
            break;
            
        case AUTOTUNE_OFF:
        case AUTOTUNE_COMPLETE:
        case AUTOTUNE_FAILED:
        default:
            break;
    }
    
    _autotuneLastTemp = currentTemp;
}

void TemperatureController::_analyzeAutotuneResults() {
    if (_autotunePeakCount < 6) {
        _setFault("Insufficient data for autotune");
        Serial.println("[TempCtrl] Autotune: Not enough peaks detected");
        return;
    }
    
    // Calculate average oscillation amplitude (peak-to-peak)
    float amplitudeSum = 0.0;
    int amplitudeCount = 0;
    
    for (int i = 0; i < _autotunePeakCount - 1; i++) {
        float amplitude = fabs(_autotunePeaks[i+1] - _autotunePeaks[i]);
        amplitudeSum += amplitude;
        amplitudeCount++;
    }
    
    float avgAmplitude = amplitudeSum / amplitudeCount;
    
    // Calculate average period (time between peaks)
    unsigned long periodSum = 0;
    int periodCount = 0;
    
    for (int i = 0; i < _autotunePeakCount - 2; i += 2) {
        unsigned long period = _autotunePeakTimes[i+2] - _autotunePeakTimes[i];
        periodSum += period;
        periodCount++;
    }
    
    float avgPeriod = (float)periodSum / (float)periodCount / 1000.0;  // Convert to seconds
    
    // Calculate ultimate gain (Ku) using relay method
    float outputStep = _autotuneOutputHigh - _autotuneOutputLow;
    float Ku = (4.0 * outputStep) / (PI * avgAmplitude);
    
    // Apply Ziegler-Nichols PID tuning rules
    _control->kp = 0.6 * Ku;
    _control->ki = 1.2 * Ku / avgPeriod;
    _control->kd = 0.075 * Ku * avgPeriod;
    
    Serial.printf("[TempCtrl] Autotune results:\n");
    Serial.printf("  Amplitude: %.2f°C\n", avgAmplitude);
    Serial.printf("  Period: %.2f s\n", avgPeriod);
    Serial.printf("  Ku: %.2f\n", Ku);
    Serial.printf("  PID Gains: Kp=%.2f Ki=%.4f Kd=%.2f\n", 
                  _control->kp, _control->ki, _control->kd);
}

void TemperatureController::_resetPIDState() {
    _integral = 0.0;
    _lastError = 0.0;
    _lastUpdateTime = 0;
}

bool TemperatureController::_validateIndices() {
    if (!_control) return false;
    
    if (_control->sensorIndex >= MAX_NUM_OBJECTS || 
        _control->outputIndex >= MAX_NUM_OBJECTS) {
        _setFault("Invalid object indices");
        return false;
    }
    
    if (!objIndex[_control->sensorIndex].valid || 
        !objIndex[_control->outputIndex].valid) {
        _setFault("Object indices not enrolled");
        return false;
    }
    
    if (objIndex[_control->sensorIndex].type != OBJ_T_TEMPERATURE_SENSOR) {
        _setFault("Sensor index is not a temperature sensor");
        return false;
    }
    
    // Only accept digital outputs (21-25) for temperature control
    // DAC outputs (8-9) are not suitable for heater control
    if (objIndex[_control->outputIndex].type != OBJ_T_DIGITAL_OUTPUT) {
        _setFault("Output must be digital output (21-25)");
        return false;
    }
    
    return true;
}

void TemperatureController::_setFault(const char* message) {
    if (!_control) return;
    
    _control->fault = true;
    _control->newMessage = true;
    strncpy(_control->message, message, sizeof(_control->message) - 1);
    _control->message[sizeof(_control->message) - 1] = '\0';
    
    Serial.printf("[TempCtrl] FAULT: %s\n", message);
}

void TemperatureController::_clearFault() {
    if (!_control) return;
    
    _control->fault = false;
    strcpy(_control->message, "OK");
}