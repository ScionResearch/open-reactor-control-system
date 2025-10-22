#ifndef IPC_DATA_STRUCTS_H
#define IPC_DATA_STRUCTS_H

#include <stdint.h>

// ============================================================================
// IPC PROTOCOL DEFINITIONS - RP2040 Side
// Inter-Processor Communication between SAME51 and RP2040
// ============================================================================

// Protocol version
#define IPC_PROTOCOL_VERSION    0x00010000  // v1.0.0

// Debug configuration
#define IPC_DEBUG_ENABLED       0  // Set to 1 to enable verbose debug output

// Frame markers
#define IPC_START_BYTE          0x7E
#define IPC_END_BYTE            0x7E  // Fixed: was 0x7D (typo causing conflict with ESCAPE_BYTE)
#define IPC_ESCAPE_BYTE         0x7D
#define IPC_ESCAPE_XOR          0x20

// Buffer sizes
#define IPC_MAX_PAYLOAD_SIZE    1024
#define IPC_RX_BUFFER_SIZE      1280  // Max packet size with stuffing
#define IPC_TX_QUEUE_SIZE       8
#define IPC_MAX_PACKET_SIZE     (IPC_MAX_PAYLOAD_SIZE + 8)  // Payload + overhead

// Timing
#define IPC_TIMEOUT_MS          1000
#define IPC_KEEPALIVE_MS        1000

// Maximum object count (matches SAME51)
#define IPC_MAX_OBJECTS         80

// ============================================================================
// MESSAGE TYPES
// ============================================================================

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
    IPC_MSG_SENSOR_READ_REQ       = 0x20,  // Request sensor reading
    IPC_MSG_SENSOR_DATA           = 0x21,  // Sensor data response
    IPC_MSG_SENSOR_STREAM         = 0x22,  // Continuous sensor streaming (not implemented yet)
    IPC_MSG_SENSOR_BATCH          = 0x23,  // Batch sensor data (multiple sensors)
    IPC_MSG_SENSOR_BULK_READ_REQ  = 0x24,  // Request bulk sensor reading (range of indices)
    
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
    IPC_MSG_CONFIG_READ           = 0x60,  // Read configuration
    IPC_MSG_CONFIG_WRITE          = 0x61,  // Write configuration
    IPC_MSG_CONFIG_DATA           = 0x62,  // Configuration data
    IPC_MSG_CONFIG_ANALOG_INPUT   = 0x63,  // Configure analog input (ADC)
    IPC_MSG_CONFIG_ANALOG_OUTPUT  = 0x64,  // Configure analog output (DAC)
    IPC_MSG_CONFIG_RTD            = 0x65,  // Configure RTD temperature sensor
    IPC_MSG_CONFIG_GPIO           = 0x66,  // Configure digital GPIO
    IPC_MSG_CONFIG_DIGITAL_OUTPUT = 0x67,  // Configure digital output
    IPC_MSG_CONFIG_STEPPER        = 0x68,  // Configure stepper motor
    IPC_MSG_CONFIG_DCMOTOR        = 0x69,  // Configure DC motor
    IPC_MSG_CONFIG_COMPORT        = 0x6A,  // Configure COM port (serial)
};

// ============================================================================
// ERROR CODES
// ============================================================================

enum IPC_ErrorCode : uint8_t {
    IPC_ERR_NONE            = 0x00,  // No error
    IPC_ERR_CRC_FAIL        = 0x01,  // CRC validation failed
    IPC_ERR_INVALID_MSG     = 0x02,  // Unknown message type
    IPC_ERR_BUFFER_FULL     = 0x03,  // RX buffer overflow
    IPC_ERR_TIMEOUT         = 0x04,  // Packet receive timeout
    IPC_ERR_TYPE_MISMATCH   = 0x05,  // Object type mismatch
    IPC_ERR_INDEX_INVALID   = 0x06,  // Invalid object index
    IPC_ERR_QUEUE_FULL      = 0x07,  // TX queue full
    IPC_ERR_DEVICE_FAIL     = 0x08,  // Device creation/operation failed
    IPC_ERR_PARSE_FAIL      = 0x09,  // Payload parsing failed
    IPC_ERR_NOT_IMPLEMENTED = 0x0A,  // Feature not implemented
};

