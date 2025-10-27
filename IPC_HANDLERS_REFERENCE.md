# IPC Handlers Reference

Complete guide to IPC message handlers for device integration.

---

## Overview

IPC (Inter-Processor Communication) handlers process messages between SYS MCU and IO MCU. For device integration, you need to implement:

1. **Sensor Read Handler** - Read control/sensor objects
2. **Device Control Handler** - Execute control commands
3. **Device Config Handler** - Apply configuration changes

---

## File: `orc-io-mcu/src/drivers/ipc/ipc_handlers.cpp`

---

## 1. Sensor Read Handler

Handles `IPC_MSG_SENSOR_READ_REQ` - reads object data for IPC transmission.

### Add Control Object Handler

Add to `ipc_handle_sensor_read_req()` switch statement:

```cpp
case OBJ_T_DEVICE_CONTROL: {
    DeviceControl_t *ctrl = (DeviceControl_t*)obj;
    
    // Primary value: Setpoint
    data.value = ctrl->setpoint;
    strncpy(data.unit, ctrl->setpointUnit, sizeof(data.unit) - 1);
    
    // Additional value: Actual value (if different from setpoint)
    if (ctrl->actualValue != ctrl->setpoint) {
        data.valueCount = 1;
        data.additionalValues[0] = ctrl->actualValue;
        strncpy(data.additionalUnits[0], ctrl->setpointUnit, 
                sizeof(data.additionalUnits[0]) - 1);
    }
    
    // Set flags for status
    if (ctrl->fault) {
        data.flags |= IPC_SENSOR_FLAG_FAULT;
    }
    if (!ctrl->connected) {
        data.flags |= IPC_SENSOR_FLAG_FAULT;  // Treat disconnected as fault
    }
    if (ctrl->newMessage) {
        data.flags |= IPC_SENSOR_FLAG_NEW_MSG;
        strncpy(data.message, ctrl->message, sizeof(data.message) - 1);
    }
    
    break;
}
```

**What this does:**
- Sends setpoint as primary value
- Sends actual value as additional value
- Sets fault flag if device has fault OR is disconnected
- Includes message if available

**Called by:** Background polling on SYS MCU (every 1 second)

---

## 2. Device Control Handler

Handles `IPC_MSG_DEVICE_CONTROL` - executes control commands.

### Required Includes

Add at top of file:
```cpp
#include "../peripheral/drv_modbus_alicat_mfc.h"
#include "../peripheral/drv_modbus_hamilton_ph.h"
// ... add includes for all your devices
```

### Main Handler Function

Already exists - you just add your device case:

```cpp
void ipc_handle_device_control(const uint8_t *payload, uint16_t len) {
    // Parse command
    const IPC_DeviceControlCmd_t* cmd = (const IPC_DeviceControlCmd_t*)payload;
    
    // Validate command structure
    if (len != sizeof(IPC_DeviceControlCmd_t)) {
        char errMsg[100];
        sprintf(errMsg, "Invalid size: got %d, expected %d", 
                len, sizeof(IPC_DeviceControlCmd_t));
        Serial.printf("[IPC] ERROR: %s\n", errMsg);
        ipc_sendError(IPC_ERR_PARSE_FAIL, errMsg);
        return;
    }
    
    // Get control object
    if (cmd->index < 50 || cmd->index >= 70) {
        Serial.printf("[DEV CTRL] ERROR: Invalid control index %d (must be 50-69)\n", 
                     cmd->index);
        ipc_sendError(IPC_ERR_PARAM_INVALID, "Control index must be 50-69");
        return;
    }
    
    ObjIndexEntry* entry = &objIndex[cmd->index];
    if (!entry->inUse || entry->objType != OBJ_T_DEVICE_CONTROL) {
        Serial.printf("[DEV CTRL] ERROR: No control object at index %d\n", cmd->index);
        ipc_sendError(IPC_ERR_PARAM_INVALID, "No control object at index");
        return;
    }
    
    DeviceControl_t* control = (DeviceControl_t*)entry->obj;
    
    // Response variables
    bool success = false;
    uint8_t errorCode = CTRL_ERR_NONE;
    char message[100] = "";
    
    // Dispatch based on command type
    switch(cmd->command) {
        case DEV_CMD_SET_SETPOINT: {
            // ... (see next section)
            break;
        }
        
        case DEV_CMD_RESET_FAULT: {
            // ... (see next section)
            break;
        }
        
        default:
            Serial.printf("[DEV CTRL] ERROR: Unknown command %d\n", cmd->command);
            errorCode = CTRL_ERR_INVALID_CMD;
            strcpy(message, "Unknown device control command");
            break;
    }
    
    // Send acknowledgment
    ipc_sendControlAck_v2(cmd->index, cmd->objectType, cmd->command, 
                         success, errorCode, message);
}
```

