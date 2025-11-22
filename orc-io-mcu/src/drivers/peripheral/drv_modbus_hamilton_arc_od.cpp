#include "drv_modbus_hamilton_arc_od.h"

// Initialize static instance registry
HamiltonArcOD* HamiltonArcOD::_instances[248] = {nullptr};

// Constructor
HamiltonArcOD::HamiltonArcOD(ModbusDriver_t *modbusDriver, uint8_t slaveID) 
    : _modbusDriver(modbusDriver), _slaveID(slaveID), _odUnitCode(0), _tempUnitCode(0) {
    // Initialize optical density sensor object
    _odSensor.opticalDensity = 0.0;
    _odSensor.fault = false;
    _odSensor.newMessage = false;
    strcpy(_odSensor.unit, "--");
    _odSensor.message[0] = '\0';
    
    // Initialize temperature sensor object
    _temperatureSensor.temperature = 0.0;
    _temperatureSensor.fault = false;
    _temperatureSensor.newMessage = false;
    strcpy(_temperatureSensor.unit, "--");
    _temperatureSensor.message[0] = '\0';

    // Register with control object
    _controlObj.slaveID = _slaveID;
    _controlObj.deviceType = IPC_DEV_HAMILTON_OD;
    _controlObj.connected = false;
    _controlObj.fault = false;
    _controlObj.newMessage = false;
    _controlObj.setpoint = 0.0;
    _controlObj.actualValue = 0.0;
    _controlObj.setpointUnit[0] = '\0';
    _controlObj.message[0] = '\0';
    
    // Register this instance in the static registry for callback routing
    if (_slaveID > 0 && _slaveID < 248) {
        _instances[_slaveID] = this;
    }
}

// Destructor
HamiltonArcOD::~HamiltonArcOD() {
    // Unregister this instance from the static registry
    if (_slaveID > 0 && _slaveID < 248) {
        _instances[_slaveID] = nullptr;
    }
}

// Update method - queues Modbus requests for OD and temperature
void HamiltonArcOD::update() {
    if (_disconnected) {
        if (_waitCount < 10) {
            _waitCount++;
            return;  // Skip this cycle to reduce request rate and reduce queue buildup due to timeouts
        } 
        _waitCount = 0;  // Reset wait counter
        _modbusDriver->modbus.clearSlaveQueue(_slaveID);
    }

    const uint8_t functionCode = 3;  // Read holding registers
    
    // Request OD data (register HAMILTON_PMC_1_ADDR, HAMILTON_PMC_REG_SIZE registers)
    // Use slave ID as requestId for callback routing
    uint16_t odAddress = HAMILTON_PMC_1_ADDR;
    if (!_modbusDriver->modbus.pushRequest(_slaveID, functionCode, odAddress, _odBuffer, HAMILTON_PMC_REG_SIZE, odResponseHandler, _slaveID)) {
        return;  // Queue full, try again next time
    }
    
    // Only request temperature data if we have no errors (to reduce timeout delays)
    if (_errCount == 0) {
        // Request temperature data (register HAMILTON_PMC_6_ADDR, HAMILTON_PMC_REG_SIZE registers)
        uint16_t tempAddress = HAMILTON_PMC_6_ADDR;
        if (!_modbusDriver->modbus.pushRequest(_slaveID, functionCode, tempAddress, _tempBuffer, HAMILTON_PMC_REG_SIZE, temperatureResponseHandler, _slaveID)) {
            return;  // Queue full, try again next time
        }
    }
}

// Static callback for OD response
void HamiltonArcOD::odResponseHandler(bool valid, uint16_t *data, uint32_t requestId) {
    // requestId is the slave ID - use it to route to the correct instance
    if (requestId > 0 && requestId < 248 && _instances[requestId] != nullptr) {
        _instances[requestId]->handleODResponse(valid, data);
    }
}

// Static callback for temperature response
void HamiltonArcOD::temperatureResponseHandler(bool valid, uint16_t *data, uint32_t requestId) {
    // requestId is the slave ID - use it to route to the correct instance
    if (requestId > 0 && requestId < 248 && _instances[requestId] != nullptr) {
        _instances[requestId]->handleTemperatureResponse(valid, data);
    }
}

