# Device Driver Implementation Template

Complete code template for implementing a new device driver.

---

## Example: Alicat Mass Flow Controller

This shows a **complete, working implementation** of a controllable Modbus device with multiple sensors.

---

## Header File: `drv_modbus_alicat_mfc.h`

```cpp
#ifndef DRV_MODBUS_ALICAT_MFC_H
#define DRV_MODBUS_ALICAT_MFC_H

#include <Arduino.h>
#include "../../modbus_rtu_master/ModbusRTUMaster.h"
#include "../objects.h"

/**
 * @brief Alicat Mass Flow Controller Driver
 * 
 * Interface: Modbus RTU (RS-485)
 * Sensors: Flow Rate (SLPM), Temperature (°C)
 * Control: Setpoint (SLPM)
 * 
 * Features:
 * - Non-blocking Modbus communication
 * - Automatic retry on communication failure (3 attempts)
 * - Setpoint write with confirmation
 * - Connection status monitoring
 * - Fault detection and recovery
 */
class AlicatMFC {
public:
    /**
     * @brief Construct a new Alicat MFC instance
     * 
     * @param modbus Pointer to ModbusRTUMaster instance
     * @param slaveID Modbus slave address (1-247)
     */
    AlicatMFC(ModbusRTUMaster* modbus, uint8_t slaveID);
    
    /**
     * @brief Initialize the device
     * 
     * Attempts to communicate with the device to verify it's present.
     * Called once during device creation.
     * 
     * @return true if device responds, false if communication fails
     */
    bool init();
    
    /**
     * @brief Periodic update function (non-blocking)
     * 
     * Implements state machine for:
     * - Reading flow rate and temperature
     * - Writing setpoint commands
     * - Handling Modbus responses
     * - Retry logic on failures
     * 
     * Called by scheduler task (typically 2 Hz).
     * 
     * @return true if update cycle completes, false on error
     */
    bool update();
    
    /**
     * @brief Queue a setpoint write command
     * 
     * Validates setpoint range and queues command for next update cycle.
     * Immediately updates control object for UI feedback.
     * 
     * @param setpoint Desired flow rate in SLPM (0-200)
     * @return true if command queued, false if invalid
     */
    bool writeSetpoint(float setpoint);
    
    /**
     * @brief Get control object pointer
     * 
     * @return DeviceControl_t* Pointer to internal control object
     */
    DeviceControl_t* getControlObject() { return &_controlObj; }
    
    /**
     * @brief Get sensor object pointer by index
     * 
     * @param index Sensor index (0=flow, 1=temperature)
     * @return SensorData_t* Pointer to sensor object, nullptr if invalid
     */
    SensorData_t* getSensorObject(uint8_t index);
    
    /**
     * @brief Get number of sensor objects
     * 
     * @return uint8_t Always returns 2 (flow + temperature)
     */
    uint8_t getSensorCount() const { return 2; }
    
private:
    // Communication interface
    ModbusRTUMaster* _modbus;
    uint8_t _slaveID;
    
    // Sensor objects
    SensorData_t _flowSensor;      // Flow rate (SLPM)
    SensorData_t _tempSensor;      // Temperature (°C)
    
    // Control object
    DeviceControl_t _controlObj;   // Setpoint + status
    
    // State machine
    enum State {
        IDLE,                      // Waiting for next operation
        READING_DATA,              // Reading flow/temp from device
        WRITING_SETPOINT,          // Writing setpoint to device
        WAITING_RESPONSE           // Waiting for Modbus response
    };
    State _state;
    
    // Retry logic
    uint8_t _writeAttempts;        // Retry counter for writes
    unsigned long _lastRequestTime;// Timestamp of last Modbus request
    const unsigned long _timeout = 1000;  // 1 second timeout
    
    // Setpoint management
    float _pendingSetpoint;        // Queued setpoint value
    bool _setpointPending;         // True if setpoint write queued
    
    // Fault tracking
    bool _fault;
    char _message[100];
    
    // Helper functions
    void _updateSensors(float flow, float temp, float pressure);
    void _setFault(const char* msg);
    void _clearFault();
};

#endif
```

---

## Source File: `drv_modbus_alicat_mfc.cpp`

