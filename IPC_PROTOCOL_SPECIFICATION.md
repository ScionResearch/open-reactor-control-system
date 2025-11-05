# Inter-Processor Communication (IPC) Protocol Specification
**Version:** 2.5  
**Date:** 2025-10-27  
**Status:** ✅ Implemented & Operational

---

## 1. OVERVIEW

### 1.1 Purpose
High-speed, robust binary communication protocol between:
- **SAME51N20A** (I/O Controller) - Sensors, actuators, peripherals
- **RP2040** (System Controller) - Network, MQTT, SD, RTC, web UI

### 1.2 Physical Interface
- **Medium:** UART (Serial1)
- **Pins (SAME51):** PIN_MI_TX (52), PIN_MI_RX (53)
- **Pins (RP2040):** PIN_SI_TX (16), PIN_SI_RX (17)
- **Trace Length:** ~80mm PCB trace
- **Baud Rate:** **2,000,000 bps (2 Mbps)** ⚡
  - Tested reliable up to 3 Mbps
  - 2 Mbps selected for robust operation
- **Configuration:** 8N1 (8 data bits, no parity, 1 stop bit)

### 1.3 Design Principles
- ✅ **Lightweight:** 8-byte overhead per packet
- ✅ **Robust:** CRC16-CCITT error detection
- ✅ **Scalable:** Extensible message types, 100-object index
- ✅ **Non-blocking:** Async state machine, TX queue
- ✅ **Fast:** 2 Mbps, minimal latency (<1ms per packet)
- ✅ **Type-safe:** Object type verification on all operations

---

## 2. PACKET STRUCTURE

### 2.1 Binary Frame Format

```
┌──────────┬──────────┬──────────┬──────────┬────────────┬──────────┐
│  START   │  LENGTH  │   MSG    │ PAYLOAD  │    CRC16   │   END    │
│  BYTE    │ (2 bytes)│   TYPE   │ (N bytes)│  (2 bytes) │   BYTE   │
├──────────┼──────────┼──────────┼──────────┼────────────┼──────────┤
│   0x7E   │  uint16  │  uint8   │  0-1024  │   uint16   │   0x7E   │
└──────────┴──────────┴──────────┴──────────┴────────────┴──────────┘

Total overhead: 8 bytes per packet
```

**Field Definitions:**
- **START (0x7E):** Frame delimiter (same for start and end)
- **LENGTH:** Payload size including MSG_TYPE (big-endian uint16)
- **MSG_TYPE:** Message identifier (uint8, see section 3)
- **PAYLOAD:** Variable data (0-1024 bytes max)
- **CRC16:** CRC16-CCITT of LENGTH + MSG_TYPE + PAYLOAD
- **END (0x7E):** Frame delimiter

### 2.2 Byte Stuffing
To prevent frame marker confusion in payload:
- **0x7E** in data → **0x7D 0x5E** (escape + XOR 0x20)
- **0x7D** in data → **0x7D 0x5D** (escape + XOR 0x20)

Applied to: LENGTH, MSG_TYPE, PAYLOAD, CRC fields

### 2.3 CRC16-CCITT
- **Polynomial:** 0x1021
- **Initial Value:** 0xFFFF
- **Coverage:** LENGTH (2) + MSG_TYPE (1) + PAYLOAD (N) bytes
- **Transmission:** Big-endian (MSB first)

---

## 3. MESSAGE TYPES

### 3.1 Message Type Enumeration

