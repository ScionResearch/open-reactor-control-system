#pragma once

#include <Arduino.h>
#include "ipc_protocol.h"

// ============================================================================
// IPC DRIVER - Inter-Processor Communication Driver
// Non-blocking UART communication between SAME51 and RP2040
// ============================================================================

// IPC driver state machine
enum IPC_State : uint8_t {
    IPC_STATE_IDLE,
    IPC_STATE_RECEIVING,
    IPC_STATE_PROCESSING,
    IPC_STATE_ERROR
};

// TX packet queue entry
struct IPC_TxPacket_t {
    uint8_t msgType;
    uint16_t payloadLen;
    uint8_t payload[IPC_MAX_PAYLOAD_SIZE];
};

// IPC driver structure
struct IPC_Driver_t {
    // Hardware interface
    HardwareSerial *uart;
    
    // State machine
    IPC_State state;
    uint32_t lastActivity;
    uint32_t lastKeepalive;
    bool connected;
    
    // RX buffer and packet parsing
    uint8_t rxBuffer[IPC_RX_BUFFER_SIZE];
    uint16_t rxBufferPos;
    bool escapeNext;
    uint32_t rxStartTime;
    
    // Parsed packet (before processing)
    uint16_t rxPayloadLen;
    uint8_t rxMsgType;
    uint8_t rxPayload[IPC_MAX_PAYLOAD_SIZE];
    
    // TX queue
    IPC_TxPacket_t txQueue[IPC_TX_QUEUE_SIZE];
    uint8_t txQueueHead;
    uint8_t txQueueTail;
    bool txInProgress;
    
    // Statistics
    uint32_t rxPacketCount;
    uint32_t txPacketCount;
    uint32_t rxErrorCount;
    uint32_t txErrorCount;
    uint32_t crcErrorCount;
    
    // Fault/message tracking
    bool fault;
    bool newMessage;
    char message[100];
};

// Global IPC driver instance
extern IPC_Driver_t ipcDriver;

// ============================================================================
// INITIALIZATION & UPDATE
// ============================================================================

/**
 * @brief Initialize IPC driver
 * @return true if successful
 */
bool ipc_init(void);

/**
 * @brief Non-blocking update function (called from task)
 * Handles RX byte processing, packet parsing, and TX queue
 */
void ipc_update(void);

// ============================================================================
// PACKET TRANSMISSION
// ============================================================================

/**
 * @brief Send a packet with specified message type and payload
 * @param msgType Message type (IPC_MsgType enum)
 * @param payload Pointer to payload data (can be NULL if len=0)
 * @param len Payload length in bytes
 * @return true if packet queued successfully
 */
bool ipc_sendPacket(uint8_t msgType, const uint8_t *payload, uint16_t len);

/**
 * @brief Send a packet (internal use - processes TX queue)
 * @return true if packet sent
 */
bool ipc_processTxQueue(void);

// ============================================================================
// HELPER FUNCTIONS FOR COMMON OPERATIONS
// ============================================================================

/**
 * @brief Send PING keepalive message
 */
bool ipc_sendPing(void);

/**
 * @brief Send PONG response
 */
bool ipc_sendPong(void);

/**
 * @brief Send HELLO handshake
 */
bool ipc_sendHello(void);

/**
 * @brief Send error notification
 * @param errorCode Error code (IPC_ErrorCode enum)
 * @param message Error message string
 */
bool ipc_sendError(uint8_t errorCode, const char *message);

/**
 * @brief Send complete object index synchronization
 * @return true if all packets queued successfully
 */
bool ipc_sendIndexSync(void);

/**
 * @brief Send index add notification
 * @param index Object index
 */
bool ipc_sendIndexAdd(uint16_t index);

/**
 * @brief Send index remove notification
 * @param index Object index
 */
bool ipc_sendIndexRemove(uint16_t index);

/**
 * @brief Send sensor data for a specific index
 * @param index Object index
 * @return true if packet queued successfully
 */
bool ipc_sendSensorData(uint16_t index);

/**
 * @brief Send batch sensor data
 * @param indices Array of object indices
 * @param count Number of indices
 * @return true if packet queued successfully
 */
