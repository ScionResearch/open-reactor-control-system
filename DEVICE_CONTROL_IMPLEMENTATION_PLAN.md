# Device Control Implementation Plan
**Version:** 1.0  
**Date:** 2025-10-27  
**Status:** 🚧 Architecture Defined - Ready for Implementation

---

## OVERVIEW

This document outlines the implementation plan for device-level IPC communication using a **dual-index architecture** to support peripheral device control (setpoints, commands) and status monitoring (Modbus connection, faults).

---

## NEW OBJECT INDEX ALLOCATION (v2.5)

### Expanded from 80 to 100 Indices

```
┌─────────────┬──────────────────────────────────────────────────────────┐
│   0-32      │ Fixed onboard objects (ADC, DAC, RTD, GPIO, outputs,     │
│             │ motors, energy monitors)                                  │
├─────────────┼──────────────────────────────────────────────────────────┤
│   33-37     │ COM ports (5 slots) - RS-232/RS-485 Modbus ports        │
├─────────────┼──────────────────────────────────────────────────────────┤
│   38-39     │ Reserved for onboard device feedback                      │
├─────────────┼──────────────────────────────────────────────────────────┤
│   40-49     │ Controller objects (10 slots) - PID loops, sequencers   │
│             │ 🚧 Reserved for future implementation                     │
├─────────────┼──────────────────────────────────────────────────────────┤
│   50-69     │ Device control objects (20 slots) ✅ NEW                 │
│             │ - Setpoints, commands, connection status                 │
│             │ - One control object per peripheral device               │
├─────────────┼──────────────────────────────────────────────────────────┤
│   70-99     │ Device sensor objects (30 slots) ✅ EXPANDED            │
│             │ - Sensor readings from peripheral devices                │
│             │ - 1-4 sensors per device (flow, pressure, temp, etc.)    │
└─────────────┴──────────────────────────────────────────────────────────┘
```

### Key Changes from v2.4:
- ✅ **Expanded MAX_NUM_OBJECTS:** 80 → 100
- ✅ **Added OBJ_T_DEVICE_CONTROL:** New object type for device control
- ✅ **Device sensors moved:** 60-79 → 70-99 (increased from 20 to 30 slots)
- ✅ **Added control object layer:** Indices 50-69 for device setpoints/commands

---

## DUAL-INDEX DEVICE ARCHITECTURE

### Design Philosophy
Separate **control** (user-writable setpoints, commands) from **sensors** (continuously polled readings) for:
- Clean API design
- Efficient bulk polling
- Scalability (more sensors per device)

### Example: Alicat MFC

```
┌──────────────────────────────────────────────────────────────┐
│ Index 50: Device Control (OBJ_T_DEVICE_CONTROL)             │
├──────────────────────────────────────────────────────────────┤
│  setpoint: 100.0          // Target flow rate               │
│  actualValue: 99.8        // Feedback from sensor 70        │
│  setpointUnit: "SLPM"     // Flow unit                      │
│  connected: true          // Modbus responding              │
│  fault: false                                                │
│  message: "OK"                                               │
│  slaveID: 1               // Modbus address                 │
│  deviceType: IPC_DEV_ALICAT_MFC                             │
│  startSensorIndex: 70     // First sensor index             │
│  sensorCount: 2           // Flow + Pressure                │
└──────────────────────────────────────────────────────────────┘
                              │
                              ├─────────────────┐
                              ▼                 ▼
        ┌──────────────────────────┐   ┌──────────────────────────┐
        │ Index 70: Flow Sensor    │   │ Index 71: Pressure       │
        ├──────────────────────────┤   ├──────────────────────────┤
        │ type: OBJ_T_FLOW_SENSOR  │   │ type: OBJ_T_PRESSURE     │
        │ value: 99.8              │   │ value: 25.3              │
        │ unit: "SLPM"             │   │ unit: "PSI"              │
        │ fault: false             │   │ fault: false             │
        └──────────────────────────┘   └──────────────────────────┘
```

### Hamilton pH Probe

