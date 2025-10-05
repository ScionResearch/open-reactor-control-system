#include "drv_modbus_hamilton_arc_od.h"

// Initialize static member
HamiltonArcOD* HamiltonArcOD::_currentInstance = nullptr;

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
}

// Update method - queues Modbus requests for OD and temperature
void HamiltonArcOD::update() {
    const uint8_t functionCode = 3;  // Read holding registers
    
    // Set current instance for callbacks
    _currentInstance = this;
    
    // Request OD data (register HAMILTON_PMC_1_ADDR, HAMILTON_PMC_REG_SIZE registers)
    uint16_t odAddress = HAMILTON_PMC_1_ADDR;
    if (!_modbusDriver->modbus.pushRequest(_slaveID, functionCode, odAddress, _dataBuffer, HAMILTON_PMC_REG_SIZE, odResponseHandler)) {
        return;  // Queue full, try again next time
    }
    
    // Request temperature data (register HAMILTON_PMC_6_ADDR, HAMILTON_PMC_REG_SIZE registers)
    uint16_t tempAddress = HAMILTON_PMC_6_ADDR;
    if (!_modbusDriver->modbus.pushRequest(_slaveID, functionCode, tempAddress, _dataBuffer, HAMILTON_PMC_REG_SIZE, temperatureResponseHandler)) {
        return;  // Queue full, try again next time
    }
}

// Static callback for OD response
void HamiltonArcOD::odResponseHandler(bool valid, uint16_t *data) {
    if (_currentInstance) {
        _currentInstance->handleODResponse(valid, data);
    }
}

// Static callback for temperature response
void HamiltonArcOD::temperatureResponseHandler(bool valid, uint16_t *data) {
    if (_currentInstance) {
        _currentInstance->handleTemperatureResponse(valid, data);
    }
}

// Instance method to handle OD response
void HamiltonArcOD::handleODResponse(bool valid, uint16_t *data) {
    if (!valid) {
        _odSensor.fault = true;
        snprintf(_odSensor.message, sizeof(_odSensor.message), "Invalid OD data from Hamilton Arc OD sensor (ID %d)", _slaveID);
        _odSensor.newMessage = true;
        return;
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
