#include "drv_modbus_hamilton_arc_do.h"

// Initialize static instance registry
HamiltonArcDO* HamiltonArcDO::_instances[248] = {nullptr};

// Constructor
HamiltonArcDO::HamiltonArcDO(ModbusDriver_t *modbusDriver, uint8_t slaveID) 
    : _modbusDriver(modbusDriver), _slaveID(slaveID), _doUnitCode(0), _tempUnitCode(0) {
    // Initialize dissolved oxygen sensor object
    _doSensor.dissolvedOxygen = 0.0;
    _doSensor.fault = false;
    _doSensor.newMessage = false;
    strcpy(_doSensor.unit, "--");
    _doSensor.message[0] = '\0';
    
    // Initialize temperature sensor object
    _temperatureSensor.temperature = 0.0;
    _temperatureSensor.fault = false;
    _temperatureSensor.newMessage = false;
    strcpy(_temperatureSensor.unit, "--");
    _temperatureSensor.message[0] = '\0';

    // Register with control object
    _controlObj.slaveID = _slaveID;
    _controlObj.deviceType = IPC_DEV_HAMILTON_DO;
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
HamiltonArcDO::~HamiltonArcDO() {
    // Unregister this instance from the static registry
    if (_slaveID > 0 && _slaveID < 248) {
        _instances[_slaveID] = nullptr;
    }
}

// Update method - queues Modbus requests for DO and temperature
void HamiltonArcDO::update() {
    const uint8_t functionCode = 3;  // Read holding registers
    
    // Request DO data (register HAMILTON_PMC_1_ADDR, HAMILTON_PMC_REG_SIZE registers)
    // Use slave ID as requestId for callback routing
    uint16_t doAddress = HAMILTON_PMC_1_ADDR;
    if (!_modbusDriver->modbus.pushRequest(_slaveID, functionCode, doAddress, _dataBuffer, HAMILTON_PMC_REG_SIZE, doResponseHandler, _slaveID)) {
        return;  // Queue full, try again next time
    }
    
    // Request temperature data (register HAMILTON_PMC_6_ADDR, HAMILTON_PMC_REG_SIZE registers)
    uint16_t tempAddress = HAMILTON_PMC_6_ADDR;
    if (!_modbusDriver->modbus.pushRequest(_slaveID, functionCode, tempAddress, _dataBuffer, HAMILTON_PMC_REG_SIZE, temperatureResponseHandler, _slaveID)) {
        return;  // Queue full, try again next time
    }
}

// Static callback for DO response
void HamiltonArcDO::doResponseHandler(bool valid, uint16_t *data, uint32_t requestId) {
    // requestId is the slave ID - use it to route to the correct instance
    if (requestId > 0 && requestId < 248 && _instances[requestId] != nullptr) {
        _instances[requestId]->handleDOResponse(valid, data);
    }
}

// Static callback for temperature response
void HamiltonArcDO::temperatureResponseHandler(bool valid, uint16_t *data, uint32_t requestId) {
    // requestId is the slave ID - use it to route to the correct instance
    if (requestId > 0 && requestId < 248 && _instances[requestId] != nullptr) {
        _instances[requestId]->handleTemperatureResponse(valid, data);
    }
}

// Instance method to handle DO response
void HamiltonArcDO::handleDOResponse(bool valid, uint16_t *data) {
    if (!valid && !_controlObj.fault) {
        if (!_firstConnect) {
            _controlObj.fault = true;
            snprintf(_doSensor.message, sizeof(_doSensor.message), "Invalid or no response from Hamilton DO sensor (ID %d)", _slaveID);
        } else {
            snprintf(_doSensor.message, sizeof(_doSensor.message), "Hamilton DO sensor (ID %d) has not yet connected", _slaveID);
        }
        _doSensor.newMessage = true;
        
        // Update control object with fault status
        _controlObj.connected = false;
        _controlObj.newMessage = true;
        strncpy(_controlObj.message, _doSensor.message, sizeof(_controlObj.message));
        
        return;
    } else if (valid) {
        if (_controlObj.fault) {
            _controlObj.fault = false;
            snprintf(_doSensor.message, sizeof(_doSensor.message), "Hamilton DO sensor (ID %d) communication %s", _slaveID, _firstConnect ? "established" : "restored");
            _firstConnect = false;
            _doSensor.newMessage = true;
            
            // Update control object with fault status
            _controlObj.connected = true;
            _controlObj.newMessage = true;
            strncpy(_controlObj.message, _doSensor.message, sizeof(_controlObj.message));
        }
    
        // Check for unit change (first 2 registers contain unit code as uint32_t)
        uint32_t newUnitCode;
        memcpy(&newUnitCode, &data[0], sizeof(uint32_t));
        if (newUnitCode != _doUnitCode) {
            _doUnitCode = newUnitCode;
            const char* unitStr = getHamiltonUnit(_doUnitCode);
            strncpy(_doSensor.unit, unitStr, sizeof(_doSensor.unit) - 1);
            _doSensor.unit[sizeof(_doSensor.unit) - 1] = '\0';  // Ensure null termination
        }
        
        // Extract float from registers (data[2] and data[3] contain the float)
        float dissolvedOxygen;
        memcpy(&dissolvedOxygen, &data[2], sizeof(float));
        _doSensor.dissolvedOxygen = dissolvedOxygen;
        _doSensor.fault = false;
        
        // Update device control object
        _controlObj.setpoint = 0.0f;  // No setpoint for sensor-only device (future pH control will use this)
        _controlObj.actualValue = _doSensor.dissolvedOxygen;  // Current pH reading
        strncpy(_controlObj.setpointUnit, _doSensor.unit, sizeof(_controlObj.setpointUnit));
        _controlObj.connected = true;  // Got valid Modbus response
        _controlObj.fault = _doSensor.fault || _temperatureSensor.fault;
        _controlObj.newMessage = _doSensor.newMessage || _temperatureSensor.newMessage;
        if (_doSensor.newMessage) {
            strncpy(_controlObj.message, _doSensor.message, sizeof(_controlObj.message));
        } else if (_temperatureSensor.newMessage) {
            strncpy(_controlObj.message, _temperatureSensor.message, sizeof(_controlObj.message));
        }
    }
}

// Instance method to handle temperature response
void HamiltonArcDO::handleTemperatureResponse(bool valid, uint16_t *data) {
    if (!valid) {
        _temperatureSensor.fault = true;
        snprintf(_temperatureSensor.message, sizeof(_temperatureSensor.message), 
                 "Invalid temperature data from Hamilton Arc DO sensor (ID %d)", _slaveID);
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