### Add Your Device: SET_SETPOINT Command

Add inside `DEV_CMD_SET_SETPOINT` case:

```cpp
case DEV_CMD_SET_SETPOINT: {
    // Find device instance from control index
    ManagedDevice* dev = DeviceManager::findDeviceByControlIndex(cmd->index);
    if (!dev) {
        Serial.printf("[DEV CTRL] ERROR: No device found for control index %d\n", 
                     cmd->index);
        errorCode = CTRL_ERR_INVALID_INDEX;
        strcpy(message, "Device not found");
        break;
    }
    
    // Dispatch to device-specific handler based on device type
    switch(control->deviceType) {
        // ================================================================
        // YOUR DEVICE HANDLER
        // ================================================================
        case IPC_DEV_ALICAT_MFC: {
            Serial.printf("[DEV CTRL] Set MFC setpoint: index=%d, value=%.2f\n", 
                         cmd->index, cmd->setpoint);
            
            // Get device instance
            AlicatMFC* mfc = (AlicatMFC*)dev->deviceInstance;
            if (!mfc) {
                Serial.printf("[DEV CTRL] ERROR: MFC device instance is null\n");
                errorCode = CTRL_ERR_DRIVER_FAULT;
                strcpy(message, "MFC device instance not found");
                break;
            }
            
            // Store setpoint in control object for immediate feedback
            control->setpoint = cmd->setpoint;
            
            // Write setpoint to hardware via driver
            bool writeSuccess = mfc->writeSetpoint(cmd->setpoint);
            if (writeSuccess) {
                success = true;
                snprintf(message, sizeof(message), 
                        "Setpoint %.2f SLPM sent to MFC", cmd->setpoint);
                Serial.printf("[DEV CTRL] MFC setpoint command queued successfully\n");
            } else {
                success = false;
                errorCode = CTRL_ERR_DRIVER_FAULT;
                strcpy(message, "Failed to queue setpoint command");
                Serial.printf("[DEV CTRL] ERROR: Failed to queue MFC setpoint\n");
            }
            break;
        }
        
        // ================================================================
        // ADD MORE DEVICES HERE
        // ================================================================
        case IPC_DEV_PRESSURE_CTRL: {
            // Similar pattern for pressure controller
            PressureController* pc = (PressureController*)dev->deviceInstance;
            // ... validate ...
            control->setpoint = cmd->setpoint;
            if (pc->writeSetpoint(cmd->setpoint)) {
                success = true;
                snprintf(message, sizeof(message), 
                        "Setpoint %.2f PSI sent", cmd->setpoint);
            } else {
                errorCode = CTRL_ERR_DRIVER_FAULT;
                strcpy(message, "Failed to set pressure");
            }
            break;
        }
        
        // ================================================================
        // SENSOR-ONLY DEVICES (DON'T SUPPORT SETPOINT)
        // ================================================================
        case IPC_DEV_HAMILTON_PH:
        case IPC_DEV_HAMILTON_DO:
        case IPC_DEV_HAMILTON_OD: {
            Serial.printf("[DEV CTRL] ERROR: Device type %d does not support setpoint\n", 
                         control->deviceType);
            errorCode = CTRL_ERR_INVALID_CMD;
            strcpy(message, "Device does not support setpoint control");
            break;
        }
        
        default:
            Serial.printf("[DEV CTRL] ERROR: Unknown device type %d\n", 
                         control->deviceType);
            errorCode = CTRL_ERR_TYPE_MISMATCH;
            strcpy(message, "Unknown device type");
            break;
    }
    break;
}
```

