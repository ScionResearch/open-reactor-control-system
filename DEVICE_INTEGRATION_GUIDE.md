# Device Integration Guide

**Version:** 1.0  
**Last Updated:** October 2024  
**Purpose:** Complete reference for integrating new peripheral devices into the Open Reactor Control System

---

## Table of Contents

1. [Overview](#overview)
2. [Architecture Summary](#architecture-summary)
3. [Integration Checklist](#integration-checklist)
4. [Part 1: Device Driver (IO MCU)](#part-1-device-driver-io-mcu)
5. [Part 2: Device Manager (IO MCU)](#part-2-device-manager-io-mcu)
6. [Part 3: IPC Handlers (IO MCU)](#part-3-ipc-handlers-io-mcu)
7. [Part 4: Configuration (SYS MCU)](#part-4-configuration-sys-mcu)
8. [Part 5: REST API (SYS MCU)](#part-5-rest-api-sys-mcu)
9. [Part 6: Web UI](#part-6-web-ui)
10. [Testing](#testing)

---

## Overview

The Open Reactor Control System uses a **dual-MCU architecture** with a **dual-index device model**:

- **IO MCU (SAME51):** Hardware communication (Modbus, I2C, analog)
- **SYS MCU (RP2040):** Network, storage, UI
- **IPC Protocol:** UART inter-processor communication
- **Dual-Index:**
  - Control Objects (50-69): Device status & setpoints
  - Sensor Objects (70-99): Sensor readings

### Example: Alicat MFC
- Control Index: 52 (setpoint, actual, connected, fault)
- Sensor Indices: 72 (flow), 73 (temp)
- Interface: Modbus RTU, Slave ID 1

---

## Architecture Summary

```
Web UI → REST API → SYS MCU → IPC → IO MCU → Device Hardware

Object Index System:
├── 0-7    : ADC inputs
├── 8-9    : DAC outputs  
├── 10-12  : RTD sensors
├── 13-20  : GPIO pins
├── 21-25  : Digital outputs
├── 26-30  : Motors
├── 31-32  : Power monitors
├── 37-40  : COM ports
├── 50-69  : Device control objects ← Your device control
└── 70-99  : Device sensor objects  ← Your device sensors
```

---

## Integration Checklist

### Phase 1: Device Driver (IO MCU)
- [ ] Create driver header `drv_<interface>_<device>.h`
- [ ] Implement driver source `drv_<interface>_<device>.cpp`
- [ ] Add init() method
- [ ] Add update() state machine
- [ ] Add writeSetpoint() if controllable
- [ ] Add getControlObject() and getSensorObject()
- [ ] Add device type to `objects.h` and `ipc_protocol.h`

### Phase 2: Device Manager (IO MCU)
- [ ] Add factory case in DeviceManager::createDevice()
- [ ] Register control object (index 50-69)
- [ ] Register sensor objects (index 70-99)
- [ ] Create scheduler task for update()
- [ ] Store ManagedDevice entry
- [ ] Add delete case in DeviceManager::deleteDevice()

### Phase 3: IPC Handlers (IO MCU)
- [ ] Add OBJ_T_DEVICE_CONTROL handler in sensor read
- [ ] Add device case in ipc_handle_device_control()
- [ ] Implement DEV_CMD_SET_SETPOINT handler
- [ ] Implement DEV_CMD_RESET_FAULT handler
- [ ] Test with serial monitor

### Phase 4: Configuration (SYS MCU)
- [ ] Add driver definition to DRIVER_TYPES
- [ ] Update ioConfig structures if needed
- [ ] Add driver to Web UI dropdown

### Phase 5: REST API (SYS MCU)
- [ ] Verify /api/devices returns control data
- [ ] Test POST /api/device/{index}/setpoint
- [ ] Test POST /api/device/{index}/fault/reset

### Phase 6: Web UI
- [ ] Add driver to DRIVER_DEFINITIONS with hasSetpoint flag
- [ ] Test device card rendering
- [ ] Test setpoint control
- [ ] Test fault display

---

## Part 1: Device Driver (IO MCU)

See: `DEVICE_DRIVER_TEMPLATE.md` for complete code examples

### Key Files
- `orc-io-mcu/src/drivers/peripheral/drv_<interface>_<device>.h`
- `orc-io-mcu/src/drivers/peripheral/drv_<interface>_<device>.cpp`

### Required Methods

```cpp
class YourDevice {
public:
    YourDevice(InterfaceType* bus, uint8_t address);
    bool init();                              // One-time initialization
    bool update();                            // Periodic state machine
    bool writeSetpoint(float value);          // For controllable devices
    DeviceControl_t* getControlObject();      // Return control object
    SensorData_t* getSensorObject(uint8_t i); // Return sensor i
    uint8_t getSensorCount() const;           // How many sensors
    
private:
    DeviceControl_t _controlObj;              // REQUIRED for all devices
    SensorData_t _sensors[4];                 // Up to 4 sensors
    // ... your state machine variables
};
```

### State Machine Pattern

```cpp
enum State { IDLE, READING, WRITING, WAITING };

bool YourDevice::update() {
    switch (_state) {
        case IDLE:
            if (_setpointPending) {
                _state = WRITING;
            } else {
                _state = READING;
            }
            break;
            
        case READING:
            _bus->readRegisters(_addr, 0, 4);
            _state = WAITING;
            break;
            
        case WRITING:
            _bus->writeRegister(_addr, 10, _pendingSetpoint);
            _setpointPending = false;
            _state = WAITING;
            break;
            
        case WAITING:
            if (timeout) {
                _setFault("Timeout");
                _state = IDLE;
            } else if (response_ready) {
                _parseResponse();
                _state = IDLE;
            }
            break;
    }
    return true;
}
```

### Object Registration
Add to `orc-io-mcu/src/drivers/objects.h`:
```cpp
enum IPC_DeviceType {
    // ... existing ...
    IPC_DEV_YOUR_DEVICE = 10,  // Pick next available number
};
```

---

## Part 2: Device Manager (IO MCU)

See: `DEVICE_MANAGER_INTEGRATION.md` for details

### File: `orc-io-mcu/src/drivers/device_manager.cpp`

Add to `createDevice()`:
```cpp
case IPC_DEV_YOUR_DEVICE: {
    // Get interface (Modbus/I2C/etc)
    InterfaceType* bus = getInterface(config);
    
    // Create instance
    YourDevice* dev = new YourDevice(bus, config->address);
    if (!dev || !dev->init()) {
        delete dev;
        return false;
    }
    
    deviceInstance = dev;
    sensorCount = dev->getSensorCount();
    break;
}
```

Register objects:
```cpp
// Control object (index startIndex - 20)
uint8_t controlIdx = startIndex - 20;
registerObject(controlIdx, OBJ_T_DEVICE_CONTROL, 
               dev->getControlObject(), name, unit);

// Sensor objects (indices startIndex, startIndex+1, ...)
for (uint8_t i = 0; i < sensorCount; i++) {
    registerObject(startIndex + i, OBJ_T_SENSOR,
                  dev->getSensorObject(i), sensorName, unit);
}
```

Create scheduler task:
```cpp
ScheduledTask* task = scheduler.addTask(
    "DEV_72", 2000,  // 2 second interval
    [](void* ctx) { ((YourDevice*)ctx)->update(); },
    deviceInstance
);
task->start();
```

---

## Part 3: IPC Handlers (IO MCU)

See: `IPC_HANDLERS_GUIDE.md` for details

### File: `orc-io-mcu/src/drivers/ipc/ipc_handlers.cpp`

Add include:
```cpp
#include "../peripheral/drv_<your_device>.h"
```

Add sensor read handler:
```cpp
case OBJ_T_DEVICE_CONTROL: {
    DeviceControl_t *ctrl = (DeviceControl_t*)obj;
    data.value = ctrl->setpoint;
    strncpy(data.unit, ctrl->setpointUnit, sizeof(data.unit) - 1);
    
    if (ctrl->actualValue != ctrl->setpoint) {
        data.valueCount = 1;
        data.additionalValues[0] = ctrl->actualValue;
    }
    
    if (ctrl->fault) data.flags |= IPC_SENSOR_FLAG_FAULT;
    if (!ctrl->connected) data.flags |= IPC_SENSOR_FLAG_FAULT;
    break;
}
```

Add control command handler in `ipc_handle_device_control()`:
```cpp
case IPC_DEV_YOUR_DEVICE: {
    YourDevice* dev = (YourDevice*)managedDev->deviceInstance;
    if (!dev) {
        errorCode = CTRL_ERR_DRIVER_FAULT;
        break;
    }
    
    control->setpoint = cmd->setpoint;
    
    if (dev->writeSetpoint(cmd->setpoint)) {
        success = true;
        snprintf(message, sizeof(message), "Setpoint %.2f sent", cmd->setpoint);
    } else {
        errorCode = CTRL_ERR_DRIVER_FAULT;
        strcpy(message, "Failed to queue command");
    }
    break;
}
```

---

## Part 4: Configuration (SYS MCU)

### File: `orc-sys-mcu/src/config/ioConfig.h`

No changes typically needed - uses existing structures.

### File: `orc-sys-mcu/data/devices.js`

Add driver definition:
```javascript
const DRIVER_DEFINITIONS = {
    10: { 
        name: 'Your Device Name',
        interface: 0,  // 0=Modbus, 1=Analogue, 2=Motor
        description: 'Brief description',
        hasSetpoint: true  // true if controllable, false if sensor-only
    }
};
```

---

## Part 5: REST API (SYS MCU)

### File: `orc-sys-mcu/src/utils/ipcManager.cpp`

Already polls control objects (50-69) every 1 second. No changes needed.

### File: `orc-sys-mcu/src/network/network.cpp`

Device endpoints already exist:
- `GET /api/devices` - Returns all devices with control data
- `POST /api/device/{controlIndex}/setpoint` - Set setpoint
- `POST /api/device/{controlIndex}/fault/reset` - Reset fault

The `handleGetDevices()` function automatically includes control data from cache.

---

## Part 6: Web UI

### File: `orc-sys-mcu/data/devices.js`

Add driver with `hasSetpoint` flag (see Part 4).

Device cards automatically render based on `hasSetpoint`:
- `true`: Shows setpoint input, actual value, "Set" button
- `false`: Shows value only (sensor-only device)

Polling is automatic (2 seconds) - no changes needed.

---

## Testing

### 1. IO MCU Serial Monitor
```
[DEV MGR] Created Your Device: index=72, sensors=2
[YOUR_DEV] Initialized successfully
[YOUR_DEV] Reading data...
[YOUR_DEV] Update successful: value=25.3
[DEV CTRL] Set device setpoint: index=52, value=100.0
[YOUR_DEV] Setpoint command queued successfully
```

### 2. SYS MCU Serial Monitor
```
[IPC] Polling objects: 0-40, 50-69, 70-99
[WEB] Device API: Serving control data for device 72
[WEB] POST /api/device/52/setpoint - value=100.0
[IPC] Device setpoint command sent
```

### 3. Web UI
- Open Devices tab
- Device card shows "Connected"
- Setpoint and actual values display
- Enter setpoint, click "Set"
- Values update within 2 seconds

### 4. Verification Steps
1. Device appears in devices list
2. Status shows "Connected"
3. Sensor values update in real-time
4. Setpoint changes reach hardware
5. Fault conditions display correctly
6. Messages show in UI

---

## Quick Reference

### Object Indices
- **Control:** sensorIndex - 20 (e.g., 72 → 52)
- **Sensor:** 70-99 (your startIndex)

### Key Structures
```cpp
DeviceControl_t {
    float setpoint;
    float actualValue;
    char setpointUnit[8];
    bool connected;
    bool fault;
    char message[100];
    uint8_t deviceType;
    uint8_t startSensorIndex;
    uint8_t sensorCount;
}

SensorData_t {
    float value;
    char unit[8];
    char name[40];
    bool fault;
    uint32_t timestamp;
}
```

### IPC Commands
- `IPC_MSG_DEVICE_CONTROL` - Send control command
- `DEV_CMD_SET_SETPOINT` - Set device setpoint
- `DEV_CMD_RESET_FAULT` - Clear fault condition

### REST Endpoints
- `GET /api/devices` - List all devices with control data
- `POST /api/device/{controlIdx}/setpoint` - Set setpoint
- `POST /api/device/{controlIdx}/fault/reset` - Reset fault

---

## Common Patterns

### Sensor-Only Device (pH Probe)
- Has control object (status only)
- `hasSetpoint: false`
- No writeSetpoint() method
- UI shows value only

### Controllable Device (MFC)
- Has control object with setpoint
- `hasSetpoint: true`
- Implements writeSetpoint()
- UI shows setpoint input

### Multi-Sensor Device
- One control object
- Multiple sensor objects (up to 4)
- Each sensor has separate index
- All sensors in Sensors tab

---

## Error Codes

```cpp
CTRL_ERR_NONE = 0x00          // Success
CTRL_ERR_INVALID_INDEX = 0x01 // Control index not found
CTRL_ERR_TYPE_MISMATCH = 0x02 // Wrong device type
CTRL_ERR_INVALID_CMD = 0x03   // Unknown command
CTRL_ERR_OUT_OF_RANGE = 0x04  // Setpoint out of range
CTRL_ERR_NOT_ENABLED = 0x05   // Device not enabled
CTRL_ERR_DRIVER_FAULT = 0x06  // Driver error
CTRL_ERR_TIMEOUT = 0x07       // Communication timeout
```

---

## Troubleshooting

**Device not appearing in UI:**
- Check DeviceManager factory case
- Verify object registration
- Check IPC sensor read handler

**Setpoint not reaching device:**
- Check IPC control command handler
- Verify writeSetpoint() implementation
- Check device state machine

**"DISCONNECTED" status:**
- Check init() return value
- Verify communication in update()
- Check _controlObj.connected flag

**Sensor values not updating:**
- Check scheduler task is running
- Verify update() state machine
- Check sensor object registration

---

## Files Modified Summary

### IO MCU
1. `src/drivers/peripheral/drv_<device>.h` - Driver header
2. `src/drivers/peripheral/drv_<device>.cpp` - Driver implementation
3. `src/drivers/objects.h` - Add device type enum
4. `src/drivers/device_manager.cpp` - Factory pattern
5. `src/drivers/ipc/ipc_handlers.cpp` - IPC handlers
6. `src/drivers/ipc/ipc_protocol.h` - Device type enum

### SYS MCU
7. `data/devices.js` - Driver definition with hasSetpoint

### Both
8. `lib/IPCprotocol/IPCDataStructs.h` - Shared IPC structures

---

**End of Guide**

For detailed code examples, see:
- `DEVICE_DRIVER_TEMPLATE.md` - Complete driver implementation
- `IPC_HANDLERS_GUIDE.md` - IPC protocol details

**Estimated Time:** 2-8 hours depending on device complexity
