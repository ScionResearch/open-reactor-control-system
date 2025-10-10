/**
 * @file IPCProtocol.cpp
 * @brief Inter-Processor Communication Protocol Implementation (RP2040 side)
 * @author Open Reactor Control System
 * @date 2024
 * 
 * This file implements the IPC protocol for communication between the RP2040
 * and SAME51 MCUs over UART. It provides packet framing, byte stuffing,
 * CRC16 error checking, and message dispatching.
 */

#include "IPCProtocol.h"
#include <Arduino.h>
#include <string.h>

// =============================================================================
// CRC16-CCITT Calculation
// =============================================================================

/**
 * @brief Calculate CRC16-CCITT checksum
 * @param data Pointer to data buffer
 * @param length Length of data in bytes
 * @return uint16_t CRC16 checksum
 */
static uint16_t calculateCRC16(const uint8_t *data, uint16_t length) {
    uint16_t crc = 0xFFFF;
    for (uint16_t i = 0; i < length; i++) {
        crc ^= (uint16_t)data[i] << 8;
        for (uint8_t j = 0; j < 8; j++) {
            if (crc & 0x8000) {
                crc = (crc << 1) ^ 0x1021;
            } else {
                crc <<= 1;
            }
        }
    }
    return crc;
}

// =============================================================================
// IPCProtocol Class Implementation
// =============================================================================

IPCProtocol::IPCProtocol(HardwareSerial *uart) 
    : _uart(uart)
    , _state(IPC_STATE_IDLE)
    , _rxBufferIndex(0)
    , _rxPacketLength(0)
    , _rxMessageType(0)
    , _rxEscapeNext(false)
    , _txQueueHead(0)
    , _txQueueTail(0)
    , _handlerCount(0)
    , _rxPacketCount(0)
    , _txPacketCount(0)
    , _rxErrorCount(0)
    , _crcErrorCount(0)
    , _lastRxTime(0)
    , _lastTxTime(0)
{
    memset(_rxBuffer, 0, sizeof(_rxBuffer));
    memset(_txQueue, 0, sizeof(_txQueue));
    memset(_handlers, 0, sizeof(_handlers));
}

// =============================================================================
// Initialization
// =============================================================================

void IPCProtocol::begin(uint32_t baudRate) {
    _uart->begin(baudRate);
    _state = IPC_STATE_IDLE;
    _rxBufferIndex = 0;
    _rxPacketLength = 0;
    _rxMessageType = 0;
    _rxEscapeNext = false;
    _txQueueHead = 0;
    _txQueueTail = 0;
    _handlerCount = 0;
}

// =============================================================================
// Update Function (called from task)
// =============================================================================

void IPCProtocol::update() {
    // Process incoming bytes
    while (_uart->available()) {
        uint8_t byte = _uart->read();
        processRxByte(byte);
    }
    
    // Process TX queue
    if (_txQueueHead != _txQueueTail) {
        sendNextPacket();
    }
}

// =============================================================================
// Receive State Machine
// =============================================================================

void IPCProtocol::processRxByte(uint8_t byte) {
    _lastRxTime = millis();
    
    switch (_state) {
        case IPC_STATE_IDLE:
            if (byte == IPC_START_BYTE) {
                #if IPC_DEBUG_ENABLED
                Serial.println("[IPC RX] START detected");
                #endif
                _state = IPC_STATE_RECEIVING;
                _rxBufferIndex = 0;
                _rxEscapeNext = false;
            }
            break;
            
        case IPC_STATE_RECEIVING:
            // Handle escape sequences
            if (_rxEscapeNext) {
                byte ^= IPC_ESCAPE_XOR;  // Un-escape
                _rxEscapeNext = false;
            } else if (byte == IPC_ESCAPE_BYTE) {
                _rxEscapeNext = true;
                return;  // Don't store the escape byte
            } else if (byte == IPC_END_BYTE) {
                // Packet complete (END byte is 0x7E, same as START)
                #if IPC_DEBUG_ENABLED
                Serial.printf("[IPC RX] END detected, %u bytes buffered\n", _rxBufferIndex);
                #endif
                _state = IPC_STATE_PROCESSING;
                processRxPacket();
                _state = IPC_STATE_IDLE;
                return;
            }
            // Note: Removed START_BYTE check here since START==END (both 0x7E)
            // When we're already RECEIVING, 0x7E should be treated as END, not START
            
            // Store byte in buffer
            if (_rxBufferIndex < (IPC_MAX_PAYLOAD_SIZE + 5)) {  // LENGTH(2) + TYPE(1) + CRC(2)
                _rxBuffer[_rxBufferIndex++] = byte;
            } else {
                // Buffer overflow
                _rxErrorCount++;
                _state = IPC_STATE_ERROR;
                _state = IPC_STATE_IDLE;  // Immediately return to idle
            }
            break;
            
        case IPC_STATE_PROCESSING:
        case IPC_STATE_ERROR:
            // These states are handled synchronously, should not receive bytes here
            break;
    }
}

