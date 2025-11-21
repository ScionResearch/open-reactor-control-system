#include "drv_modbus_hamilton_ph.h"

// Initialize static instance registry
HamiltonPHProbe* HamiltonPHProbe::_instances[248] = {nullptr};

// Constructor
HamiltonPHProbe::HamiltonPHProbe(ModbusDriver_t *modbusDriver, uint8_t slaveID) 
    : _modbusDriver(modbusDriver), _slaveID(slaveID), _phUnitCode(0), _tempUnitCode(0) {
    // Initialize sensor objects
    _phSensor.ph = 0.0;
    _phSensor.fault = false;
    _phSensor.newMessage = false;
    strcpy(_phSensor.unit, "--");
    _phSensor.message[0] = '\0';
    
    _temperatureSensor.temperature = 0.0;
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
    const uint8_t functionCode = 3;  // Read holding registers
    
    // Request pH data (register HAMILTON_PMC_1_ADDR, HAMILTON_PMC_REG_SIZE registers)
    // Use slave ID as requestId for callback routing
    uint16_t phAddress = HAMILTON_PMC_1_ADDR;
    if (!_modbusDriver->modbus.pushRequest(_slaveID, functionCode, phAddress, _dataBuffer, HAMILTON_PMC_REG_SIZE, phResponseHandler, _slaveID)) {
        return;  // Queue full, try again next time
    }
    
    // Request temperature data (register HAMILTON_PMC_6_ADDR, HAMILTON_PMC_REG_SIZE registers)
    uint16_t tempAddress = HAMILTON_PMC_6_ADDR;
    if (!_modbusDriver->modbus.pushRequest(_slaveID, functionCode, tempAddress, _dataBuffer, HAMILTON_PMC_REG_SIZE, temperatureResponseHandler, _slaveID)) {
        return;  // Queue full, try again next time
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
    if (!valid && !_controlObj.fault) {
        if (!_firstConnect) {
            _controlObj.fault = true;
            snprintf(_phSensor.message, sizeof(_phSensor.message), "Invalid or no response from Hamilton pH probe (ID %d)", _slaveID);
            _phSensor.fault = true;
            snprintf(_phSensor.message, sizeof(_phSensor.message), "Invalid pH data from pH probe (ID %d)", _slaveID);
            _phSensor.newMessage = true;
        } else {
            snprintf(_phSensor.message, sizeof(_phSensor.message), "Hamilton pH probe (ID %d) has not yet connected", _slaveID);
        }
        
        // Update control object with fault status
        _controlObj.connected = false;
        _controlObj.newMessage = true;
        strncpy(_controlObj.message, _phSensor.message, sizeof(_controlObj.message));
        
        return;
    } else if (valid) {
        if (_controlObj.fault) {
            _controlObj.fault = false;
            snprintf(_phSensor.message, sizeof(_phSensor.message), "Hamilton pH probe (ID %d) communication %s", _slaveID, _firstConnect ? "established" : "restored");
            _firstConnect = false;
            _phSensor.newMessage = true;
            
            // Update control object with fault status
            _controlObj.connected = true;
            _controlObj.newMessage = true;
            strncpy(_controlObj.message, _phSensor.message, sizeof(_controlObj.message));
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
        _phSensor.fault = false;
        
        // Update device control object
        _controlObj.setpoint = 0.0f;  // No setpoint for sensor-only device (future pH control will use this)
        _controlObj.actualValue = _phSensor.ph;  // Current pH reading
        strncpy(_controlObj.setpointUnit, _phSensor.unit, sizeof(_controlObj.setpointUnit));
        _controlObj.connected = true;  // Got valid Modbus response
        _controlObj.fault = _phSensor.fault || _temperatureSensor.fault;
        _controlObj.newMessage = _phSensor.newMessage || _temperatureSensor.newMessage;
        if (_phSensor.newMessage) {
            strncpy(_controlObj.message, _phSensor.message, sizeof(_controlObj.message));
        } else if (_temperatureSensor.newMessage) {
            strncpy(_controlObj.message, _temperatureSensor.message, sizeof(_controlObj.message));
        }
    }
}

// Instance method to handle temperature response
void HamiltonPHProbe::handleTemperatureResponse(bool valid, uint16_t *data) {
    if (_firstConnect) return;  // Do nothing if main measurment has not yet completed first connection
    
    if (!valid) {
        _temperatureSensor.fault = true;
        snprintf(_temperatureSensor.message, sizeof(_temperatureSensor.message), 
                 "Invalid temperature data from pH probe (ID %d)", _slaveID);
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