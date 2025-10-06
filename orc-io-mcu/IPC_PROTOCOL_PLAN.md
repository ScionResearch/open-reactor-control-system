# Inter-Processor Communication (IPC) Protocol Plan
**Date:** 2025-10-05  
**Status:** Draft for Review

## 1. OVERVIEW

### 1.1 Purpose
Design and implement a lightweight, robust, scalable inter-processor communication protocol between:
- **SAME51N20A** (Sensor/Control MCU) - This firmware
- **RP2040** (Management Interface MCU) - External web UI, MQTT, SD card, RTC, Ethernet

### 1.2 Physical Interface
- **Medium:** UART (Serial1)
- **Pins:** PIN_MI_TX (52), PIN_MI_RX (53)
- **Trace Length:** ~80mm on PCB
- **Baud Rate:** 1,000,000 bps (1 Mbps)
- **Configuration:** 8N1 (8 data bits, no parity, 1 stop bit)

### 1.3 Design Requirements
- ✅ **Lightweight:** Minimal overhead, efficient encoding
- ✅ **Robust:** CRC16 error checking on all packets
- ✅ **Scalable:** Extensible message types and object indices
- ✅ **Non-blocking:** Async processing using existing task scheduler
- ✅ **Fast:** Minimal latency, low CPU usage

---

## 2. PACKET STRUCTURE

### 2.1 Binary Packet Format
All communication uses binary packets with the following structure:

```
┌──────────┬──────────┬──────────┬──────────┬────────────┬──────────┐
│  START   │  LENGTH  │   MSG    │ PAYLOAD  │    CRC16   │   END    │
│  BYTE    │ (2 bytes)│   TYPE   │ (N bytes)│  (2 bytes) │   BYTE   │
├──────────┼──────────┼──────────┼──────────┼────────────┼──────────┤
│   0x7E   │  uint16  │  uint8   │  varies  │   uint16   │   0x7D   │
└──────────┴──────────┴──────────┴──────────┴────────────┴──────────┘

Total overhead: 8 bytes per packet
```

**Field Definitions:**
- **START_BYTE (0x7E):** Frame start marker
- **LENGTH:** Total payload length (big-endian uint16)
- **MSG_TYPE:** Message type identifier (uint8)
- **PAYLOAD:** Variable length data (0-1024 bytes max)
- **CRC16:** CRC16-CCITT of LENGTH + MSG_TYPE + PAYLOAD
- **END_BYTE (0x7D):** Frame end marker

### 2.2 Byte Stuffing
To prevent data confusion with frame markers:
- If 0x7E or 0x7D appears in LENGTH, MSG_TYPE, PAYLOAD, or CRC:
  - Replace with escape sequence: 0x7D followed by (byte XOR 0x20)
  - 0x7E → 0x7D 0x5E
  - 0x7D → 0x7D 0x5D

### 2.3 CRC16 Calculation
- **Algorithm:** CRC16-CCITT (polynomial 0x1021, initial value 0xFFFF)
- **Coverage:** LENGTH (2 bytes) + MSG_TYPE (1 byte) + PAYLOAD (N bytes)
- **Transmitted:** Big-endian (MSB first)

---

## 3. MESSAGE TYPES

### 3.1 Message Type Enumeration