```cpp
enum IPC_MsgType : uint8_t {
    // Handshake & Status (0x00-0x0F)
    IPC_MSG_PING            = 0x00,  // Keepalive ping
    IPC_MSG_PONG            = 0x01,  // Ping response ✅ CRITICAL
    IPC_MSG_HELLO           = 0x02,  // Initial handshake
    IPC_MSG_HELLO_ACK       = 0x03,  // Handshake acknowledgment
    IPC_MSG_ERROR           = 0x04,  // Error notification
    
    // Object Index Management (0x10-0x1F)
    IPC_MSG_INDEX_SYNC_REQ  = 0x10,  // Request full index sync
    IPC_MSG_INDEX_SYNC_DATA = 0x11,  // Index sync data packet
    IPC_MSG_INDEX_ADD       = 0x12,  // Add object to index
    IPC_MSG_INDEX_REMOVE    = 0x13,  // Remove object from index
    IPC_MSG_INDEX_UPDATE    = 0x14,  // Update object metadata
    
    // Sensor Data (0x20-0x2F)
    IPC_MSG_SENSOR_READ_REQ      = 0x20,  // Request sensor reading
    IPC_MSG_SENSOR_DATA          = 0x21,  // Sensor data response
    IPC_MSG_SENSOR_STREAM        = 0x22,  // Continuous streaming
    IPC_MSG_SENSOR_BATCH         = 0x23,  // Batch sensor data
    IPC_MSG_SENSOR_BULK_READ_REQ = 0x24,  // ✅ Bulk read request (multiple sensors)
    
    // Control Data (0x30-0x3F)
    IPC_MSG_CONTROL_WRITE   = 0x30,  // Write setpoint/parameter
    IPC_MSG_CONTROL_ACK     = 0x31,  // Write acknowledgment
    IPC_MSG_CONTROL_READ    = 0x32,  // Read control parameters
    IPC_MSG_CONTROL_DATA    = 0x33,  // Control data response
    
    // Device Management (0x40-0x4F)
    IPC_MSG_DEVICE_CREATE   = 0x40,  // Create peripheral device
    IPC_MSG_DEVICE_DELETE   = 0x41,  // Delete peripheral device
    IPC_MSG_DEVICE_CONFIG   = 0x42,  // Configure device
    IPC_MSG_DEVICE_STATUS   = 0x43,  // Device status response
    
    // Fault & Message (0x50-0x5F)
    IPC_MSG_FAULT_NOTIFY    = 0x50,  // Fault notification
    IPC_MSG_MESSAGE_NOTIFY  = 0x51,  // General message
    IPC_MSG_FAULT_CLEAR     = 0x52,  // Clear fault
    
    // Configuration (0x60-0x6F)
    IPC_MSG_CONFIG_READ     = 0x60,  // Read configuration
    IPC_MSG_CONFIG_WRITE    = 0x61,  // Write configuration
    IPC_MSG_CONFIG_DATA     = 0x62,  // Configuration data
    IPC_MSG_CALIBRATE       = 0x63,  // Calibration command
};
```

### 3.2 Message Flow Examples

#### Startup Handshake
```
RP2040 → SAME51: IPC_MSG_HELLO (protocol v1.0.0, firmware v1.0.1)
SAME51 → RP2040: IPC_MSG_HELLO_ACK (firmware v1.0.0, 64 objects max)
RP2040 → SAME51: IPC_MSG_INDEX_SYNC_REQ
SAME51 → RP2040: IPC_MSG_INDEX_SYNC_DATA (entries 0-9)
SAME51 → RP2040: IPC_MSG_INDEX_SYNC_DATA (entries 10-19)
...
```

#### Keepalive (Every 1 Second)
```
SAME51 → RP2040: IPC_MSG_PING
RP2040 → SAME51: IPC_MSG_PONG
```
⚠️ **Connection timeout:** 3 missed PINGs (3 seconds)

---

## 4. PAYLOAD STRUCTURES

### 4.1 Handshake Messages

#### HELLO (0x02)
```cpp
struct IPC_Hello_t {
    uint32_t protocolVersion;  // e.g., 0x00010000 = v1.0.0
    uint32_t firmwareVersion;  // e.g., 0x00010001 = v1.0.1
    char deviceName[32];       // "SAME51-IO-MCU" or "RP2040-ORC-SYS"
} __attribute__((packed));
```

#### HELLO_ACK (0x03)
```cpp
struct IPC_HelloAck_t {
    uint32_t protocolVersion;
    uint32_t firmwareVersion;
    uint16_t maxObjectCount;      // SAME51: 64 (MAX_NUM_OBJECTS)
    uint16_t currentObjectCount;  // Currently registered objects
} __attribute__((packed));
```

### 4.2 Object Index Messages

#### INDEX_SYNC_DATA (0x11)
```cpp
struct IPC_IndexEntry_t {
    uint16_t index;           // 0-79
    uint8_t objectType;       // ObjectType enum
    uint8_t flags;            // Bit 0: valid, Bit 1: fixed
    char name[40];            // Null-terminated name
    char unit[8];             // Unit string (if applicable)
} __attribute__((packed));

struct IPC_IndexSync_t {
    uint16_t packetNum;       // Current packet (0-based)
    uint16_t totalPackets;    // Total packets in sync
    uint8_t entryCount;       // Entries in this packet
    IPC_IndexEntry_t entries[10];  // Up to 10 entries/packet
} __attribute__((packed));
```

### 4.3 Sensor Data Messages

