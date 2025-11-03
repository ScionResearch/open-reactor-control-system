#pragma once

#include <Arduino.h>
#include "../objects.h"

// ============================================================================
// IPC PROTOCOL DEFINITIONS
// Inter-Processor Communication between SAME51 and RP2040
// ============================================================================

// Protocol version
#define IPC_PROTOCOL_VERSION    0x00010000  // v1.0.0

// Debug configuration
#define IPC_DEBUG_ENABLED       0  // Set to 1 to enable verbose debug output

// Frame markers
#define IPC_START_BYTE          0x7E
#define IPC_END_BYTE            0x7E  // Fixed: was 0x7D (typo)
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
    IPC_MSG_DEVICE_QUERY    = 0x43,  // Query device status
    IPC_MSG_DEVICE_STATUS   = 0x44,  // Device status response
    IPC_MSG_DEVICE_CONTROL  = 0x45,  // Device control command (set setpoint, etc.)
    
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
    IPC_MSG_CONFIG_TEMP_CONTROLLER = 0x6C,  // Configure temperature controller
    IPC_MSG_CONFIG_PH_CONTROLLER  = 0x6D,  // Configure pH controller
    IPC_MSG_CONFIG_PRESSURE_CTRL  = 0x6E,  // Configure pressure controller
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
    IPC_ERR_PARAM_INVALID   = 0x0B,  // Parameter validation failed (out of range, constraint violation)
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

// Control loop parameter types (for PID controllers, sequencers, etc.)
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

// Control loop write message (for PID/sequencer parameters)
struct IPC_ControlWrite_t {
    uint16_t index;
    uint8_t objectType;      // Type verification
    uint8_t paramType;       // IPC_ControlParamType
    float value;             // Parameter value
} __attribute__((packed));

// Output control command types
enum DigitalOutputCommand : uint8_t {
    DOUT_CMD_SET_STATE = 0x01,   // Set on/off state
    DOUT_CMD_SET_PWM   = 0x02,   // Set PWM duty cycle
    DOUT_CMD_DISABLE   = 0x03,   // Disable output
};

enum StepperCommand : uint8_t {
    STEPPER_CMD_SET_RPM   = 0x01,  // Set target RPM
    STEPPER_CMD_SET_DIR   = 0x02,  // Set direction
    STEPPER_CMD_START     = 0x03,  // Start motor
    STEPPER_CMD_STOP      = 0x04,  // Stop motor
    STEPPER_CMD_UPDATE    = 0x05,  // Update RPM while running
};

enum DCMotorCommand : uint8_t {
    DCMOTOR_CMD_SET_POWER = 0x01,  // Set power percentage
    DCMOTOR_CMD_SET_DIR   = 0x02,  // Set direction
    DCMOTOR_CMD_START     = 0x03,  // Start motor
    DCMOTOR_CMD_STOP      = 0x04,  // Stop motor
    DCMOTOR_CMD_UPDATE    = 0x05,  // Update power while running
};

enum AnalogOutputCommand : uint8_t {
    AOUT_CMD_SET_VALUE = 0x01,  // Set output value in mV (0-10240)
    AOUT_CMD_DISABLE   = 0x02,  // Disable output (set to 0)
};

// Temperature Controller command types (indices 40-42)
enum TempControllerCommand : uint8_t {
    TEMP_CTRL_CMD_SET_SETPOINT    = 0x01,  // Set target setpoint
    TEMP_CTRL_CMD_ENABLE          = 0x02,  // Enable controller
    TEMP_CTRL_CMD_DISABLE         = 0x03,  // Disable controller
    TEMP_CTRL_CMD_START_AUTOTUNE  = 0x04,  // Start PID autotune
    TEMP_CTRL_CMD_STOP_AUTOTUNE   = 0x05,  // Stop autotune
};

// Device control command types (for peripheral devices like MFC, pH controllers, etc.)
enum DeviceControlCommand : uint8_t {
    DEV_CMD_SET_SETPOINT    = 0x01,  // Set device setpoint (MFC flow, pH target, etc.)
    DEV_CMD_RESET_FAULT     = 0x02,  // Clear fault condition
    DEV_CMD_ENABLE          = 0x03,  // Enable device
    DEV_CMD_DISABLE         = 0x04,  // Disable device
};

// Control error codes
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

