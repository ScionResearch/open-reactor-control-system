#include "ctrl_do.h"
#include "sys_init.h"

DOController::DOController(DissolvedOxygenControl_t* control)
    : _control(control),
      _lastUpdateTime(0) {
    
    if (_control) {
        _control->fault = false;
        _control->currentDO_mg_L = 0.0f;
        _control->error_mg_L = 0.0f;
        _control->currentStirrerOutput = 0.0f;
        _control->currentMFCOutput = 0.0f;
        snprintf(_control->message, sizeof(_control->message), "DO Controller initialized");
        _control->newMessage = true;
        
        // Sort profile points if any exist
        if (_control->numPoints > 0) {
            _sortProfile();
        }
    }
    
    Serial.printf("[DO CTRL %d] DO controller created\n", _control->index);
}

DOController::~DOController() {
    // Stop all outputs
    _stopAllOutputs();
    
    Serial.printf("[DO CTRL %d] DO controller destroyed\n", _control->index);
}

void DOController::_stopAllOutputs() {
    if (!_control) return;
    
    // Stop stirrer output
    if (_control->stirrerEnabled) {
        _applyStirrerOutput(0.0f);
    }
    
    // Stop MFC output
    if (_control->mfcEnabled) {
        _applyMFCOutput(0.0f);
    }
    
    // Reset output tracking
    _control->currentStirrerOutput = 0.0f;
    _control->currentMFCOutput = 0.0f;
}

void DOController::update() {
    if (!_control) return;
    
    // Run at 1 Hz
    uint32_t now = millis();
    if (now - _lastUpdateTime < 1000) {
        return;  // Not time yet
    }
    _lastUpdateTime = now;
    
    // Only update if enabled
    if (!_control->enabled) {
        // Set outputs to zero when disabled
        _stopAllOutputs();
        return;
    }
    
    // Validate indices (sensor and outputs)
    if (!_validateIndices()) {
        // Validation failed - disable controller for safety
        if (_control->enabled) {
            _control->enabled = false;
            _stopAllOutputs();
            Serial.printf("[DO CTRL %d] Disabled controller due to invalid indices\n", _control->index);
        }
        return;
    }
    
    // Read DO sensor
    float currentDO = _readDOSensor();
    if (isnan(currentDO)) {
        // Sensor read failed or sensor is faulty - stop outputs
        _stopAllOutputs();
        return;  // Fault already set by _readDOSensor()
    }
    
    _control->currentDO_mg_L = currentDO;
    _control->error_mg_L = _control->setpoint_mg_L - currentDO;
    _clearFault();  // Clear fault if we got a valid reading
    
    // Calculate and apply outputs
    _calculateOutputs();
}

void DOController::setSetpoint(float setpoint_mg_L) {
    if (_control) {
        _control->setpoint_mg_L = setpoint_mg_L;
        Serial.printf("[DO CTRL %d] Setpoint set to %.2f mg/L\n", 
                     _control->index, setpoint_mg_L);
    }
}

void DOController::setProfile(uint8_t numPoints, const DOProfilePoint* points) {
    if (!_control || numPoints > 20) return;
    
    _control->numPoints = numPoints;
    memcpy(_control->profile, points, numPoints * sizeof(DOProfilePoint));
    
    // Sort profile by error value
    _sortProfile();
    
    Serial.printf("[DO CTRL %d] Profile updated (%d points)\n", 
                 _control->index, numPoints);
}

float DOController::_readDOSensor() {
    // Scan object index for DO sensor (indices 70-99)
    for (int i = 70; i < 100; i++) {
        if (objIndex[i].valid && objIndex[i].type == OBJ_T_DISSOLVED_OXYGEN_SENSOR) {
            DissolvedOxygenSensor_t* sensor = (DissolvedOxygenSensor_t*)objIndex[i].obj;
            if (!sensor) {
                _setFault("DO sensor object is null");
                return NAN;
            }
            
            // Check for sensor fault
            if (sensor->fault) {
                _setFault("WARNING: DO sensor fault detected");
                if (_control->enabled) {
                    _control->enabled = false;
                    _stopAllOutputs();
                    Serial.printf("[DO CTRL %d] Disabling controller due to sensor fault\n", _control->index);
                }
                return NAN;
            }
            
            return sensor->dissolvedOxygen;
        }
    }
    
    // No DO sensor found
    _setFault("DO sensor not found or not enrolled");
    return NAN;
}