#### SENSOR_DATA (0x21)
```cpp
struct IPC_SensorData_t {
    uint16_t index;          // Object index
    uint8_t objectType;      // Type verification
    uint8_t flags;           // Bit 0: fault, Bit 1: newMessage, Bit 2: running, Bit 3: direction
    float value;             // Primary sensor value
    char unit[8];            // Unit string (for primary value)
    uint32_t timestamp;      // Optional (0 if unused)
    char message[100];       // Optional message
    
    // Multi-value extension (v2.3+)
    uint8_t valueCount;      // Number of additional values (0 = only primary value)
    float additionalValues[4];     // Up to 4 additional values
    char additionalUnits[4][8];    // Units for each additional value
} __attribute__((packed));
```

**Multi-Value Support (v2.3):**
- Complex objects can now transmit multiple related values in a single message
- `valueCount = 0`: Traditional single-value mode (backward compatible)
- `valueCount > 0`: Additional values available in `additionalValues[]` array
- Each additional value has its own unit string in `additionalUnits[]`

**Example Use Cases:**
- **DC Motors:** Primary = power (%), Additional[0] = current (A)
- **Energy Monitors:** Primary = voltage (V), Additional[0] = current (A), Additional[1] = power (W)
- **Hamilton Probes:** Primary = pH/DO, Additional[0] = temperature (°C)
- **Stepper Motor:** Primary = RPM, Additional[0] = current (A)

**Benefits:**
- Single IPC message instead of multiple sensor objects
- Atomically consistent (all values from same measurement cycle)
- More efficient use of object index space
- Reduced IPC traffic

#### SENSOR_BULK_READ_REQ (0x24) ✅ NEW
**Purpose:** Request multiple sensor readings in one operation

```cpp
struct IPC_SensorBulkReadReq_t {
    uint16_t startIndex;     // Starting object index (e.g., 0)
    uint16_t count;          // Number of objects to read (e.g., 21)
} __attribute__((packed));
```

**Response:** Multiple `IPC_MSG_SENSOR_DATA` (0x21) packets sent sequentially

**Example Flow:**
```
RP2040 → SAME51: SENSOR_BULK_READ_REQ (startIndex=0, count=31)
SAME51 → RP2040: SENSOR_DATA (index=0, ADC 1)
SAME51 → RP2040: SENSOR_DATA (index=1, ADC 2)
...
SAME51 → RP2040: SENSOR_DATA (index=20, GPIO 8)
SAME51 → RP2040: SENSOR_DATA (index=21, Digital Output 1)
...
SAME51 → RP2040: SENSOR_DATA (index=30, DC Motor 4)
```

**TX Queue Management:** 
- IO MCU TX queue limited to 8 packets
- Handler actively drains queue by calling `ipc_processTxQueue()` between sends
- Waits for queue space before sending each response (prevents overflow)
- Typical performance: 31 objects read in ~30-40ms

**Benefits:**
- Single 4-byte request vs 31 individual requests (96% reduction)
- Prevents RX buffer overflow on SYS MCU
- Efficient bulk object polling for cache updates
- Used by background polling: 31 objects (sensors + outputs) updated every 1 second
- Provides real-time output status (running state, power, current, etc.)

### 4.4 Control Messages 🚧 IN PROGRESS

#### CONTROL_WRITE (0x30) - Digital Output Control
**Purpose:** Control digital outputs (on/off, PWM duty cycle)

```cpp
struct IPC_DigitalOutputControl_t {
    uint16_t index;          // Output index (21-25)
    uint8_t objectType;      // Type verification (OBJ_T_DIGITAL_OUTPUT)
    uint8_t command;         // Command type (see below)
    bool state;              // Output state (true=on, false=off)
    float pwmDuty;           // PWM duty cycle (0-100%)
    uint8_t reserved[3];     // Padding for alignment
} __attribute__((packed));
```

**Command Types:**
```cpp
enum DigitalOutputCommand : uint8_t {
    DOUT_CMD_SET_STATE = 0x01,   // Set on/off state
    DOUT_CMD_SET_PWM   = 0x02,   // Set PWM duty cycle
    DOUT_CMD_DISABLE   = 0x03,   // Disable output
};
```

#### CONTROL_WRITE (0x30) - Stepper Motor Control
**Purpose:** Control stepper motor parameters and motion

```cpp
struct IPC_StepperControl_t {
    uint16_t index;          // Stepper index (26)
    uint8_t objectType;      // Type verification (OBJ_T_STEPPER_MOTOR)
    uint8_t command;         // Command type (see below)
    float rpm;               // Target RPM
    bool direction;          // true=forward, false=reverse
    bool enable;             // Enable motor
    uint8_t reserved[2];     // Padding
} __attribute__((packed));
```