void IPCProtocol::processRxPacket() {
    
    // Minimum packet: LENGTH(2) + TYPE(1) + CRC(2) = 5 bytes
    if (_rxBufferIndex < 5) {
        _rxErrorCount++;
        return;
    }
    
    // Extract length (big-endian)
    // Length = MSG_TYPE (1) + PAYLOAD (N), does NOT include LENGTH field or CRC
    _rxPacketLength = ((uint16_t)_rxBuffer[0] << 8) | _rxBuffer[1];
    
    // Validate length (min = 1 for MSG_TYPE only, max = 1 + max payload)
    if (_rxPacketLength < 1 || _rxPacketLength > (IPC_MAX_PAYLOAD_SIZE + 1)) {
        Serial.printf("[IPC] ERROR: Invalid packet length %d\n", _rxPacketLength);
        _rxErrorCount++;
        return;
    }
    
    // Check if we received the expected number of bytes
    uint16_t expectedBytes = 2 + _rxPacketLength + 2;
    if (_rxBufferIndex != expectedBytes) {
        Serial.printf("[IPC] ERROR: Length mismatch (got %d, expected %d)\n",
                      _rxBufferIndex, expectedBytes);
        _rxErrorCount++;
        return;
    }
    
    // Extract message type
    _rxMessageType = _rxBuffer[2];
    
    // Extract CRC (last 2 bytes)
    uint16_t receivedCRC = ((uint16_t)_rxBuffer[_rxBufferIndex - 2] << 8) | 
                           _rxBuffer[_rxBufferIndex - 1];
    
    // Calculate CRC over LENGTH + TYPE + PAYLOAD
    uint16_t calculatedCRC = calculateCRC16(_rxBuffer, _rxBufferIndex - 2);
    
    // Verify CRC
    if (receivedCRC != calculatedCRC) {
        Serial.printf("[IPC] ERROR: CRC mismatch (0x%04X != 0x%04X)\n", 
                      receivedCRC, calculatedCRC);
        _crcErrorCount++;
        return;
    }
    
    // Packet is valid, dispatch to handler
    _rxPacketCount++;
    
    // Payload starts at index 3 (after LENGTH(2) + MSG_TYPE(1))
    // Payload length = _rxPacketLength - 1 (subtract MSG_TYPE byte)
    uint16_t payloadLength = _rxPacketLength - 1;
    uint8_t *payload = (payloadLength > 0) ? &_rxBuffer[3] : nullptr;
    
    dispatchMessage(_rxMessageType, payload, payloadLength);
}

// =============================================================================
// Transmit Functions
// =============================================================================

bool IPCProtocol::sendPacket(uint8_t messageType, const uint8_t *payload, uint16_t payloadLength) {
    // Check if queue is full
    uint8_t nextTail = (_txQueueTail + 1) % IPC_TX_QUEUE_SIZE;
    if (nextTail == _txQueueHead) {
        return false;  // Queue full
    }
    
    // Validate payload length
    if (payloadLength > IPC_MAX_PAYLOAD_SIZE) {
        return false;
    }
    
    // Add packet to queue
    IPC_TxPacket_t *packet = &_txQueue[_txQueueTail];
    packet->messageType = messageType;
    packet->payloadLength = payloadLength;
    if (payloadLength > 0 && payload != nullptr) {
        memcpy(packet->payload, payload, payloadLength);
    }
    
    _txQueueTail = nextTail;
    return true;
}

