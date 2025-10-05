#pragma once

#include "sys_init.h"

// Forward declarations to avoid circular includes
struct ModbusDriver_t;

/**
 * @brief Alicat Mass Flow Controller Driver Class
 * 
 * Supports communication with Alicat MFC devices via Modbus RTU.
 * Reads flow, pressure, and setpoint data. Supports setpoint writing with validation.
 * 
 * Register Map (starting at 1349):
 * - 1349: Setpoint (float)
 * - 1351: Valve Drive (float)
 * - 1353: Pressure (float)
 * - 1355: Secondary Pressure (float)
 * - 1357: Barometric Pressure (float)
 * - 1359: Temperature (float)
 * - 1361: Volumetric Flow (float)
 * - 1363: Mass Flow (float)
 */
class AlicatMFC {
public:
    /**
     * @brief Construct a new Alicat MFC instance
     * 
     * @param modbusDriver Pointer to the ModbusDriver_t managing the serial port
     * @param slaveID Modbus slave ID of the MFC (typically 1-247)
     */
    AlicatMFC(ModbusDriver_t *modbusDriver, uint8_t slaveID);
    
    /**
     * @brief Update the MFC readings
     * 
     * Call this periodically (e.g., every 2000ms) to request new data from the MFC.
     * This is non-blocking and queues Modbus requests.
     */
    void update();
    
    /**
     * @brief Write a new setpoint to the MFC
     * 
     * @param setpointDesired flow setpoint (units depend on MFC configuration)
     * @return true if request was queued successfully
     */
    bool writeSetpoint(float setpoint);
    
    /**
     * @brief Get the flow sensor object
     * @return Reference to the FlowSensor_t object containing flow data
     */
    FlowSensor_t& getFlowSensor() { return _flowSensor; }
    
    /**
     * @brief Get the pressure sensor object
     * @return Reference to the PressureSensor_t object containing pressure data
     */
    PressureSensor_t& getPressureSensor() { return _pressureSensor; }
    
    /**
     * @brief Get the current setpoint
     * @return Current setpoint value
     */
    float getSetpoint() const { return _setpoint; }
    
    /**
     * @brief Get the Modbus slave ID
     * @return Slave ID of this MFC
     */
    uint8_t getSlaveID() const { return _slaveID; }
    
    /**
     * @brief Check if there's a fault condition
     * @return true if fault detected
     */
    bool hasFault() const { return _fault; }
    
    /**
     * @brief Check if there's a new message
     * @return true if new message available
     */
    bool hasNewMessage() const { return _newMessage; }
    
    /**
     * @brief Get the message string
     * @return Pointer to message string
     */
    const char* getMessage() const { return _message; }
    
    /**
     * @brief Clear the message flag
     */
    void clearMessage() { _newMessage = false; }
    
private:
    ModbusDriver_t *_modbusDriver;          ///< Pointer to the Modbus driver managing the serial port
    uint8_t _slaveID;                        ///< Modbus slave ID
    FlowSensor_t _flowSensor;                ///< Flow sensor data
    PressureSensor_t _pressureSensor;        ///< Pressure sensor data
    float _setpoint;                         ///< Current setpoint
    bool _fault;                             ///< Fault flag
    bool _newMessage;                        ///< New message flag
    char _message[100];                      ///< Message buffer
    
    uint16_t _dataBuffer[16];                ///< Data buffer for Modbus transactions
    uint16_t _writeBuffer[2];                ///< Write buffer for setpoint writes
    
    // Setpoint write management
    bool _newSetpoint;                       ///< Flag indicating a setpoint write is pending validation
    float _pendingSetpoint;                  ///< Pending setpoint value for validation
    int _writeAttempts;                      ///< Number of write attempts (for retry logic)
    
    // Static callback context pointer (updated before each request)
    static AlicatMFC* _currentInstance;
    
    // Static callback functions for Modbus responses
    static void mfcResponseHandler(bool valid, uint16_t *data);
    static void mfcWriteResponseHandler(bool valid, uint16_t *data);
    
    // Instance methods called by static callbacks
    void handleMfcResponse(bool valid, uint16_t *data);
    void handleWriteResponse(bool valid, uint16_t *data);
};