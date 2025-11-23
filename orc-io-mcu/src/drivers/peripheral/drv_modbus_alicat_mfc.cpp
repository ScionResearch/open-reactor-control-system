#include "drv_modbus_alicat_mfc.h"

// Initialize static instance registry
AlicatMFC* AlicatMFC::_instances[248] = {nullptr};

// Constructor
AlicatMFC::AlicatMFC(ModbusDriver_t *modbusDriver, uint8_t slaveID)
    : _modbusDriver(modbusDriver), _slaveID(slaveID), _setpoint(0.0),
      _maxFlowRate_mL_min(1250.0),  // Default to Alicat max (1250 mL/min)
      _fault(false), _newMessage(false), _newSetpoint(false),
      _pendingSetpoint(0.0), _writeAttempts(0), 
      _setpointUnitCode(0), _flowUnitCode(0), _pressureUnitCode(0),
      _firstConnect(true) {  // Initialize first connect flag for this instance
    // Initialize flow sensor
    _flowSensor.flow = NAN;  // Initialize as NaN until first valid reading
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

    // Initialize control object
    _controlObj.slaveID = _slaveID;
    _controlObj.deviceType = IPC_DEV_ALICAT_MFC;
    _controlObj.connected = false;
    _controlObj.fault = false;
    _controlObj.newMessage = false;
    _controlObj.setpoint = 0.0;
    _controlObj.actualValue = 0.0;
    _controlObj.setpointUnit[0] = '\0';
    _controlObj.message[0] = '\0';

    // Local vars to keep track of connection state
    _firstConnect = true;   // Mark as first connection
    _err = false;
    _errCount = 0;          // Initialize error count
    _waitCount = 0;         // Initialize wait count
    _maxErrors = 5;         // Maximum consecutive errors before marking as disconnected(fault)
    
    // Register this instance in the static registry for callback routing
    if (_slaveID > 0 && _slaveID < 248) {
        _instances[_slaveID] = this;
    }
}

// Destructor
AlicatMFC::~AlicatMFC() {
    // Unregister this instance from the static registry
    if (_slaveID > 0 && _slaveID < 248) {
        _instances[_slaveID] = nullptr;
    }
}

// Update method - queues Modbus request for MFC data
void AlicatMFC::update() {
    if (_controlObj.fault) {
        if (_waitCount < 10) {
            _waitCount++;
            return;  // Skip this cycle to reduce request rate and reduce queue buildup due to timeouts
        } 
        _waitCount = 0;  // Reset wait counter
        _modbusDriver->modbus.clearSlaveQueue(_slaveID);
    }

    const uint8_t functionCode = 3;  // Read holding registers
    const uint16_t address = 1349;   // Starting address
    
    // Request MFC data (16 registers starting at 1349)
    // Use slave ID as requestId for callback routing
    if (!_modbusDriver->modbus.pushRequest(_slaveID, functionCode, address, _dataBuffer, 16, mfcResponseHandler, _slaveID)) {
        return;  // Queue full, try again next time
    }
    
    // Only request other registers if not in fault state to reduce queue flooding
    if (_errCount == 0) {
        // Request setpoint unit (1649, 1 register - uint16)
        const uint16_t setpointUnitAddress = 1649;
        if (!_modbusDriver->modbus.pushRequest(_slaveID, functionCode, setpointUnitAddress, &_unitBuffer[0], 1, unitsResponseHandler, _slaveID)) {
            return;  // Queue full, try again next time
        }
        
        // Request pressure unit (1673, 1 register - uint16)
        const uint16_t pressureUnitAddress = 1673;
        if (!_modbusDriver->modbus.pushRequest(_slaveID, functionCode, pressureUnitAddress, &_unitBuffer[1], 1, unitsResponseHandler, _slaveID)) {
            return;  // Queue full, try again next time
        }
        
        // Request flow unit (1721, 1 register - uint16)
        const uint16_t flowUnitAddress = 1721;
        if (!_modbusDriver->modbus.pushRequest(_slaveID, functionCode, flowUnitAddress, &_unitBuffer[2], 1, unitsResponseHandler, _slaveID)) {
            return;  // Queue full, try again next time
        }
    }
}

// Write setpoint method
bool AlicatMFC::writeSetpoint(float setpoint, bool mLmin) {
    // If setpoint is in mL/min instead of current units, and setpoint unit is not mL/min, convert it
    if (mLmin && _setpointUnitCode != 4) {
        setpoint *= _flowConversionFactor;
    }
    _pendingSetpoint = setpoint;
    _writeAttempts = 0;  // Reset attempts
    
    const uint8_t functionCode = 16;  // Write multiple registers
    const uint16_t address = 1349;    // Setpoint register address
    
    // Convert float to swapped uint16 format
    _modbusDriver->modbus.float32ToSwappedUint16(setpoint, _writeBuffer);
    
    // Queue the write request - use slave ID as requestId for callback routing
    if (!_modbusDriver->modbus.pushRequest(_slaveID, functionCode, address, _writeBuffer, 2, mfcWriteResponseHandler, _slaveID)) {
        return false;  // Queue full
    }
    
    return true;
}