```cpp
enum IPC_MsgType : uint8_t {
    // Handshake & Status (0x00-0x0F)
    IPC_MSG_PING            = 0x00,  // Keepalive ping
    IPC_MSG_PONG            = 0x01,  // Ping response
    IPC_MSG_HELLO           = 0x02,  // Initial handshake
    IPC_MSG_HELLO_ACK       = 0x03,  // Handshake acknowledgment
    IPC_MSG_ERROR           = 0x04,  // Error notification
    
    // Object Index Management (0x10-0x1F)
    IPC_MSG_INDEX_SYNC_REQ  = 0x10,  // Request full index sync
    IPC_MSG_INDEX_SYNC_DATA = 0x11,  // Index sync data packet
    IPC_MSG_INDEX_ADD       = 0x12,  // Add object to index
    IPC_MSG_INDEX_REMOVE    = 0x13,  // Remove object from index
    IPC_MSG_INDEX_UPDATE    = 0x14,  // Update object metadata (name, etc.)
    
    // Sensor Data (0x20-0x2F)
    IPC_MSG_SENSOR_READ_REQ = 0x20,  // Request sensor reading
    IPC_MSG_SENSOR_DATA     = 0x21,  // Sensor data response
    IPC_MSG_SENSOR_STREAM   = 0x22,  // Continuous sensor streaming
    IPC_MSG_SENSOR_BATCH    = 0x23,  // Batch sensor data (multiple sensors)
    
    // Control Data (0x30-0x3F)
    IPC_MSG_CONTROL_WRITE   = 0x30,  // Write control value/setpoint
    IPC_MSG_CONTROL_ACK     = 0x31,  // Control write acknowledgment
    IPC_MSG_CONTROL_READ    = 0x32,  // Read control parameters
    IPC_MSG_CONTROL_DATA    = 0x33,  // Control data response
    
    // Device Management (0x40-0x4F)
    IPC_MSG_DEVICE_CREATE   = 0x40,  // Create peripheral device instance
    IPC_MSG_DEVICE_DELETE   = 0x41,  // Delete peripheral device instance
    IPC_MSG_DEVICE_CONFIG   = 0x42,  // Configure device parameters
    IPC_MSG_DEVICE_STATUS   = 0x43,  // Device status response
    
    // Fault & Message (0x50-0x5F)
    IPC_MSG_FAULT_NOTIFY    = 0x50,  // Fault notification
    IPC_MSG_MESSAGE_NOTIFY  = 0x51,  // General message notification
    IPC_MSG_FAULT_CLEAR     = 0x52,  // Clear fault
    
    // Configuration (0x60-0x6F)
    IPC_MSG_CONFIG_READ     = 0x60,  // Read configuration
    IPC_MSG_CONFIG_WRITE    = 0x61,  // Write configuration
    IPC_MSG_CONFIG_DATA     = 0x62,  // Configuration data
    IPC_MSG_CALIBRATE       = 0x63,  // Calibration command
};
```

### 3.2 Key Message Flow Examples

#### 3.2.1 Initial Handshake
```
RP2040 → SAME51: IPC_MSG_HELLO
SAME51 → RP2040: IPC_MSG_HELLO_ACK
RP2040 → SAME51: IPC_MSG_INDEX_SYNC_REQ
SAME51 → RP2040: IPC_MSG_INDEX_SYNC_DATA (packet 1/N)
SAME51 → RP2040: IPC_MSG_INDEX_SYNC_DATA (packet 2/N)
...
```

#### 3.2.2 Sensor Read
```
RP2040 → SAME51: IPC_MSG_SENSOR_READ_REQ [index=5]
SAME51 → RP2040: IPC_MSG_SENSOR_DATA [index=5, value=23.45, unit="°C"]
```

#### 3.2.3 Control Write
```
RP2040 → SAME51: IPC_MSG_CONTROL_WRITE [index=12, setpoint=37.5]
SAME51 → RP2040: IPC_MSG_CONTROL_ACK [index=12, success=true]
```

#### 3.2.4 Device Creation
```
RP2040 → SAME51: IPC_MSG_DEVICE_CREATE [type=OBJ_T_PH_SENSOR, modbusPort=2, slaveID=3]
SAME51 → RP2040: IPC_MSG_INDEX_ADD [index=42, type=OBJ_T_PH_SENSOR, name="pH Probe 1"]
```

---

## 4. PAYLOAD STRUCTURES

### 4.1 Handshake Messages

#### HELLO (0x02)
```cpp
struct IPC_Hello_t {
    uint32_t protocolVersion;  // Protocol version (e.g., 0x00010000 = v1.0.0)
    uint32_t firmwareVersion;  // Firmware version
    char deviceName[32];       // Device identifier
};
```

#### HELLO_ACK (0x03)
```cpp
struct IPC_HelloAck_t {
    uint32_t protocolVersion;
    uint32_t firmwareVersion;
    uint16_t maxObjectCount;   // Max objects supported
    uint16_t currentObjectCount;
};
```

### 4.2 Object Index Messages

#### INDEX_SYNC_DATA (0x11)
```cpp
struct IPC_IndexEntry_t {
    uint16_t index;           // Object index (0-79)
    uint8_t objectType;       // ObjectType enum value
    uint8_t flags;            // Bit 0: valid, Bit 1: fixed, Bit 2-7: reserved
    char name[40];            // Object name (null-terminated)
    char unit[8];             // Unit string (if applicable)
};

struct IPC_IndexSync_t {
    uint16_t packetNum;       // Packet number (0-based)
    uint16_t totalPackets;    // Total packets in sync
    uint8_t entryCount;       // Entries in this packet
    IPC_IndexEntry_t entries[10];  // Up to 10 entries per packet
};
```

#### INDEX_ADD (0x12)
```cpp
struct IPC_IndexAdd_t {
    uint16_t index;
    uint8_t objectType;
    uint8_t flags;
    char name[40];
    char unit[8];
};
```