### RESET_FAULT Command

Already implemented generically - no device-specific code needed:

```cpp
case DEV_CMD_RESET_FAULT: {
    control->fault = false;
    control->newMessage = false;
    strcpy(control->message, "Fault cleared");
    success = true;
    strcpy(message, "Fault reset");
    Serial.printf("[DEV CTRL] Fault cleared for device at index %d\n", cmd->index);
    break;
}
```

---

## 3. Command Response

Acknowledgment is sent automatically:

```cpp
ipc_sendControlAck_v2(
    cmd->index,        // Control index (50-69)
    cmd->objectType,   // OBJ_T_DEVICE_CONTROL
    cmd->command,      // Command that was executed
    success,           // true if successful
    errorCode,         // Error code if failed
    message            // Human-readable message
);
```

**What happens:**
1. SYS MCU receives acknowledgment
2. Logs success or error
3. Returns HTTP response to Web UI
4. Next poll shows updated values

---

## Error Codes Reference

```cpp
// Success
CTRL_ERR_NONE = 0x00

// Client errors (4xx equivalent)
CTRL_ERR_INVALID_INDEX = 0x01    // Control index not found
CTRL_ERR_TYPE_MISMATCH = 0x02    // Wrong device type for command
CTRL_ERR_INVALID_CMD = 0x03      // Unknown command code
CTRL_ERR_OUT_OF_RANGE = 0x04     // Setpoint outside valid range
CTRL_ERR_NOT_ENABLED = 0x05      // Device not enabled

// Server errors (5xx equivalent)
CTRL_ERR_DRIVER_FAULT = 0x06     // Device driver error
CTRL_ERR_TIMEOUT = 0x07          // Communication timeout
```

**Usage:**
```cpp
// Validation error
if (setpoint < 0 || setpoint > 200) {
    errorCode = CTRL_ERR_OUT_OF_RANGE;
    strcpy(message, "Setpoint must be 0-200 SLPM");
    break;
}

// Driver error
if (!mfc->writeSetpoint(setpoint)) {
    errorCode = CTRL_ERR_DRIVER_FAULT;
    strcpy(message, "Failed to queue command");
    break;
}

// Success
success = true;
errorCode = CTRL_ERR_NONE;
snprintf(message, sizeof(message), "Setpoint %.2f sent", setpoint);
```

---

## Flow Diagram

