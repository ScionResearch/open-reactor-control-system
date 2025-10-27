#include "drv_modbus_hamilton_ph.h"

// Initialize static member
HamiltonPHProbe* HamiltonPHProbe::_currentInstance = nullptr;

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
}

// Update method - queues Modbus requests for pH and temperature
void HamiltonPHProbe::update() {
    //Serial.println("PHProbe-update");
    const uint8_t functionCode = 3;  // Read holding registers
    
    // Set current instance for callbacks
    _currentInstance = this;
    
    // Request pH data (register HAMILTON_PMC_1_ADDR, HAMILTON_PMC_REG_SIZE registers)
    uint16_t phAddress = HAMILTON_PMC_1_ADDR;
    if (!_modbusDriver->modbus.pushRequest(_slaveID, functionCode, phAddress, _dataBuffer, HAMILTON_PMC_REG_SIZE, phResponseHandler)) {
        return;  // Queue full, try again next time
    }
    
    // Request temperature data (register HAMILTON_PMC_6_ADDR, HAMILTON_PMC_REG_SIZE registers)
    uint16_t tempAddress = HAMILTON_PMC_6_ADDR;
    if (!_modbusDriver->modbus.pushRequest(_slaveID, functionCode, tempAddress, _dataBuffer, HAMILTON_PMC_REG_SIZE, temperatureResponseHandler)) {
        return;  // Queue full, try again next time
    }
}
// Static callback for pH response
void HamiltonPHProbe::phResponseHandler(bool valid, uint16_t *data) {
    if (_currentInstance) {
        _currentInstance->handlePhResponse(valid, data);
    }
}

// Static callback for temperature response
void HamiltonPHProbe::temperatureResponseHandler(bool valid, uint16_t *data) {
    if (_currentInstance) {
        _currentInstance->handleTemperatureResponse(valid, data);
    }
}

// Instance method to handle pH response
void HamiltonPHProbe::handlePhResponse(bool valid, uint16_t *data) {
    if (!valid) {
        _phSensor.fault = true;
        snprintf(_phSensor.message, sizeof(_phSensor.message), "Invalid pH data from pH probe (ID %d)", _slaveID);
        _phSensor.newMessage = true;
        
        // Update control object with fault status
        _controlObj.connected = false;
        _controlObj.fault = true;
        _controlObj.newMessage = true;
        strncpy(_controlObj.message, _phSensor.message, sizeof(_controlObj.message));
        
        return;
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
    _controlObj.slaveID = _slaveID;
    _controlObj.deviceType = IPC_DEV_HAMILTON_PH;
}

// Instance method to handle temperature response
void HamiltonPHProbe::handleTemperatureResponse(bool valid, uint16_t *data) {
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