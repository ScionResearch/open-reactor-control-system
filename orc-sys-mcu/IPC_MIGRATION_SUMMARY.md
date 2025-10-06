# IPC Protocol Migration Summary - RP2040 Side

## Overview
This document summarizes the changes made to migrate the RP2040 (orc-sys-mcu) IPC implementation to be compatible with the new SAME51 IPC protocol.

## Date
2024 - Phase 1 & 2 Implementation

## Changes Made

### 1. IPCProtocol Library Replacement

**Location:** `lib/IPCprotocol/`

#### Files Modified/Created:
- **IPCProtocol.h** - Completely rewritten
- **IPCProtocol.cpp** - Completely rewritten  
- **IPCDataStructs.h** - Enhanced with new protocol structures and legacy compatibility

#### Key Features:
- **Binary packet framing** with start/end bytes (0x7E)
- **Byte stuffing** for escape sequences (escape byte 0x7D, XOR 0x20)
- **CRC16-CCITT** error checking (polynomial 0x1021)
- **Non-blocking operation** with TX queue (8 packets deep)
- **Message handler registration** system for flexible callback dispatch
- **Helper functions** for common message types

#### Protocol Constants:
- Max payload size: 1024 bytes
- TX queue depth: 8 packets
- Max handlers: 32
- Baud rate: **1 Mbps** (increased from 19200)

#### Message Types Supported:
- **Handshake:** PING, PONG, HELLO, HELLO_ACK, ERROR
- **Index Sync:** INDEX_SYNC_REQ, INDEX_SYNC_DATA, INDEX_ADD, INDEX_REMOVE
- **Sensor Data:** SENSOR_READ_REQ, SENSOR_DATA, SENSOR_BATCH
- **Control:** CONTROL_WRITE, CONTROL_ACK, CONTROL_READ
- **Device Management:** DEVICE_CREATE, DEVICE_DELETE, DEVICE_STATUS
- **Faults:** FAULT_NOTIFY, MESSAGE_NOTIFY, FAULT_CLEAR
- **Configuration:** CONFIG_READ, CONFIG_WRITE, CALIBRATE

### 2. IPC Manager Update

**Location:** `src/utils/ipcManager.cpp` and `ipcManager.h`

#### Changes:
- Updated `init_ipcManager()` to use 1 Mbps baud rate
- Removed old message callback system
- Implemented new message handler registration system
- Added HELLO handshake on initialization
- Sends initial HELLO message to SAME51 with protocol version

#### New Message Handlers:
- `handlePing()` - Responds with PONG to keepalive checks
- `handleHello()` - Processes HELLO from SAME51, sends HELLO_ACK and INDEX_SYNC_REQ
- `handleError()` - Logs IPC error messages
- `handleSensorData()` - Processes sensor data and forwards to MQTT
- `handleFaultNotify()` - Logs fault notifications

### 3. MQTT Integration

**Location:** `src/network/mqttManager.cpp` and `mqttManager.h`

#### New Function:
- **`publishSensorDataIPC(const IPC_SensorData_t* data)`** - Publishes sensor data received via new IPC protocol

#### Features:
- Maps object types to MQTT topics
- Uses object index in topic path
- Extracts value, unit, status, and flags from IPC_SensorData_t
- Handles fault flags and message strings
- Maintains timestamp from IPC or generates current time

#### Topic Mapping:
```cpp
OBJ_T_TEMPERATURE_SENSOR  -> sensors/temperature
OBJ_T_PH_SENSOR          -> sensors/ph
OBJ_T_DISSOLVED_OXYGEN_SENSOR -> sensors/do
OBJ_T_OPTICAL_DENSITY_SENSOR  -> sensors/od
OBJ_T_FLOW_SENSOR        -> sensors/gasflow
OBJ_T_PRESSURE_SENSOR    -> sensors/pressure
OBJ_T_POWER_SENSOR       -> sensors/power
OBJ_T_ANALOG_INPUT       -> sensors/analog
```

#### JSON Payload Structure:
```json
{
  "timestamp": "2024-01-15T12:30:45Z",
  "value": 25.5,
  "unit": "°C",
  "status": 0,
  "fault": false,
  "message": "optional message"
}
```

### 4. Backward Compatibility

**Location:** `lib/IPCprotocol/IPCDataStructs.h`