**Command Types:**
```cpp
enum StepperCommand : uint8_t {
    STEPPER_CMD_SET_RPM   = 0x01,  // Set target RPM
    STEPPER_CMD_SET_DIR   = 0x02,  // Set direction
    STEPPER_CMD_START     = 0x03,  // Start motor
    STEPPER_CMD_STOP      = 0x04,  // Stop motor
    STEPPER_CMD_UPDATE    = 0x05,  // Update RPM while running
};
```

#### CONTROL_WRITE (0x30) - DC Motor Control
**Purpose:** Control DC motor power and direction

```cpp
struct IPC_DCMotorControl_t {
    uint16_t index;          // Motor index (27-30)
    uint8_t objectType;      // Type verification (OBJ_T_BDC_MOTOR)
    uint8_t command;         // Command type (see below)
    float power;             // Power percentage (0-100%)
    bool direction;          // true=forward, false=reverse
    bool enable;             // Enable motor
    uint8_t reserved[2];     // Padding
} __attribute__((packed));
```

**Command Types:**
```cpp
enum DCMotorCommand : uint8_t {
    DCMOTOR_CMD_SET_POWER = 0x01,  // Set power percentage
    DCMOTOR_CMD_SET_DIR   = 0x02,  // Set direction
    DCMOTOR_CMD_START     = 0x03,  // Start motor
    DCMOTOR_CMD_STOP      = 0x04,  // Stop motor
    DCMOTOR_CMD_UPDATE    = 0x05,  // Update power while running
};
```

#### CONTROL_ACK (0x31) - Command Acknowledgment
**Purpose:** Acknowledge control command success/failure

```cpp
struct IPC_ControlAck_t {
    uint16_t index;          // Object index
    uint8_t objectType;      // Object type
    uint8_t command;         // Command that was executed
    bool success;            // true=success, false=failure
    uint8_t errorCode;       // Error code if success=false
    char message[100];       // Status/error message
} __attribute__((packed));
```

**Error Codes:**
```cpp
enum ControlErrorCode : uint8_t {
    CTRL_ERR_NONE           = 0x00,  // No error
    CTRL_ERR_INVALID_INDEX  = 0x01,  // Invalid object index
    CTRL_ERR_TYPE_MISMATCH  = 0x02,  // Object type mismatch
    CTRL_ERR_INVALID_CMD    = 0x03,  // Invalid command
    CTRL_ERR_OUT_OF_RANGE   = 0x04,  // Parameter out of range
    CTRL_ERR_NOT_ENABLED    = 0x05,  // Device not enabled
    CTRL_ERR_DRIVER_FAULT   = 0x06,  // Hardware driver fault
    CTRL_ERR_TIMEOUT        = 0x07,  // Command timeout
};
```

### 4.5 Configuration Messages ✅ IMPLEMENTED

#### CONFIG_ANALOG_INPUT (ADC Configuration)
```cpp
struct IPC_ConfigAnalogInput_t {
    uint8_t index;           // ADC index (0-7)
    char unit[8];            // Unit string (e.g., "mV", "V", "mA")
    float calScale;          // Calibration scale factor
    float calOffset;         // Calibration offset
} __attribute__((packed));
```

#### CONFIG_RTD (Temperature Sensor Configuration)
```cpp
struct IPC_ConfigRTD_t {
    uint8_t index;           // RTD index (10-12)
    char unit[8];            // Unit string (e.g., "C", "degC")
    uint8_t sensorType;      // 0=PT100, 1=PT1000
    uint8_t wireMode;        // 2=2-wire, 3=3-wire, 4=4-wire
    float calScale;          // Calibration scale factor
    float calOffset;         // Calibration offset
} __attribute__((packed));
```

**Configuration Flow:**
```
RP2040 → SAME51: CONFIG_ANALOG_INPUT (index=0, unit="V", scale=1.0, offset=0.0)
SAME51: Updates ADC configuration in real-time
```

---

## 5. OBJECT INDEX SYSTEM

### 5.1 Index Organization (80 Objects Total)

The object index provides a unified addressing scheme for all sensors, actuators, and control loops accessible via IPC.

**Fixed Indices (0-59):** Onboard SAME51 hardware & control objects

#### **Analog I/O (0-9)**
- **0-7:** Analog Inputs (ADC channels 1-8) - MCP3464
- **8-9:** Analog Outputs (DAC channels 1-2) - MCP48FEB

#### **Temperature Sensors (10-12)**
- **10-12:** RTD Temperature Sensors (PT100/PT1000 1-3) - MAX31865

#### **Digital I/O (13-20)**
- **13-20:** Digital GPIO (8 channels) - General purpose I/O

#### **Digital Outputs (21-25)**
- **21-24:** Digital Outputs (4 channels) - Open-drain, PWM capable
- **25:** Heater Output (1 channel) - High current, 1Hz PWM