```
┌──────────────────────────────────────────────────────────────┐
│ Index 51: Device Control (OBJ_T_DEVICE_CONTROL)             │
├──────────────────────────────────────────────────────────────┤
│  setpoint: 7.0            // Target pH (if dosing control)  │
│  actualValue: 7.25        // Feedback from sensor 72        │
│  setpointUnit: "pH"                                          │
│  connected: true          // Modbus responding              │
│  fault: false                                                │
│  message: "OK"                                               │
│  slaveID: 2                                                  │
│  deviceType: IPC_DEV_HAMILTON_PH                            │
│  startSensorIndex: 72                                        │
│  sensorCount: 2           // pH + Temperature               │
└──────────────────────────────────────────────────────────────┘
                              │
                              ├─────────────────┐
                              ▼                 ▼
        ┌──────────────────────────┐   ┌──────────────────────────┐
        │ Index 72: pH Sensor      │   │ Index 73: Temperature    │
        ├──────────────────────────┤   ├──────────────────────────┤
        │ type: OBJ_T_PH_SENSOR    │   │ type: OBJ_T_TEMPERATURE  │
        │ value: 7.25              │   │ value: 25.1              │
        │ unit: "pH"               │   │ unit: "°C"               │
        │ fault: false             │   │ fault: false             │
        └──────────────────────────┘   └──────────────────────────┘
```

---

## IMPLEMENTATION STATUS

### ✅ **Phase 1: Architecture & Data Structures** (COMPLETE)

**IO MCU (`objects.h`):**
- ✅ Expanded `MAX_NUM_OBJECTS` from 80 to 100
- ✅ Added `OBJ_T_DEVICE_CONTROL` enum value
- ✅ Added `DeviceControl_t` structure with all required fields
- ✅ Updated comments documenting new index allocation

**Device Manager (`device_manager.h`):**
- ✅ Expanded `MAX_DYNAMIC_DEVICES` to 30
- ✅ Updated `ManagedDevice` structure:
  - Added `controlIndex` (50-69)
  - Renamed `startIndex` → `startSensorIndex` (70-99)
  - Renamed `objectCount` → `sensorCount`
  - Added `controlObject` pointer

**IPC Protocol Spec (`IPC_PROTOCOL_SPECIFICATION.md`):**
- ✅ Updated to v2.5
- ✅ Documented new 100-slot allocation
- ✅ Added section 5.5: "Dual-Index Device Model"
- ✅ Updated object type mappings
- ✅ Updated controller object indices (40-49)

---

### 🚧 **Phase 2: IO MCU Device Driver Updates** (PENDING)

**Goal:** Update device driver classes to populate control objects

**Files to Modify:**
1. `drv_modbus_alicat_mfc.cpp`
2. `drv_modbus_hamilton_ph.cpp`
3. `drv_modbus_hamilton_do.cpp`
4. `drv_modbus_hamilton_od.cpp`

**Changes Needed:**

```cpp
// In AlicatMFC class
class AlicatMFC {
private:
    DeviceControl_t _controlObj;  // ✅ ADD: Control object

public:
    DeviceControl_t* getControlObject() { return &_controlObj; }  // ✅ ADD

    void update() {
        // ... existing sensor updates ...
        
        // ✅ ADD: Update control object
        _controlObj.setpoint = _setpoint;
        _controlObj.actualValue = _flowSensor.value;
        strncpy(_controlObj.setpointUnit, _setpointUnit, sizeof(_controlObj.setpointUnit));
        _controlObj.connected = true;  // Modbus response received
        _controlObj.fault = _fault;
        _controlObj.newMessage = _newMessage;
        strncpy(_controlObj.message, _message, sizeof(_controlObj.message));
        _controlObj.slaveID = _slaveID;
        _controlObj.deviceType = IPC_DEV_ALICAT_MFC;
    }
};
```

**Apply same pattern to all device drivers**

---

### 🚧 **Phase 3: Device Manager Registration** (PENDING)

**Goal:** Register both control and sensor objects during device creation

**File:** `device_manager.cpp`

**Changes to `createDevice()`:**