#### INDEX_REMOVE (0x13)
```cpp
struct IPC_IndexRemove_t {
    uint16_t index;
    uint8_t objectType;  // Must match for safety
};
```

### 4.3 Sensor Data Messages

#### SENSOR_READ_REQ (0x20)
```cpp
struct IPC_SensorReadReq_t {
    uint16_t index;
};
```

#### SENSOR_DATA (0x21)
```cpp
struct IPC_SensorData_t {
    uint16_t index;
    uint8_t objectType;      // Type verification
    uint8_t flags;           // Bit 0: fault, Bit 1: newMessage, Bit 2-7: reserved
    float value;             // Primary sensor value
    char unit[8];            // Unit string
    uint32_t timestamp;      // Optional timestamp (0 if not used)
    char message[100];       // Optional message (if newMessage flag set)
};
```

#### SENSOR_BATCH (0x23)
```cpp
struct IPC_SensorBatchEntry_t {
    uint16_t index;
    float value;
    uint8_t flags;           // Fault/message flags
};

struct IPC_SensorBatch_t {
    uint8_t count;           // Number of sensors in batch
    IPC_SensorBatchEntry_t sensors[20];  // Up to 20 sensors per batch
};
```

### 4.4 Control Messages

#### CONTROL_WRITE (0x30)
```cpp
struct IPC_ControlWrite_t {
    uint16_t index;
    uint8_t objectType;      // Type verification
    uint8_t paramType;       // 0=setpoint, 1=enable, 2=PID_Kp, 3=PID_Ki, 4=PID_Kd, etc.
    float value;             // Parameter value
};
```

#### CONTROL_ACK (0x31)
```cpp
struct IPC_ControlAck_t {
    uint16_t index;
    uint8_t success;         // 0=fail, 1=success
    char message[50];        // Error message if failed
};
```

### 4.5 Device Management Messages

#### DEVICE_CREATE (0x40)
```cpp
struct IPC_DeviceCreate_t {
    uint8_t deviceType;      // ObjectType for the device
    uint8_t modbusPort;      // Modbus port (0-3)
    uint8_t slaveID;         // Modbus slave ID
    char name[40];           // Device name
    // Device-specific parameters can follow
};
```

#### DEVICE_STATUS (0x43)
```cpp
struct IPC_DeviceStatus_t {
    uint16_t assignedIndex;  // Index assigned to device
    uint8_t success;         // Creation/operation success
    char message[100];       // Status/error message
};
```

### 4.6 Fault & Message Notifications

#### FAULT_NOTIFY (0x50)
```cpp
struct IPC_FaultNotify_t {
    uint16_t index;
    uint8_t objectType;
    uint8_t severity;        // 0=info, 1=warning, 2=error, 3=critical
    char message[100];
    uint32_t timestamp;
};
```

---

## 5. OBJECT INDEX MANAGEMENT

### 5.1 Index Organization

**Fixed Indices (0-39):** Reserved for onboard hardware
- 0-7: Analog Inputs (ADC channels 1-8)
- 8-9: Analog Outputs (DAC channels 1-2)
- 10-12: RTD Temperature Sensors (3x MAX31865)
- 13-20: Digital GPIO (8 main)
- 21-25: Digital Outputs (4 standard + 1 heater)
- 26: Stepper Motor
- 27-30: DC Motors (4x DRV8235)
- 31-32: Power Sensors (2x INA260)
- 33-36: Modbus Ports (4 ports)
- 37-39: Reserved for future onboard devices

**Dynamic Indices (40-79):** User-created peripheral devices
- Assigned sequentially as devices are created
- Indices recycled when devices are deleted

### 5.2 Index Synchronization Protocol

**Initial Sync (on startup/reconnection):**
1. RP2040 sends `IPC_MSG_INDEX_SYNC_REQ`
2. SAME51 responds with multiple `IPC_MSG_INDEX_SYNC_DATA` packets
3. Each packet contains up to 10 index entries
4. RP2040 rebuilds its index table from received data

**Runtime Updates:**
- When a device is created: SAME51 sends `IPC_MSG_INDEX_ADD`
- When a device is deleted: SAME51 sends `IPC_MSG_INDEX_REMOVE`
- When object metadata changes: SAME51 sends `IPC_MSG_INDEX_UPDATE`

### 5.3 Type Safety
Every message that references an object index includes the `objectType` field for verification:
- If type mismatch detected → reject message, send `IPC_MSG_ERROR`
- Prevents commands intended for one device type being sent to wrong device