// Instance method to handle OD response
void HamiltonArcOD::handleODResponse(bool valid, uint16_t *data) {
    if (!valid) {
        _errCount++;
        if (_disconnected) return;
        if (_errCount > 5) {
            _disconnected = true;
            _controlObj.fault = true;
            _controlObj.connected = false;
            _controlObj.newMessage = true;
            snprintf(_odSensor.message, sizeof(_odSensor.message), "No response from Hamilton OD sensor (ID %d)", _slaveID);
            _odSensor.newMessage = true;
            strncpy(_controlObj.message, _odSensor.message, sizeof(_controlObj.message));
            Serial.printf("[OD] No response from slave ID %d, marking as disconnected\n", _slaveID);
            return;
        }
    }
    if (!valid && !_controlObj.fault) {
        if (!_firstConnect) {
            _controlObj.fault = true;
            snprintf(_odSensor.message, sizeof(_odSensor.message), "Invalid or no response from Hamilton Arc OD sensor (ID %d)", _slaveID);
        } else {
            snprintf(_odSensor.message, sizeof(_odSensor.message), "Hamilton Arc OD sensor (ID %d) has not yet connected", _slaveID);
        }
        _odSensor.newMessage = true;
        
        // Update control object with fault status
        _controlObj.connected = false;
        _controlObj.newMessage = true;
        strncpy(_controlObj.message, _odSensor.message, sizeof(_controlObj.message));
        
        return;
    } else if (valid) {
        if (_controlObj.fault) {
            _controlObj.fault = false;
            _errCount = 0;
            _disconnected = false;
            snprintf(_odSensor.message, sizeof(_odSensor.message), "Hamilton Arc OD sensor (ID %d) communication %s", _slaveID, _firstConnect ? "established" : "restored");
            _firstConnect = false;
            _odSensor.newMessage = true;
            
            // Update control object with fault status
            _controlObj.connected = true;
            _controlObj.newMessage = true;
            strncpy(_controlObj.message, _odSensor.message, sizeof(_controlObj.message));
        }
    
        // Check for unit change (first 2 registers contain unit code as uint32_t)
        uint32_t newUnitCode;
        memcpy(&newUnitCode, &data[0], sizeof(uint32_t));
        if (newUnitCode != _odUnitCode) {
            _odUnitCode = newUnitCode;
            const char* unitStr = getHamiltonUnit(_odUnitCode);
            strncpy(_odSensor.unit, unitStr, sizeof(_odSensor.unit) - 1);
            _odSensor.unit[sizeof(_odSensor.unit) - 1] = '\0';  // Ensure null termination
        }
        
        // Extract float from registers (data[2] and data[3] contain the float)
        float opticalDensity;
        memcpy(&opticalDensity, &data[2], sizeof(float));
        _odSensor.opticalDensity = opticalDensity;
        _odSensor.fault = false;
        
        // Update device control object
        _controlObj.setpoint = 0.0f;  // No setpoint for sensor-only device (future pH control will use this)
        _controlObj.actualValue = _odSensor.opticalDensity;  // Current pH reading
        strncpy(_controlObj.setpointUnit, _odSensor.unit, sizeof(_controlObj.setpointUnit));
        _controlObj.connected = true;  // Got valid Modbus response
        _controlObj.fault = _odSensor.fault || _temperatureSensor.fault;
        _controlObj.newMessage = _odSensor.newMessage || _temperatureSensor.newMessage;
        if (_odSensor.newMessage) {
            strncpy(_controlObj.message, _odSensor.message, sizeof(_controlObj.message));
        } else if (_temperatureSensor.newMessage) {
            strncpy(_controlObj.message, _temperatureSensor.message, sizeof(_controlObj.message));
        }
    }
}

// Instance method to handle temperature response
void HamiltonArcOD::handleTemperatureResponse(bool valid, uint16_t *data) {
    if (!valid) {
        _temperatureSensor.fault = true;
        snprintf(_temperatureSensor.message, sizeof(_temperatureSensor.message), 
                 "Invalid temperature data from Hamilton Arc OD sensor (ID %d)", _slaveID);
        _temperatureSensor.newMessage = true;
        return;
    }
    
    // Check for unit change (first 2 registers contain unit code as uint32_t)
    uint32_t newUnitCode;
    memcpy(&newUnitCode, &data[0], sizeof(uint32_t));
    if (newUnitCode != _tempUnitCode) {
        _tempUnitCode = newUnitCode;
        const char* unitStr = getHamiltonUnit(_tempUnitCode);
        strncpy(_temperatureSensor.unit, unitStr, sizeof(_temperatureSensor.unit) - 1);
        _temperatureSensor.unit[sizeof(_temperatureSensor.unit) - 1] = '\0';  // Ensure null termination
    }
    
    // Extract float from registers (data[2] and data[3] contain the float)
    float temperature;
    memcpy(&temperature, &data[2], sizeof(float));
    _temperatureSensor.temperature = temperature;
    _temperatureSensor.fault = false;
}