#### **Motion Control (26-30)**
- **26:** Stepper Motor - TMC5130 driver
- **27-30:** DC Motors (4 channels) - DRV8235 drivers

#### **Energy Monitoring (31-32)**
- **31:** Main Power Monitor - INA260 sensor 1 (voltage V, current A, power W)
- **32:** Heater Power Monitor - INA260 sensor 2 (voltage V, current A, power W)

#### **Communication Ports (33-36)**
- **33:** Modbus Port 1 (RS-232) - Serial2
- **34:** Modbus Port 2 (RS-232) - Serial3
- **35:** Modbus Port 3 (RS-485) - Serial4
- **36:** Modbus Port 4 (RS-485) - Serial5

#### **Onboard Device Feedback (37-39)** 🚧 Reserved
- **37-39:** Reserved for additional onboard device status/feedback

#### **Controller Objects (40-49)** **WIP**
- **40-42:** Temperature Control (3 loops) - PID controllers
- **43:** pH Control - Dosing control
- **44-47:** Feed/waste Control - pumps
- **48:** Dissolved Oxygen Control - Gas mixing + stirrer
- **49:** Reserved

**Device Control Objects (50-69):** Peripheral device control
- Control interface for dynamic devices (setpoints, commands, status)
- Each device gets one control object (MFC setpoint, pump speed, etc.)
- Linked to sensor objects via `startSensorIndex` field
- **20 slots available** for device control

**Device Sensor Objects (70-99):** User-created peripheral sensors
- Sensor readings from dynamic devices (pH, DO, OD, flow, pressure, etc.)
- User-created via `IPC_MSG_DEVICE_CREATE`
- Assigned sequentially starting at 70, recycled on deletion
- Each device can have 1-4 sensors (e.g., MFC: flow + pressure)
- **30 slots available** for device sensors

### 5.2 Object Type Mapping

Each index has an associated `ObjectType` from the enum:

```cpp
// Sensors
OBJ_T_ANALOG_INPUT              // Indices 0-7
OBJ_T_ANALOG_OUTPUT             // Indices 8-9
OBJ_T_TEMPERATURE_SENSOR        // Indices 10-12, 60+
OBJ_T_DIGITAL_INPUT             // Indices 13-20
OBJ_T_DIGITAL_OUTPUT            // Indices 21-25
OBJ_T_ENERGY_SENSOR             // Indices 31-32 (multi-value: V, A, W)
OBJ_T_VOLTAGE_SENSOR            // Reserved for future use
OBJ_T_CURRENT_SENSOR            // Reserved for future use
OBJ_T_POWER_SENSOR              // Reserved for future use

// Motion
OBJ_T_STEPPER_MOTOR             // Index 26
OBJ_T_BDC_MOTOR                 // Indices 27-30

// Communication
OBJ_T_SERIAL_RS232_PORT         // Indices 33-34
OBJ_T_SERIAL_RS485_PORT         // Indices 35-36

// Controllers (40-49) - Future
OBJ_T_TEMPERATURE_CONTROL       // Indices 40-42
OBJ_T_PH_CONTROL                // Index 43
OBJ_T_DISSOLVED_OXYGEN_CONTROL  // Index 44
OBJ_T_OPTICAL_DENSITY_CONTROL   // Index 45
OBJ_T_GAS_FLOW_CONTROL          // Indices 46-47
OBJ_T_PUMP_CONTROL              // Index 48
OBJ_T_FEED_WASTE_CONTROL        // Index 49

// Device Controls (50-69) - ✅ Active
OBJ_T_DEVICE_CONTROL            // Control/status for peripheral devices

// Device Sensors (70-99) - ✅ Active
OBJ_T_PH_SENSOR                 // Indices 70+ (dynamic)
OBJ_T_DISSOLVED_OXYGEN_SENSOR   // Indices 70+ (dynamic)
OBJ_T_OPTICAL_DENSITY_SENSOR    // Indices 70+ (dynamic)
OBJ_T_FLOW_SENSOR               // Indices 70+ (dynamic)
OBJ_T_PRESSURE_SENSOR           // Indices 70+ (dynamic)
OBJ_T_TEMPERATURE_SENSOR        // Indices 10-12, 70+ (dynamic)
```

### 5.3 Unit Strings

Standard unit strings for sensor data:

