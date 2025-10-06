# IPC Protocol Implementation Plan - RP2040 Side

## Executive Summary

This document outlines the plan to implement the new IPC (Inter-Processor Communication) protocol on the RP2040 System Controller to match the protocol already implemented on the SAME51 I/O Controller.

**Status:** The SAME51 side is **complete and compiled**. This plan addresses the RP2040 implementation.

---

## 1. Current State Analysis

### 1.1 Existing RP2040 IPC Architecture

**Current Protocol:**
- **Library:** `lib/IPCprotocol/` (IPCProtocol.h/cpp, IPCDataStructs.h)
- **Frame Format:** `[0xAA][msgId][objId][len][data...][CRC16][0x55]`
- **CRC:** CRC16 with 0xA001 polynomial
- **Max Payload:** 128 bytes
- **Features:** Simple callback registration, no byte stuffing, no state machine

**Current Integration:**
- **Manager:** `src/utils/ipcManager.cpp/h`
- **Serial Interface:** Serial1 @ 19200 baud (GPIO 16/17)
- **Core Assignment:** Managed on Core 1
- **Data Flow:** IPC → status updates → MQTT publish

**Key Components:**
- Callback-based message handling
- Direct integration with `statusManager` (global status struct)
- Automatic forwarding to MQTT via `mqttManager`
- Simple sensor data structures (PowerSensor, TemperatureSensor, etc.)

### 1.2 New SAME51 IPC Protocol

**New Protocol Features:**
- **Frame Format:** `[0x7E][LEN(2)][MSG_TYPE][PAYLOAD][CRC16][0x7D]`
- **Byte Stuffing:** 0x7E and 0x7D escaped as 0x7D + (byte XOR 0x20)
- **CRC:** CRC16-CCITT with 0x1021 polynomial
- **Max Payload:** 1024 bytes
- **State Machine:** IDLE → RECEIVING → PROCESSING → ERROR
- **Message Types:** 20+ structured message types

**SAME51 Architecture:**
- Object index system (80 objects: 0-39 fixed, 40-79 dynamic)
- Type-safe operations (type verification on all commands)
- Comprehensive payload structures
- Non-blocking TX queue (8 packets deep)
- Keepalive system (PING/PONG every 1 second)

---

## 2. Implementation Strategy

### 2.1 Approach: **Incremental Replacement**

We will replace the existing IPC library while maintaining API compatibility where possible:

1. ✅ Keep existing file structure (`lib/IPCprotocol/`, `utils/ipcManager`)
2. ✅ Replace protocol implementation to match SAME51
3. ✅ Update data structures to support new message types
4. ✅ Maintain existing MQTT integration
5. ✅ Add object index management capabilities
6. ✅ Keep dual-core architecture intact

### 2.2 Baud Rate

**Change Required:** 19200 → **1,000,000 bps** (1 Mbps)

**Rationale:**
- Matches SAME51 configuration
- Higher throughput for larger payloads
- Short PCB traces (both MCUs on same board) support high speed

---

## 3. Implementation Plan

### Phase 1: Protocol Library Replacement ⭐

**Files to Modify:**
- `lib/IPCprotocol/IPCProtocol.h`
- `lib/IPCprotocol/IPCProtocol.cpp`
- `lib/IPCprotocol/IPCDataStructs.h`

**Tasks:**

#### 3.1.1 Update `IPCProtocol.h`
- Add state machine enum (IDLE, RECEIVING, PROCESSING, ERROR)
- Update frame constants (0x7E start, 0x7D escape/end)
- Add TX queue structure (8 packets deep)
- Increase max payload to 1024 bytes
- Add helper functions for common operations:
  - `sendPing()`, `sendPong()`, `sendHello()`
  - `sendSensorReadRequest(uint16_t index)`
  - `sendControlWrite(uint16_t index, ...)`
  - `sendIndexSyncRequest()`
  - `sendDeviceCreate(...)`

#### 3.1.2 Update `IPCProtocol.cpp`
- Implement CRC16-CCITT (0x1021 polynomial)
- Add byte stuffing/unstuffing logic
- Implement state machine for RX processing
- Add TX queue management
- Add packet framing with escape sequences
- Implement timeout and error handling