void DOController::_calculateOutputs() {
    if (!_control) return;
    
    // Check if we have a valid profile
    if (_control->numPoints == 0) {
        _control->fault = true;
        snprintf(_control->message, sizeof(_control->message), "No profile configured");
        _control->newMessage = true;
        return;
    }
    
    float error = _control->error_mg_L;
    
    // Interpolate outputs from profile
    if (_control->stirrerEnabled) {
        float stirrerOutput = _interpolateProfile(error, true);
        _control->currentStirrerOutput = stirrerOutput;
        _applyStirrerOutput(stirrerOutput);
    }
    
    if (_control->mfcEnabled) {
        float mfcOutput = _interpolateProfile(error, false);
        _control->currentMFCOutput = mfcOutput;
        _applyMFCOutput(mfcOutput);
    }
    
    _control->fault = false;
}

void DOController::_applyStirrerOutput(float output) {
    if (!_control || !_control->stirrerEnabled) return;
    
    uint8_t stirrerIndex = _control->stirrerIndex;
    
    // Validate stirrer output before applying
    if (output > 0.0f && !_validateStirrerOutput()) {
        _setFault("Stirrer output is invalid or offline");
        if (_control->enabled) {
            _control->enabled = false;
            _stopAllOutputs();
        }
        return;
    }
    
    if (_control->stirrerType == 0) {
        // DC Motor (indices 27-30)
        if (stirrerIndex >= 27 && stirrerIndex <= 30) {
            uint8_t motorNum = stirrerIndex - 27;
            uint8_t power = constrain((uint8_t)output, 0, 100);
            
            if (power > 0) {
                motor_run(motorNum, power, true);  // Forward direction
            } else {
                motor_stop(motorNum);
            }
        }
    } else if (_control->stirrerType == 1) {
        // Stepper Motor (index 26)
        if (stirrerIndex == 26 && objIndex[26].valid) {
            StepperDevice_t* stepper = (StepperDevice_t*)objIndex[26].obj;
            if (stepper) {
                // Profile output is RPM value directly, clamp to stepper's configured maxRPM
                float rpm = constrain(output, 0.0f, stepper->maxRPM);
                bool enabledState = (rpm > 0);
                
                // Only update driver if RPM or enabled state has changed
                bool needsUpdate = (fabs(stepper->rpm - rpm) > 0.1f) || (stepper->enabled != enabledState);
                
                stepper->rpm = rpm;
                stepper->direction = true;  // Forward direction
                stepper->enabled = enabledState;
                
                if (needsUpdate) {
                    stepper_update_cfg(true);  // Apply changes
                }
            }
        }
    }
}

void DOController::_applyMFCOutput(float output_mL_min) {
    if (!_control || !_control->mfcEnabled) return;
    
    uint8_t devIdx = _control->mfcDeviceIndex;
    
    // Validate MFC output before applying
    if (output_mL_min > 0.0f && !_validateMFCOutput()) {
        _setFault("MFC output is invalid or offline");
        if (_control->enabled) {
            _control->enabled = false;
            _stopAllOutputs();
        }
        return;
    }
    
    // Find MFC device in device index (50-69)
    if (devIdx >= 50 && devIdx < 70 && objIndex[devIdx].valid) {
        if (objIndex[devIdx].type == OBJ_T_DEVICE_CONTROL) {
            DeviceControl_t* dev = (DeviceControl_t*)objIndex[devIdx].obj;
            if (dev && dev->deviceType == IPC_DEV_ALICAT_MFC) {
                // Get AlicatMFC instance from device manager
                ManagedDevice* managedDev = DeviceManager::findDeviceByControlIndex(devIdx);
                if (managedDev && managedDev->deviceInstance) {
                    AlicatMFC* mfc = (AlicatMFC*)managedDev->deviceInstance;
                    
                    // Convert from mL/min to device units if necessary
                    float setpointInDeviceUnits = mfc->convertFromMLmin(output_mL_min);
                    
                    // Update control object setpoint (in device units for correct display)
                    dev->setpoint = setpointInDeviceUnits;
                    
                    // Write setpoint to hardware, in mL/min (forces conversion if necessary)
                    mfc->writeSetpoint(output_mL_min, true);
                }
            }
        }
    }
}

float DOController::_interpolateProfile(float error, bool forStirrer) {
    if (!_control || _control->numPoints == 0) return 0.0f;
    
    // Handle single point (constant output)
    if (_control->numPoints == 1) {
        return forStirrer ? _control->profile[0].stirrerOutput 
                          : _control->profile[0].mfcOutput_mL_min;
    }
    
    // Clamp to first point if error is below range
    if (error <= _control->profile[0].error_mg_L) {
        return forStirrer ? _control->profile[0].stirrerOutput 
                          : _control->profile[0].mfcOutput_mL_min;
    }
    
    // Clamp to last point if error is above range
    int lastIdx = _control->numPoints - 1;
    if (error >= _control->profile[lastIdx].error_mg_L) {
        return forStirrer ? _control->profile[lastIdx].stirrerOutput 
                          : _control->profile[lastIdx].mfcOutput_mL_min;
    }
    
    // Find bracketing points and interpolate
    for (int i = 0; i < _control->numPoints - 1; i++) {
        float x1 = _control->profile[i].error_mg_L;
        float x2 = _control->profile[i + 1].error_mg_L;
        
        if (error >= x1 && error <= x2) {
            float y1 = forStirrer ? _control->profile[i].stirrerOutput 
                                  : _control->profile[i].mfcOutput_mL_min;
            float y2 = forStirrer ? _control->profile[i + 1].stirrerOutput 
                                  : _control->profile[i + 1].mfcOutput_mL_min;
            
            // Linear interpolation: y = y1 + (x - x1) * (y2 - y1) / (x2 - x1)
            float slope = (y2 - y1) / (x2 - x1);
            return y1 + (error - x1) * slope;
        }
    }
    
    // Should never reach here if profile is sorted correctly
    return 0.0f;
}