#### Legacy Support Added:
- Old `MessageTypes` enum (0x80-0x88)
- Legacy sensor structs (PowerSensor, TemperatureSensor, etc.)
- Legacy `Message` struct
- Old `publishSensorData(const Message& msg)` function retained

This allows existing code to continue functioning while new code uses the enhanced IPC protocol.

## Protocol Specification

### Packet Structure:
```
[START] [LENGTH_HI] [LENGTH_LO] [MSG_TYPE] [PAYLOAD...] [CRC_HI] [CRC_LO] [END]
```

- **START/END:** 0x7E
- **LENGTH:** 16-bit big-endian (includes TYPE + PAYLOAD + CRC bytes)
- **MSG_TYPE:** 8-bit message type identifier
- **PAYLOAD:** Variable length (0-1024 bytes)
- **CRC:** 16-bit CRC16-CCITT checksum

### Byte Stuffing:
- Escape byte: 0x7D
- XOR value: 0x20
- Bytes requiring escape: 0x7E, 0x7D

### State Machine:
1. **IDLE** - Waiting for START byte
2. **RECEIVING** - Collecting packet bytes with escape handling
3. **PROCESSING** - Validating CRC and dispatching message
4. **ERROR** - Error recovery (returns to IDLE)

## Performance Characteristics

- **Baud rate:** 1 Mbps
- **Expected throughput:** ~500 packets/second sustained
- **Expected latency:** <10ms for request/response
- **CPU overhead:** <0.5% per 5ms task interval
- **TX queue:** 8 packets deep, prevents blocking

## Testing Status

### Phase 1: ✅ COMPLETED
- IPCProtocol library replacement
- Compilation successful

### Phase 2: 🔄 IN PROGRESS
- IPC Manager updated
- MQTT integration added
- **TODO:** Verify communication with SAME51
- **TODO:** Test message handlers

### Phase 3: ⏳ PENDING
- Object Index Management

### Phase 4: ⏳ PENDING
- Control API integration

### Phase 5: ⏳ PENDING
- Configuration Management

### Phase 6: ⏳ PENDING
- End-to-end testing and validation

## Known Issues

1. **Timestamp conversion** - IPC timestamp (ms) to ISO8601 conversion not yet implemented in `publishSensorDataIPC()`
2. **Status manager** - Not yet updated to use new IPC protocol structures
3. **Object index manager** - Not yet implemented on RP2040 side

## Next Steps

1. ✅ Test compilation
2. Test basic communication with SAME51
3. Implement object index manager on RP2040
4. Update control API to use new IPC protocol
5. Add configuration management handlers
6. Perform end-to-end integration testing

## API Changes

### For Application Code:

#### Old Way (deprecated but still works):
```cpp
Message msg;
msg.msgId = MSG_TEMPERATURE_SENSOR;
msg.objId = 0;
// ... fill data
ipc.send(msg);
```

#### New Way (recommended):
```cpp
// Register handler
ipc.registerHandler(IPC_MSG_SENSOR_DATA, handleSensorData);

// Send sensor data
IPC_SensorData_t data;
data.index = 0;
data.objectType = OBJ_T_TEMPERATURE_SENSOR;
data.value = 25.5;
data.flags = 0;
strncpy(data.unit, "°C", sizeof(data.unit));
data.timestamp = millis();
ipc.sendSensorData(&data);
```

## Files Modified

### Created:
- `lib/IPCprotocol/IPCProtocol.h`
- `lib/IPCprotocol/IPCProtocol.cpp`
- `IPC_MIGRATION_SUMMARY.md` (this file)

### Modified:
- `lib/IPCprotocol/IPCDataStructs.h`
- `src/utils/ipcManager.h`
- `src/utils/ipcManager.cpp`
- `src/network/mqttManager.h`
- `src/network/mqttManager.cpp`

### Unchanged (backward compatible):
- `src/network/MqttTopicRegistry.h`
- `src/utils/statusManager.cpp`
- Main application code

## Notes

- The new IPC protocol is designed to match the SAME51 implementation exactly
- Both sides must use the same protocol version for compatibility
- CRC validation ensures data integrity over UART
- Non-blocking design prevents IPC from stalling the main loop
- Message handlers are called from the `ipc.update()` function, which should be called regularly (e.g., every 5-10ms)

## Support

For questions or issues related to this migration, refer to:
- `IPC_PROTOCOL_PLAN.md` - Original protocol specification
- `orc-io-mcu/src/drivers/ipc/` - SAME51 reference implementation