// Digital Output Control (indices 21-25)
struct IPC_DigitalOutputControl_t {
    uint16_t index;          // Output index (21-25)
    uint8_t objectType;      // Type verification (OBJ_T_DIGITAL_OUTPUT)
    uint8_t command;         // DigitalOutputCommand
    uint8_t state;           // Output state (0=off, 1=on) - changed from bool
    uint8_t reserved1;       // Padding
    uint16_t reserved2;      // Padding
    float pwmDuty;           // PWM duty cycle (0-100%)
} __attribute__((packed));

// Stepper Motor Control (index 26)
struct IPC_StepperControl_t {
    uint16_t index;          // Stepper index (26)
    uint8_t objectType;      // Type verification (OBJ_T_STEPPER_MOTOR)
    uint8_t command;         // StepperCommand
    float rpm;               // Target RPM
    bool direction;          // true=forward, false=reverse
    bool enable;             // Enable motor
    uint8_t reserved[2];     // Padding
} __attribute__((packed));

// DC Motor Control (indices 27-30)
struct IPC_DCMotorControl_t {
    uint16_t index;          // Motor index (27-30)
    uint8_t objectType;      // Type verification (OBJ_T_BDC_MOTOR)
    uint8_t command;         // DCMotorCommand
    float power;             // Power percentage (0-100%)
    bool direction;          // true=forward, false=reverse
    bool enable;             // Enable motor
    uint8_t reserved[2];     // Padding
} __attribute__((packed));

// Analog Output (DAC) Control (indices 8-9)
struct IPC_AnalogOutputControl_t {
    uint16_t index;          // DAC index (8-9)
    uint8_t objectType;      // Type verification (OBJ_T_ANALOG_OUTPUT)
    uint8_t command;         // AnalogOutputCommand
    float value;             // Output value in mV (0-10240)
} __attribute__((packed));

// Device Control (indices 50-69) - for peripheral devices like MFC, pH controllers, etc.
struct IPC_DeviceControlCmd_t {
    uint16_t index;          // Control object index (50-69)
    uint8_t objectType;      // Type verification (OBJ_T_DEVICE_CONTROL)
    uint8_t command;         // DeviceControlCommand
    float setpoint;          // New setpoint value
    uint8_t reserved[8];     // Reserved for future use
} __attribute__((packed));

// Control Acknowledgment (for all control commands)
struct IPC_ControlAck_t {
    uint16_t index;
    uint8_t objectType;      // Object type
    uint8_t command;         // Command that was executed
    bool success;            // true=success, false=failure
    uint8_t errorCode;       // ControlErrorCode if success=false
    char message[100];       // Status/error message
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

/**
 * Dynamic device types - Modbus, I2C, SPI, and Analog peripheral devices
 * Devices are created dynamically in object index range 60-79 (20 slots)
 * Each device can occupy 1-4 object indices depending on sensor count
 */
enum IPC_DeviceType : uint8_t {
    IPC_DEV_NONE                = 0x00,
    
    // Modbus RTU Devices (1-19) - Use Modbus COM ports 0-3
    IPC_DEV_HAMILTON_PH         = 0x01,  // pH probe (2 sensors: pH + Temperature)
    IPC_DEV_HAMILTON_DO         = 0x02,  // Dissolved Oxygen probe (2 sensors: DO + Temperature)
    IPC_DEV_HAMILTON_OD         = 0x03,  // Optical Density probe (2 sensors: OD + Temperature)
    IPC_DEV_ALICAT_MFC          = 0x04,  // Mass Flow Controller (2-3 sensors: Flow + Pressure + Setpoint)
    IPC_DEV_MODBUS_GENERIC      = 0x05,  // Generic Modbus RTU device
    
    // I2C Devices (20-39) - Future expansion
    IPC_DEV_BME280              = 0x14,  // Temperature + Humidity + Pressure
    IPC_DEV_SCD40               = 0x15,  // CO2 + Temperature + Humidity
    IPC_DEV_INA260              = 0x16,  // Voltage + Current + Power (additional)
    
    // SPI Devices (40-59) - Future expansion
    IPC_DEV_MAX31865            = 0x28,  // RTD Temperature (additional)
    
    // Analog Devices (60-79) - Future expansion
    IPC_DEV_ANALOG_SENSOR       = 0x3C,  // External ADC, pressure transducer, etc.
    IPC_DEV_PRESSURE_CTRL       = 0x3D,  // Analog pressure controller (0-10V output)
    
