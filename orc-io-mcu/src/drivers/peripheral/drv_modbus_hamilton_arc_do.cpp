#include "drv_modbus_hamilton_arc_do.h"

// Initialize static member
HamiltonArcDO* HamiltonArcDO::_currentInstance = nullptr;

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
}

// Update method - queues Modbus requests for DO and temperature
void HamiltonArcDO::update() {
    const uint8_t functionCode = 3;  // Read holding registers
    
    // Set current instance for callbacks
    _currentInstance = this;
    
    // Request DO data (register HAMILTON_PMC_1_ADDR, HAMILTON_PMC_REG_SIZE registers)
    uint16_t doAddress = HAMILTON_PMC_1_ADDR;
    if (!_modbusDriver->modbus.pushRequest(_slaveID, functionCode, doAddress, _dataBuffer, HAMILTON_PMC_REG_SIZE, doResponseHandler)) {
        return;  // Queue full, try again next time
    }
    
    // Request temperature data (register HAMILTON_PMC_6_ADDR, HAMILTON_PMC_REG_SIZE registers)
    uint16_t tempAddress = HAMILTON_PMC_6_ADDR;
    if (!_modbusDriver->modbus.pushRequest(_slaveID, functionCode, tempAddress, _dataBuffer, HAMILTON_PMC_REG_SIZE, temperatureResponseHandler)) {
        return;  // Queue full, try again next time
    }
}

// Static callback for DO response
void HamiltonArcDO::doResponseHandler(bool valid, uint16_t *data) {
    if (_currentInstance) {
        _currentInstance->handleDOResponse(valid, data);
    }
}

// Static callback for temperature response
void HamiltonArcDO::temperatureResponseHandler(bool valid, uint16_t *data) {
    if (_currentInstance) {
        _currentInstance->handleTemperatureResponse(valid, data);
    }
}

// Instance method to handle DO response
void HamiltonArcDO::handleDOResponse(bool valid, uint16_t *data) {
    if (!valid) {
        _doSensor.fault = true;
        snprintf(_doSensor.message, sizeof(_doSensor.message), "Invalid DO data from Hamilton Arc DO sensor (ID %d)", _slaveID);
        _doSensor.newMessage = true;
        return;
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
