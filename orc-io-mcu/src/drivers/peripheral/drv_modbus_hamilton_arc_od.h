#pragma once

#include "sys_init.h"

// Forward declarations to avoid circular includes
struct ModbusDriver_t;

/**
 * @brief Hamilton Arc Optical Density Sensor Driver Class
 * 
 * Supports communication with Hamilton Arc OD sensors via Modbus RTU.
 * Reads optical density value and temperature from the sensor.
 * 
 * Register Map (Primary Measurement Channels):
 * - PMC 1 (2089): OD value with units (10 registers)
 * - PMC 6 (2409): Temperature with units (10 registers)
 */
class HamiltonArcOD {
public:
    /**
     * @brief Construct a new Hamilton Arc OD Sensor instance
     * 
     * @param modbusDriver Pointer to the ModbusDriver_t managing the serial port
     * @param slaveID Modbus slave ID of the OD sensor (typically 1-247)
     */
    HamiltonArcOD(ModbusDriver_t *modbusDriver, uint8_t slaveID);
    
    /**
     * @brief Destructor - unregisters instance from callback routing
     */
    ~HamiltonArcOD();
    
    /**
     * @brief Update the OD sensor readings
     * 
     * Call this periodically (e.g., every 2000ms) to request new data from the sensor.
     * This is non-blocking and queues Modbus requests.
     */
    void update();
    
    /**
     * @brief Get the optical density sensor object
     * @return Reference to the OpticalDensitySensor_t object containing OD data
     */
    OpticalDensitySensor_t& getODSensor() { return _odSensor; }
    
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
     * @return true if fault detected in OD or temperature sensor
     */
    bool hasFault() const { return _odSensor.fault || _temperatureSensor.fault; }
    
    /**
     * @brief Check if there's a new message from either sensor
     * @return true if new message available
     */
    bool hasNewMessage() const { return _odSensor.newMessage || _temperatureSensor.newMessage; }
    
    /**
     * @brief Get the message string (prioritizes fault messages)
     * @return Pointer to message string from faulted sensor, or first sensor with message
     */
    const char* getMessage() const {
        if (_odSensor.fault) return _odSensor.message;
        if (_temperatureSensor.fault) return _temperatureSensor.message;
        if (_odSensor.newMessage) return _odSensor.message;
        if (_temperatureSensor.newMessage) return _temperatureSensor.message;
        return "";
    }
    
    /**
     * @brief Clear the message flags from both sensors
     */
    void clearMessages() {
        _odSensor.newMessage = false;
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
    OpticalDensitySensor_t _odSensor;        ///< Optical density sensor data
    TemperatureSensor_t _temperatureSensor;  ///< Temperature sensor data
    DeviceControl_t _controlObj;             ///< Device control object
    uint16_t _dataBuffer[10];                ///< Data buffer for Modbus transactions
    
    // Unit tracking for each instance
    uint32_t _odUnitCode;                    ///< OD unit code (for change detection)
    uint32_t _tempUnitCode;                  ///< Temperature unit code (for change detection)

    bool _firstConnect;                      ///< First connection flag
    
    // Static instance registry for callback routing (indexed by slave ID)
    static HamiltonArcOD* _instances[248];
    
    // Static callback functions for Modbus responses
    static void odResponseHandler(bool valid, uint16_t *data, uint32_t requestId);
    static void temperatureResponseHandler(bool valid, uint16_t *data, uint32_t requestId);
    
    // Instance methods called by static callbacks
    void handleODResponse(bool valid, uint16_t *data);
    void handleTemperatureResponse(bool valid, uint16_t *data);
};