| Object Type | Primary Unit | Notes |
|-------------|-------------|-------|
| Analog Input | `mV` | Millivolts (configurable) |
| Analog Output | `mV` | Millivolts (0-10240 mV) |
| Temperature | `°C` | Celsius (configurable to °F/K) |
| Digital I/O | ` ` | Empty (boolean state) |
| Digital Output | ` ` | Empty (boolean state) |
| Energy Monitor | `V` | Primary: Volts, Additional: A, W |
| Stepper Motor | `rpm` | Revolutions per minute |
| DC Motor | `%` | Power percentage |
| Modbus Port | ` ` | Empty (status/config) |
| pH Sensor | `pH` | pH units (0-14) |
| DO Sensor | `mg/L` or `%` | Dissolved oxygen |
| OD Sensor | `OD` or `AU` | Optical density |
| Flow Sensor | `SLPM` | Standard liters per minute |
| Pressure Sensor | `kPa` or `bar` | Pressure units |

### 5.4 Type Safety
Every command includes `objectType` field:
- Prevents wrong commands to wrong device types
- Mismatch → `IPC_MSG_ERROR` with code `IPC_ERR_TYPE_MISMATCH`
- Validated on both SAME51 and RP2040 sides

### 5.5 Dual-Index Device Model ✅ **NEW v2.5**

Peripheral devices (MFCs, pH probes, pumps) use a **dual-index architecture** separating control from sensors:

#### **Control Object (50-69)**
- **Purpose:** Device setpoints, commands, status
- **One per device:** Single control point for the device
- **Contains:**
  - `setpoint` - Target value (flow rate, pH, etc.)
  - `actualValue` - Feedback from primary sensor
  - `connected` - Modbus/I2C connection status
  - `fault` - Fault condition flag
  - `message` - Status/error messages
  - `startSensorIndex` - Link to sensor objects (70-99)
  - `sensorCount` - Number of associated sensors

#### **Sensor Objects (70-99)**
- **Purpose:** Real-time sensor readings
- **1-4 per device:** Flow, pressure, temperature, etc.
- **Polled continuously:** Included in bulk read (indices 0-99)
- **Standard sensor format:** Value, unit, type, fault flags

#### **Example: Alicat MFC**
```
Index 50: Device Control
  - Setpoint: 100.0 SLPM
  - Actual: 99.8 SLPM (from sensor 70)
  - Connected: true
  - Fault: false
  
Index 70: Flow Sensor (OBJ_T_FLOW_SENSOR)
  - Value: 99.8
  - Unit: "SLPM"
  - Type: FLOW_SENSOR
  
Index 71: Pressure Sensor (OBJ_T_PRESSURE_SENSOR)
  - Value: 25.3
  - Unit: "PSI"
  - Type: PRESSURE_SENSOR
```

#### **Benefits:**
- ✅ **Clean separation:** Control (write) vs sensors (read)
- ✅ **Efficient polling:** Sensors in bulk read, control on-demand
- ✅ **Scalable:** 20 devices with up to 30 total sensors
- ✅ **Consistent API:** Control via `/api/device/{index}`, sensors via `/api/inputs`

---

## 6. DRIVER ARCHITECTURE

### 6.1 SAME51 Implementation

**Files:**
- `src/drivers/ipc/drv_ipc.h` - Driver structure and API
- `src/drivers/ipc/drv_ipc.cpp` - State machine, TX/RX
- `src/drivers/ipc/ipc_protocol.h` - Protocol definitions
- `src/drivers/ipc/ipc_handlers.cpp` - Message handlers

**State Machine:**
```
IDLE → Check for RX bytes, process TX queue
  ↓ (START byte detected)
RECEIVING → Accumulate bytes, unstuff, detect END
  ↓ (END byte detected)
PROCESSING → Validate CRC, dispatch to handler
  ↓
IDLE (or ERROR on failure)
```

**Key Functions:**
```cpp
bool ipc_init(void);
void ipc_update(void);  // Non-blocking, called @ 5ms
bool ipc_sendPacket(uint8_t msgType, const uint8_t *payload, uint16_t len);
bool ipc_sendPing(void);
bool ipc_sendPong(void);
bool ipc_sendHello(void);
bool ipc_isConnected(void);
```

### 6.2 RP2040 Implementation

**Files:**
- `lib/IPCprotocol/IPCProtocol.h` - Protocol class
- `lib/IPCprotocol/IPCProtocol.cpp` - Implementation
- `lib/IPCprotocol/IPCDataStructs.h` - Message structures
- `src/utils/ipcManager.cpp` - Integration layer

**Key Functions:**
```cpp
void begin(uint32_t baudRate = 2000000);
void update(void);  // Non-blocking
bool sendPacket(uint8_t messageType, const uint8_t *payload, uint16_t length);
bool sendPing(void);
bool sendHello(uint32_t protocol, uint32_t firmware, const char* name);
void registerHandler(uint8_t messageType, MessageCallback callback);
```