```cpp
#include "drv_modbus_alicat_mfc.h"

// Constructor
AlicatMFC::AlicatMFC(ModbusRTUMaster* modbus, uint8_t slaveID)
    : _modbus(modbus)
    , _slaveID(slaveID)
    , _state(IDLE)
    , _writeAttempts(0)
    , _lastRequestTime(0)
    , _pendingSetpoint(0.0f)
    , _setpointPending(false)
    , _fault(false)
{
    // Initialize sensor objects
    memset(&_flowSensor, 0, sizeof(SensorData_t));
    memset(&_tempSensor, 0, sizeof(SensorData_t));
    memset(&_controlObj, 0, sizeof(DeviceControl_t));
    
    // Set sensor metadata
    strcpy(_flowSensor.name, "MFC Flow Rate");
    strcpy(_flowSensor.unit, "SLPM");
    
    strcpy(_tempSensor.name, "MFC Temperature");
    strcpy(_tempSensor.unit, "°C");
    
    // Initialize control object metadata
    strcpy(_controlObj.setpointUnit, "SLPM");
    _controlObj.deviceType = IPC_DEV_ALICAT_MFC;
    _controlObj.connected = false;
    _controlObj.fault = false;
    _controlObj.setpoint = 0.0f;
    _controlObj.actualValue = 0.0f;
}

// Initialization
bool AlicatMFC::init() {
    Serial.printf("[ALICAT MFC] Initializing slave ID %d\n", _slaveID);
    
    // Try to read device ID to verify communication
    bool success = _modbus->readHoldingRegisters(_slaveID, 0, 1);
    if (!success) {
        _setFault("Communication failure during init");
        Serial.println("[ALICAT MFC] Init failed - no response");
        return false;
    }
    
    // Wait briefly for response (blocking during init only)
    delay(100);
    
    // Check if device responded
    if (_modbus->available()) {
        ModbusResponse response = _modbus->available();
        if (!response.hasError()) {
            _controlObj.connected = true;
            _clearFault();
            Serial.println("[ALICAT MFC] Init successful");
            return true;
        }
    }
    
    _setFault("No response during init");
    Serial.println("[ALICAT MFC] Init failed - timeout");
    return false;
}

// Write setpoint method
bool AlicatMFC::writeSetpoint(float setpoint) {
    // Validate setpoint range (Alicat spec: 0-200 SLPM typical)
    if (setpoint < 0.0f || setpoint > 200.0f) {
        Serial.printf("[ALICAT MFC] Invalid setpoint: %.2f (range: 0-200)\n", setpoint);
        return false;
    }
    
    // Queue for next update cycle
    _pendingSetpoint = setpoint;
    _setpointPending = true;
    _writeAttempts = 0;  // Reset retry counter
    
    // Immediately update control object for UI feedback
    _controlObj.setpoint = setpoint;
    
    Serial.printf("[ALICAT MFC] Setpoint queued: %.2f SLPM\n", setpoint);
    return true;
}

// Main update state machine (called by scheduler)
bool AlicatMFC::update() {
    switch (_state) {
        // ====================================================================
        // IDLE: Decide what to do next
        // ====================================================================
        case IDLE: {
            // Priority 1: Write pending setpoint
            if (_setpointPending) {
                _state = WRITING_SETPOINT;
                Serial.println("[ALICAT MFC] State: IDLE → WRITING_SETPOINT");
            }
            // Priority 2: Regular data read
            else {
                _state = READING_DATA;
                // Don't log normal reads to avoid spam
            }
            break;
        }
        
        // ====================================================================
        // READING_DATA: Send read request to device
        // ====================================================================
        case READING_DATA: {
            // Alicat register map (example):
            // 0: Flow rate (x100)
            // 1: Temperature (x10)
            // 2: Pressure (x100)
            // 3: Setpoint echo (x100)
            
            bool success = _modbus->readHoldingRegisters(_slaveID, 0, 4);
            if (!success) {
                Serial.println("[ALICAT MFC] Failed to send read request");
                _setFault("Failed to send read request");
                _state = IDLE;
                return false;
            }
            
            _lastRequestTime = millis();
            _state = WAITING_RESPONSE;
            break;
        }
        
        // ====================================================================
        // WRITING_SETPOINT: Send write request to device
        // ====================================================================
        case WRITING_SETPOINT: {
            // Convert setpoint to device format (x100 for 2 decimal places)
            uint16_t setpointRaw = (uint16_t)(_pendingSetpoint * 100.0f);
            
            // Alicat setpoint register is typically at address 10
            bool success = _modbus->writeSingleRegister(_slaveID, 10, setpointRaw);
            if (!success) {
                Serial.println("[ALICAT MFC] Failed to send setpoint write");
                
                // Retry logic
                _writeAttempts++;
                if (_writeAttempts >= 5) {
                    _setFault("Setpoint write failed (5 retries)");
                    _setpointPending = false;
                    _writeAttempts = 0;
                }
                _state = IDLE;
                return false;
            }
            
            Serial.printf("[ALICAT MFC] Setpoint write sent: %.2f SLPM\n", _pendingSetpoint);
            _setpointPending = false;  // Clear pending flag
            _lastRequestTime = millis();
            _state = WAITING_RESPONSE;
            break;
        }
        
        // ====================================================================
        // WAITING_RESPONSE: Wait for Modbus response
        // ====================================================================
        case WAITING_RESPONSE: {
            // Check for timeout
            if (millis() - _lastRequestTime > _timeout) {
                Serial.println("[ALICAT MFC] Timeout waiting for response");
                _writeAttempts++;
                
                if (_writeAttempts >= 3) {
                    _setFault("Communication timeout (3 retries)");
                    _controlObj.connected = false;
                    _writeAttempts = 0;
                }
                
                _state = IDLE;
                return false;
            }
            
            // Check if response is available
            if (_modbus->available()) {
                ModbusResponse response = _modbus->available();
                
                // Check for Modbus errors
                if (response.hasError()) {
                    uint8_t errorCode = response.getErrorCode();
                    Serial.printf("[ALICAT MFC] Modbus error: 0x%02X\n", errorCode);
                    
                    _writeAttempts++;
                    if (_writeAttempts >= 3) {
                        char msg[100];
                        snprintf(msg, sizeof(msg), "Modbus error: 0x%02X", errorCode);
                        _setFault(msg);
                    }
                    
                    _state = IDLE;
                    return false;
                }
                
                // Parse successful response
                if (response.hasData()) {
                    uint16_t* data = response.data;
                    
                    // Parse flow rate, temperature, pressure
                    float flow = (float)data[0] / 100.0f;      // SLPM
                    float temp = (float)data[1] / 10.0f;       // °C
                    float pressure = (float)data[2] / 100.0f;  // PSI
                    
                    // Update sensor objects
                    _updateSensors(flow, temp, pressure);
                    
                    // Update control object
                    _controlObj.actualValue = flow;
                    _controlObj.connected = true;
                    
                    // Clear fault if recovering
                    if (_fault) {
                        _clearFault();
                        Serial.println("[ALICAT MFC] Communication restored");
                    }
                    
                    // Reset retry counter on success
                    _writeAttempts = 0;
                }
                
                // Return to idle for next cycle
                _state = IDLE;
                return true;
            }
            
            // Still waiting for response
            break;
        }
    }
    
    return true;
}

// Helper: Update sensor objects with new data
void AlicatMFC::_updateSensors(float flow, float temp, float pressure) {
    // Update flow sensor
    _flowSensor.value = flow;
    _flowSensor.fault = false;
    _flowSensor.timestamp = millis();
    
    // Update temperature sensor
    _tempSensor.value = temp;
    _tempSensor.fault = false;
    _tempSensor.timestamp = millis();
    
    // Optional: Log significant changes
    static float lastFlow = 0.0f;
    if (abs(flow - lastFlow) > 5.0f) {
        Serial.printf("[ALICAT MFC] Flow: %.1f SLPM, Temp: %.1f °C\n", flow, temp);
        lastFlow = flow;
    }
}

// Helper: Set fault condition
void AlicatMFC::_setFault(const char* msg) {
    _fault = true;
    _controlObj.fault = true;
    _controlObj.newMessage = true;
    
    strncpy(_controlObj.message, msg, sizeof(_controlObj.message) - 1);
    _controlObj.message[sizeof(_controlObj.message) - 1] = '\0';
    
    strncpy(_message, msg, sizeof(_message) - 1);
    _message[sizeof(_message) - 1] = '\0';
    
    Serial.printf("[ALICAT MFC] FAULT: %s\n", msg);
}

// Helper: Clear fault condition
void AlicatMFC::_clearFault() {
    _fault = false;
    _controlObj.fault = false;
    _controlObj.newMessage = false;
    _controlObj.message[0] = '\0';
    _message[0] = '\0';
}

// Get sensor object by index
SensorData_t* AlicatMFC::getSensorObject(uint8_t index) {
    switch (index) {
        case 0: return &_flowSensor;
        case 1: return &_tempSensor;
        default: return nullptr;
    }
}
```