void IPCProtocol::sendNextPacket() {
    if (_txQueueHead == _txQueueTail) {
        return;  // Queue empty
    }
    
    IPC_TxPacket_t *packet = &_txQueue[_txQueueHead];
    
    // Build packet in temporary buffer
    uint8_t tempBuffer[IPC_MAX_PAYLOAD_SIZE + 5];  // LENGTH(2) + TYPE(1) + PAYLOAD + CRC(2)
    uint16_t tempIndex = 0;
    
    // LENGTH = TYPE(1) + PAYLOAD length (does NOT include LENGTH field or CRC)
    uint16_t packetLength = 1 + packet->payloadLength;
    tempBuffer[tempIndex++] = (packetLength >> 8) & 0xFF;
    tempBuffer[tempIndex++] = packetLength & 0xFF;
    
    // TYPE
    tempBuffer[tempIndex++] = packet->messageType;
    
    // PAYLOAD
    if (packet->payloadLength > 0) {
        memcpy(&tempBuffer[tempIndex], packet->payload, packet->payloadLength);
        tempIndex += packet->payloadLength;
    }
    
    // Calculate CRC over LENGTH + TYPE + PAYLOAD
    uint16_t crc = calculateCRC16(tempBuffer, tempIndex);
    tempBuffer[tempIndex++] = (crc >> 8) & 0xFF;
    tempBuffer[tempIndex++] = crc & 0xFF;
    
    #if IPC_DEBUG_ENABLED
    // Print packet being sent
    Serial.printf("[IPC TX] Sending packet, tempBuffer (%u bytes): ", tempIndex);
    for (uint16_t i = 0; i < tempIndex; i++) {
        Serial.printf("%02X ", tempBuffer[i]);
    }
    Serial.println();
    #endif
    
    // Send START byte
    _uart->write(IPC_START_BYTE);
    
    // Send packet with byte stuffing
    for (uint16_t i = 0; i < tempIndex; i++) {
        uint8_t byte = tempBuffer[i];
        if (byte == IPC_START_BYTE || byte == IPC_END_BYTE || byte == IPC_ESCAPE_BYTE) {
            _uart->write(IPC_ESCAPE_BYTE);
            _uart->write(byte ^ IPC_ESCAPE_XOR);
        } else {
            _uart->write(byte);
        }
    }
    
    // Send END byte
    _uart->write(IPC_END_BYTE);
    
    // Update statistics
    _txPacketCount++;
    _lastTxTime = millis();
    
    // Remove packet from queue
    _txQueueHead = (_txQueueHead + 1) % IPC_TX_QUEUE_SIZE;
}

// =============================================================================
// Message Dispatching
// =============================================================================

void IPCProtocol::dispatchMessage(uint8_t messageType, const uint8_t *payload, uint16_t payloadLength) {
    // Find handler for this message type
    for (uint8_t i = 0; i < _handlerCount; i++) {
        if (_handlers[i].messageType == messageType) {
            if (_handlers[i].callback != nullptr) {
                _handlers[i].callback(messageType, payload, payloadLength);
            } else {
                Serial.printf("[IPC] ERROR: Handler for type 0x%02X has null callback\n", messageType);
            }
            return;
        }
    }
    
    // No handler found
    Serial.printf("[IPC] WARNING: No handler for message type 0x%02X\n", messageType);
}

// =============================================================================
// Handler Registration
// =============================================================================

bool IPCProtocol::registerHandler(uint8_t messageType, IPC_MessageCallback callback) {
    if (_handlerCount >= IPC_MAX_HANDLERS) {
        return false;
    }
    
    // Check if handler already exists
    for (uint8_t i = 0; i < _handlerCount; i++) {
        if (_handlers[i].messageType == messageType) {
            _handlers[i].callback = callback;  // Update existing handler
            return true;
        }
    }
    
    // Add new handler
    _handlers[_handlerCount].messageType = messageType;
    _handlers[_handlerCount].callback = callback;
    _handlerCount++;
    return true;
}

// =============================================================================
// Helper Functions for Common Messages
// =============================================================================

bool IPCProtocol::sendPing() {
    return sendPacket(IPC_MSG_PING, nullptr, 0);
}

bool IPCProtocol::sendPong() {
    return sendPacket(IPC_MSG_PONG, nullptr, 0);
}

bool IPCProtocol::sendHello(uint32_t protocolVersion, uint32_t firmwareVersion, const char* deviceName) {
    IPC_Hello_t hello;
    hello.protocolVersion = protocolVersion;
    hello.firmwareVersion = firmwareVersion;
    strncpy(hello.deviceName, deviceName, sizeof(hello.deviceName) - 1);
    hello.deviceName[sizeof(hello.deviceName) - 1] = '\0';
    
    return sendPacket(IPC_MSG_HELLO, (uint8_t*)&hello, sizeof(hello));
}