    // Custom/User-defined (80-254)
    IPC_DEV_CUSTOM              = 0xFF,  // User-defined device
};

/**
 * Device bus types - determines which hardware interface is used
 */
enum IPC_BusType : uint8_t {
    IPC_BUS_NONE                = 0xFF,  // No bus (inactive/unassigned)
    IPC_BUS_MODBUS_RTU          = 0x00,  // Modbus RTU (RS-232/RS-485)
    IPC_BUS_I2C                 = 0x01,  // I2C
    IPC_BUS_SPI                 = 0x02,  // SPI
    IPC_BUS_ANALOG              = 0x03,  // Analog (ADC-based)
    IPC_BUS_DIGITAL             = 0x04,  // Digital I/O
};

/**
 * Device configuration - hardware-specific parameters (sent to IO MCU)
 * Name and dashboard settings are kept on SYS MCU only
 */
struct IPC_DeviceConfig_t {
    uint8_t deviceType;         // IPC_DeviceType enum
    uint8_t busType;            // IPC_BusType enum
    uint8_t busIndex;           // Bus/port index (COM port 0-3, I2C bus 0-1, etc.)
    uint8_t address;            // Device address (Modbus slave ID, I2C address, SPI CS, etc.)
    uint8_t objectCount;        // Number of sensor objects created (1-4, set by IO MCU)
    uint8_t reserved[3];        // Padding for future expansion
} __attribute__((packed));

/**
 * Create a new dynamic device instance
 * IO MCU will allocate object indices starting at startIndex
 */
struct IPC_DeviceCreate_t {
    uint8_t startIndex;         // First object index to allocate (60-79)
    IPC_DeviceConfig_t config;  // Device configuration
} __attribute__((packed));

/**
 * Delete a dynamic device instance
 * Removes device and frees all associated object indices
 */
struct IPC_DeviceDelete_t {
    uint8_t startIndex;         // First object index of device
} __attribute__((packed));

/**
 * Query device status
 * Requests status information for a specific device
 */
struct IPC_DeviceQuery_t {
    uint8_t startIndex;         // First object index of device
} __attribute__((packed));

/**
 * Update device configuration (change bus, address, etc.)
 * Device must be deleted and recreated if type changes
 */
struct IPC_DeviceConfigUpdate_t {
    uint8_t startIndex;         // First object index of device
    IPC_DeviceConfig_t config;  // New configuration
} __attribute__((packed));

/**
 * Device status response - sent after create/delete/config operations
 */
struct IPC_DeviceStatus_t {
    uint8_t startIndex;         // First object index
    bool active;                // Device is active and updating
    bool fault;                 // Device has a fault condition
    uint8_t objectCount;        // Number of object indices allocated
    uint8_t sensorIndices[4];   // Actual object indices assigned (0 = unused)
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
    uint8_t data[200];       // Configuration data
    char message[100];
} __attribute__((packed));

// ============================================================================
// OBJECT CONFIGURATION MESSAGES
// ============================================================================

/**
 * @brief Analog Input (ADC) configuration
 * Message type: IPC_MSG_CONFIG_ANALOG_INPUT
 */
struct IPC_ConfigAnalogInput_t {
    uint16_t index;          // Object index (0-7 for ADC inputs)
    uint8_t _padding[2];     // Alignment padding
    char unit[8];            // Unit string (e.g., "mV", "V", "A")
    float calScale;          // Calibration scale factor
    float calOffset;         // Calibration offset
} __attribute__((packed));

/**
 * @brief Analog Output (DAC) configuration
 * Message type: IPC_MSG_CONFIG_ANALOG_OUTPUT
 */
struct IPC_ConfigAnalogOutput_t {
    uint16_t index;          // Object index (8-9 for DAC outputs)
    uint8_t _padding[2];     // Alignment padding
    char unit[8];            // Unit string (e.g., "mV", "V", "mA")
    float calScale;          // Calibration scale factor
    float calOffset;         // Calibration offset
} __attribute__((packed));

/**
 * @brief RTD Temperature Sensor configuration
 * Message type: IPC_MSG_CONFIG_RTD
 */
struct IPC_ConfigRTD_t {
    uint16_t index;          // Object index (10-12 for RTD sensors)
    uint8_t wireConfig;      // 2, 3, or 4 wire configuration
    uint8_t _padding;        // Alignment padding
    char unit[8];            // Unit string (e.g., "C", "F", "K")
    float calScale;          // Calibration scale
    float calOffset;         // Calibration offset
    uint16_t nominalOhms;    // 100 (PT100) or 1000 (PT1000)
    uint8_t _padding2[2];    // Alignment padding
} __attribute__((packed));

/**
 * @brief GPIO configuration
 * Message type: IPC_MSG_CONFIG_GPIO
 */
struct IPC_ConfigGPIO_t {
    uint16_t index;          // Object index (13-20 for GPIO)
    char name[32];           // Custom name
    uint8_t pullMode;        // 0=None (High-Z), 1=Pull-up, 2=Pull-down
    uint8_t enabled;         // Enable/disable flag
} __attribute__((packed));

/**
 * @brief Digital Output configuration
 * Message type: IPC_MSG_CONFIG_DIGITAL_OUTPUT
 */
struct IPC_ConfigDigitalOutput_t {
    uint16_t index;          // Object index (21-25 for digital outputs)
    char name[32];           // Custom name
    uint8_t mode;            // 0=On/Off, 1=PWM
    uint8_t enabled;         // Enable/disable flag
} __attribute__((packed));

/**
 * @brief Stepper Motor configuration
 * Message type: IPC_MSG_CONFIG_STEPPER
 */
struct IPC_ConfigStepper_t {
    uint16_t index;          // Object index (26 for stepper motor)
    char name[32];           // Custom name
    uint16_t stepsPerRev;    // Steps per revolution (e.g., 200)
    uint16_t maxRPM;         // Maximum RPM
    uint16_t holdCurrent_mA; // Hold current in mA
    uint16_t runCurrent_mA;  // Run current in mA
    uint16_t acceleration;   // Acceleration in RPM/s
    uint8_t invertDirection; // Invert direction flag
    uint8_t enabled;         // Enable/disable flag
} __attribute__((packed));

/**
 * @brief DC Motor configuration
 * Message type: IPC_MSG_CONFIG_DCMOTOR
 */
struct IPC_ConfigDCMotor_t {
    uint16_t index;          // Object index (27-30 for DC motors)
    char name[32];           // Custom name
    uint8_t invertDirection; // Invert direction flag
    uint8_t enabled;         // Enable/disable flag
} __attribute__((packed));

/**
 * @brief COM Port configuration
 * Message type: IPC_MSG_CONFIG_COMPORT
 */
struct IPC_ConfigComPort_t {
    uint8_t index;           // COM port index (0-3: RS232-1,2 / RS485-1,2)
    uint32_t baudRate;       // Baud rate (1200, 2400, 4800, 9600, 19200, 38400, 57600, 115200)
    uint8_t dataBits;        // Data bits (fixed to 8 for Modbus)
    float stopBits;          // Stop bits (1.0 or 2.0)
    uint8_t parity;          // Parity: 0=none, 1=odd, 2=even
} __attribute__((packed));

/**
 * @brief Pressure controller calibration configuration
 * Sent from SYS MCU to IO MCU to configure pressure controller calibration
 * Uses scale/offset calibration pattern (matches ADC/DAC/RTD)
 * Formula: pressure = scale * voltage_mV + offset
 */
struct IPC_ConfigPressureCtrl_t {
    uint8_t controlIndex;        // Control object index (50-69)
    uint8_t dacIndex;            // DAC output index (8 or 9)
    char unit[8];                // Pressure unit (Pa, kPa, bar, PSI, atm, mbar)
    float scale;                 // Calibration scale factor (Pa/mV)
    float offset;                // Calibration offset (Pa)
} __attribute__((packed));

/**
 * @brief Temperature Controller configuration
 * Message type: IPC_MSG_CONFIG_TEMP_CONTROLLER
 * 
 * Sent from SYS MCU to IO MCU to configure temperature controllers (indices 40-42)
 * Supports both On/Off and PID control methods
 */
struct IPC_ConfigTempController_t {
    uint8_t index;               // Controller index (40-42)
    bool isActive;               // true=create/update, false=delete
    char name[40];               // Controller name
    bool enabled;                // Enable/disable controller
    uint16_t pvSourceIndex;      // Process value sensor index (temperature sensor)
    uint16_t outputIndex;        // Output index (21-25 digital, 8-9 DAC)
    uint8_t controlMethod;       // 0=On/Off, 1=PID
    uint8_t _padding;            // Alignment
    float setpoint;              // Target temperature
    float hysteresis;            // On/Off mode hysteresis (deadband)
    float kP;                    // PID proportional gain
    float kI;                    // PID integral gain
    float kD;                    // PID derivative gain
    float integralWindup;        // Anti-windup limit
    float outputMin;             // Output clamp min (0-100%)
    float outputMax;             // Output clamp max (0-100%)
} __attribute__((packed));

/**
 * @brief Temperature Controller runtime control
 * Message type: IPC_MSG_CONTROL_WRITE (with OBJ_T_TEMPERATURE_CONTROL)
 * 
 * Runtime commands for temperature controllers: setpoint, enable, disable, autotune
 */
struct IPC_TempControllerControl_t {
    uint16_t index;              // Controller index (40-42)
    uint8_t objectType;          // OBJ_T_TEMPERATURE_CONTROL
    uint8_t command;             // TempControllerCommand
    float setpoint;              // For SET_SETPOINT and AUTOTUNE commands
    float autotuneOutputStep;    // Output step size for autotune (default 50%)
    uint8_t reserved[6];         // Reserved for future use
} __attribute__((packed));

/**
 * @brief pH Controller configuration
 * Message type: IPC_MSG_CONFIG_PH_CONTROLLER
 * Sent from SYS MCU to IO MCU to configure pH controller (index 43)
 */
struct IPC_ConfigpHController_t {
    uint8_t index;                   // Controller index (always 43)
    bool isActive;                   // true=create/update, false=delete
    char name[40];                   // Controller name
    bool enabled;                    // Enable/disable controller
    uint16_t pvSourceIndex;          // pH sensor index (typically 70-99 for Hamilton probes)
    float setpoint;                  // Target pH
    float deadband;                  // Hysteresis around setpoint
    