#### 3.1.3 Update `IPCDataStructs.h`
**Critical:** Replace old message types with new protocol structures:

```cpp
// Remove old MessageTypes enum
// Remove old sensor structs

// Add new IPC message types (from SAME51)
enum IPC_MsgType {
    IPC_MSG_PING = 0x00,
    IPC_MSG_PONG = 0x01,
    IPC_MSG_HELLO = 0x02,
    // ... (20+ message types)
};

// Add new payload structures
struct IPC_Hello_t { ... };
struct IPC_SensorData_t { ... };
struct IPC_ControlWrite_t { ... };
struct IPC_IndexEntry_t { ... };
// ... etc
```

### Phase 2: IPC Manager Updates 🔧

**Files to Modify:**
- `src/utils/ipcManager.cpp`
- `src/utils/ipcManager.h`

**Tasks:**

#### 3.2.1 Update Initialization
```cpp
void init_ipcManager(void) {
  Serial1.setRX(PIN_SI_RX);
  Serial1.setTX(PIN_SI_TX);
  ipc.begin(1000000);  // 1 Mbps instead of 19200
  
  // Send HELLO handshake
  ipc.sendHello();
  
  // Wait for HELLO_ACK with timeout
  // ...
  
  // Request full index sync
  ipc.sendIndexSyncRequest();
}
```

#### 3.2.2 Add Message Handlers
Register handlers for new message types:
- `handleHelloAck()` - Process handshake response
- `handleIndexSyncData()` - Build local object index
- `handleSensorData()` - Process sensor readings (forward to MQTT)
- `handleFaultNotify()` - Process fault notifications
- `handleDeviceStatus()` - Process device creation responses
- `handlePong()` - Process keepalive responses

#### 3.2.3 Update manageIPC()
```cpp
void manageIPC(void) {
  // Non-blocking update (state machine)
  ipc.update();
  
  // Send keepalive if needed
  static unsigned long lastPing = 0;
  if (millis() - lastPing > 1000) {
    ipc.sendPing();
    lastPing = millis();
  }
  
  // Update connection status
  if (ipc.isConnected()) {
    status.ipcOK = true;
  } else {
    status.ipcOK = false;
  }
}
```

### Phase 3: Object Index Management 📋

**New File:** `src/utils/objectIndex.cpp/h`

**Purpose:** Maintain a local copy of the SAME51 object index for:
- Sensor data routing
- Control command targeting
- Device management
- Type-safe operations

**Structure:**
```cpp
struct ObjectIndexEntry {
  uint16_t index;
  uint8_t objectType;
  bool valid;
  bool fixed;
  char name[40];
  char unit[8];
};

class ObjectIndexManager {
public:
  void clear();
  void addEntry(const IPC_IndexEntry_t& entry);
  void removeEntry(uint16_t index);
  ObjectIndexEntry* getEntry(uint16_t index);
  uint16_t getCount();
  void printIndex();
  
private:
  ObjectIndexEntry _index[80];  // Match SAME51 capacity
};
```

### Phase 4: MQTT Integration Updates 📡

**Files to Modify:**
- `src/network/mqttManager.cpp/h`

**Tasks:**

#### 3.4.1 Update Sensor Data Publishing
```cpp
void publishSensorData(const IPC_SensorData_t& data) {
  // Get object info from index
  ObjectIndexEntry* obj = objectIndex.getEntry(data.index);
  
  // Build topic based on object type and index
  String topic = buildTopicFromObject(obj);
  
  // Create JSON payload
  JsonDocument doc;
  doc["timestamp"] = getISOTimestamp();  // RP2040 has RTC
  doc["value"] = data.value;
  doc["unit"] = data.unit;
  doc["online"] = !(data.flags & IPC_SENSOR_FLAG_FAULT);
  
  if (data.flags & IPC_SENSOR_FLAG_NEW_MSG) {
    doc["message"] = data.message;
  }
  
  // Publish
  publishMQTT(topic, doc);
}
```

#### 3.4.2 Route to Status Manager
Update `statusManager` to work with new index-based system instead of hardcoded sensor types.