---

## 6. DRIVER ARCHITECTURE

### 6.1 File Structure
```
src/drivers/
├── ipc/
│   ├── drv_ipc.h          # IPC driver header
│   ├── drv_ipc.cpp        # IPC driver implementation
│   ├── ipc_protocol.h     # Protocol definitions (structs, enums)
│   └── ipc_handlers.cpp   # Message handler functions
```

### 6.2 IPC Driver Structure

```cpp
// IPC driver state machine
enum IPC_State {
    IPC_STATE_IDLE,
    IPC_STATE_RECEIVING,
    IPC_STATE_PROCESSING,
    IPC_STATE_ERROR
};

// IPC driver structure
struct IPC_Driver_t {
    // Hardware interface
    HardwareSerial *uart;
    
    // State machine
    IPC_State state;
    uint32_t lastActivity;
    
    // RX buffer and packet parsing
    uint8_t rxBuffer[1280];      // Max packet: 8 header + 1024 payload + 256 stuffing
    uint16_t rxBufferPos;
    bool escapeNext;
    
    // TX queue
    struct TxPacket_t {
        uint8_t msgType;
        uint16_t payloadLen;
        uint8_t payload[1024];
    };
    TxPacket_t txQueue[8];
    uint8_t txQueueHead;
    uint8_t txQueueTail;
    
    // Statistics
    uint32_t rxPacketCount;
    uint32_t txPacketCount;
    uint32_t rxErrorCount;
    uint32_t txErrorCount;
    
    // Callbacks
    void (*onPacketReceived)(uint8_t msgType, uint8_t *payload, uint16_t len);
    void (*onError)(uint8_t errorCode);
};

extern IPC_Driver_t ipcDriver;
```

### 6.3 Key Functions

```cpp
// Initialization
bool ipc_init(void);

// Non-blocking update (called from task)
void ipc_update(void);

// Packet transmission
bool ipc_sendPacket(uint8_t msgType, const uint8_t *payload, uint16_t len);

// Helper functions for common operations
bool ipc_sendSensorData(uint16_t index);
bool ipc_sendSensorBatch(uint16_t *indices, uint8_t count);
bool ipc_sendIndexSync(void);
bool ipc_sendFault(uint16_t index, const char *message);

// CRC calculation
uint16_t ipc_calcCRC16(const uint8_t *data, uint16_t len);
```

---

## 7. NON-BLOCKING OPERATION

### 7.1 Task Scheduler Integration

Add IPC task to existing scheduler:
```cpp
ScheduledTask* ipc_task = tasks.addTask(ipc_update, 5, true, true);  // 5ms, high priority
```

### 7.2 State Machine Flow

```
IDLE:
  - Check for incoming bytes
  - If START_BYTE received → transition to RECEIVING
  - Check TX queue, send packets if available

RECEIVING:
  - Read bytes into rxBuffer
  - Handle byte stuffing (escape sequences)
  - If END_BYTE received → transition to PROCESSING
  - If timeout or buffer overflow → transition to ERROR

PROCESSING:
  - Validate CRC
  - Parse message type and payload
  - Call appropriate handler function
  - Transition to IDLE

ERROR:
  - Clear buffers
  - Increment error counter
  - Send error notification (if possible)
  - Transition to IDLE
```

### 7.3 Performance Targets

At 1 Mbps with 8N1:
- Bit time: 1 µs
- Byte time: 10 µs (8 data + 1 start + 1 stop)
- 100 bytes: ~1 ms transmission time

**Expected CPU Usage:**
- IPC task @ 5ms: < 0.5% CPU (based on similar UART tasks)
- Packet processing: < 100 µs per packet

---

## 8. ERROR HANDLING

### 8.1 Error Types

```cpp
enum IPC_ErrorCode : uint8_t {
    IPC_ERR_CRC_FAIL       = 0x01,  // CRC validation failed
    IPC_ERR_INVALID_MSG    = 0x02,  // Unknown message type
    IPC_ERR_BUFFER_FULL    = 0x03,  // RX buffer overflow
    IPC_ERR_TIMEOUT        = 0x04,  // Packet receive timeout
    IPC_ERR_TYPE_MISMATCH  = 0x05,  // Object type mismatch
    IPC_ERR_INDEX_INVALID  = 0x06,  // Invalid object index
    IPC_ERR_QUEUE_FULL     = 0x07,  // TX queue full
    IPC_ERR_DEVICE_FAIL    = 0x08,  // Device creation/operation failed
};
```

### 8.2 Error Recovery