void DOController::_sortProfile() {
    if (!_control || _control->numPoints <= 1) return;
    
    // Simple bubble sort (sufficient for small arrays up to 20 elements)
    for (int i = 0; i < _control->numPoints - 1; i++) {
        for (int j = 0; j < _control->numPoints - i - 1; j++) {
            if (_control->profile[j].error_mg_L > _control->profile[j + 1].error_mg_L) {
                // Swap
                DOProfilePoint temp = _control->profile[j];
                _control->profile[j] = _control->profile[j + 1];
                _control->profile[j + 1] = temp;
            }
        }
    }
}

bool DOController::_validateIndices() {
    if (!_control) return false;
    
    // Validate DO sensor exists (scans indices 70-99)
    bool sensorFound = false;
    for (int i = 70; i < 100; i++) {
        if (objIndex[i].valid && objIndex[i].type == OBJ_T_DISSOLVED_OXYGEN_SENSOR) {
            sensorFound = true;
            break;
        }
    }
    
    if (!sensorFound) {
        _setFault("DO sensor not enrolled");
        return false;
    }
    
    // Validate stirrer output if enabled
    if (_control->stirrerEnabled) {
        if (!_validateStirrerOutput()) {
            _setFault("Stirrer output is invalid or offline");
            return false;
        }
    }
    
    // Validate MFC output if enabled
    if (_control->mfcEnabled) {
        if (!_validateMFCOutput()) {
            _setFault("MFC output is invalid or offline");
            return false;
        }
    }
    
    return true;
}

bool DOController::_validateStirrerOutput() {
    if (!_control) return false;
    
    uint8_t stirrerIndex = _control->stirrerIndex;
    
    if (_control->stirrerType == 0) {
        // DC Motor (indices 27-30)
        if (stirrerIndex < 27 || stirrerIndex > 30) {
            return false;
        }
        if (stirrerIndex >= MAX_NUM_OBJECTS || !objIndex[stirrerIndex].valid) {
            return false;
        }
        if (objIndex[stirrerIndex].type != OBJ_T_BDC_MOTOR) {
            return false;
        }
    } 
    else if (_control->stirrerType == 1) {
        // Stepper Motor (index 26)
        if (stirrerIndex != 26) {
            return false;
        }
        if (!objIndex[26].valid) {
            return false;
        }
        if (objIndex[26].type != OBJ_T_STEPPER_MOTOR) {
            return false;
        }
    }
    else {
        // Unknown stirrer type
        return false;
    }
    
    return true;
}

bool DOController::_validateMFCOutput() {
    if (!_control) return false;
    
    uint8_t devIdx = _control->mfcDeviceIndex;
    
    // MFC device indices (50-69)
    if (devIdx < 50 || devIdx >= 70) {
        return false;
    }
    
    if (devIdx >= MAX_NUM_OBJECTS || !objIndex[devIdx].valid) {
        return false;
    }
    
    if (objIndex[devIdx].type != OBJ_T_DEVICE_CONTROL) {
        return false;
    }
    
    // Check if MFC is connected
    DeviceControl_t* dev = (DeviceControl_t*)objIndex[devIdx].obj;
    if (!dev || !dev->connected) {
        Serial.printf("[DO CTRL %d] MFC %d is offline\n", _control->index, devIdx);
        return false;
    }
    
    // Verify it's actually an MFC device
    if (dev->deviceType != IPC_DEV_ALICAT_MFC) {
        return false;
    }
    
    return true;
}

void DOController::_setFault(const char* message) {
    if (!_control) return;
    
    _control->fault = true;
    _control->newMessage = true;
    strncpy(_control->message, message, sizeof(_control->message) - 1);
    _control->message[sizeof(_control->message) - 1] = '\0';
    
    Serial.printf("[DO CTRL %d] FAULT: %s\n", _control->index, message);
}

void DOController::_clearFault() {
    if (!_control) return;
    
    _control->fault = false;
    strcpy(_control->message, "OK");
}
