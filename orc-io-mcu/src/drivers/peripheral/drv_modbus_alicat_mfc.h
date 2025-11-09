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
 * 
 * Units register addresses - all uint32 type (two registers each)
 * - 1649: Setpoint unit
 * - 1673: Pressure unit
 * - 1709: Temperature unit
 * - 1721: Volumetric Flow unit
 * - 1733: Mass Flow unit
 */

// Alicat units array - static to avoid multiple definition
static const char* alicatFlowUnits[64] = {
    "", "---", "SµL/m", "SmL/s", "SmL/m", "SmL/h", "SL/s", "SLPM",
    "SL/h", "SCCS", "", "", "SCCM", "Scm³/h", "Sm³/m", "Sm³/h",
    "Sm³/d", "Sin³/m", "SCFM", "SCFH", "kSCFM", "SCFD", "", "",
    "", "", "", "", "", "", "", "",
    "NµL/m", "NmL/s", "NmL/m", "NmL/h", "NL/s", "NLPM", "NL/h", "",
    "", "NCCS", "NCCM", "Ncm³/h", "Nm³/m", "Nm³/h", "Nm³/d", "",
    "", "", "", "", "", "", "", "",
    "", "", "", "", "", "", "Count", "%"
};

static const char* alicatPressureUnits[64] = {
    "", "---", "Pa", "hPa", "kPa", "MPa", "mbar", "bar",
    "g/cm²", "kg/cm²", "PSI", "PSF", "mTorr", "torr", "mmHg", "inHg",
    "mmH₂0", "mmH₂0", "cmH₂0", "cmH₂0", "inH₂0", "inH₂0", "atm", "",
    "", "", "", "", "", "", "", "",
    "", "", "", "", "", "", "", "",
    "", "", "", "", "", "", "", "",
    "", "", "", "", "", "", "", "",
    "", "", "", "", "", "V", "Count", "%"
};

static const char* alicatTempUnits[6] __attribute__((unused)) = {
    "", "---", "°C", "°F", "°K", "°Ra"
};

/**
 * @brief Get Alicat flow unit string from unit code
 * 
 * @param unitCode Unit code (index into alicatFlowUnits array, 0-63)
 * @return const char* Pointer to unit string, or "?" if invalid
 */
inline const char* getAlicatFlowUnit(uint16_t unitCode) {
    if (unitCode < 64) {
        return alicatFlowUnits[unitCode];
    }
    return "?";  // Default if code is out of range
}

/**
 * @brief Get Alicat pressure unit string from unit code
 * 
 * @param unitCode Unit code (index into alicatPressureUnits array, 0-63)
 * @return const char* Pointer to unit string, or "?" if invalid
 */
inline const char* getAlicatPressureUnit(uint16_t unitCode) {
    if (unitCode < 64) {
        return alicatPressureUnits[unitCode];
    }
    return "?";  // Default if code is out of range
}

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
     * @brief Get the setpoint unit string
     * @return Pointer to setpoint unit string
     */
    const char* getSetpointUnit() const { return _setpointUnit; }
    
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
    
    /**
     * @brief Get the device control object
     * @return Pointer to the DeviceControl_t object for this device
     */
    DeviceControl_t* getControlObject() { return &_controlObj; }
    
    /**
     * @brief Set the maximum flow rate for this MFC
     * @param maxFlowRate Maximum flow rate in mL/min
     */
    void setMaxFlowRate(float maxFlowRate) { _maxFlowRate_mL_min = maxFlowRate; }
    
    /**
     * @brief Get the maximum flow rate for this MFC
     * @return Maximum flow rate in mL/min
     */
    float getMaxFlowRate() const { return _maxFlowRate_mL_min; }
    
private:
    ModbusDriver_t *_modbusDriver;          ///< Pointer to the Modbus driver managing the serial port
    uint8_t _slaveID;                        ///< Modbus slave ID
    FlowSensor_t _flowSensor;                ///< Flow sensor data
    PressureSensor_t _pressureSensor;        ///< Pressure sensor data
    DeviceControl_t _controlObj;             ///< Device control object (indices 50-69)
    float _setpoint;                         ///< Current setpoint
    char _setpointUnit[10];                  ///< Setpoint unit string
    float _maxFlowRate_mL_min;               ///< Maximum flow rate capability (mL/min)
    bool _fault;                             ///< Fault flag
    bool _newMessage;                        ///< New message flag
    char _message[100];                      ///< Message buffer
    
    uint16_t _dataBuffer[16];                ///< Data buffer for Modbus transactions
    uint16_t _writeBuffer[2];                ///< Write buffer for setpoint writes
    
    // Setpoint write management
    bool _newSetpoint;                       ///< Flag indicating a setpoint write is pending validation
    float _pendingSetpoint;                  ///< Pending setpoint value for validation
    int _writeAttempts;                      ///< Number of write attempts (for retry logic)
    
    // Unit tracking for each instance
    uint16_t _setpointUnitCode;              ///< Setpoint unit code (for change detection)
    uint16_t _flowUnitCode;                  ///< Flow unit code (for change detection)
    uint16_t _pressureUnitCode;              ///< Pressure unit code (for change detection)
    uint16_t _unitBuffer[3];                 ///< Buffer for unit read requests (3 units x 1 reg each)
    
    // Static callback context pointer (updated before each request)
    static AlicatMFC* _currentInstance;
    
    // Static callback functions for Modbus responses
    static void mfcResponseHandler(bool valid, uint16_t *data);
    static void mfcWriteResponseHandler(bool valid, uint16_t *data);
    static void unitsResponseHandler(bool valid, uint16_t *data);
    
    // Instance methods called by static callbacks
    void handleMfcResponse(bool valid, uint16_t *data);
    void handleWriteResponse(bool valid, uint16_t *data);
    void handleUnitsResponse(bool valid, uint16_t *data);
};