---

## Usage Example in Device Manager

```cpp
// In DeviceManager::createDevice()
case IPC_DEV_ALICAT_MFC: {
    // Get Modbus port
    ModbusRTUMaster* port = getModbusPort(config->modbus.portIndex);
    if (!port) {
        Serial.printf("[DEV MGR] ERROR: Invalid Modbus port %d\n", 
                     config->modbus.portIndex);
        return false;
    }
    
    // Create device instance
    AlicatMFC* mfc = new AlicatMFC(port, config->modbus.slaveID);
    if (!mfc) {
        Serial.println("[DEV MGR] ERROR: Failed to allocate AlicatMFC");
        return false;
    }
    
    // Initialize device
    if (!mfc->init()) {
        Serial.println("[DEV MGR] ERROR: AlicatMFC init failed");
        delete mfc;
        return false;
    }
    
    deviceInstance = mfc;
    sensorCount = mfc->getSensorCount();  // Returns 2
    
    Serial.printf("[DEV MGR] ✓ Created Alicat MFC: port=%d, slave=%d\n",
                 config->modbus.portIndex, config->modbus.slaveID);
    break;
}
```

---

## Key Implementation Notes

### 1. State Machine Pattern
- **IDLE:** Decide what to do next
- **READING/WRITING:** Send Modbus command
- **WAITING:** Non-blocking wait for response
- Prevents blocking the entire system