// Static callback for MFC data response
void AlicatMFC::mfcResponseHandler(bool valid, uint16_t *data, uint32_t requestId) {
    // requestId is the slave ID - use it to route to the correct instance
    if (requestId > 0 && requestId < 248 && _instances[requestId] != nullptr) {
        _instances[requestId]->handleMfcResponse(valid, data);
    }
}

// Static callback for write response
void AlicatMFC::mfcWriteResponseHandler(bool valid, uint16_t *data, uint32_t requestId) {
    // requestId is the slave ID - use it to route to the correct instance
    if (requestId > 0 && requestId < 248 && _instances[requestId] != nullptr) {
        _instances[requestId]->handleWriteResponse(valid, data);
    }
}

// Instance method to handle MFC data response
void AlicatMFC::handleMfcResponse(bool valid, uint16_t *data) {
    // Invalid response, already in fault state - return early
    if (!valid && _controlObj.fault) return;

    // Invalid response, not yet connected and not yet flagged for error - flag error and set control object state
    if (!valid && !_err && _firstConnect) {
        snprintf(_controlObj.message, sizeof(_controlObj.message), "Alicat MFC (ID %d) has not yet connected", _slaveID);
        _controlObj.newMessage = true;
        _err = true;
        return;
    }

    // Invalid response, not yet connected - return early
    if(!valid && _firstConnect) return;

    // Invalid response - update error counter, message and return
    if(!valid && _errCount < _maxErrors) {
        _err = true;
        _errCount++;
        snprintf(_controlObj.message, sizeof(_controlObj.message), "Alicat MFC (ID %d) timeout, consecutive errors: %lu", _slaveID, _errCount);
        _controlObj.newMessage = true;
        return;
    }

    // Invalid response, error count > max errors
    if (!valid) {
        // Set fault state in control object
        _controlObj.fault = true;
        _controlObj.connected = false;
        _controlObj.newMessage = true;
        // Set fault state in sensor objects
        _flowSensor.fault = true;
        _pressureSensor.fault = true;
        _fault = true;
        snprintf(_controlObj.message, sizeof(_controlObj.message), "Alicat MFC (ID %d) offline", _slaveID);
        _controlObj.newMessage = true;
        return;
    }

    // Valid response, previous state was error or fault or not yet connected
    if (valid && (_err || _controlObj.fault || _firstConnect)) {
        _controlObj.fault = false;
        _controlObj.connected = true;
        _flowSensor.fault = false;
        _pressureSensor.fault = false;
        _fault = false;
        _errCount = 0;
        _err = false;
        snprintf(_controlObj.message, sizeof(_controlObj.message), "Alicat MFC (ID %d) communication %s", _slaveID, _firstConnect ? "established" : "restored");
        _controlObj.newMessage = true;
        _newMessage = true;
        writeSetpoint(_setpoint, false);
        _firstConnect = false;
    } else {
        _newMessage = false;
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
    
    // Update device control object
    _controlObj.setpoint = _setpoint;
    _controlObj.actualValue = _flowSensor.flow;  // Primary feedback is flow
    strncpy(_controlObj.setpointUnit, _setpointUnit, sizeof(_controlObj.setpointUnit));

    // If we just wrote a setpoint, validate it
    if (_newSetpoint) {
        if (fabs(_setpoint - _pendingSetpoint) > _adjustedAbsDevFlow) {
            _fault = true;
            _controlObj.fault = true;
            snprintf(_message, sizeof(_message), 
                    "Setpoint write validation failed for MFC (ID %d): expected %0.4f, got %0.4f", 
                    _slaveID, _pendingSetpoint, _setpoint);
            _newMessage = true;
        } else {
            _fault = false;  // Clear fault flag on successful validation
            _controlObj.fault = false;
            snprintf(_message, sizeof(_message), 
                    "Setpoint write successful for MFC (ID %d): setpoint is now %0.4f", 
                    _slaveID, _setpoint);
            _newMessage = true;
        }
        _newSetpoint = false;
        _controlObj.newMessage = true;
        strncpy(_controlObj.message, _message, sizeof(_controlObj.message));
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
void AlicatMFC::unitsResponseHandler(bool valid, uint16_t *data, uint32_t requestId) {
    // requestId is the slave ID - use it to route to the correct instance
    if (requestId > 0 && requestId < 248 && _instances[requestId] != nullptr) {
        _instances[requestId]->handleUnitsResponse(valid, data);
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

        // Update flow conversion factor for setpoint translation
        _flowConversionFactor = getAlicatFlowConversionFactor(_setpointUnitCode);
        _adjustedAbsDevFlow = _flowConversionFactor * 3.2;  // 3.2mL/min allowable deviation from setpoint due to device characterstics
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