bool IPCProtocol::sendError(uint8_t errorCode, const char* message) {
    IPC_Error_t error;
    error.errorCode = errorCode;
    if (message != nullptr) {
        strncpy(error.message, message, sizeof(error.message) - 1);
        error.message[sizeof(error.message) - 1] = '\0';
    } else {
        error.message[0] = '\0';
    }
    
    return sendPacket(IPC_MSG_ERROR, (uint8_t*)&error, sizeof(error));
}

bool IPCProtocol::sendSensorData(const IPC_SensorData_t* data) {
    if (data == nullptr) return false;
    return sendPacket(IPC_MSG_SENSOR_DATA, (uint8_t*)data, sizeof(IPC_SensorData_t));
}

bool IPCProtocol::sendSensorBatch(const IPC_SensorBatch_t* batch) {
    if (batch == nullptr || batch->count == 0) return false;
    
    // Calculate actual size: header + (count * entry size)
    uint16_t size = sizeof(uint8_t) + (batch->count * sizeof(IPC_SensorBatchEntry_t));
    return sendPacket(IPC_MSG_SENSOR_BATCH, (uint8_t*)batch, size);
}

bool IPCProtocol::sendIndexSyncRequest() {
    return sendPacket(IPC_MSG_INDEX_SYNC_REQ, nullptr, 0);
}

bool IPCProtocol::sendIndexAdd(const IPC_IndexAdd_t* entry) {
    if (entry == nullptr) return false;
    return sendPacket(IPC_MSG_INDEX_ADD, (uint8_t*)entry, sizeof(IPC_IndexAdd_t));
}

bool IPCProtocol::sendIndexRemove(uint16_t index, uint8_t objectType) {
    IPC_IndexRemove_t remove;
    remove.index = index;
    remove.objectType = objectType;
    return sendPacket(IPC_MSG_INDEX_REMOVE, (uint8_t*)&remove, sizeof(remove));
}

bool IPCProtocol::sendDeviceCreate(const IPC_DeviceCreate_t* device) {
    if (device == nullptr) return false;
    return sendPacket(IPC_MSG_DEVICE_CREATE, (uint8_t*)device, sizeof(IPC_DeviceCreate_t));
}

bool IPCProtocol::sendDeviceDelete(uint16_t index, uint8_t objectType) {
    IPC_DeviceDelete_t deleteMsg;
    deleteMsg.index = index;
    deleteMsg.objectType = objectType;
    return sendPacket(IPC_MSG_DEVICE_DELETE, (uint8_t*)&deleteMsg, sizeof(deleteMsg));
}

bool IPCProtocol::sendDeviceStatus(const IPC_DeviceStatus_t* status) {
    if (status == nullptr) return false;
    return sendPacket(IPC_MSG_DEVICE_STATUS, (uint8_t*)status, sizeof(IPC_DeviceStatus_t));
}

bool IPCProtocol::sendFaultNotify(const IPC_FaultNotify_t* fault) {
    if (fault == nullptr) return false;
    return sendPacket(IPC_MSG_FAULT_NOTIFY, (uint8_t*)fault, sizeof(IPC_FaultNotify_t));
}

bool IPCProtocol::sendFaultClear(uint16_t index) {
    IPC_FaultClear_t clear;
    clear.index = index;
    return sendPacket(IPC_MSG_FAULT_CLEAR, (uint8_t*)&clear, sizeof(clear));
}

// =============================================================================
// Legacy Message Support
// =============================================================================

bool IPCProtocol::sendMessage(const Message& msg) {
    // Convert legacy Message to new IPC protocol packet
    // Simply send the old message format as-is with the legacy message type
    return sendPacket(msg.msgId, msg.data, msg.dataLength);
}

// =============================================================================
// Statistics
// =============================================================================

void IPCProtocol::getStatistics(IPC_Statistics_t *stats) {
    if (stats == nullptr) return;
    
    stats->rxPacketCount = _rxPacketCount;
    stats->txPacketCount = _txPacketCount;
    stats->rxErrorCount = _rxErrorCount;
    stats->crcErrorCount = _crcErrorCount;
    stats->lastRxTime = _lastRxTime;
    stats->lastTxTime = _lastTxTime;
}

void IPCProtocol::resetStatistics() {
    _rxPacketCount = 0;
    _txPacketCount = 0;
    _rxErrorCount = 0;
    _crcErrorCount = 0;
}