---

## 7. PERFORMANCE CHARACTERISTICS

### 7.1 Timing @ 2 Mbps
- **Bit time:** 0.5 µs
- **Byte time:** 5 µs (8 data + start + stop bits)
- **100-byte packet:** ~0.5 ms transmission
- **1024-byte packet:** ~5.1 ms transmission

### 7.2 CPU Usage
- **SAME51 IPC task (5ms):** <0.5% CPU
- **RP2040 IPC update:** <0.5% CPU
- **Packet processing:** 50-100 µs per packet

### 7.3 Throughput
- **Theoretical max:** ~200 KB/s (2 Mbps / 10 bits per byte)
- **Practical max:** ~150 KB/s (accounting for overhead)
- **Typical load:** <10 KB/s (sensor data + control)

---

## 8. ERROR HANDLING

### 8.1 Error Codes
```cpp
enum IPC_ErrorCode : uint8_t {
    IPC_ERR_CRC_FAIL       = 0x01,  // CRC validation failed
    IPC_ERR_INVALID_MSG    = 0x02,  // Unknown message type
    IPC_ERR_BUFFER_FULL    = 0x03,  // RX buffer overflow
    IPC_ERR_TIMEOUT        = 0x04,  // Packet timeout
    IPC_ERR_TYPE_MISMATCH  = 0x05,  // Object type mismatch
    IPC_ERR_INDEX_INVALID  = 0x06,  // Invalid object index
    IPC_ERR_QUEUE_FULL     = 0x07,  // TX queue full
    IPC_ERR_NOT_IMPLEMENTED= 0x08,  // Feature not implemented
    IPC_ERR_PARSE_FAIL     = 0x09,  // Payload parse error
};
```

### 8.2 Recovery Strategies
- **CRC error:** Discard packet, log error, continue
- **Buffer overflow:** Clear buffer, resync
- **Connection timeout:** Clear connected flag, await reconnect
- **Type mismatch:** Send error response, reject command

### 8.3 Keepalive System
- SAME51 sends PING every 1000 ms (`IPC_KEEPALIVE_MS`)
- RP2040 responds with PONG
- **Both sides** update `lastActivity` on:
  - Any successful RX packet
  - Any successful TX packet
- Connection timeout: 3000 ms (3 x keepalive interval)
- **Critical:** PONG handler must be implemented on both sides

---

## 9. IMPLEMENTATION STATUS

### 9.1 ✅ Completed Features
- [x] Binary packet framing with byte stuffing
- [x] CRC16-CCITT validation
- [x] State machine RX/TX processing
- [x] TX queue (8 packets deep) with active draining
- [x] PING/PONG keepalive with timeout
- [x] HELLO handshake with version checking
- [x] Message handler registration
- [x] Error detection and reporting
- [x] Non-blocking operation
- [x] 2 Mbps reliable communication
- [x] **Bulk sensor read (SENSOR_BULK_READ_REQ)**
- [x] **Background sensor polling (1 Hz cache updates)**
- [x] **Object cache system on SYS MCU**

### 9.2 🚧 In Progress
- [x] **Output control commands (CONTROL_WRITE, CONTROL_ACK)**
  - [x] Digital outputs (on/off, PWM)
  - [x] Stepper motor (RPM, direction, start/stop)
  - [x] DC motors (power, direction, start/stop)
- [x] **Extend bulk read to include outputs (indices 0-30)**
  - [x] Updated polling from 21 to 31 objects
  - [x] Increased UART FIFO buffer to 8192 bytes
- [ ] **Output configuration persistence on SYS MCU**
- [ ] **Output control command senders on SYS MCU**
- [ ] **Web API endpoints for output control**
- [ ] Object index synchronization (partial - fixed objects working)
- [ ] Device management (create/delete)
- [ ] Calibration protocol

### 9.3 📋 Planned Features
- [ ] Sensor batch messages (multiple sensors per packet)
- [ ] Automatic reconnection with state rebuild
- [ ] REST API for device management
- [ ] MQTT integration for sensor & output data
- [ ] Control loop objects (PID, sequencers)

---

## 10. TESTING & VALIDATION

### 10.1 Protocol Tests
- ✅ PING/PONG exchange
- ✅ HELLO handshake
- ✅ CRC validation (intentional corruption)
- ✅ Byte stuffing (0x7E, 0x7D in payload)
- ✅ Connection timeout detection
- ✅ TX/RX packet counting
- ✅ Baud rate up to 3 Mbps

### 10.2 Test Commands (RP2040 Terminal)
```
ping        - Send PING, wait for PONG
hello       - Send HELLO, initiate handshake
ipc-stats   - Display TX/RX counters, errors
```