```
┌─────────────────────────────────────────────────────────────────┐
│ 1. User enters setpoint in Web UI → Clicks "Set"               │
└────────────┬────────────────────────────────────────────────────┘
             │
             ▼
┌─────────────────────────────────────────────────────────────────┐
│ 2. Web UI sends POST /api/device/52/setpoint                   │
│    Body: {"setpoint": 100.0}                                   │
└────────────┬────────────────────────────────────────────────────┘
             │
             ▼
┌─────────────────────────────────────────────────────────────────┐
│ 3. SYS MCU handleSetDeviceSetpoint() creates IPC command       │
│    - Command: DEV_CMD_SET_SETPOINT                             │
│    - Index: 52                                                  │
│    - Value: 100.0                                               │
└────────────┬────────────────────────────────────────────────────┘
             │
             ▼  (IPC_MSG_DEVICE_CONTROL via UART)
┌─────────────────────────────────────────────────────────────────┐
│ 4. IO MCU ipc_handle_device_control() receives command         │
│    - Validates control index (50-69)                           │
│    - Gets control object at index 52                           │
│    - Checks device type                                         │
└────────────┬────────────────────────────────────────────────────┘
             │
             ▼
┌─────────────────────────────────────────────────────────────────┐
│ 5. Dispatch to device handler (IPC_DEV_ALICAT_MFC)            │
│    - Find ManagedDevice by control index                       │
│    - Cast to AlicatMFC* instance                               │
│    - Call mfc->writeSetpoint(100.0)                            │
└────────────┬────────────────────────────────────────────────────┘
             │
             ▼
┌─────────────────────────────────────────────────────────────────┐
│ 6. Driver queues command                                        │
│    - Sets _pendingSetpoint = 100.0                             │
│    - Sets _setpointPending = true                              │
│    - Updates _controlObj.setpoint = 100.0                      │
│    - Returns true                                               │
└────────────┬────────────────────────────────────────────────────┘
             │
             ▼
┌─────────────────────────────────────────────────────────────────┐
│ 7. Send IPC acknowledgment                                      │
│    ipc_sendControlAck_v2(52, success=true, "Setpoint sent")    │
└────────────┬────────────────────────────────────────────────────┘
             │
             ▼  (IPC_MSG_CONTROL_ACK via UART)
┌─────────────────────────────────────────────────────────────────┐
│ 8. SYS MCU receives ACK                                         │
│    - Logs: "Device setpoint set: control[52] = 100.0"          │
│    - Returns HTTP 200 {"success": true}                        │
└────────────┬────────────────────────────────────────────────────┘
             │
             ▼
┌─────────────────────────────────────────────────────────────────┐
│ 9. Web UI receives success response                            │
│    - Shows success toast                                        │
│    - Next poll (2 sec) shows updated values                    │
└─────────────────────────────────────────────────────────────────┘

Meanwhile on IO MCU:
┌─────────────────────────────────────────────────────────────────┐
│ 10. Scheduler calls mfc->update() (next 2 sec cycle)           │
│     - State machine sees _setpointPending = true               │
│     - Transitions to WRITING_SETPOINT                          │
│     - Sends Modbus write to hardware                           │
│     - Waits for confirmation                                    │
│     - Updates _controlObj.actualValue when confirmed           │
└─────────────────────────────────────────────────────────────────┘
```

---

## Testing Serial Output

### Normal Operation

```
[DEV CTRL] Set MFC setpoint: index=52, value=100.00
[DEV CTRL] MFC setpoint command queued successfully
[IPC] Control ACK sent: index=52, success=1
[ALICAT MFC] Setpoint write sent: 100.00 SLPM
[ALICAT MFC] Flow: 99.8 SLPM, Temp: 23.5 °C
```

### Device Not Found

```
[DEV CTRL] Set MFC setpoint: index=52, value=100.00
[DEV CTRL] ERROR: No device found for control index 52
[IPC] Control ACK sent: index=52, success=0, error=0x01
```

### Driver Failure

```
[DEV CTRL] Set MFC setpoint: index=52, value=100.00
[ALICAT MFC] Invalid setpoint: 250.00 (range: 0-200)
[DEV CTRL] ERROR: Failed to queue MFC setpoint command
[IPC] Control ACK sent: index=52, success=0, error=0x06
```

### Communication Timeout

```
[ALICAT MFC] Setpoint write sent: 100.00 SLPM
[ALICAT MFC] Timeout waiting for response
[ALICAT MFC] FAULT: Communication timeout (3 retries)
[IPC] Sensor data sent: index=52, fault=1
```

---

## Complete Handler Template