### Phase 5: Control API Integration 🎛️

**Files to Modify:**
- REST API handlers (wherever control commands come from web UI)
- `src/controls/controlManager.cpp/h`

**Tasks:**

#### 3.5.1 Add Control Command Functions
```cpp
bool sendTemperatureSetpoint(uint16_t sensorIndex, float setpoint) {
  IPC_ControlWrite_t cmd;
  cmd.index = sensorIndex;
  cmd.objectType = OBJ_T_TEMPERATURE_SENSOR;
  cmd.paramType = IPC_CTRL_SETPOINT;
  cmd.value = setpoint;
  
  return ipc.sendPacket(IPC_MSG_CONTROL_WRITE, (uint8_t*)&cmd, sizeof(cmd));
}

bool sendPIDParameters(uint16_t controlIndex, float kp, float ki, float kd) {
  // Send multiple IPC_CTRL_PID_KP/KI/KD messages
  // ...
}

bool enableControl(uint16_t index, bool enable) {
  // Send IPC_CTRL_ENABLE message
  // ...
}
```

#### 3.5.2 Add Device Management
```cpp
bool createPeripheralDevice(uint8_t deviceType, uint8_t modbusPort, 
                            uint8_t slaveID, const char* name) {
  IPC_DeviceCreate_t cmd;
  cmd.deviceType = deviceType;
  cmd.modbusPort = modbusPort;
  cmd.slaveID = slaveID;
  strncpy(cmd.name, name, sizeof(cmd.name));
  
  return ipc.sendPacket(IPC_MSG_DEVICE_CREATE, (uint8_t*)&cmd, sizeof(cmd));
}
```

### Phase 6: Configuration & Calibration Management ⚙️

**Purpose:** RP2040 is the "single source of truth" for configuration and calibration data.

**Tasks:**

#### 3.6.1 Configuration Storage
Use existing LittleFS to store:
- Object configurations (analog input ranges, units, etc.)
- Control parameters (PID values, setpoints, etc.)
- Calibration data (scale, offset, timestamps)
- Device configurations (Modbus addresses, names, etc.)

#### 3.6.2 Startup Sequence
```cpp
void rebuildSAME51State(void) {
  // 1. Wait for HELLO handshake
  // 2. Request index sync
  // 3. Load configs from LittleFS
  // 4. Send all calibration data to SAME51
  // 5. Send all control parameters to SAME51
  // 6. Create peripheral devices (if any)
  // 7. Enable controls
}
```

#### 3.6.3 REST API Endpoints
Add endpoints for:
- `POST /api/calibrate/{index}` - Send calibration data
- `POST /api/device/create` - Create peripheral device
- `DELETE /api/device/{index}` - Delete peripheral device
- `POST /api/control/{index}` - Update control parameters

### Phase 7: Testing & Validation ✅

**Test Cases:**

1. **Protocol Communication**
   - [ ] Handshake (HELLO → HELLO_ACK)
   - [ ] Keepalive (PING → PONG)
   - [ ] CRC error detection
   - [ ] Byte stuffing (send 0x7E and 0x7D in payload)
   - [ ] Large payloads (>512 bytes)
   - [ ] TX queue overflow handling

2. **Object Index**
   - [ ] Index sync (request → receive all entries)
   - [ ] Dynamic device add
   - [ ] Dynamic device remove
   - [ ] Type mismatch detection

3. **Sensor Data Flow**
   - [ ] Read all sensor types
   - [ ] MQTT publishing
   - [ ] Status updates
   - [ ] Fault notifications
   - [ ] Message notifications

4. **Control Commands**
   - [ ] Setpoint changes
   - [ ] PID parameter updates
   - [ ] Enable/disable controls
   - [ ] Acknowledgment handling

5. **Device Management**
   - [ ] Create Hamilton pH probe
   - [ ] Create Alicat MFC
   - [ ] Verify index assignment
   - [ ] Delete device

6. **Configuration**
   - [ ] Calibration data send
   - [ ] Config persistence
   - [ ] State rebuild on reboot

---

## 4. File-by-File Changes Summary

