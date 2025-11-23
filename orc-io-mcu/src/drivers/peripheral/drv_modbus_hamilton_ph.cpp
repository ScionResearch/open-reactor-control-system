#include "drv_modbus_hamilton_ph.h"

// Initialize static instance registry
HamiltonPHProbe* HamiltonPHProbe::_instances[248] = {nullptr};

// Constructor
HamiltonPHProbe::HamiltonPHProbe(ModbusDriver_t *modbusDriver, uint8_t slaveID) 
    : _modbusDriver(modbusDriver), _slaveID(slaveID), _phUnitCode(0), _tempUnitCode(0) {
    // Initialize sensor objects
    _phSensor.ph = NAN;  // Initialize as NaN until first valid reading
    _phSensor.fault = false;
    _phSensor.newMessage = false;
    strcpy(_phSensor.unit, "--");
    _phSensor.message[0] = '\0';
    
    // Initialise temperature sensor object
    _temperatureSensor.temperature = NAN;  // Initialize as NaN until first valid reading
    _temperatureSensor.fault = false;
    _temperatureSensor.newMessage = false;
    strcpy(_temperatureSensor.unit, "--");
    _temperatureSensor.message[0] = '\0';

    // Register with control object
    _controlObj.slaveID = _slaveID;
    _controlObj.deviceType = IPC_DEV_HAMILTON_PH;
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
HamiltonPHProbe::~HamiltonPHProbe() {
    // Unregister this instance from the static registry
    if (_slaveID > 0 && _slaveID < 248) {
        _instances[_slaveID] = nullptr;
    }
}

// Update method - queues Modbus requests for pH and temperature
void HamiltonPHProbe::update() {
    if (_controlObj.fault) {
        if (_waitCount < 10) {
            _waitCount++;
            return;  // Skip this cycle to reduce request rate and reduce queue buildup due to timeouts
        } 
        _waitCount = 0;  // Reset wait counter
        _modbusDriver->modbus.clearSlaveQueue(_slaveID);
    }

    const uint8_t functionCode = 3;  // Read holding registers
    
    // Request pH data (register HAMILTON_PMC_1_ADDR, HAMILTON_PMC_REG_SIZE registers)
    // Use slave ID as requestId for callback routing
    uint16_t phAddress = HAMILTON_PMC_1_ADDR;
    if (!_modbusDriver->modbus.pushRequest(_slaveID, functionCode, phAddress, _phBuffer, HAMILTON_PMC_REG_SIZE, phResponseHandler, _slaveID)) {
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

// Static callback for pH response
void HamiltonPHProbe::phResponseHandler(bool valid, uint16_t *data, uint32_t requestId) {
    // requestId is the slave ID - use it to route to the correct instance
    if (requestId > 0 && requestId < 248 && _instances[requestId] != nullptr) {
        _instances[requestId]->handlePhResponse(valid, data);
    }
}

// Static callback for temperature response
void HamiltonPHProbe::temperatureResponseHandler(bool valid, uint16_t *data, uint32_t requestId) {
    // requestId is the slave ID - use it to route to the correct instance
    if (requestId > 0 && requestId < 248 && _instances[requestId] != nullptr) {
        _instances[requestId]->handleTemperatureResponse(valid, data);
    }
}

// Instance method to handle pH response
void HamiltonPHProbe::handlePhResponse(bool valid, uint16_t *data) {
    // Invalid response, already in fault state - return early
    if (!valid && _controlObj.fault) return;

    // Invalid response, not yet connected and not yet flagged for error - flag error and set control object state
    if (!valid && !_err && _firstConnect) {
        snprintf(_controlObj.message, sizeof(_controlObj.message), "Hamilton Arc pH sensor (ID %d) has not yet connected", _slaveID);
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
        snprintf(_controlObj.message, sizeof(_controlObj.message), "Hamilton Arc pH sensor (ID %d) timeout, consecutive errors: %lu", _slaveID, _errCount);
        _controlObj.newMessage = true;
        return;
    }

    // Invalid response, error count > max errors
    if (!valid) {
        // Set fault state in control object
        _controlObj.fault = true;
        _controlObj.connected = false;
        _controlObj.newMessage = true;
        // Set fault state in pH object
        _phSensor.fault = true;
        snprintf(_controlObj.message, sizeof(_controlObj.message), "Hamilton Arc pH sensor (ID %d) offline", _slaveID);
        _controlObj.newMessage = true;
        return;
    }

    // Valid response, previous state was error or fault or not yet connected
    if (valid && (_err || _controlObj.fault || _firstConnect)) {
        _controlObj.fault = false;
        _controlObj.connected = true;
        _phSensor.fault = false;
        _errCount = 0;
        _err = false;
        snprintf(_controlObj.message, sizeof(_controlObj.message), "Hamilton Arc pH sensor (ID %d) communication %s", _slaveID, _firstConnect ? "established" : "restored");
        _controlObj.newMessage = true;
        _firstConnect = false;
    }
    
    // Check for unit change (first 2 registers contain unit code as uint32_t)
    uint32_t newUnitCode;
    memcpy(&newUnitCode, &data[0], sizeof(uint32_t));
    if (newUnitCode != _phUnitCode) {
        _phUnitCode = newUnitCode;
        const char* unitStr = getHamiltonUnit(_phUnitCode);
        strncpy(_phSensor.unit, unitStr, sizeof(_phSensor.unit) - 1);
        _phSensor.unit[sizeof(_phSensor.unit) - 1] = '\0';  // Ensure null termination
    }
    
    // Extract float from registers (data[2] and data[3] contain the float)
    float pH;
    memcpy(&pH, &data[2], sizeof(float));
    _phSensor.ph = pH;
    
    // Update device control object
    _controlObj.actualValue = _phSensor.ph;  // Current pH reading
    strncpy(_controlObj.setpointUnit, _phSensor.unit, sizeof(_controlObj.setpointUnit));
}

// Instance method to handle temperature response
void HamiltonPHProbe::handleTemperatureResponse(bool valid, uint16_t *data) {
    if (_firstConnect) return;  // Do nothing if main measurment has not yet completed first connection
    
    if (!valid) {
        _temperatureSensor.fault = true;
        snprintf(_temperatureSensor.message, sizeof(_temperatureSensor.message), 
                 "Invalid temperature data from Hamilton Arc pH sensor (ID %d)", _slaveID);
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