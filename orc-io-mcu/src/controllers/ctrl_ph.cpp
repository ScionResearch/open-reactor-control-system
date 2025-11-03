#include "ctrl_ph.h"
#include "sys_init.h"

pHController::pHController(pHControl_t* control) 
    : _control(control),
      _doseStartTime(0),
      _dosing(false),
      _dosingAcid(false) {
    
    if (_control) {
        _control->fault = false;
        _control->currentOutput = 0;  // 0=off, 1=dosing acid, 2=dosing alkaline
        _control->currentpH = 0.0f;
        _control->lastAcidDoseTime = 0;
        _control->lastAlkalineDoseTime = 0;
        _control->acidCumulativeVolume_mL = 0.0f;
        _control->alkalineCumulativeVolume_mL = 0.0f;
        snprintf(_control->message, sizeof(_control->message), "pH Controller initialized");
        _control->newMessage = true;
    }
    
    Serial.println("[pH CTRL] pH controller created");
}

pHController::~pHController() {
    // Stop any active dosing
    if (_dosing) {
        if (_dosingAcid && _control->acidEnabled) {
            _stopOutput(_control->acidOutputType, _control->acidOutputIndex);
        } else if (!_dosingAcid && _control->alkalineEnabled) {
            _stopOutput(_control->alkalineOutputType, _control->alkalineOutputIndex);
        }
    }
    
    Serial.println("[pH CTRL] pH controller destroyed");
}

void pHController::update() {
    if (!_control) return;
    
    // Read pH sensor
    float pH = _readpH();
    if (isnan(pH)) {
        _control->fault = true;
        snprintf(_control->message, sizeof(_control->message), "pH sensor read error");
        _control->newMessage = true;
        return;
    }
    
    _control->currentpH = pH;
    _control->fault = false;
    
    // Update dosing timeout
    _updateDosingTimeout();
    
    // Check if automatic dosing is needed (only if enabled and not currently dosing)
    if (_control->enabled && !_dosing) {
        _checkDosing();
    }
}

void pHController::setSetpoint(float pH) {
    if (_control) {
        _control->setpoint = pH;
        Serial.printf("[pH CTRL] Setpoint updated to %.2f\n", pH);
    }
}

bool pHController::doseAcid() {
    if (!_control || !_control->acidEnabled) {
        Serial.println("[pH CTRL] Acid dosing not configured");
        return false;
    }
    
    if (!_canDoseAcid()) {
        Serial.println("[pH CTRL] Acid dosing interval not met");
        return false;
    }
    
    if (_dosing) {
        Serial.println("[pH CTRL] Already dosing");
        return false;
    }
    
    Serial.println("[pH CTRL] Manual acid dose");
    
    return _activateOutput(
        _control->acidOutputType,
        _control->acidOutputIndex,
        _control->acidMotorPower,
        _control->acidDosingTime_ms
    );
}

bool pHController::doseAlkaline() {
    if (!_control || !_control->alkalineEnabled) {
        Serial.println("[pH CTRL] Alkaline dosing not configured");
        return false;
    }
    
    if (!_canDoseAlkaline()) {
        Serial.println("[pH CTRL] Alkaline dosing interval not met");
        return false;
    }
    
    if (_dosing) {
        Serial.println("[pH CTRL] Already dosing");
        return false;
    }
    
    Serial.println("[pH CTRL] Manual alkaline dose");
    
    return _activateOutput(
        _control->alkalineOutputType,
        _control->alkalineOutputIndex,
        _control->alkalineMotorPower,
        _control->alkalineDosingTime_ms
    );
}

float pHController::_readpH() {
    if (!_control || _control->sensorIndex >= MAX_NUM_OBJECTS) {
        return NAN;
    }
    
    // Get pH sensor from object index
    if (!objIndex[_control->sensorIndex].valid) {
        return NAN;
    }
    
    // Check object type
    if (objIndex[_control->sensorIndex].type != OBJ_T_PH_SENSOR) {
        return NAN;
    }
    
    // Read pH value from sensor
    PhSensor_t* sensor = (PhSensor_t*)objIndex[_control->sensorIndex].obj;
    if (!sensor) {
        return NAN;
    }
    
    return sensor->ph;
}

void pHController::_checkDosing() {
    float pH = _control->currentpH;
    float setpoint = _control->setpoint;
    float deadband = _control->deadband;
    
    // Check if pH is too high (need acid)
    if (pH > (setpoint + deadband)) {
        if (_control->acidEnabled && _canDoseAcid()) {
            Serial.printf("[pH CTRL] pH too high (%.2f > %.2f), dosing acid\n", 
                         pH, setpoint + deadband);
            
            _activateOutput(
                _control->acidOutputType,
                _control->acidOutputIndex,
                _control->acidMotorPower,
                _control->acidDosingTime_ms
            );
        }
    }
    // Check if pH is too low (need alkaline)
    else if (pH < (setpoint - deadband)) {
        if (_control->alkalineEnabled && _canDoseAlkaline()) {
            Serial.printf("[pH CTRL] pH too low (%.2f < %.2f), dosing alkaline\n", 
                         pH, setpoint - deadband);
            
            _activateOutput(
                _control->alkalineOutputType,
                _control->alkalineOutputIndex,
                _control->alkalineMotorPower,
                _control->alkalineDosingTime_ms
            );
        }
    }
}