    // Acid dosing configuration
    bool acidEnabled;
    uint8_t acidOutputType;          // 0=Digital output, 1=DC motor
    uint8_t acidOutputIndex;         // Digital output (21-25) or DC motor (27-30)
    uint8_t acidMotorPower;          // Motor power level (0-100%), ignored if digital
    uint16_t acidDosingTime_ms;      // Duration to activate output (milliseconds)
    uint32_t acidDosingInterval_ms;  // Minimum time between doses (milliseconds)
    float acidVolumePerDose_mL;      // Volume per dose in mL (user configured)
    
    // Alkaline dosing configuration  
    bool alkalineEnabled;
    uint8_t alkalineOutputType;      // 0=Digital output, 1=DC motor
    uint8_t alkalineOutputIndex;     // Digital output (21-25) or DC motor (27-30)
    uint8_t alkalineMotorPower;      // Motor power level (0-100%), ignored if digital
    uint16_t alkalineDosingTime_ms;  // Duration to activate output (milliseconds)
    uint32_t alkalineDosingInterval_ms;  // Minimum time between doses (milliseconds)
    float alkalineVolumePerDose_mL;  // Volume per dose in mL (user configured)
    
    uint8_t _padding[2];             // Alignment
} __attribute__((packed));

/**
 * @brief pH Controller runtime control commands
 */
enum pHControllerCommand : uint8_t {
    PH_CMD_SET_SETPOINT = 0,
    PH_CMD_ENABLE = 1,
    PH_CMD_DISABLE = 2,
    PH_CMD_DOSE_ACID = 3,           // Manual acid dose
    PH_CMD_DOSE_ALKALINE = 4,       // Manual alkaline dose
    PH_CMD_RESET_ACID_VOLUME = 5,   // Reset acid cumulative volume
    PH_CMD_RESET_BASE_VOLUME = 6    // Reset alkaline cumulative volume
};

/**
 * @brief pH Controller runtime control
 * Message type: IPC_MSG_CONTROL_WRITE (with OBJ_T_PH_CONTROL)
 * 
 * Runtime commands for pH controller: setpoint, enable, disable, manual dosing
 */
struct IPC_pHControllerControl_t {
    uint16_t index;                  // Controller index (always 43)
    uint8_t objectType;              // OBJ_T_PH_CONTROL
    uint8_t command;                 // pHControllerCommand
    float setpoint;                  // For SET_SETPOINT command
    uint8_t reserved[8];             // Reserved for future use
} __attribute__((packed));

// ============================================================================
// CRC16 CALCULATION
// ============================================================================

// CRC16-CCITT (polynomial 0x1021, initial value 0xFFFF)
uint16_t ipc_calcCRC16(const uint8_t *data, uint16_t length);

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