---

## 11. CONFIGURATION

### 11.1 Compile-Time Configuration

**SAME51** (`ipc_protocol.h`):
```cpp
#define IPC_PROTOCOL_VERSION    0x00010000  // v1.0.0
#define IPC_TX_QUEUE_SIZE       8           // TX queue depth
#define IPC_KEEPALIVE_MS        1000        // Keepalive interval
#define IPC_DEBUG_ENABLED       0           // Debug output (0=off, 1=on)
#define MAX_NUM_OBJECTS         64          // Max object index
```

**RP2040** (`IPCDataStructs.h`):
```cpp
#define IPC_PROTOCOL_VERSION    0x00010000
#define IPC_TX_QUEUE_SIZE       8
#define IPC_MAX_PAYLOAD_SIZE    1024
#define IPC_MAX_HANDLERS        32
#define IPC_DEBUG_ENABLED       0
```

### 11.2 Baud Rate Configuration

**SAME51** (`drv_ipc.cpp`):
```cpp
ipcDriver.uart->begin(2000000);  // 2 Mbps
```

**RP2040** (`ipcManager.cpp`):
```cpp
ipc.begin(2000000);  // 2 Mbps
```

---

## 12. FUTURE ENHANCEMENTS

### 12.1 Protocol Extensions
- **Message priorities:** Separate queues for critical vs normal
- **Flow control:** Handshake to prevent overflow
- **Compression:** Optional payload compression for large data
- **Encryption:** Optional AES for sensitive data

### 12.2 Feature Additions
- **Streaming mode:** Continuous sensor data without requests
- **Batch operations:** Multiple commands in single packet
- **Firmware updates:** OTA update protocol over IPC
- **Diagnostics:** Built-in protocol analyzer/sniffer

---

## 13. TROUBLESHOOTING

### 13.1 Common Issues

**No communication:**
- Check UART pins (TX/RX crossed between MCUs)
- Verify baud rate matches (2 Mbps on both sides)
- Check ground connection
- Verify Serial1 on correct pins

**Connection timeout:**
- Check PONG handler is registered on both sides
- Verify keepalive is enabled (not disabled for debugging)
- Check `lastActivity` updates on RX/TX

**CRC errors:**
- Check for noise on UART lines
- Verify CRC polynomial matches (0x1021)
- Check byte stuffing implementation
- Try lower baud rate

**Missing messages:**
- Check TX queue size (may be full)
- Verify handler is registered for message type
- Check message type value matches enum
- Enable debug output (`IPC_DEBUG_ENABLED = 1`)

---

## 14. REFERENCES

### 14.1 Related Documents
- `HELLO_HANDSHAKE_IMPLEMENTED.md` - Handshake implementation guide
- `IPC_CLEANUP_COMPLETE.md` - Debug output cleanup
- `PROJECT_OVERVIEW.md` - System architecture

### 14.2 Standards
- **CRC16-CCITT:** ITU-T Recommendation V.41
- **UART:** RS-232 compatible (8N1)

---

**Document Version:** 2.4  
**Protocol Version:** v1.0.0  
**Last Updated:** 2025-10-25  
**Status:** ✅ Operational at 2 Mbps with multi-value sensor data + output control  
**Maintainer:** Open Reactor Control System Team

**Recent Updates (v2.4):**
- Consolidated power sensors from 6 objects to 2 energy monitors (indices 31-32)
- Added `OBJ_T_ENERGY_SENSOR` type with multi-value support (voltage, current, power)
- Shifted COM ports from indices 37-40 to 33-36
- Reserved indices 37-40 for future expansion
- Deprecated individual voltage/current/power sensor types (kept for future use)

**Previous Updates (v2.3):**
- Added multi-value sensor data extension to `IPC_SensorData_t`
- Implemented `valueCount`, `additionalValues[]`, and `additionalUnits[]` fields
- DC motors now transmit power (%) + current (A) in single message
- Backward compatible with single-value objects
- Fixed DRV8235 library bug (static buffer sharing across motor instances)

**Previous Updates (v2.2):**
- Added CONTROL_WRITE (0x30) message structures for outputs
- Added CONTROL_ACK (0x31) acknowledgment structure
- Defined control commands for digital outputs, stepper motor, DC motors
- Added error codes for control command failures
- Updated implementation status for output control feature

**Previous Updates (v2.1):**
- Added SENSOR_BULK_READ_REQ message type (0x24)
- Implemented TX queue draining to handle bulk responses
- Added background sensor polling architecture
- Documented object cache system on SYS MCU