bool pHController::_canDoseAcid() {
    if (_control->acidDosingInterval_ms == 0) return true;
    
    uint32_t elapsed = millis() - _control->lastAcidDoseTime;
    return elapsed >= _control->acidDosingInterval_ms;
}

bool pHController::_canDoseAlkaline() {
    if (_control->alkalineDosingInterval_ms == 0) return true;
    
    uint32_t elapsed = millis() - _control->lastAlkalineDoseTime;
    return elapsed >= _control->alkalineDosingInterval_ms;
}

bool pHController::_activateOutput(uint8_t type, uint8_t index, uint8_t power, uint16_t duration) {
    bool success = false;
    
    if (type == 0) {
        // Digital output
        if (index >= 21 && index <= 25 && objIndex[index].valid) {
            DigitalOutput_t* output = (DigitalOutput_t*)objIndex[index].obj;
            if (output) {
                output->state = true;
                success = true;
                Serial.printf("[pH CTRL] Activated digital output %d for %d ms\n", index, duration);
            }
        }
    } else if (type == 1) {
        // DC motor
        if (index >= 27 && index <= 30) {
            uint8_t motorNum = index - 27;
            success = motor_run(motorNum, power, true);  // Forward direction
            if (success) {
                Serial.printf("[pH CTRL] Activated DC motor %d at %d%% for %d ms\n", 
                             index, power, duration);
            }
        }
    }
    
    if (success) {
        _dosing = true;
        _dosingAcid = (type == _control->acidOutputType && index == _control->acidOutputIndex);
        _doseStartTime = millis();
        
        // Update control state
        _control->currentOutput = _dosingAcid ? 1.0f : 2.0f;
        
        // Update last dose time and accumulate volume
        if (_dosingAcid) {
            _control->lastAcidDoseTime = millis();
            _control->acidCumulativeVolume_mL += _control->acidVolumePerDose_mL;
            Serial.printf("[pH CTRL] Acid dose: +%.2f mL (total: %.2f mL)\n", 
                         _control->acidVolumePerDose_mL, 
                         _control->acidCumulativeVolume_mL);
        } else {
            _control->lastAlkalineDoseTime = millis();
            _control->alkalineCumulativeVolume_mL += _control->alkalineVolumePerDose_mL;
            Serial.printf("[pH CTRL] Alkaline dose: +%.2f mL (total: %.2f mL)\n", 
                         _control->alkalineVolumePerDose_mL, 
                         _control->alkalineCumulativeVolume_mL);
        }
        
        snprintf(_control->message, sizeof(_control->message), 
                "Dosing %s for %d ms", _dosingAcid ? "acid" : "alkaline", duration);
        _control->newMessage = true;
    }
    
    return success;
}

void pHController::_stopOutput(uint8_t type, uint8_t index) {
    if (type == 0) {
        // Digital output
        if (index >= 21 && index <= 25 && objIndex[index].valid) {
            DigitalOutput_t* output = (DigitalOutput_t*)objIndex[index].obj;
            if (output) {
                output->state = false;
                Serial.printf("[pH CTRL] Stopped digital output %d\n", index);
            }
        }
    } else if (type == 1) {
        // DC motor
        if (index >= 27 && index <= 30) {
            uint8_t motorNum = index - 27;
            motor_stop(motorNum);
            Serial.printf("[pH CTRL] Stopped DC motor %d\n", index);
        }
    }
    
    _dosing = false;
    _control->currentOutput = 0;
}

void pHController::_updateDosingTimeout() {
    if (!_dosing) return;
    
    uint16_t duration = _dosingAcid ? _control->acidDosingTime_ms : _control->alkalineDosingTime_ms;
    uint32_t elapsed = millis() - _doseStartTime;
    
    if (elapsed >= duration) {
        // Dose complete
        Serial.printf("[pH CTRL] Dose complete (%.1f sec)\n", duration / 1000.0f);
        
        if (_dosingAcid && _control->acidEnabled) {
            _stopOutput(_control->acidOutputType, _control->acidOutputIndex);
        } else if (!_dosingAcid && _control->alkalineEnabled) {
            _stopOutput(_control->alkalineOutputType, _control->alkalineOutputIndex);
        }
        
        snprintf(_control->message, sizeof(_control->message), "Dose complete");
        _control->newMessage = true;
    }
}

void pHController::resetAcidVolume() {
    if (_control) {
        _control->acidCumulativeVolume_mL = 0.0f;
        Serial.println("[pH CTRL] Acid cumulative volume reset to 0.0 mL");
    }
}

void pHController::resetAlkalineVolume() {
    if (_control) {
        _control->alkalineCumulativeVolume_mL = 0.0f;
        Serial.println("[pH CTRL] Alkaline cumulative volume reset to 0.0 mL");
    }
}
