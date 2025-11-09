#include "drv_modbus_alicat_mfc.h"

// Initialize static member
AlicatMFC* AlicatMFC::_currentInstance = nullptr;

// Constructor
AlicatMFC::AlicatMFC(ModbusDriver_t *modbusDriver, uint8_t slaveID)
    : _modbusDriver(modbusDriver), _slaveID(slaveID), _setpoint(0.0),
      _maxFlowRate_mL_min(1250.0),  // Default to Alicat max (1250 mL/min)
      _fault(false), _newMessage(false), _newSetpoint(false),
      _pendingSetpoint(0.0), _writeAttempts(0), 
      _setpointUnitCode(0), _flowUnitCode(0), _pressureUnitCode(0) {
    // Initialize flow sensor
    _flowSensor.flow = 0.0;
    _flowSensor.fault = false;
    _flowSensor.newMessage = false;
    strcpy(_flowSensor.unit, "--");  // Default unit, will be updated from device
    _flowSensor.message[0] = '\0';

    // Initialize pressure sensor
    _pressureSensor.pressure = 0.0;
    _pressureSensor.fault = false;
    _pressureSensor.newMessage = false;
    strcpy(_pressureSensor.unit, "--");  // Default unit, will be updated from device
    _pressureSensor.message[0] = '\0';
    
    // Initialize setpoint unit
    strcpy(_setpointUnit, "--");  // Default unit, will be updated from device
    
    // Initialize message
    _message[0] = '\0';
}

// Update method - queues Modbus request for MFC data
void AlicatMFC::update() {
    const uint8_t functionCode = 3;  // Read holding registers
    const uint16_t address = 1349;   // Starting address
    
    // Set current instance for callbacks
    _currentInstance = this;
    
    // Request MFC data (16 registers starting at 1349)
    if (!_modbusDriver->modbus.pushRequest(_slaveID, functionCode, address, _dataBuffer, 16, mfcResponseHandler)) {
        return;  // Queue full, try again next time
    }
    
    // Request setpoint unit (1649, 1 register - uint16)
    const uint16_t setpointUnitAddress = 1649;
    if (!_modbusDriver->modbus.pushRequest(_slaveID, functionCode, setpointUnitAddress, &_unitBuffer[0], 1, unitsResponseHandler)) {
        return;  // Queue full, try again next time
    }
    
    // Request pressure unit (1673, 1 register - uint16)
    const uint16_t pressureUnitAddress = 1673;
    if (!_modbusDriver->modbus.pushRequest(_slaveID, functionCode, pressureUnitAddress, &_unitBuffer[1], 1, unitsResponseHandler)) {
        return;  // Queue full, try again next time
    }
    
    // Request flow unit (1721, 1 register - uint16)
    const uint16_t flowUnitAddress = 1721;
    if (!_modbusDriver->modbus.pushRequest(_slaveID, functionCode, flowUnitAddress, &_unitBuffer[2], 1, unitsResponseHandler)) {
        return;  // Queue full, try again next time
    }
}

// Write setpoint method
bool AlicatMFC::writeSetpoint(float setpoint) {
    _pendingSetpoint = setpoint;
    _writeAttempts = 0;  // Reset attempts
    
    const uint8_t functionCode = 16;  // Write multiple registers
    const uint16_t address = 1349;    // Setpoint register address
    
    // Convert float to swapped uint16 format
    _modbusDriver->modbus.float32ToSwappedUint16(setpoint, _writeBuffer);
    
    // Set current instance for callbacks
    _currentInstance = this;
    
    // Queue the write request
    if (!_modbusDriver->modbus.pushRequest(_slaveID, functionCode, address, _writeBuffer, 2, mfcWriteResponseHandler)) {
        return false;  // Queue full
    }
    
    return true;
}

// Static callback for MFC data response
void AlicatMFC::mfcResponseHandler(bool valid, uint16_t *data) {
    if (_currentInstance) {
        _currentInstance->handleMfcResponse(valid, data);
    }
}

// Static callback for write response
void AlicatMFC::mfcWriteResponseHandler(bool valid, uint16_t *data) {
    if (_currentInstance) {
        _currentInstance->handleWriteResponse(valid, data);
    }
}