- **CRC errors:** Discard packet, increment counter, continue
- **Buffer overflow:** Clear buffer, send error notification, resync
- **Timeout:** Clear partial packet, return to IDLE
- **Type mismatch:** Reject command, send error response
- **Queue full:** Drop oldest packet (with warning)

### 8.3 Keepalive & Reconnection

- RP2040 sends `IPC_MSG_PING` every 1 second
- SAME51 responds with `IPC_MSG_PONG`
- If 3 consecutive pings missed → trigger reconnection
- Reconnection includes full index resync

---

## 9. IMPLEMENTATION PHASES

### Phase 1: Core Protocol ✅
- [x] Define packet structure
- [x] Define message types
- [x] Design payload structures
- [x] Plan error handling

### Phase 2: Driver Implementation
- [ ] Create `drv_ipc.h` with structures and enums
- [ ] Implement `ipc_init()` and UART configuration
- [ ] Implement packet framing and byte stuffing
- [ ] Implement CRC16 calculation
- [ ] Create TX queue management

### Phase 3: Message Handlers
- [ ] Implement handshake handlers (HELLO, PONG)
- [ ] Implement index sync handlers
- [ ] Implement sensor read/write handlers
- [ ] Implement control read/write handlers
- [ ] Implement device management handlers

### Phase 4: Integration
- [ ] Add IPC task to scheduler
- [ ] Integrate with object index system
- [ ] Add helper functions for common operations
- [ ] Create test message sequences

### Phase 5: Testing & Optimization
- [ ] Test packet transmission/reception
- [ ] Test error handling and recovery
- [ ] Test full index synchronization
- [ ] Test device creation/deletion
- [ ] Measure CPU usage and optimize
- [ ] Stress test with high message rates

---

## 10. SCALABILITY CONSIDERATIONS

### 10.1 Future Expansion
- **Protocol versioning:** `protocolVersion` in HELLO allows future changes
- **Extended message types:** 256 possible message types (0x00-0xFF)
- **Extended object types:** Can expand ObjectType enum as needed
- **Larger payloads:** Max 1024 bytes allows complex data structures

### 10.2 Optimization Opportunities
- **Compression:** Add optional compression for large payloads
- **Batching:** Group multiple sensor reads into single batch packets
- **Priority queues:** Separate queues for high/low priority messages
- **Flow control:** Add flow control if one side gets overwhelmed

---

## 11. OPEN QUESTIONS FOR DISCUSSION

1. **Index Assignment Strategy:** Should SAME51 assign indices, or should RP2040 request specific indices?
   - **Recommendation:** SAME51 assigns to prevent conflicts
   
2. **Streaming vs. Polling:** Should sensors stream data periodically, or only respond to requests?
   - **Recommendation:** Hybrid - critical sensors stream, others poll on demand
   
3. **Timestamp Source:** Should SAME51 include timestamps, or does RP2040 add them (has RTC)?
   - **Recommendation:** Optional field, RP2040 adds if needed (it has RTC)
   
4. **Flash Configuration:** Should index mappings be stored in SAME51 flash for persistence?
   - **Recommendation:** Not initially - rebuild on startup, add later if needed
   
5. **Calibration Protocol:** How should calibration data be synchronized?
   - **Recommendation:** Use CONFIG_WRITE/CONFIG_READ messages with calibration-specific payload

6. **Multi-sensor Objects:** Some devices provide multiple sensors (pH probe = pH + temp). How to handle?
   - **Recommendation:** Create separate index entries for each sensor, link via naming convention

---

## 12. COMPARISON WITH ALTERNATIVES

### Why Not Modbus RTU?
- ❌ Higher overhead (RTU frame + function codes)
- ❌ Limited to predefined register maps
- ❌ Not optimized for heterogeneous objects
- ✅ But: Well-tested, standardized

### Why Not JSON over UART?
- ❌ Text encoding is inefficient (3-5x larger)
- ❌ Parsing overhead on MCU
- ❌ No built-in error checking
- ✅ But: Human-readable, easy to debug

### Why Custom Binary Protocol?
- ✅ Minimal overhead (8 bytes)
- ✅ Efficient binary encoding
- ✅ CRC16 error detection
- ✅ Tailored to our object system
- ✅ Extensible message types
- ❌ But: Need to implement both ends

---

## 13. NEXT STEPS

1. **Review this plan** with stakeholder
2. **Refine** based on feedback
3. **Implement Phase 2** (Core driver)
4. **Create test harness** for validation
5. **Document** as we build

---

**Author:** Cascade AI Assistant  
**Review Status:** Awaiting approval
