#pragma once

#include "sys_init.h"

// Forward declarations to avoid circular includes
struct ModbusDriver_t;

/**
 * @brief Hamilton pH Probe Driver Class
 * 
 * Supports communication with Hamilton pH probes via Modbus RTU.
 * Reads pH value and temperature from the probe.
 * 
 * Register Map:
 * - 2089: pH value (float, 2 registers)
 * - 2409: Temperature (float, 2 registers)
 */
class HamiltonPHProbe {
public:
    /**
     * @brief Construct a new Hamilton pH Probe instance
     * 
     * @param modbusDriver Pointer to the ModbusDriver_t managing the serial port
     * @param slaveID Modbus slave ID of the pH probe (typically 1-247)
     */
    HamiltonPHProbe(ModbusDriver_t *modbusDriver, uint8_t slaveID);
    
    /**
     * @brief Destructor - unregisters instance from callback routing
     */
    ~HamiltonPHProbe();
    
    /**
     * @brief Update the pH probe readings
     * 
     * Call this periodically (e.g., every 2000ms) to request new data from the probe.
     * This is non-blocking and queues Modbus requests.
     */
    void update();
    
    /**
     * @brief Get the pH sensor object
     * @return Reference to the PhSensor_t object containing pH data
     */
    PhSensor_t& getPhSensor() { return _phSensor; }
    
    /**
     * @brief Get the temperature sensor object
     * @return Reference to the TemperatureSensor_t object containing temperature data
     */
    TemperatureSensor_t& getTemperatureSensor() { return _temperatureSensor; }
    
    /**
     * @brief Get the Modbus slave ID
     * @return Slave ID of this probe
     */
    uint8_t getSlaveID() const { return _slaveID; }
    
    /**
     * @brief Check if there's a fault condition in either sensor
     * @return true if fault detected in pH or temperature sensor
     */
    bool hasFault() const { return _phSensor.fault || _temperatureSensor.fault; }
    
    /**
     * @brief Check if there's a new message from either sensor
     * @return true if new message available
     */
    bool hasNewMessage() const { return _phSensor.newMessage || _temperatureSensor.newMessage; }
    
    /**
     * @brief Get the message string (prioritizes fault messages)
     * @return Pointer to message string from faulted sensor, or first sensor with message
     */
    const char* getMessage() const {
        if (_phSensor.fault) return _phSensor.message;
        if (_temperatureSensor.fault) return _temperatureSensor.message;
        if (_phSensor.newMessage) return _phSensor.message;
        if (_temperatureSensor.newMessage) return _temperatureSensor.message;
        return "";
    }
    
    /**
     * @brief Clear the message flags from both sensors
     */
    void clearMessages() {
        _phSensor.newMessage = false;
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
    PhSensor_t _phSensor;                    ///< pH sensor data
    TemperatureSensor_t _temperatureSensor;  ///< Temperature sensor data
    uint16_t _phBuffer[10];                  ///< Data buffer for pH Modbus transactions
    uint16_t _tempBuffer[10];                ///< Data buffer for temperature Modbus transactions
    DeviceControl_t _controlObj;             ///< Device control object

    bool _firstConnect;                      ///< Flag to track first successful connection for this pH probe instance
    uint32_t _errCount;                      ///< Consecutive error count
    bool _disconnected;                      ///< Disconnection flag (true when no valid consecutive responses after 5 attempts)
    uint8_t _waitCount;                      ///< Wait counter for reduced requests when disconnected
    
    // Unit tracking for each instance
    uint32_t _phUnitCode;                    ///< pH unit code (for change detection)
    uint32_t _tempUnitCode;                  ///< Temperature unit code (for change detection)
    
    // Static instance registry for callback routing (indexed by slave ID)
    static HamiltonPHProbe* _instances[248];
    
    // Static callback functions for Modbus responses
    static void phResponseHandler(bool valid, uint16_t *data, uint32_t requestId);
    static void temperatureResponseHandler(bool valid, uint16_t *data, uint32_t requestId);
    
    // Instance methods called by static callbacks
    void handlePhResponse(bool valid, uint16_t *data);
    void handleTemperatureResponse(bool valid, uint16_t *data);
};