```cpp
bool DeviceManager::createDevice(uint8_t startSensorIndex, IPC_DeviceConfig_t* config) {
    // Validate sensor index range (70-99)
    if (startSensorIndex < 70 || startSensorIndex > 99) {
        Serial.printf("[DEV MGR] ERROR: Invalid start index %d (must be 70-99)\n", startSensorIndex);
        return false;
    }
    
    // Find free device slot
    int deviceSlot = findFreeDeviceSlot();
    if (deviceSlot < 0) return false;
    
    // Calculate control index (startSensorIndex - 20)
    // E.g., sensor 70 → control 50, sensor 80 → control 60
    uint8_t controlIndex = startSensorIndex - 20;
    
    // Validate control index range (50-69)
    if (controlIndex < 50 || controlIndex >= 70) {
        Serial.printf("[DEV MGR] ERROR: Invalid control index %d\n", controlIndex);
        return false;
    }
    
    // Check if control index is already in use
    if (objIndex[controlIndex].valid) {
        Serial.printf("[DEV MGR] ERROR: Control index %d already in use\n", controlIndex);
        return false;
    }
    
    // Factory pattern: Create device instance based on type
    void* deviceInstance = nullptr;
    DeviceControl_t* controlObj = nullptr;
    uint8_t sensorCount = 0;
    
    switch (config->deviceType) {
        case IPC_DEV_ALICAT_MFC: {
            AlicatMFC* mfc = new AlicatMFC(modbusDriver, config->address);
            deviceInstance = mfc;
            controlObj = mfc->getControlObject();
            sensorCount = 2;  // Flow + Pressure
            
            // Register sensors
            objIndex[startSensorIndex].type = OBJ_T_FLOW_SENSOR;
            objIndex[startSensorIndex].obj = &mfc->getFlowSensor();
            objIndex[startSensorIndex].valid = true;
            
            objIndex[startSensorIndex + 1].type = OBJ_T_PRESSURE_SENSOR;
            objIndex[startSensorIndex + 1].obj = &mfc->getPressureSensor();
            objIndex[startSensorIndex + 1].valid = true;
            break;
        }
        
        case IPC_DEV_HAMILTON_PH: {
            HamiltonPHProbe* ph = new HamiltonPHProbe(modbusDriver, config->address);
            deviceInstance = ph;
            controlObj = ph->getControlObject();
            sensorCount = 2;  // pH + Temperature
            
            // Register sensors
            objIndex[startSensorIndex].type = OBJ_T_PH_SENSOR;
            objIndex[startSensorIndex].obj = &ph->getPhSensor();
            objIndex[startSensorIndex].valid = true;
            
            objIndex[startSensorIndex + 1].type = OBJ_T_TEMPERATURE_SENSOR;
            objIndex[startSensorIndex + 1].obj = &ph->getTemperatureSensor();
            objIndex[startSensorIndex + 1].valid = true;
            break;
        }
        
        // ... other device types ...
    }
    
    // ✅ NEW: Register control object
    objIndex[controlIndex].type = OBJ_T_DEVICE_CONTROL;
    objIndex[controlIndex].obj = controlObj;
    objIndex[controlIndex].valid = true;
    snprintf(objIndex[controlIndex].name, sizeof(objIndex[controlIndex].name),
             "Device Control %d", controlIndex);
    
    // Initialize control object fields
    controlObj->startSensorIndex = startSensorIndex;
    controlObj->sensorCount = sensorCount;
    controlObj->slaveID = config->address;
    controlObj->deviceType = config->deviceType;
    
    // Save device info
    devices[deviceSlot].type = config->deviceType;
    devices[deviceSlot].config = *config;
    devices[deviceSlot].controlIndex = controlIndex;
    devices[deviceSlot].startSensorIndex = startSensorIndex;
    devices[deviceSlot].sensorCount = sensorCount;
    devices[deviceSlot].deviceInstance = deviceInstance;
    devices[deviceSlot].controlObject = controlObj;
    devices[deviceSlot].active = true;
    
    // Create scheduler task
    // ... existing code ...
    
    return true;
}
```

**Changes to `deleteDevice()`:**

```cpp
bool DeviceManager::deleteDevice(uint8_t startSensorIndex) {
    // Find device by sensor index
    int deviceSlot = findDeviceBySensorIndex(startSensorIndex);
    if (deviceSlot < 0) return false;
    
    ManagedDevice* device = &devices[deviceSlot];
    
    // Unregister control object
    if (device->controlIndex > 0 && device->controlIndex < MAX_NUM_OBJECTS) {
        objIndex[device->controlIndex].valid = false;
        objIndex[device->controlIndex].obj = nullptr;
    }
    
    // Unregister sensor objects
    for (uint8_t i = 0; i < device->sensorCount; i++) {
        uint8_t idx = device->startSensorIndex + i;
        if (idx < MAX_NUM_OBJECTS) {
            objIndex[idx].valid = false;
            objIndex[idx].obj = nullptr;
        }
    }
    
    // Delete device instance
    // ... existing code ...
}
```

