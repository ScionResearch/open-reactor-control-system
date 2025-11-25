/**
 * @file IPCProtocol.h
 * @brief Inter-Processor Communication Protocol Header (RP2040 side)
 * @author Open Reactor Control System
 * @date 2024
 * 
 * This file defines the IPC protocol interface and data structures for
 * communication between the RP2040 and SAME51 MCUs over UART.
 */

#ifndef IPC_PROTOCOL_H
#define IPC_PROTOCOL_H

#include <Arduino.h>
#include "IPCDataStructs.h"

// =============================================================================
// IPC Driver State Machine
// =============================================================================

typedef enum {
    IPC_STATE_IDLE = 0,       // Waiting for start byte
    IPC_STATE_RECEIVING,      // Receiving packet data
    IPC_STATE_PROCESSING,     // Processing received packet
    IPC_STATE_ERROR           // Error state
} IPC_State_t;

// =============================================================================
// Transmit Queue Entry
// =============================================================================

typedef struct {
    uint8_t messageType;                    // Message type
    uint16_t payloadLength;                 // Payload length
    uint8_t payload[IPC_MAX_PAYLOAD_SIZE];  // Payload data
} IPC_TxPacket_t;

// =============================================================================
// Message Handler Callback
// =============================================================================

typedef void (*IPC_MessageCallback)(uint8_t messageType, const uint8_t *payload, uint16_t length);

// =============================================================================
// Message Handler Entry
// =============================================================================

typedef struct {
    uint8_t messageType;
    IPC_MessageCallback callback;
} IPC_MessageHandler_t;

// =============================================================================
// Statistics Structure
// =============================================================================

typedef struct {
    uint32_t rxPacketCount;   // Received packets
    uint32_t txPacketCount;   // Transmitted packets
    uint32_t rxErrorCount;    // Receive errors
    uint32_t crcErrorCount;   // CRC errors
    uint32_t lastRxTime;      // Last RX timestamp (ms)
    uint32_t lastTxTime;      // Last TX timestamp (ms)
} IPC_Statistics_t;

// =============================================================================
// IPCProtocol Class
// =============================================================================

#define IPC_MAX_HANDLERS 32  // Maximum number of message handlers

class IPCProtocol {
public:
    /**
     * @brief Constructor
     * @param uart Pointer to HardwareSerial instance
     */
    IPCProtocol(HardwareSerial *uart);
    
    /**
     * @brief Initialize IPC protocol
     * @param baudRate UART baud rate (default 2000000 = 2 Mbps, tested up to 3 Mbps)
     */
    void begin(uint32_t baudRate = 2000000);
    
    /**
     * @brief Update function - call regularly from task
     */
    void update();
    
    /**
     * @brief Send a packet
     * @param messageType Message type
     * @param payload Pointer to payload data (can be nullptr)
     * @param payloadLength Length of payload in bytes
     * @return true if packet was queued successfully
     */
    bool sendPacket(uint8_t messageType, const uint8_t *payload, uint16_t payloadLength);
    
    /**
     * @brief Register a message handler
     * @param messageType Message type to handle
     * @param callback Callback function
     * @return true if handler was registered successfully
     */
    bool registerHandler(uint8_t messageType, IPC_MessageCallback callback);
    
    /**
     * @brief Get statistics
     * @param stats Pointer to statistics structure
     */
    void getStatistics(IPC_Statistics_t *stats);
    
    /**
     * @brief Reset statistics counters
     */
    void resetStatistics();
    
    /**
     * @brief Reset RX state machine to resync after errors
     * Call this after long blocking operations that may have caused
     * UART buffer overflow and corrupted packet boundaries.
     */
    void resetRxState();
    
    // Helper functions for common messages
    bool sendPing();
    bool sendPong();
    bool sendHello(uint32_t protocolVersion, uint32_t firmwareVersion, const char* deviceName);
    bool sendError(uint8_t errorCode, const char* message);
    
    // Sensor data helpers
    bool sendSensorData(const IPC_SensorData_t* data);
    bool sendSensorBatch(const IPC_SensorBatch_t* batch);
    
    // Index sync helpers
    bool sendIndexSyncRequest();
    bool sendIndexAdd(const IPC_IndexAdd_t* entry);
    bool sendIndexRemove(uint16_t index, uint8_t objectType);
    
    // Device management helpers
    bool sendDeviceCreate(const IPC_DeviceCreate_t* device);
    bool sendDeviceDelete(uint16_t index, uint8_t objectType);
    bool sendDeviceStatus(const IPC_DeviceStatus_t* status);
    
    // Fault notification helpers
    bool sendFaultNotify(const IPC_FaultNotify_t* fault);
    bool sendFaultClear(uint16_t index);
    
    // Legacy message support (for backward compatibility)
    bool sendMessage(const Message& msg);
    
private:
    HardwareSerial *_uart;          // UART instance
    IPC_State_t _state;             // State machine state
    
    // Receive buffer and state
    uint8_t _rxBuffer[IPC_MAX_PAYLOAD_SIZE + 5];  // LENGTH(2) + TYPE(1) + PAYLOAD + CRC(2)
    uint16_t _rxBufferIndex;        // Current position in RX buffer
    uint16_t _rxPacketLength;       // Expected packet length
    uint8_t _rxMessageType;         // Received message type
    bool _rxEscapeNext;             // Escape sequence flag
    
    // Transmit queue
    IPC_TxPacket_t _txQueue[IPC_TX_QUEUE_SIZE];
    uint8_t _txQueueHead;           // Queue head index
    uint8_t _txQueueTail;           // Queue tail index
    
    // Message handlers
    IPC_MessageHandler_t _handlers[IPC_MAX_HANDLERS];
    uint8_t _handlerCount;
    
    // Statistics
    uint32_t _rxPacketCount;
    uint32_t _txPacketCount;
    uint32_t _rxErrorCount;
    uint32_t _crcErrorCount;
    uint32_t _lastRxTime;
    uint32_t _lastTxTime;
    
    // Internal methods
    void processRxByte(uint8_t byte);
    void processRxPacket();
    void sendNextPacket();
    void dispatchMessage(uint8_t messageType, const uint8_t *payload, uint16_t payloadLength);
};

#endif // IPC_PROTOCOL_H