// ============================================================================
// OBJECT TYPES (MUST match SAME51 objects.h exactly!)
// ============================================================================

enum IPC_ObjectType : uint8_t {
    // Sensors
    OBJ_T_ANALOG_INPUT              = 0,
    OBJ_T_DIGITAL_INPUT             = 1,
    OBJ_T_TEMPERATURE_SENSOR        = 2,
    OBJ_T_PH_SENSOR                 = 3,
    OBJ_T_DISSOLVED_OXYGEN_SENSOR   = 4,
    OBJ_T_OPTICAL_DENSITY_SENSOR    = 5,
    OBJ_T_FLOW_SENSOR               = 6,
    OBJ_T_PRESSURE_SENSOR           = 7,
    OBJ_T_VOLTAGE_SENSOR            = 8,
    OBJ_T_CURRENT_SENSOR            = 9,
    OBJ_T_POWER_SENSOR              = 10,
    // Outputs
    OBJ_T_ANALOG_OUTPUT             = 11,
    OBJ_T_DIGITAL_OUTPUT            = 12,
    // Motion drivers
    OBJ_T_STEPPER_MOTOR             = 13,
    OBJ_T_BDC_MOTOR                 = 14,
    // External devices (high numbers to avoid conflicts)
    OBJ_T_HAMILTON_PH_PROBE         = 50,
    OBJ_T_HAMILTON_DO_PROBE         = 51,
    OBJ_T_HAMILTON_OD_PROBE         = 52,
    OBJ_T_ALICAT_MFC                = 53,
};

// ============================================================================
// PAYLOAD STRUCTURES
// ============================================================================

// Handshake messages ----------------------------------------------------

struct IPC_Hello_t {
    uint32_t protocolVersion;  // Protocol version (e.g., 0x00010000 = v1.0.0)
    uint32_t firmwareVersion;  // Firmware version
    char deviceName[32];       // Device identifier
} __attribute__((packed));

struct IPC_HelloAck_t {
    uint32_t protocolVersion;
    uint32_t firmwareVersion;
    uint16_t maxObjectCount;   // Max objects supported
    uint16_t currentObjectCount;
} __attribute__((packed));

struct IPC_Error_t {
    uint8_t errorCode;         // IPC_ErrorCode enum
    char message[100];         // Error description
} __attribute__((packed));

// Object Index messages -------------------------------------------------

struct IPC_IndexEntry_t {
    uint16_t index;           // Object index (0-79)
    uint8_t objectType;       // ObjectType enum value
    uint8_t flags;            // Bit 0: valid, Bit 1: fixed, Bit 2-7: reserved
    char name[40];            // Object name (null-terminated)
    char unit[8];             // Unit string (if applicable)
} __attribute__((packed));

struct IPC_IndexSync_t {
    uint16_t packetNum;       // Packet number (0-based)
    uint16_t totalPackets;    // Total packets in sync
    uint8_t entryCount;       // Entries in this packet
    IPC_IndexEntry_t entries[10];  // Up to 10 entries per packet
} __attribute__((packed));

struct IPC_IndexAdd_t {
    uint16_t index;
    uint8_t objectType;
    uint8_t flags;
    char name[40];
    char unit[8];
} __attribute__((packed));

struct IPC_IndexRemove_t {
    uint16_t index;
    uint8_t objectType;  // Must match for safety
} __attribute__((packed));

struct IPC_IndexUpdate_t {
    uint16_t index;
    uint8_t objectType;  // Must match for safety
    char name[40];
    char unit[8];
} __attribute__((packed));

// Sensor Data messages --------------------------------------------------

struct IPC_SensorReadReq_t {
    uint16_t index;
} __attribute__((packed));

struct IPC_SensorBulkReadReq_t {
    uint16_t startIndex;     // Starting index
    uint16_t count;          // Number of consecutive indices to read
} __attribute__((packed));

struct IPC_SensorData_t {
    uint16_t index;
    uint8_t objectType;      // Type verification
    uint8_t flags;           // Bit 0: fault, Bit 1: newMessage, Bit 2: running, Bit 3: direction
    float value;             // Primary sensor value
    char unit[8];            // Unit string (for primary value)
    uint32_t timestamp;      // Optional timestamp (0 if not used)
    char message[100];       // Optional message (if newMessage flag set)
    