---

### 🚧 **Phase 4: IPC Protocol - Device Control Messages** (PENDING)

**Goal:** Add IPC message types and handlers for device control commands

**File:** `ipc_protocol.h`

**Add Message Type:**
```cpp
enum IPC_MsgType : uint8_t {
    // ... existing types ...
    
    IPC_MSG_DEVICE_CONTROL  = 0x45,  // ✅ NEW: Device control command
};
```

**Add Control Command Structure:**
```cpp
// Device control command
struct IPC_DeviceControlCmd_t {
    uint8_t index;          // Control object index (50-69)
    uint8_t command;        // Command type
    float setpoint;         // New setpoint value
} __attribute__((packed));

enum DeviceControlCommand : uint8_t {
    DEV_CMD_SET_SETPOINT    = 0x01,  // Set control setpoint
    DEV_CMD_RESET_FAULT     = 0x02,  // Clear fault condition
    DEV_CMD_ENABLE          = 0x03,  // Enable device
    DEV_CMD_DISABLE         = 0x04,  // Disable device
};
```

**File:** `ipc_handlers.cpp`

**Add Handler:**
```cpp
void ipc_handle_device_control(const uint8_t *payload, uint16_t len) {
    if (len != sizeof(IPC_DeviceControlCmd_t)) {
        Serial.printf("[IPC] ERROR: Invalid device control length\n");
        return;
    }
    
    IPC_DeviceControlCmd_t *cmd = (IPC_DeviceControlCmd_t*)payload;
    
    // Validate index range (50-69)
    if (cmd->index < 50 || cmd->index >= 70) {
        Serial.printf("[IPC] ERROR: Invalid device control index %d\n", cmd->index);
        return;
    }
    
    // Calculate device slot from control index
    // Control index 50 → device slot 0, 51 → slot 1, etc.
    uint8_t deviceSlot = cmd->index - 50;
    ManagedDevice* device = DeviceManager::getDevice(deviceSlot);
    
    if (!device || device->type == IPC_DEV_NONE) {
        Serial.printf("[IPC] ERROR: No device at index %d\n", cmd->index);
        return;
    }
    
    bool success = false;
    char message[128] = "";
    
    switch (cmd->command) {
        case DEV_CMD_SET_SETPOINT: {
            // Dispatch to specific device type
            if (device->type == IPC_DEV_ALICAT_MFC) {
                AlicatMFC* mfc = (AlicatMFC*)device->deviceInstance;
                success = mfc->writeSetpoint(cmd->setpoint);
                if (success) {
                    snprintf(message, sizeof(message), "Setpoint %.1f queued", cmd->setpoint);
                } else {
                    strcpy(message, "Failed to queue setpoint write");
                }
            }
            // ... other device types ...
            break;
        }
        
        case DEV_CMD_RESET_FAULT: {
            // Clear fault flags
            DeviceControl_t* ctrl = (DeviceControl_t*)device->controlObject;
            ctrl->fault = false;
            ctrl->newMessage = false;
            strcpy(ctrl->message, "Fault cleared");
            success = true;
            break;
        }
        
        // ... other commands ...
    }
    
    // Send acknowledgment
    ipc_sendControlAck_v2(cmd->index, success ? CTRL_ERR_NONE : CTRL_ERR_DRIVER_FAULT, message);
}
```

**Register Handler in Message Dispatcher:**
```cpp
case IPC_MSG_DEVICE_CONTROL:
    ipc_handle_device_control(payload, len);
    break;
```

---

### 🚧 **Phase 5: SYS MCU - Device Control API** (PENDING)

**Goal:** Add REST API endpoints for device control

**File:** `network.cpp`

**Add API Endpoints:**

