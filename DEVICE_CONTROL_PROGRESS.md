# Device Control Implementation Progress
**Last Updated:** 2025-10-27 8:20 PM  
**Status:** Phase 2 In Progress

---

## ✅ PHASE 1: COMPLETE - Architecture & Data Structures

**Files Modified:**
- ✅ `orc-io-mcu/src/drivers/objects.h` - Expanded to 100 slots, added DeviceControl_t
- ✅ `orc-io-mcu/src/drivers/device_manager.h` - Updated for dual-index model
- ✅ `IPC_PROTOCOL_SPECIFICATION.md` - Updated to v2.5
- ✅ `DEVICE_CONTROL_IMPLEMENTATION_PLAN.md` - Created comprehensive plan

---

## 🚧 PHASE 2: IN PROGRESS - Device Driver Updates

### ✅ Alicat MFC Driver (COMPLETE)
**Files Modified:**
- ✅ `drv_modbus_alicat_mfc.h`:
  - Added `DeviceControl_t _controlObj` member
  - Added `getControlObject()` method
  
- ✅ `drv_modbus_alicat_mfc.cpp`:
  - Updated `handleMfcResponse()` to populate control object on success
  - Added control object fault updates on Modbus errors
  - Sets: setpoint, actualValue (flow), connected, fault, message, slaveID, deviceType

### ✅ Hamilton pH Probe Driver (COMPLETE)
**Files Modified:**
- ✅ `drv_modbus_hamilton_ph.h`:
  - Added `DeviceControl_t _controlObj` member
  - Added `getControlObject()` method
  
- ✅ `drv_modbus_hamilton_ph.cpp`:
  - Updated `handlePhResponse()` to populate control object
  - Added control object fault updates on Modbus errors
  - Sets: actualValue (pH), connected, fault, message, slaveID, deviceType
  - Note: setpoint=0 for sensor-only device (future pH control will use this)

### 🔲 Hamilton DO Probe Driver (PENDING)
**Files to Modify:**
- `drv_modbus_hamilton_do.h` - Add control object
- `drv_modbus_hamilton_do.cpp` - Populate in response handlers

### 🔲 Hamilton OD Probe Driver (PENDING)
**Files to Modify:**
- `drv_modbus_hamilton_od.h` - Add control object
- `drv_modbus_hamilton_od.cpp` - Populate in response handlers

---

## 🔲 PHASE 3: PENDING - Device Manager Registration

**Goal:** Register both control and sensor objects during device creation

**File:** `device_manager.cpp`

**Key Changes Needed:**
1. Update `createDevice()` to:
   - Calculate control index (startSensorIndex - 20)
   - Validate control index range (50-69)
   - Register control object in objIndex[]
   - Initialize controlObj fields (startSensorIndex, sensorCount, slaveID, deviceType)
   - Store controlIndex in ManagedDevice struct

2. Update `deleteDevice()` to:
   - Unregister control object (indices 50-69)
   - Unregister sensor objects (indices 70-99)

**Example Registration Code:**
```cpp
// Calculate control index
uint8_t controlIndex = startSensorIndex - 20;  // 70→50, 71→51, etc.

// Register control object
objIndex[controlIndex].type = OBJ_T_DEVICE_CONTROL;
objIndex[controlIndex].obj = device->getControlObject();
objIndex[controlIndex].valid = true;
snprintf(objIndex[controlIndex].name, sizeof(objIndex[controlIndex].name),
         "Device Control %d", controlIndex);

// Initialize control object
controlObj->startSensorIndex = startSensorIndex;
controlObj->sensorCount = sensorCount;
controlObj->slaveID = config->address;
controlObj->deviceType = config->deviceType;
```

---

## 🔲 PHASE 4: PENDING - IPC Device Control Protocol

**Goal:** Add IPC messages for device control commands

**Files to Modify:**
1. `ipc_protocol.h`:
   - Add `IPC_MSG_DEVICE_CONTROL = 0x45`
   - Add `IPC_DeviceControlCmd_t` structure
   - Add `DeviceControlCommand` enum (SET_SETPOINT, RESET_FAULT, etc.)