    // Multi-value extension (for complex objects with multiple readings)
    uint8_t valueCount;      // Number of additional values (0 = only primary value)
    float additionalValues[4];     // Up to 4 additional values
    char additionalUnits[4][8];    // Units for each additional value
} __attribute__((packed));

struct IPC_SensorBatchEntry_t {
    uint16_t index;
    float value;
    uint8_t flags;           // Fault/message flags
} __attribute__((packed));

struct IPC_SensorBatch_t {
    uint8_t count;           // Number of sensors in batch
    IPC_SensorBatchEntry_t sensors[20];  // Up to 20 sensors per batch
} __attribute__((packed));

// Control Data messages -------------------------------------------------

// Control loop parameter types (for PID, sequencers)
enum IPC_ControlParamType : uint8_t {
    IPC_CTRL_SETPOINT       = 0x00,
    IPC_CTRL_ENABLE         = 0x01,
    IPC_CTRL_PID_KP         = 0x02,
    IPC_CTRL_PID_KI         = 0x03,
    IPC_CTRL_PID_KD         = 0x04,
    IPC_CTRL_INTERVAL       = 0x05,
    IPC_CTRL_MAX_DOSE_TIME  = 0x06,
    IPC_CTRL_DUTY           = 0x07,
    IPC_CTRL_PERCENT        = 0x08,
};

// Output control command types
enum DigitalOutputCommand : uint8_t {
    DOUT_CMD_SET_STATE = 0x01,
    DOUT_CMD_SET_PWM   = 0x02,
    DOUT_CMD_DISABLE   = 0x03,
};

enum StepperCommand : uint8_t {
    STEPPER_CMD_SET_RPM   = 0x01,
    STEPPER_CMD_SET_DIR   = 0x02,
    STEPPER_CMD_START     = 0x03,
    STEPPER_CMD_STOP      = 0x04,
    STEPPER_CMD_UPDATE    = 0x05,
};

enum DCMotorCommand : uint8_t {
    DCMOTOR_CMD_SET_POWER = 0x01,
    DCMOTOR_CMD_SET_DIR   = 0x02,
    DCMOTOR_CMD_START     = 0x03,
    DCMOTOR_CMD_STOP      = 0x04,
    DCMOTOR_CMD_UPDATE    = 0x05,
};

enum AnalogOutputCommand : uint8_t {
    AOUT_CMD_SET_VALUE = 0x01,  // Set output value in mV (0-10240)
    AOUT_CMD_DISABLE   = 0x02,  // Disable output (set to 0)
};

enum ControlErrorCode : uint8_t {
    CTRL_ERR_NONE           = 0x00,
    CTRL_ERR_INVALID_INDEX  = 0x01,
    CTRL_ERR_TYPE_MISMATCH  = 0x02,
    CTRL_ERR_INVALID_CMD    = 0x03,
    CTRL_ERR_OUT_OF_RANGE   = 0x04,
    CTRL_ERR_NOT_ENABLED    = 0x05,
    CTRL_ERR_DRIVER_FAULT   = 0x06,
    CTRL_ERR_TIMEOUT        = 0x07,
};

// Control loop write (for PID parameters)
struct IPC_ControlWrite_t {
    uint16_t index;
    uint8_t objectType;      // Type verification
    uint8_t paramType;       // IPC_ControlParamType
    float value;             // Parameter value
} __attribute__((packed));

// Digital Output Control
typedef struct __attribute__((packed)) {
    uint16_t index;          // Output index (21-25)
    uint8_t objectType;      // OBJ_T_DIGITAL_OUTPUT
    uint8_t command;         // DigitalOutputCommand
    uint8_t state;           // Output state (0=off, 1=on) - changed from bool
    uint8_t reserved1;       // Padding
    uint16_t reserved2;      // Padding
    float pwmDuty;           // PWM duty 0-100%
} IPC_DigitalOutputControl_t;

// Stepper Motor Control
typedef struct __attribute__((packed)) {
    uint16_t index;          // Stepper index (26)
    uint8_t objectType;      // OBJ_T_STEPPER_MOTOR
    uint8_t command;         // StepperCommand
    float rpm;               // Target RPM
    bool direction;          // Direction
    bool enable;             // Enable
    uint8_t reserved[2];     // Padding
} IPC_StepperControl_t;

