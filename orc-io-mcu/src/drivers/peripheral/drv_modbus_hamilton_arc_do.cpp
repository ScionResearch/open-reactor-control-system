#include "drv_modbus_hamilton_arc_do.h"

// Initialise static instance registry
HamiltonArcDO* HamiltonArcDO::_instances[248] = {nullptr};

// Constructor
HamiltonArcDO::HamiltonArcDO(ModbusDriver_t *modbusDriver, uint8_t slaveID) 
    : _modbusDriver(modbusDriver), _slaveID(slaveID), _doUnitCode(0), _tempUnitCode(0) {
    // Initialise dissolved oxygen sensor object
    _doSensor.dissolvedOxygen = NAN;  // Initialize as NaN until first valid reading
    _doSensor.fault = false;
    _doSensor.newMessage = false;
    strcpy(_doSensor.unit, "--");
    _doSensor.message[0] = '\0';
    
    // Initialise temperature sensor object
    _temperatureSensor.temperature = NAN;  // Initialize as NaN until first valid reading
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

    // Local vars to keep track of connection state
    _firstConnect = true;   // Mark as first connection
    _err = false;
    _errCount = 0;          // Initialise error count
    _waitCount = 0;         // Initialise wait count
    _maxErrors = 5;         // Maximum consecutive errors before marking as disconnected(fault)
    
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
    if (_controlObj.fault) {
        if (_waitCount < 10) {
            _waitCount++;
            return;  // Skip this cycle to reduce request rate and reduce queue buildup due to timeouts
        } 
        _waitCount = 0;  // Reset wait counter
        _modbusDriver->modbus.clearSlaveQueue(_slaveID);
    }

    const uint8_t functionCode = 3;  // Read holding registers    

    // Request DO data (register HAMILTON_PMC_1_ADDR, HAMILTON_PMC_REG_SIZE registers)
    // Use slave ID as requestId for callback routing
    uint16_t doAddress = HAMILTON_PMC_1_ADDR;
    if (!_modbusDriver->modbus.pushRequest(_slaveID, functionCode, doAddress, _doBuffer, HAMILTON_PMC_REG_SIZE, doResponseHandler, _slaveID)) {
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
    // Invalid response, already in fault state - return early
    if (!valid && _controlObj.fault) return;

    // Invalid response, not yet connected and not yet flagged for error - flag error and set control object state
    if (!valid && !_err && _firstConnect) {
        snprintf(_controlObj.message, sizeof(_controlObj.message), "Hamilton Arc DO sensor (ID %d) has not yet connected", _slaveID);
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
        snprintf(_controlObj.message, sizeof(_controlObj.message), "Hamilton Arc DO sensor (ID %d) timeout, consecutive errors: %lu", _slaveID, _errCount);
        _controlObj.newMessage = true;
        return;
    }

    // Invalid response, error count > max errors
    if (!valid) {
        // Set fault state in control object
        _controlObj.fault = true;
        _controlObj.connected = false;
        _controlObj.newMessage = true;
        // Set fault state in DO object
        _doSensor.fault = true;
        snprintf(_controlObj.message, sizeof(_controlObj.message), "Hamilton Arc DO sensor (ID %d) offline", _slaveID);
        _controlObj.newMessage = true;
        return;
    }

    // Valid response, previous state was error or fault or not yet connected
    if (valid && (_err || _controlObj.fault || _firstConnect)) {
        _controlObj.fault = false;
        _controlObj.connected = true;
        _doSensor.fault = false;
        _errCount = 0;
        _err = false;
        snprintf(_controlObj.message, sizeof(_controlObj.message), "Hamilton Arc DO sensor (ID %d) communication %s", _slaveID, _firstConnect ? "established" : "restored");
        _controlObj.newMessage = true;
        _firstConnect = false;
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
    float DO;
    memcpy(&DO, &data[2], sizeof(float));
    _doSensor.dissolvedOxygen = DO;
    
    // Update device control object
    _controlObj.actualValue = _doSensor.dissolvedOxygen;  // Current DO reading
    strncpy(_controlObj.setpointUnit, _doSensor.unit, sizeof(_controlObj.setpointUnit));
}

// Instance method to handle temperature response
void HamiltonArcDO::handleTemperatureResponse(bool valid, uint16_t *data) {
    if (_firstConnect) return;  // Do nothing if main measurment has not yet completed first connection
    
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
