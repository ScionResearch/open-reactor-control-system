#include "ctrl_flow.h"
#include "sys_init.h"

FlowController::FlowController(FlowControl_t* control)
    : _control(control),
      _doseStartTime(0),
      _dosing(false) {
    
    if (_control) {
        _control->fault = false;
        _control->currentOutput = 0;  // 0=off, 1=dosing
        _control->cumulativeVolume_mL = 0.0f;
        _control->lastDoseTime = 0;
        snprintf(_control->message, sizeof(_control->message), "Flow Controller initialized");
        _control->newMessage = true;
        
        // Calculate dosing parameters from initial configuration
        calculateDosingParameters();
    }
    
    Serial.printf("[FLOW CTRL %d] Flow controller created\n", _control->index);
}

FlowController::~FlowController() {
    // Stop any active dosing
    if (_dosing) {
        _stopOutput();
    }
    
    Serial.printf("[FLOW CTRL %d] Flow controller destroyed\n", _control->index);
}

void FlowController::update() {
    if (!_control) return;
    
    // Update dosing timeout (check if current dose has finished)
    _updateDosingTimeout();
    
    // Check if automatic dosing is needed (only if enabled and not currently dosing)
    if (_control->enabled && !_dosing && _canDose()) {
        _startDose();
    }
}

void FlowController::setFlowRate(float flowRate_mL_min) {
    if (_control) {
        _control->flowRate_mL_min = flowRate_mL_min;
        Serial.printf("[FLOW CTRL %d] Flow rate set to %.2f mL/min\n", 
                     _control->index, flowRate_mL_min);
        
        // Recalculate dosing parameters
        calculateDosingParameters();
    }
}

bool FlowController::manualDose() {
    if (!_control) {
        Serial.printf("[FLOW CTRL %d] Manual dose failed: invalid control\n", _control->index);
        return false;
    }
    
    if (!_canDose()) {
        Serial.printf("[FLOW CTRL %d] Manual dose failed: interval not met\n", _control->index);
        return false;
    }
    
    if (_dosing) {
        Serial.printf("[FLOW CTRL %d] Manual dose failed: already dosing\n", _control->index);
        return false;
    }
    
    Serial.printf("[FLOW CTRL %d] Manual dose triggered\n", _control->index);
    return _startDose();
}

void FlowController::resetVolume() {
    if (_control) {
        _control->cumulativeVolume_mL = 0.0f;
        Serial.printf("[FLOW CTRL %d] Cumulative volume reset to 0.0 mL\n", _control->index);
    }
}

void FlowController::calculateDosingParameters() {
    if (!_control) return;
    
    // Calculate volume per dose (from calibration)
    float volumePerDose = _control->calibrationVolume_mL;
    
    // Validate calibration data
    if (volumePerDose <= 0.0f) {
        _control->fault = true;
        snprintf(_control->message, sizeof(_control->message), 
                "Invalid calibration: volume must be > 0");
        _control->newMessage = true;
        _control->calculatedInterval_ms = 0;
        Serial.printf("[FLOW CTRL %d] ERROR: Invalid calibration volume (%.3f mL)\n", 
                     _control->index, volumePerDose);
        return;
    }
    
    // Calculate how many doses per minute are needed for target flow rate
    // Flow rate in mL/min, volume per dose in mL
    // Doses per minute = flow rate / volume per dose
    float dosesPerMinute = _control->flowRate_mL_min / volumePerDose;
    
    // Calculate interval between doses in milliseconds
    // Interval = 60000 ms / doses per minute
    if (dosesPerMinute > 0.0f) {
        _control->calculatedInterval_ms = (uint32_t)(60000.0f / dosesPerMinute);
    } else {
        _control->calculatedInterval_ms = 0;  // Disabled (flow rate = 0)
        _control->fault = false;
        Serial.printf("[FLOW CTRL %d] Flow rate = 0, dosing disabled\n", _control->index);
        return;
    }
    
    // Use calibration dose time for actual dosing
    _control->calculatedDoseTime_ms = _control->calibrationDoseTime_ms;
    
    // Check if interval is less than dose time (physically impossible - can't start new dose before previous finishes)
    if (_control->calculatedInterval_ms < _control->calculatedDoseTime_ms) {
        // Calculate maximum achievable flow rate
        // Max doses per minute = 60000 ms / dose time
        // Max flow rate = max doses per minute * volume per dose
        float maxDosesPerMin = 60000.0f / (float)_control->calculatedDoseTime_ms;
        float maxFlowRate = maxDosesPerMin * volumePerDose;
        
        Serial.printf("[FLOW CTRL %d] WARNING: Setpoint %.2f mL/min exceeds pump capacity (max: %.2f mL/min)\n",
                     _control->index, _control->flowRate_mL_min, maxFlowRate);
        Serial.printf("[FLOW CTRL %d] Interval (%lu ms) < dose time (%u ms) - capping to max flow rate\n",
                     _control->index, _control->calculatedInterval_ms, _control->calculatedDoseTime_ms);
        
        // Cap setpoint to maximum achievable flow rate
        _control->flowRate_mL_min = maxFlowRate;
        
        // Recalculate interval at max flow rate (should equal dose time)
        _control->calculatedInterval_ms = _control->calculatedDoseTime_ms;
        
        _control->fault = true;
        snprintf(_control->message, sizeof(_control->message), 
                "Setpoint capped at max: %.1f mL/min", maxFlowRate);
        _control->newMessage = true;
    } else {
        // Apply other safety limits
        bool limited = false;
        if (_control->calculatedInterval_ms < _control->minDosingInterval_ms) {
            _control->calculatedInterval_ms = _control->minDosingInterval_ms;
            limited = true;
        }
        
        if (_control->calculatedDoseTime_ms > _control->maxDosingTime_ms) {
            _control->calculatedDoseTime_ms = _control->maxDosingTime_ms;
            limited = true;
        }
        
        // Check if safety limit affected operation
        if (limited) {
            _control->fault = true;
            snprintf(_control->message, sizeof(_control->message), 
                    "Flow rate limited by safety constraints");
            _control->newMessage = true;
            Serial.printf("[FLOW CTRL %d] WARNING: Flow rate limited by safety (interval: %lu ms, dose: %u ms)\n",
                         _control->index, _control->calculatedInterval_ms, _control->calculatedDoseTime_ms);
        } else {
            _control->fault = false;
        }
    }
    
    Serial.printf("[FLOW CTRL %d] Calculated: %.2f mL/min → dose every %lu ms (%.3f mL/dose)\n",
                 _control->index, 
                 _control->flowRate_mL_min,
                 _control->calculatedInterval_ms,
                 volumePerDose);
}