// DC Motor Control
typedef struct __attribute__((packed)) {
    uint16_t index;          // Motor index (27-30)
    uint8_t objectType;      // OBJ_T_BDC_MOTOR
    uint8_t command;         // DCMotorCommand
    float power;             // Power 0-100%
    bool direction;          // Direction
    bool enable;             // Enable
    uint8_t reserved[2];     // Padding
} IPC_DCMotorControl_t;

// Analog Output (DAC) Control
typedef struct __attribute__((packed)) {
    uint16_t index;          // DAC index (8-9)
    uint8_t objectType;      // OBJ_T_ANALOG_OUTPUT
    uint8_t command;         // AnalogOutputCommand
    float value;             // Output value in mV (0-10240)
} IPC_AnalogOutputControl_t;

// Control Acknowledgment
struct IPC_ControlAck_t {
    uint16_t index;
    uint8_t objectType;
    uint8_t command;
    bool success;
    uint8_t errorCode;       // ControlErrorCode
    char message[100];
} __attribute__((packed));

struct IPC_ControlRead_t {
    uint16_t index;
    uint8_t objectType;      // Type verification
} __attribute__((packed));

struct IPC_ControlData_t {
    uint16_t index;
    uint8_t objectType;
    bool enabled;
    float setpoint;
    float kp;
    float ki;
    float kd;
    // Additional fields can be added based on control type
} __attribute__((packed));

// Device Management messages --------------------------------------------

enum IPC_DeviceType : uint8_t {
    IPC_DEV_HAMILTON_PH      = 0x01,
    IPC_DEV_HAMILTON_DO      = 0x02,
    IPC_DEV_HAMILTON_OD      = 0x03,
    IPC_DEV_ALICAT_MFC       = 0x04,
    // Add more device types as needed
};

struct IPC_DeviceCreate_t {
    uint8_t deviceType;      // IPC_DeviceType
    uint8_t modbusPort;      // Modbus port (0-3)
    uint8_t slaveID;         // Modbus slave ID
    char name[40];           // Device name
} __attribute__((packed));

struct IPC_DeviceDelete_t {
    uint16_t index;
    uint8_t objectType;      // Must match for safety
} __attribute__((packed));

struct IPC_DeviceStatus_t {
    uint16_t assignedIndex[4];  // Indices assigned to device (device + sensors)
    uint8_t indexCount;         // Number of indices assigned
    uint8_t success;            // Creation/operation success
    char message[100];          // Status/error message
} __attribute__((packed));

// Fault & Message notifications -----------------------------------------

enum IPC_FaultSeverity : uint8_t {
    IPC_FAULT_INFO      = 0x00,
    IPC_FAULT_WARNING   = 0x01,
    IPC_FAULT_ERROR     = 0x02,
    IPC_FAULT_CRITICAL  = 0x03,
};

struct IPC_FaultNotify_t {
    uint16_t index;
    uint8_t objectType;
    uint8_t severity;        // IPC_FaultSeverity
    char message[100];
    uint32_t timestamp;
} __attribute__((packed));

struct IPC_MessageNotify_t {
    uint16_t index;
    uint8_t objectType;
    char message[100];
    uint32_t timestamp;
} __attribute__((packed));

struct IPC_FaultClear_t {
    uint16_t index;
} __attribute__((packed));

// Configuration messages ------------------------------------------------

enum IPC_ConfigType : uint8_t {
    IPC_CFG_ANALOG_INPUT    = 0x01,
    IPC_CFG_ANALOG_OUTPUT   = 0x02,
    IPC_CFG_DIGITAL_OUTPUT  = 0x03,
    IPC_CFG_GPIO            = 0x04,
    IPC_CFG_RTD             = 0x05,
    IPC_CFG_MODBUS_PORT     = 0x06,
    IPC_CFG_CALIBRATION     = 0x07,
};

struct IPC_ConfigRead_t {
    uint16_t index;
    uint8_t configType;      // IPC_ConfigType
} __attribute__((packed));

struct IPC_ConfigWrite_t {
    uint16_t index;
    uint8_t configType;      // IPC_ConfigType
    uint8_t dataLen;         // Length of config data
    uint8_t data[200];       // Configuration data (varies by type)
} __attribute__((packed));