### 4.1 Files to CREATE
- `src/utils/objectIndex.h` - Object index manager
- `src/utils/objectIndex.cpp` - Implementation

### 4.2 Files to REPLACE/HEAVILY MODIFY
- `lib/IPCprotocol/IPCProtocol.h` - New protocol definition
- `lib/IPCprotocol/IPCProtocol.cpp` - New protocol implementation
- `lib/IPCprotocol/IPCDataStructs.h` - New message structures

### 4.3 Files to UPDATE
- `src/utils/ipcManager.h` - Add new function prototypes
- `src/utils/ipcManager.cpp` - Update to use new protocol
- `src/network/mqttManager.cpp` - Update sensor data routing
- `src/utils/statusManager.cpp` - Update to work with index system
- `src/hardware/pins.h` - (No change, already correct)

### 4.4 Files to REVIEW (minimal changes)
- `src/sys_init.cpp` - Update baud rate only
- `src/main.cpp` - No changes needed
- `src/storage/sdManager.cpp` - May add config file handling

---

## 5. Implementation Order

**Recommended Sequence:**

1. **Phase 1** - Replace protocol library (core functionality)
2. **Phase 3** - Add object index manager (needed for Phase 2)
3. **Phase 2** - Update IPC manager (integration)
4. **Phase 4** - Update MQTT publishing (data flow)
5. **Phase 5** - Add control APIs (two-way communication)
6. **Phase 6** - Add configuration management (single source of truth)
7. **Phase 7** - Testing and validation

**Estimated Effort:**
- Phases 1-3: 4-6 hours (core protocol)
- Phases 4-5: 3-4 hours (integration)
- Phase 6: 2-3 hours (configuration)
- Phase 7: 2-3 hours (testing)
- **Total: 11-16 hours**

---

## 6. Compatibility & Migration

### 6.1 Breaking Changes
- Baud rate change (19200 → 1000000)
- Message format incompatible with old protocol
- Callback signatures change
- Data structures completely different

### 6.2 Migration Path
Both MCUs must be updated simultaneously. There is no backward compatibility.

**Deployment Steps:**
1. Flash SAME51 with new firmware
2. Flash RP2040 with new firmware
3. Power cycle system
4. Verify handshake
5. Verify sensor data flow
6. Test control commands

---

## 7. Risk Mitigation

**Risk:** Protocol mismatch causes communication failure
**Mitigation:** Handshake verification, version checking in HELLO message

**Risk:** Lost packets during high traffic
**Mitigation:** TX queue, CRC validation, retry logic

**Risk:** Configuration loss during development
**Mitigation:** Keep backups in LittleFS, provide factory reset

**Risk:** MQTT flooding from sensor updates
**Mitigation:** Configurable publish interval (already exists)

---

## 8. Success Criteria

Implementation is complete when:

✅ RP2040 and SAME51 complete handshake successfully  
✅ Object index sync works (all 40+ objects received)  
✅ Sensor data flows: SAME51 → RP2040 → MQTT  
✅ Control commands work: Web UI → RP2040 → SAME51  
✅ Device creation works (dynamic peripheral devices)  
✅ Calibration data can be sent from RP2040  
✅ Keepalive maintains connection status  
✅ System recovers from RP2040 reboot (state rebuild)  
✅ System recovers from SAME51 reboot (config resend)  

---

## 9. Documentation Updates Needed

- [ ] Update `PLANNING.md` with new IPC details
- [ ] Update `MQTT_DATA_FLOW.md` with index-based routing
- [ ] Create `IPC_API_REFERENCE.md` for control functions
- [ ] Update web UI documentation (if device management UI added)

---

## 10. Next Steps

**Immediate Action:**
1. Review and approve this plan
2. Begin Phase 1: Protocol library replacement
3. Set up test hardware for validation

**Questions to Resolve:**
1. Should RP2040 poll sensors or rely on SAME51 push?
2. How often should index sync be performed? (startup only vs periodic)
3. Should configuration files be JSON or binary?
4. Do we need a web UI for device management?

---

**Document Version:** 1.0  
**Last Updated:** 2025-10-05  
**Author:** AI Assistant (Cascade)  
**Status:** Ready for Review