bool ipc_sendSensorBatch(const uint16_t *indices, uint8_t count);

/**
 * @brief Send fault notification
 * @param index Object index
 * @param severity Fault severity (IPC_FaultSeverity enum)
 * @param message Fault message
 */
bool ipc_sendFault(uint16_t index, uint8_t severity, const char *message);

/**
 * @brief Send control acknowledgment (legacy)
 * @param index Object index
 * @param success Success status
 * @param message Response message
 */
bool ipc_sendControlAck(uint16_t index, bool success, const char *message);

/**
 * @brief Send enhanced control acknowledgment with error codes
 * @param index Object index
 * @param objectType Object type
 * @param command Command that was executed
 * @param success Success status
 * @param errorCode Error code if failed
 * @param message Response message
 */
bool ipc_sendControlAck_v2(uint16_t index, uint8_t objectType, uint8_t command,
                          bool success, uint8_t errorCode, const char *message);

/**
 * @brief Send device status response
 * @param indices Array of assigned indices
 * @param indexCount Number of assigned indices
 * @param success Success status
 * @param message Status message
 */
bool ipc_sendDeviceStatus(const uint16_t *indices, uint8_t indexCount, 
                          bool success, const char *message);

// ============================================================================
// MESSAGE HANDLERS
// ============================================================================

/**
 * @brief Process received packet (called from ipc_update)
 * @param msgType Message type
 * @param payload Payload data
 * @param len Payload length
 */
void ipc_handleMessage(uint8_t msgType, const uint8_t *payload, uint16_t len);

// Individual message handlers (implemented in ipc_handlers.cpp)
void ipc_handle_ping(const uint8_t *payload, uint16_t len);
void ipc_handle_pong(const uint8_t *payload, uint16_t len);
void ipc_handle_hello(const uint8_t *payload, uint16_t len);
void ipc_handle_index_sync_req(const uint8_t *payload, uint16_t len);
void ipc_handle_sensor_read_req(const uint8_t *payload, uint16_t len);
void ipc_handle_sensor_bulk_read_req(const uint8_t *payload, uint16_t len);
void ipc_handle_control_write(const uint8_t *payload, uint16_t len);
void ipc_handle_control_loop_write(const uint8_t *payload, uint16_t len);
void ipc_handle_digital_output_control(const uint8_t *payload, uint16_t len);
void ipc_handle_analog_output_control(const uint8_t *payload, uint16_t len);
void ipc_handle_stepper_control(const uint8_t *payload, uint16_t len);
void ipc_handle_dcmotor_control(const uint8_t *payload, uint16_t len);
void ipc_handle_control_read(const uint8_t *payload, uint16_t len);
void ipc_handle_device_create(const uint8_t *payload, uint16_t len);
void ipc_handle_device_delete(const uint8_t *payload, uint16_t len);
void ipc_handle_config_write(const uint8_t *payload, uint16_t len);
void ipc_handle_config_analog_input(const uint8_t *payload, uint16_t len);
void ipc_handle_config_analog_output(const uint8_t *payload, uint16_t len);
void ipc_handle_config_rtd(const uint8_t *payload, uint16_t len);
void ipc_handle_config_gpio(const uint8_t *payload, uint16_t len);
void ipc_handle_config_digital_output(const uint8_t *payload, uint16_t len);
void ipc_handle_config_stepper(const uint8_t *payload, uint16_t len);
void ipc_handle_config_dcmotor(const uint8_t *payload, uint16_t len);
void ipc_handle_config_comport(const uint8_t *payload, uint16_t len);

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================

/**
 * @brief Check if TX queue has space
 * @return true if space available
 */
bool ipc_txQueueHasSpace(void);

/**
 * @brief Get number of packets in TX queue
 * @return Number of queued packets
 */
uint8_t ipc_txQueueCount(void);

/**
 * @brief Clear TX queue
 */
void ipc_clearTxQueue(void);

/**
 * @brief Clear RX buffer and reset state
 */
void ipc_clearRxBuffer(void);

/**
 * @brief Get connection status
 * @return true if connected to RP2040
 */
bool ipc_isConnected(void);

/**
 * @brief Print IPC statistics to Serial
 */
void ipc_printStats(void);
