#include "drv_modbus_alicat_mfc.h"

// Initialize static member
AlicatMFC* AlicatMFC::_currentInstance = nullptr;

// Constructor
AlicatMFC::AlicatMFC(ModbusDriver_t *modbusDriver, uint8_t slaveID)
    : _modbusDriver(modbusDriver), _slaveID(slaveID), _setpoint(0.0),
      _fault(false), _newMessage(false), _newSetpoint(false),
      _pendingSetpoint(0.0), _writeAttempts(0) {
    // Initialize flow sensor
    _flowSensor.flow = 0.0;
    _flowSensor.fault = false;
    _flowSensor.newMessage = false;
    strcpy(_flowSensor.unit, "SCCM");  // Default unit, will be updated from device
    _flowSensor.message[0] = '\0';
    
    // Initialize pressure sensor
    _pressureSensor.pressure = 0.0;
    _pressureSensor.fault = false;
    _pressureSensor.newMessage = false;
    strcpy(_pressureSensor.unit, "PSIA");  // Default unit, will be updated from device
    _pressureSensor.message[0] = '\0';
    
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