// ============================================================================
// HELPER MACROS
// ============================================================================

// Flag bit definitions for IPC_SensorData_t
#define IPC_SENSOR_FLAG_FAULT       (1 << 0)
#define IPC_SENSOR_FLAG_NEW_MSG     (1 << 1)
#define IPC_SENSOR_FLAG_RUNNING     (1 << 2)  // For motors: indicates running state
#define IPC_SENSOR_FLAG_DIRECTION   (1 << 3)  // For motors: direction (1=forward, 0=reverse)

// Flag bit definitions for IPC_IndexEntry_t
#define IPC_INDEX_FLAG_VALID        (1 << 0)
#define IPC_INDEX_FLAG_FIXED        (1 << 1)

// ============================================================================
// LEGACY MESSAGE TYPES (for backward compatibility with old RP2040 code)
// ============================================================================

enum MessageTypes : uint8_t {
    MSG_POWER_SENSOR = 0x80,
    MSG_TEMPERATURE_SENSOR = 0x81,
    MSG_PH_SENSOR = 0x82,
    MSG_DO_SENSOR = 0x83,
    MSG_OD_SENSOR = 0x84,
    MSG_GAS_FLOW_SENSOR = 0x85,
    MSG_PRESSURE_SENSOR = 0x86,
    MSG_STIRRER_SPEED_SENSOR = 0x87,
    MSG_WEIGHT_SENSOR = 0x88,
    MSG_TEMPERATURE_CONTROL = 0x90,
    MSG_PH_CONTROL = 0x91,
    MSG_DO_CONTROL = 0x92,
    MSG_GAS_FLOW_CONTROL = 0x93,
    MSG_STIRRER_SPEED_CONTROL = 0x94,
    MSG_PUMP_SPEED_CONTROL = 0x95,
    MSG_FEED_CONTROL = 0x96,
    MSG_WASTE_CONTROL = 0x97,
};

// Legacy sensor data structures (for backward compatibility)
struct PowerSensor {
    float voltage;
    float current;
    float power;
    bool online;
} __attribute__((packed));

struct TemperatureSensor {
    float celcius;
    bool online;
} __attribute__((packed));

struct PHSensor {
    float pH;
    bool online;
} __attribute__((packed));

struct DissolvedOxygenSensor {
    float oxygen;
    bool online;
} __attribute__((packed));

struct OpticalDensitySensor {
    float OD;
    bool online;
} __attribute__((packed));

struct GasFlowSensor {
    float mlPerMinute;
    bool online;
} __attribute__((packed));

struct PressureSensor {
    float kPa;
    bool online;
} __attribute__((packed));

struct StirrerSpeedSensor {
    float rpm;
    bool online;
} __attribute__((packed));

struct WeightSensor {
    float grams;
    bool online;
} __attribute__((packed));

// Legacy control data structures (for backward compatibility)
struct TemperatureControl {
    float sp_celcius;
    bool enabled;
} __attribute__((packed));

struct PHControl {
    float sp_pH;
    bool enabled;
} __attribute__((packed));

struct DissolvedOxygenControl {
    float sp_oxygen;
    bool enabled;
} __attribute__((packed));

struct GasFlowControl {
    float sp_mlPerMinute;
    bool enabled;
} __attribute__((packed));

struct StirrerSpeedControl {
    float sp_rpm;
    bool enabled;
} __attribute__((packed));

struct PumpSpeedControl {
    float sp_rpm;
    bool enabled;
} __attribute__((packed));

struct FeedControl {
    float sp_mlPerMinute;
    bool enabled;
} __attribute__((packed));

struct WasteControl {
    float sp_mlPerMinute;
    bool enabled;
} __attribute__((packed));

// ============================================================================
// OBJECT CONFIGURATION MESSAGES
// ============================================================================

/**
 * @brief Analog Input (ADC) configuration
 * Message type: IPC_MSG_CONFIG_ANALOG_INPUT
 */
typedef struct __attribute__((packed)) {
    uint16_t index;          // Object index (0-7 for ADC inputs)
    uint8_t _padding[2];     // Alignment padding
    char unit[8];            // Unit string (e.g., "mV", "V", "A")
    float calScale;          // Calibration scale factor
    float calOffset;         // Calibration offset
} IPC_ConfigAnalogInput_t;