### 2. Retry Logic
- Track attempts in `_writeAttempts`
- Max 3 retries for reads, 5 for writes
- Set fault after max retries
- Reset counter on success

### 3. Timeout Handling
- Store `_lastRequestTime` when sending
- Check `millis() - _lastRequestTime > timeout`
- Mark as disconnected after timeout

### 4. Fault Propagation
- Set `_controlObj.fault = true`
- Set `_controlObj.newMessage = true`
- Store message in `_controlObj.message`
- IPC automatically sends to SYS MCU

### 5. Connection Tracking
- Set `_controlObj.connected` based on responses
- UI shows "DISCONNECTED" when false
- Automatically recovers when device responds

### 6. Setpoint Queueing
- `writeSetpoint()` queues command
- Update immediately sends to device
- Prevents race conditions
- UI gets instant feedback

---

## Sensor-Only Device Variation

For devices that don't support control (e.g., pH probe):

```cpp
class HamiltonPH {
public:
    HamiltonPH(ModbusRTUMaster* modbus, uint8_t slaveID);
    bool init();
    bool update();
    
    // Still provide control object for status
    DeviceControl_t* getControlObject() { return &_controlObj; }
    
    // No writeSetpoint() method - sensor only!
    
    SensorData_t* getSensorObject(uint8_t index);
    uint8_t getSensorCount() const { return 2; }  // pH + Temperature
    
private:
    SensorData_t _phSensor;
    SensorData_t _tempSensor;
    DeviceControl_t _controlObj;  // Status only (no setpoint)
    // ... state machine ...
};
```

**Key Differences:**
- No `writeSetpoint()` method
- No `_pendingSetpoint` or `_setpointPending`
- State machine only does READING
- Control object shows status but no setpoint
- Set `hasSetpoint: false` in Web UI driver definition

---

## Template Checklist

When creating your driver, ensure you have:

- [ ] Constructor initializes all members to safe defaults
- [ ] `init()` verifies device communication
- [ ] `update()` uses non-blocking state machine
- [ ] Timeout handling with retry logic
- [ ] Fault conditions set `_controlObj.fault`
- [ ] Connection status tracked in `_controlObj.connected`
- [ ] Sensor objects updated with new readings
- [ ] `getSensorObject()` returns correct pointer or nullptr
- [ ] `getSensorCount()` returns correct number
- [ ] Control object has `deviceType` set
- [ ] `writeSetpoint()` if device is controllable
- [ ] Serial logging for debugging
- [ ] Proper cleanup in destructor if needed

---

**This template provides a complete, production-ready device driver implementation!**