```cpp
// GET /api/device/{index} - Get device control status
void handleGetDevice() {
    String path = server.uri();
    int index = path.substring(12).toInt();
    
    if (index < 50 || index >= 70) {
        server.send(400, "application/json", "{\"error\":\"Invalid device index\"}");
        return;
    }
    
    ObjectCache::CachedObject* obj = objectCache.getObject(index);
    if (!obj || !obj->valid) {
        server.send(404, "application/json", "{\"error\":\"Device not found\"}");
        return;
    }
    
    StaticJsonDocument<512> doc;
    doc["index"] = index;
    doc["type"] = obj->objectType;
    doc["name"] = obj->name;
    doc["setpoint"] = obj->value;
    doc["unit"] = obj->unit;
    doc["connected"] = !(obj->flags & IPC_SENSOR_FLAG_FAULT);
    doc["fault"] = (obj->flags & IPC_SENSOR_FLAG_FAULT) ? true : false;
    
    // Get actual value from associated sensor
    // Control index 50 → sensor index 70 (+20 offset)
    ObjectCache::CachedObject* sensor = objectCache.getObject(index + 20);
    if (sensor && sensor->valid) {
        doc["actualValue"] = sensor->value;
    }
    
    String response;
    serializeJson(doc, response);
    server.send(200, "application/json", response);
}

// POST /api/device/{index}/setpoint - Set device setpoint
void handleSetDeviceSetpoint() {
    String path = server.uri();
    int index = path.substring(12, path.indexOf("/setpoint")).toInt();
    
    if (index < 50 || index >= 70) {
        server.send(400, "application/json", "{\"error\":\"Invalid device index\"}");
        return;
    }
    
    StaticJsonDocument<256> doc;
    DeserializationError error = deserializeJson(doc, server.arg("plain"));
    
    if (error || !doc.containsKey("setpoint")) {
        server.send(400, "application/json", "{\"error\":\"Invalid JSON or missing setpoint\"}");
        return;
    }
    
    float setpoint = doc["setpoint"];
    
    // Send device control command via IPC
    bool sent = sendDeviceControlCommand(index, DEV_CMD_SET_SETPOINT, setpoint);
    
    if (sent) {
        server.send(200, "application/json", "{\"success\":true}");
        log(LOG_INFO, false, "Device %d setpoint set to %.2f\n", index, setpoint);
    } else {
        server.send(500, "application/json", "{\"error\":\"Failed to send command\"}");
    }
}

// IPC command sender
bool sendDeviceControlCommand(uint8_t index, uint8_t command, float setpoint) {
    IPC_DeviceControlCmd_t cmd;
    cmd.index = index;
    cmd.command = command;
    cmd.setpoint = setpoint;
    
    return ipc.sendPacket(IPC_MSG_DEVICE_CONTROL, (uint8_t*)&cmd, sizeof(cmd));
}
```

**Register Endpoints:**
```cpp
void setupServer() {
    // ... existing endpoints ...
    
    server.on("/api/device/*", HTTP_GET, handleGetDevice);
    server.on("/api/device/*/setpoint", HTTP_POST, handleSetDeviceSetpoint);
}
```

---

### 🚧 **Phase 6: Web UI - Device Control Interface** (PENDING)

**Goal:** Add web interface for device control

**File:** `devices.js` (NEW)

```javascript
// Devices tab - shows control objects and linked sensors
async function fetchDevices() {
    const response = await fetch('/api/devices');  // NEW endpoint - returns indices 50-69
    const data = await response.json();
    return data.devices;
}

async function renderDevices() {
    const devices = await fetchDevices();
    const container = document.getElementById('devices-container');
    
    container.innerHTML = devices.map(device => `
        <div class="device-card">
            <div class="device-header">
                <h3>${device.name}</h3>
                <span class="device-index">[${device.index}]</span>
                <span class="status-badge ${device.connected ? 'connected' : 'disconnected'}">
                    ${device.connected ? 'Connected' : 'Disconnected'}
                </span>
            </div>
            
            <!-- Control section -->
            <div class="device-control">
                <label>Setpoint:</label>
                <input type="number" id="setpoint-${device.index}" 
                       value="${device.setpoint}" step="0.1">
                <span>${device.unit}</span>
                <button onclick="setDeviceSetpoint(${device.index})">Set</button>
            </div>
            
            <!-- Feedback display -->
            <div class="device-feedback">
                <div class="feedback-item">
                    <span class="feedback-label">Actual:</span>
                    <span class="feedback-value">${device.actualValue.toFixed(2)} ${device.unit}</span>
                    <span class="feedback-error">(${(device.setpoint - device.actualValue).toFixed(2)})</span>
                </div>
            </div>
            
            <!-- Sensor readings -->
            <div class="device-sensors">
                <h4>Sensor Readings:</h4>
                ${device.sensors.map(s => `
                    <div class="sensor-reading">
                        <span class="sensor-name">${s.name}:</span>
                        <span class="sensor-value">${s.value.toFixed(2)} ${s.unit}</span>
                    </div>
                `).join('')}
            </div>
        </div>
    `).join('');
}

async function setDeviceSetpoint(index) {
    const setpointInput = document.getElementById(`setpoint-${index}`);
    const setpoint = parseFloat(setpointInput.value);
    
    try {
        const response = await fetch(`/api/device/${index}/setpoint`, {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ setpoint: setpoint })
        });
        
        if (response.ok) {
            showToast('success', 'Setpoint Updated', `Device ${index} setpoint set to ${setpoint}`);
            // Refresh device status
            setTimeout(renderDevices, 1000);
        } else {
            const error = await response.json();
            showToast('error', 'Error', error.error || 'Failed to update setpoint');
        }
    } catch (error) {
        showToast('error', 'Error', 'Failed to communicate with server');
    }
}

// Poll devices every 2 seconds
setInterval(renderDevices, 2000);
```

