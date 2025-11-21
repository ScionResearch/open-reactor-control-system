#pragma once

#include "sys_init.h"

// Forward declarations to avoid circular includes
struct ModbusDriver_t;

/**
 * @brief Hamilton Arc Dissolved Oxygen Sensor Driver Class
 * 
 * Supports communication with Hamilton Arc DO sensors via Modbus RTU.
 * Reads dissolved oxygen value and temperature from the sensor.
 * 
 * Register Map (Primary Measurement Channels):
 * - PMC 1 (2089): DO value with units (10 registers)
 * - PMC 6 (2409): Temperature with units (10 registers)
 */
class HamiltonArcDO {
public:
    /**
     * @brief Construct a new Hamilton Arc DO Sensor instance
     * 
     * @param modbusDriver Pointer to the ModbusDriver_t managing the serial port
     * @param slaveID Modbus slave ID of the DO sensor (typically 1-247)
     */
    HamiltonArcDO(ModbusDriver_t *modbusDriver, uint8_t slaveID);
    
    /**
     * @brief Destructor - unregisters instance from callback routing
     */
    ~HamiltonArcDO();
    
    /**
     * @brief Update the DO sensor readings
     * 
     * Call this periodically (e.g., every 2000ms) to request new data from the sensor.
     * This is non-blocking and queues Modbus requests.
     */
    void update();
    
    /**
     * @brief Get the dissolved oxygen sensor object
     * @return Reference to the DissolvedOxygenSensor_t object containing DO data
     */
    DissolvedOxygenSensor_t& getDOSensor() { return _doSensor; }
    
    /**
     * @brief Get the temperature sensor object
     * @return Reference to the TemperatureSensor_t object containing temperature data
     */
    TemperatureSensor_t& getTemperatureSensor() { return _temperatureSensor; }
    
    /**
     * @brief Get the Modbus slave ID
     * @return Slave ID of this sensor
     */
    uint8_t getSlaveID() const { return _slaveID; }
    
    /**
     * @brief Check if there's a fault condition in either sensor
     * @return true if fault detected in DO or temperature sensor
     */
    bool hasFault() const { return _doSensor.fault || _temperatureSensor.fault; }
    
    /**
     * @brief Check if there's a new message from either sensor
     * @return true if new message available
     */
    bool hasNewMessage() const { return _doSensor.newMessage || _temperatureSensor.newMessage; }
    
    /**
     * @brief Get the message string (prioritizes fault messages)
     * @return Pointer to message string from faulted sensor, or first sensor with message
     */
    const char* getMessage() const {
        if (_doSensor.fault) return _doSensor.message;
        if (_temperatureSensor.fault) return _temperatureSensor.message;
        if (_doSensor.newMessage) return _doSensor.message;
        if (_temperatureSensor.newMessage) return _temperatureSensor.message;
        return "";
    }
    
    /**
     * @brief Clear the message flags from both sensors
     */
    void clearMessages() {
        _doSensor.newMessage = false;
        _temperatureSensor.newMessage = false;
    }
    
    /**
     * @brief Get the device control object
     * @return Pointer to the DeviceControl_t object for this device
     */
    DeviceControl_t* getControlObject() { return &_controlObj; }
    
private:
    ModbusDriver_t *_modbusDriver;          ///< Pointer to the Modbus driver managing the serial port
    uint8_t _slaveID;                        ///< Modbus slave ID
    DissolvedOxygenSensor_t _doSensor;       ///< Dissolved oxygen sensor data
    TemperatureSensor_t _temperatureSensor;  ///< Temperature sensor data
    DeviceControl_t _controlObj;             ///< Device control object
    uint16_t _dataBuffer[10];                ///< Data buffer for Modbus transactions

    bool _firstConnect;                      ///< Flag to track first successful connection for this pH probe instance
    
    // Unit tracking for each instance
    uint32_t _doUnitCode;                    ///< DO unit code (for change detection)
    uint32_t _tempUnitCode;                  ///< Temperature unit code (for change detection)
    
    // Static instance registry for callback routing (indexed by slave ID)
    static HamiltonArcDO* _instances[248];
    
    // Static callback functions for Modbus responses
    static void doResponseHandler(bool valid, uint16_t *data, uint32_t requestId);
    static void temperatureResponseHandler(bool valid, uint16_t *data, uint32_t requestId);
    
    // Instance methods called by static callbacks
    void handleDOResponse(bool valid, uint16_t *data);
    void handleTemperatureResponse(bool valid, uint16_t *data);
};