/**
 * @brief Analog Output (DAC) configuration
 * Message type: IPC_MSG_CONFIG_ANALOG_OUTPUT
 */
typedef struct __attribute__((packed)) {
    uint16_t index;          // Object index (8-9 for DAC outputs)
    uint8_t _padding[2];     // Alignment padding
    char unit[8];            // Unit string (e.g., "mV", "V", "mA")
    float calScale;          // Calibration scale factor
    float calOffset;         // Calibration offset
} IPC_ConfigAnalogOutput_t;

/**
 * @brief RTD Temperature Sensor configuration
 * Message type: IPC_MSG_CONFIG_RTD
 */
typedef struct __attribute__((packed)) {
    uint16_t index;          // Object index (10-12 for RTD sensors)
    uint8_t wireConfig;      // 2, 3, or 4 wire configuration
    uint8_t _padding;        // Alignment padding
    char unit[8];            // Unit string (e.g., "C", "F", "K")
    float calScale;          // Calibration scale
    float calOffset;         // Calibration offset
    uint16_t nominalOhms;    // 100 (PT100) or 1000 (PT1000)
    uint8_t _padding2[2];    // Alignment padding
} IPC_ConfigRTD_t;

/**
 * @brief GPIO configuration
 * Message type: IPC_MSG_CONFIG_GPIO
 */
typedef struct __attribute__((packed)) {
    uint16_t index;          // Object index (13-20 for GPIO)
    char name[32];           // Custom name
    uint8_t pullMode;        // 0=None, 1=Pull-up, 2=Pull-down
    uint8_t enabled;         // Enable/disable flag
} IPC_ConfigGPIO_t;

/**
 * @brief Digital Output configuration
 * Message type: IPC_MSG_CONFIG_DIGITAL_OUTPUT
 */
typedef struct __attribute__((packed)) {
    uint16_t index;          // Object index (21-25 for digital outputs)
    char name[32];           // Custom name
    uint8_t mode;            // 0=On/Off, 1=PWM
    uint8_t enabled;         // Enable/disable flag
} IPC_ConfigDigitalOutput_t;

/**
 * @brief Stepper Motor configuration
 * Message type: IPC_MSG_CONFIG_STEPPER
 */
typedef struct __attribute__((packed)) {
    uint16_t index;          // Object index (26 for stepper motor)
    char name[32];           // Custom name
    uint16_t stepsPerRev;    // Steps per revolution (e.g., 200)
    uint16_t maxRPM;         // Maximum RPM
    uint16_t holdCurrent_mA; // Hold current in mA
    uint16_t runCurrent_mA;  // Run current in mA
    uint16_t acceleration;   // Acceleration in RPM/s
    uint8_t invertDirection; // Invert direction flag
    uint8_t enabled;         // Enable/disable flag
} IPC_ConfigStepper_t;

/**
 * @brief DC Motor configuration
 * Message type: IPC_MSG_CONFIG_DCMOTOR
 */
typedef struct __attribute__((packed)) {
    uint16_t index;          // Object index (27-30 for DC motors)
    char name[32];           // Custom name
    uint8_t invertDirection; // Invert direction flag
    uint8_t enabled;         // Enable/disable flag
} IPC_ConfigDCMotor_t;

/**
 * @brief COM Port configuration
 * Message type: IPC_MSG_CONFIG_COMPORT
 */
typedef struct __attribute__((packed)) {
    uint8_t index;           // COM port index (0-3: RS232-1,2 / RS485-1,2)
    uint32_t baudRate;       // Baud rate (1200, 2400, 4800, 9600, 19200, 38400, 57600, 115200)
    uint8_t dataBits;        // Data bits (fixed to 8 for Modbus)
    float stopBits;          // Stop bits (1.0 or 2.0)
    uint8_t parity;          // Parity: 0=none, 1=odd, 2=even
} IPC_ConfigComPort_t;

// Legacy message structure (for backward compatibility)
struct Message {
    uint8_t msgId;
    uint8_t objId;
    uint8_t dataLength;
    uint8_t data[64];
} __attribute__((packed));

#endif /* IPC_DATA_STRUCTS_H */