**Add Devices Tab to `index.html`:**
```html
<div id="tab-devices" class="tab-content">
    <h2>Peripheral Devices</h2>
    <div id="devices-container"></div>
</div>
```

---

## VALIDATION & INDEX RANGE UPDATES

### Files Requiring Index Validation Updates:

**IO MCU:**
1. `device_manager.cpp` - ✅ Update to 70-99 sensor range
2. `ipc_handlers.cpp` - ✅ Update device create/delete validators

**SYS MCU:**
1. `ipcManager.cpp` - Update bulk read range to 0-99
2. `network.cpp` - Add device control endpoints
3. `ioConfig.h` - Update MAX_DEVICE_SENSORS config if needed

---

## TESTING PLAN

### Test 1: Device Creation
1. Create Alicat MFC via API
2. ✅ Control object created at index 50
3. ✅ Flow sensor created at index 70
4. ✅ Pressure sensor created at index 71
5. ✅ Both show up in `/api/inputs`
6. ✅ Control shows up in `/api/device/50`

### Test 2: Setpoint Control
1. Set MFC setpoint to 100 SLPM via `/api/device/50/setpoint`
2. ✅ Command sent via IPC
3. ✅ Modbus write queued on IO MCU
4. ✅ Setpoint verification loop runs
5. ✅ Actual value updates in sensor index 70
6. ✅ Control object shows actual = 100.0

### Test 3: Multiple Devices
1. Create MFC at indices 50/70-71
2. Create pH probe at indices 51/72-73
3. Create DO sensor at indices 52/74-75
4. ✅ All devices independent
5. ✅ No index collisions
6. ✅ All sensors polled in bulk read

### Test 4: Device Deletion
1. Delete MFC
2. ✅ Control object 50 freed
3. ✅ Sensor objects 70-71 freed
4. ✅ Can create new device at same indices

---

## BENEFITS OF DUAL-INDEX MODEL

✅ **Clean Separation:**
- Control objects: User interaction (setpoints, commands)
- Sensor objects: Continuous monitoring (bulk polled every 1s)

✅ **Efficient Polling:**
- Sensors included in bulk read (0-99)
- Control accessed on-demand or in extended bulk read

✅ **Scalable:**
- 20 devices with control objects
- Up to 30 sensors total (1-4 per device)
- More sensors per device than old model (20 total)

✅ **Consistent API:**
- Control: `/api/device/{index}` (POST/GET)
- Sensors: `/api/inputs` (existing endpoint)
- Clear separation of concerns

✅ **Future-Proof:**
- Room for 10 controller objects (PID loops)
- Reserved indices for onboard expansion
- Clean upgrade path from v2.4

---

## NEXT STEPS

1. ✅ **Phase 1 Complete:** Architecture defined, data structures updated
2. 🚧 **Phase 2:** Update device drivers with control object support
3. 🚧 **Phase 3:** Update device manager registration
4. 🚧 **Phase 4:** Implement IPC control messages
5. 🚧 **Phase 5:** Add SYS MCU REST API
6. 🚧 **Phase 6:** Build web UI

**Estimated Implementation Time:** 6-8 hours

**Priority:** HIGH - Required for MFC flow control, pH setpoint control, and all future peripheral devices