2. `ipc_handlers.cpp`:
   - Add `ipc_handle_device_control()` handler
   - Dispatch commands to specific device types
   - Call `writeSetpoint()` for MFCs, etc.
   - Send acknowledgment with error handling

**Command Structure:**
```cpp
struct IPC_DeviceControlCmd_t {
    uint8_t index;          // Control object index (50-69)
    uint8_t command;        // SET_SETPOINT, RESET_FAULT, etc.
    float setpoint;         // New setpoint value
} __attribute__((packed));
```

---

## 🔲 PHASE 5: PENDING - SYS MCU REST API

**Goal:** Add web API endpoints for device control

**File:** `network.cpp`

**Endpoints to Add:**
1. `GET /api/device/{index}` - Get device control status
2. `POST /api/device/{index}/setpoint` - Set device setpoint
3. `GET /api/devices` - List all devices (optional)

**API Response Example:**
```json
{
  "index": 50,
  "type": "ALICAT_MFC",
  "name": "Reactor Gas Supply",
  "setpoint": 100.0,
  "actualValue": 99.8,
  "unit": "SLPM",
  "connected": true,
  "fault": false,
  "sensors": [
    {"index": 70, "name": "Flow", "value": 99.8, "unit": "SLPM"},
    {"index": 71, "name": "Pressure", "value": 25.3, "unit": "PSI"}
  ]
}
```

---

## 🔲 PHASE 6: PENDING - Web UI

**Goal:** Create device control interface

**Files to Create/Modify:**
1. `devices.js` (new) - Device control tab logic
2. `index.html` - Add devices tab and modal
3. `style.css` - Device card styling

**UI Components:**
- Device cards showing:
  - Device name and type
  - Connection status badge
  - Setpoint input and control
  - Actual value feedback
  - Error display (setpoint - actual)
  - Linked sensor readings
  
---

## TESTING CHECKLIST

### ✅ Phase 2 Testing (Driver Updates)
- [ ] Compile IO MCU firmware
- [ ] Verify no errors in AlicatMFC driver
- [ ] Verify no errors in HamiltonPHProbe driver
- [ ] Complete Hamilton DO/OD drivers
- [ ] Test device creation with control object population

### Phase 3 Testing (Device Manager)
- [ ] Create device via IPC
- [ ] Verify control object registered at correct index (50-69)
- [ ] Verify sensor objects registered at correct indices (70-99)
- [ ] Delete device and verify cleanup

### Phase 4 Testing (IPC Protocol)
- [ ] Send SET_SETPOINT command
- [ ] Verify command reaches device driver
- [ ] Verify acknowledgment received
- [ ] Test error handling

### Phase 5 Testing (REST API)
- [ ] GET /api/device/50 returns correct data
- [ ] POST setpoint updates device
- [ ] Verify control object reflects in sensors array

### Phase 6 Testing (Web UI)
- [ ] Devices tab displays all devices
- [ ] Setpoint control works
- [ ] Actual value updates in real-time
- [ ] Error display shows setpoint tracking error
- [ ] Sensor readings display correctly

---

## ESTIMATED TIME REMAINING

- ✅ Phase 1: Complete (1 hour)
- 🚧 Phase 2: 50% complete (1 hour remaining)
  - ✅ Alicat MFC
  - ✅ Hamilton pH
  - 🔲 Hamilton DO (15 min)
  - 🔲 Hamilton OD (15 min)
  
- 🔲 Phase 3: 2 hours
- 🔲 Phase 4: 1.5 hours
- 🔲 Phase 5: 1.5 hours
- 🔲 Phase 6: 2 hours

**Total Remaining: ~8 hours**

---

## NEXT IMMEDIATE STEPS

1. Complete Hamilton DO driver updates
2. Complete Hamilton OD driver updates
3. Compile and test Phase 2 changes
4. Begin Phase 3: Device Manager registration