bool FlowController::_canDose() {
    if (!_control) return false;
    
    // Check if interval has elapsed since last dose
    if (_control->calculatedInterval_ms == 0) return false;
    
    uint32_t now = millis();
    uint32_t timeSinceLastDose = now - _control->lastDoseTime;
    
    return timeSinceLastDose >= _control->calculatedInterval_ms;
}

bool FlowController::_startDose() {
    if (!_control) return false;
    
    // Start the dose
    if (!_activateOutput()) {
        return false;
    }
    
    _dosing = true;
    _doseStartTime = millis();
    _control->lastDoseTime = millis();
    _control->currentOutput = 1;  // Dosing
    
    // Accumulate volume
    _control->cumulativeVolume_mL += _control->calibrationVolume_mL;
    
    Serial.printf("[FLOW CTRL %d] Dose started: +%.3f mL (total: %.2f mL)\n",
                 _control->index,
                 _control->calibrationVolume_mL,
                 _control->cumulativeVolume_mL);
    
    snprintf(_control->message, sizeof(_control->message), 
            "Dosing for %u ms", _control->calculatedDoseTime_ms);
    _control->newMessage = true;
    
    return true;
}

bool FlowController::_activateOutput() {
    if (!_control) return false;
    
    bool success = false;
    uint8_t type = _control->outputType;
    uint8_t index = _control->outputIndex;
    uint8_t power = _control->motorPower;
    
    if (type == 0) {
        // Digital output
        if (index >= 21 && index <= 25 && objIndex[index].valid) {
            DigitalOutput_t* output = (DigitalOutput_t*)objIndex[index].obj;
            if (output) {
                output->state = true;
                success = true;
                Serial.printf("[FLOW CTRL %d] Activated digital output %d for %u ms\n",
                             _control->index, index, _control->calculatedDoseTime_ms);
            }
        }
    } else if (type == 1) {
        // DC motor
        if (index >= 27 && index <= 30) {
            uint8_t motorNum = index - 27;
            success = motor_run(motorNum, power, true);  // Forward direction
            if (success) {
                Serial.printf("[FLOW CTRL %d] Activated DC motor %d at %d%% for %u ms\n",
                             _control->index, index, power, _control->calculatedDoseTime_ms);
            }
        }
    }
    
    if (!success) {
        _control->fault = true;
        snprintf(_control->message, sizeof(_control->message), "Failed to activate output");
        _control->newMessage = true;
        Serial.printf("[FLOW CTRL %d] ERROR: Failed to activate output (type=%d, index=%d)\n",
                     _control->index, type, index);
    }
    
    return success;
}

void FlowController::_stopOutput() {
    if (!_control) return;
    
    uint8_t type = _control->outputType;
    uint8_t index = _control->outputIndex;
    
    if (type == 0) {
        // Digital output
        if (index >= 21 && index <= 25 && objIndex[index].valid) {
            DigitalOutput_t* output = (DigitalOutput_t*)objIndex[index].obj;
            if (output) {
                output->state = false;
                Serial.printf("[FLOW CTRL %d] Stopped digital output %d\n", _control->index, index);
            }
        }
    } else if (type == 1) {
        // DC motor
        if (index >= 27 && index <= 30) {
            uint8_t motorNum = index - 27;
            motor_stop(motorNum);
            Serial.printf("[FLOW CTRL %d] Stopped DC motor %d\n", _control->index, index);
        }
    }
    
    _dosing = false;
    _control->currentOutput = 0;
}

void FlowController::_updateDosingTimeout() {
    if (!_dosing) return;
    
    uint32_t elapsed = millis() - _doseStartTime;
    
    if (elapsed >= _control->calculatedDoseTime_ms) {
        // Dose complete
        Serial.printf("[FLOW CTRL %d] Dose complete (%.1f sec)\n", 
                     _control->index, _control->calculatedDoseTime_ms / 1000.0f);
        
        _stopOutput();
        
        snprintf(_control->message, sizeof(_control->message), "Dose complete");
        _control->newMessage = true;
    }
}