```cpp
case IPC_DEV_YOUR_DEVICE: {
    Serial.printf("[DEV CTRL] Set device setpoint: index=%d, value=%.2f\n", 
                 cmd->index, cmd->setpoint);
    
    // 1. Get device instance
    YourDevice* device = (YourDevice*)dev->deviceInstance;
    if (!device) {
        Serial.printf("[DEV CTRL] ERROR: Device instance is null\n");
        errorCode = CTRL_ERR_DRIVER_FAULT;
        strcpy(message, "Device instance not found");
        break;
    }
    
    // 2. Validate setpoint range (optional but recommended)
    if (cmd->setpoint < MIN_VALUE || cmd->setpoint > MAX_VALUE) {
        Serial.printf("[DEV CTRL] ERROR: Setpoint out of range: %.2f\n", 
                     cmd->setpoint);
        errorCode = CTRL_ERR_OUT_OF_RANGE;
        snprintf(message, sizeof(message), 
                "Setpoint must be %.0f-%.0f %s", 
                MIN_VALUE, MAX_VALUE, control->setpointUnit);
        break;
    }
    
    // 3. Update control object immediately for UI feedback
    control->setpoint = cmd->setpoint;
    
    // 4. Call device driver method
    bool writeSuccess = device->writeSetpoint(cmd->setpoint);
    
    // 5. Set response based on result
    if (writeSuccess) {
        success = true;
        snprintf(message, sizeof(message), 
                "Setpoint %.2f %s sent to device", 
                cmd->setpoint, control->setpointUnit);
        Serial.printf("[DEV CTRL] Device setpoint command queued successfully\n");
    } else {
        success = false;
        errorCode = CTRL_ERR_DRIVER_FAULT;
        strcpy(message, "Failed to queue setpoint command");
        Serial.printf("[DEV CTRL] ERROR: Failed to queue setpoint\n");
    }
    break;
}
```

---

## Common Mistakes to Avoid

### 1. ❌ Calling device method without validation
```cpp
// BAD - will crash if device is null
AlicatMFC* mfc = (AlicatMFC*)dev->deviceInstance;
mfc->writeSetpoint(cmd->setpoint);  // CRASH!
```

```cpp
// GOOD - check for null first
AlicatMFC* mfc = (AlicatMFC*)dev->deviceInstance;
if (!mfc) {
    errorCode = CTRL_ERR_DRIVER_FAULT;
    strcpy(message, "Device instance not found");
    break;
}
bool success = mfc->writeSetpoint(cmd->setpoint);
```

### 2. ❌ Forgetting to update control object
```cpp
// BAD - UI won't show new setpoint until next device update
mfc->writeSetpoint(cmd->setpoint);
```

```cpp
// GOOD - UI gets immediate feedback
control->setpoint = cmd->setpoint;  // Update first!
mfc->writeSetpoint(cmd->setpoint);
```

### 3. ❌ Wrong error code for situation
```cpp
// BAD - misleading error
if (!mfc) {
    errorCode = CTRL_ERR_TIMEOUT;  // Not a timeout!
}
```

```cpp
// GOOD - correct error code
if (!mfc) {
    errorCode = CTRL_ERR_DRIVER_FAULT;  // Driver issue
}
if (setpoint > 200) {
    errorCode = CTRL_ERR_OUT_OF_RANGE;  // Validation issue
}
```

### 4. ❌ Not including device header
```cpp
// BAD - compile error
AlicatMFC* mfc = (AlicatMFC*)dev->deviceInstance;
// error: 'AlicatMFC' was not declared in this scope
```

```cpp
// GOOD - include at top of file
#include "../peripheral/drv_modbus_alicat_mfc.h"
```

---

## Checklist

Before testing your IPC handlers:

- [ ] Added device header include at top of file
- [ ] Added `OBJ_T_DEVICE_CONTROL` case in sensor read handler
- [ ] Added device case in `DEV_CMD_SET_SETPOINT` handler
- [ ] Validated device instance pointer before use
- [ ] Updated control object setpoint immediately
- [ ] Called device `writeSetpoint()` method
- [ ] Set appropriate error codes
- [ ] Provided descriptive messages
- [ ] Added serial logging for debugging
- [ ] Tested with actual hardware or simulator

---

**With these handlers in place, your device is fully integrated into the IPC control system!**