// Instance method to handle MFC data response
void AlicatMFC::handleMfcResponse(bool valid, uint16_t *data) {
    if (!valid) {
        _fault = true;
        snprintf(_message, sizeof(_message), "Invalid data from Alicat MFC (ID %d)", _slaveID);
        _newMessage = true;
        
        // Update control object with fault status
        _controlObj.connected = false;  // Modbus communication failed
        _controlObj.fault = true;
        _controlObj.newMessage = true;
        strncpy(_controlObj.message, _message, sizeof(_controlObj.message));
        
        return;
    }
    
    /************************************** 
     * Data from registers 1349-1364
     *  1349: Setpoint (float)
     *  1351: Valve Drive (float)
     *  1353: Pressure (float)
     *  1355: Secondary Pressure (float)
     *  1357: Barometric Pressure (float)
     *  1359: Temperature (float)
     *  1361: Volumetric Flow (float)
     *  1363: Mass Flow (float)
    **************************************/
    
    // Extract data using swapped uint16 to float conversion
    _setpoint = _modbusDriver->modbus.swappedUint16toFloat32(&data[0]);
    _pressureSensor.pressure = _modbusDriver->modbus.swappedUint16toFloat32(&data[4]);
    _flowSensor.flow = _modbusDriver->modbus.swappedUint16toFloat32(&data[12]);
    
    // Clear fault flags on successful read
    _flowSensor.fault = false;
    _pressureSensor.fault = false;
    
    // Update device control object
    _controlObj.setpoint = _setpoint;
    _controlObj.actualValue = _flowSensor.flow;  // Primary feedback is flow
    strncpy(_controlObj.setpointUnit, _setpointUnit, sizeof(_controlObj.setpointUnit));
    _controlObj.connected = true;  // Got valid Modbus response
    _controlObj.fault = _fault;
    _controlObj.newMessage = _newMessage;
    strncpy(_controlObj.message, _message, sizeof(_controlObj.message));
    _controlObj.slaveID = _slaveID;
    _controlObj.deviceType = IPC_DEV_ALICAT_MFC;
    
    // If we just wrote a setpoint, validate it
    if (_newSetpoint) {
        if (fabs(_setpoint - _pendingSetpoint) > 0.01) {
            _fault = true;
            snprintf(_message, sizeof(_message), 
                     "Setpoint write validation failed for MFC (ID %d): expected %0.4f, got %0.4f", 
                     _slaveID, _pendingSetpoint, _setpoint);
            _newMessage = true;
        } else {
            _fault = false;  // Clear fault flag on successful validation
            snprintf(_message, sizeof(_message), 
                     "Setpoint write successful for MFC (ID %d): setpoint is now %0.4f", 
                     _slaveID, _setpoint);
            _newMessage = true;
        }
        _newSetpoint = false;
    }
}

// Instance method to handle write response
void AlicatMFC::handleWriteResponse(bool valid, uint16_t *data) {
    if (!valid) {
        if (_writeAttempts < 5) {
            _writeAttempts++;
            // Retry the write
            writeSetpoint(_pendingSetpoint);
        } else {
            _writeAttempts = 0;
            _fault = true;
            snprintf(_message, sizeof(_message), 
                     "Failed to write setpoint %0.4f to Alicat MFC (ID %d) after 5 attempts", 
                     _pendingSetpoint, _slaveID);
            _newMessage = true;
        }
        return;
    }
    
    // Write was successful, flag for validation on next read
    _newSetpoint = true;
    _writeAttempts = 0;
}

// Static callback for units response
void AlicatMFC::unitsResponseHandler(bool valid, uint16_t *data) {
    if (_currentInstance) {
        _currentInstance->handleUnitsResponse(valid, data);
    }
}

// Instance method to handle units response
void AlicatMFC::handleUnitsResponse(bool valid, uint16_t *data) {
    if (!valid) {
        // Don't set fault for unit read failures, just skip update
        return;
    }
    
    // Buffer layout:
    // _unitBuffer[0]: Setpoint unit (uint16_t)
    // _unitBuffer[1]: Pressure unit (uint16_t)
    // _unitBuffer[2]: Flow unit (uint16_t)
    
    // Extract setpoint unit code (uint16_t from buffer[0])
    uint16_t setpointUnit = _unitBuffer[0];
    if (setpointUnit != _setpointUnitCode && setpointUnit < 64) {
        _setpointUnitCode = setpointUnit;
        const char* unitStr = getAlicatFlowUnit(_setpointUnitCode);
        strncpy(_setpointUnit, unitStr, sizeof(_setpointUnit) - 1);
        _setpointUnit[sizeof(_setpointUnit) - 1] = '\0';  // Ensure null termination
    }
    
    // Extract pressure unit code (uint16_t from buffer[1])
    uint16_t pressureUnit = _unitBuffer[1];
    if (pressureUnit != _pressureUnitCode && pressureUnit < 64) {
        _pressureUnitCode = pressureUnit;
        const char* unitStr = getAlicatPressureUnit(_pressureUnitCode);
        strncpy(_pressureSensor.unit, unitStr, sizeof(_pressureSensor.unit) - 1);
        _pressureSensor.unit[sizeof(_pressureSensor.unit) - 1] = '\0';  // Ensure null termination
    }
    
    // Extract flow unit code (uint16_t from buffer[2])
    uint16_t flowUnit = _unitBuffer[2];
    if (flowUnit != _flowUnitCode && flowUnit < 64) {
        _flowUnitCode = flowUnit;
        const char* unitStr = getAlicatFlowUnit(_flowUnitCode);
        strncpy(_flowSensor.unit, unitStr, sizeof(_flowSensor.unit) - 1);
        _flowSensor.unit[sizeof(_flowSensor.unit) - 1] = '\0';  // Ensure null termination
    }
}