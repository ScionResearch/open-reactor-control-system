#include "drv_ipc.h"

// Global IPC driver instance
IPC_Driver_t ipcDriver;

// ============================================================================
// CRC16 IMPLEMENTATION
// ============================================================================

uint16_t ipc_calcCRC16(const uint8_t *data, uint16_t length) {
    uint16_t crc = 0xFFFF;  // Initial value
    
    for (uint16_t i = 0; i < length; i++) {
        crc ^= (uint16_t)data[i] << 8;
        for (uint8_t bit = 0; bit < 8; bit++) {
            if (crc & 0x8000) {
                crc = (crc << 1) ^ 0x1021;  // Polynomial
            } else {
                crc = crc << 1;
            }
        }
    }
    
    return crc;
}

// ============================================================================
// INITIALIZATION
// ============================================================================

bool ipc_init(void) {
    // Initialize structure
    memset(&ipcDriver, 0, sizeof(IPC_Driver_t));
    
    // Configure UART
    ipcDriver.uart = &Serial1;
    // Note: SAME51 Serial1 uses hardware FIFO (default ~64 bytes RX buffer)
    // This should be sufficient as IPC protocol processes bytes continuously
    ipcDriver.uart->begin(2000000);  // 2 Mbps
    
    // Set initial state
    ipcDriver.state = IPC_STATE_IDLE;
    ipcDriver.connected = false;
    ipcDriver.connectionState = IPC_CONN_DISCONNECTED;  // Start in disconnected state
    ipcDriver.hardwareReady = false;  // Will be set after hardware init
    ipcDriver.lastActivity = millis();
    ipcDriver.lastKeepalive = millis();
    ipcDriver.lastHelloBroadcast = 0;
    
    strcpy(ipcDriver.message, "IPC initialized");
    ipcDriver.newMessage = true;
    
    return true;
}

void ipc_setHardwareReady(void) {
    ipcDriver.hardwareReady = true;
    Serial.println("[IPC] Hardware ready, starting HELLO broadcasts");
}

// ============================================================================
// PACKET TRANSMISSION
// ============================================================================

bool ipc_sendPacket(uint8_t msgType, const uint8_t *payload, uint16_t len) {
    #if IPC_DEBUG_ENABLED
    Serial.printf("[IPC TX] sendPacket called: msgType=0x%02X, len=%u\n", msgType, len);
    #endif
    
    // Check payload size
    if (len > IPC_MAX_PAYLOAD_SIZE) {
        ipcDriver.fault = true;
        strcpy(ipcDriver.message, "IPC TX: Payload too large");
        #if IPC_DEBUG_ENABLED
        Serial.println("[IPC TX] ERROR: Payload too large");
        #endif
        return false;
    }
    
    // Check queue space
    if (!ipc_txQueueHasSpace()) {
        ipcDriver.txErrorCount++;
        ipcDriver.fault = true;
        strcpy(ipcDriver.message, "IPC TX: Queue full");
        #if IPC_DEBUG_ENABLED
        Serial.println("[IPC TX] ERROR: Queue full");
        #endif
        return false;
    }
    
    // Add to queue
    IPC_TxPacket_t *packet = &ipcDriver.txQueue[ipcDriver.txQueueHead];
    packet->msgType = msgType;
    packet->payloadLen = len;
    if (len > 0 && payload != nullptr) {
        memcpy(packet->payload, payload, len);
    }
    
    // Advance head
    ipcDriver.txQueueHead = (ipcDriver.txQueueHead + 1) % IPC_TX_QUEUE_SIZE;
    
    #if IPC_DEBUG_ENABLED
    Serial.printf("[IPC TX] Packet queued (head=%u, tail=%u)\n", ipcDriver.txQueueHead, ipcDriver.txQueueTail);
    #endif
    return true;
}

bool ipc_processTxQueue(void) {
    // Check if queue is empty
    if (ipcDriver.txQueueHead == ipcDriver.txQueueTail) {
        return false;
    }
    
    #if IPC_DEBUG_ENABLED
    Serial.printf("[IPC TX] Processing TX queue (head=%u, tail=%u)\n", 
                 ipcDriver.txQueueHead, ipcDriver.txQueueTail);
    #endif
    
    // Get packet from queue
    IPC_TxPacket_t *packet = &ipcDriver.txQueue[ipcDriver.txQueueTail];
    
    #if IPC_DEBUG_ENABLED
    Serial.printf("[IPC TX] Sending msgType=0x%02X, payloadLen=%u\n", 
                 packet->msgType, packet->payloadLen);
    #endif
    
    // Build packet buffer
    uint8_t txBuffer[IPC_MAX_PACKET_SIZE * 2];  // Extra space for byte stuffing
    uint16_t bufferPos = 0;
    
    // Start byte
    txBuffer[bufferPos++] = IPC_START_BYTE;
    
    // Calculate CRC over LENGTH + MSG_TYPE + PAYLOAD (NOT CRC)
    uint8_t crcData[3 + IPC_MAX_PAYLOAD_SIZE];
    uint16_t crcDataLen = 0;
    
    // LENGTH = size of (MSG_TYPE (1) + PAYLOAD (N))
    // NOTE: Length field itself and CRC are NOT included in length value
    uint16_t totalLength = 1 + packet->payloadLen;
    
    // LENGTH (big-endian)
    crcData[crcDataLen++] = (totalLength >> 8) & 0xFF;
    crcData[crcDataLen++] = totalLength & 0xFF;
    
    // MSG_TYPE
    crcData[crcDataLen++] = packet->msgType;
    
    // PAYLOAD
    if (packet->payloadLen > 0) {
        memcpy(&crcData[crcDataLen], packet->payload, packet->payloadLen);
        crcDataLen += packet->payloadLen;
    }
    
    // Calculate CRC
    uint16_t crc = ipc_calcCRC16(crcData, crcDataLen);
    
    // Add LENGTH, MSG_TYPE, PAYLOAD, CRC with byte stuffing
    for (uint16_t i = 0; i < crcDataLen; i++) {
        uint8_t byte = crcData[i];
        if (byte == IPC_START_BYTE || byte == IPC_ESCAPE_BYTE) {
            txBuffer[bufferPos++] = IPC_ESCAPE_BYTE;
            txBuffer[bufferPos++] = byte ^ IPC_ESCAPE_XOR;
        } else {
            txBuffer[bufferPos++] = byte;
        }
    }
    
    // Add CRC (big-endian) with byte stuffing
    uint8_t crcBytes[2];
    crcBytes[0] = (crc >> 8) & 0xFF;
    crcBytes[1] = crc & 0xFF;
    
    for (uint8_t i = 0; i < 2; i++) {
        uint8_t byte = crcBytes[i];
        if (byte == IPC_START_BYTE || byte == IPC_ESCAPE_BYTE) {
            txBuffer[bufferPos++] = IPC_ESCAPE_BYTE;
            txBuffer[bufferPos++] = byte ^ IPC_ESCAPE_XOR;
        } else {
            txBuffer[bufferPos++] = byte;
        }
    }
    
    // End byte
    txBuffer[bufferPos++] = IPC_END_BYTE;
    
    // Packet sent successfully (debug logging removed for clarity)
    
    // Send packet
    ipcDriver.uart->write(txBuffer, bufferPos);
    
    // Update statistics
    ipcDriver.txPacketCount++;
    // NOTE: Do NOT update lastActivity here - only RX should update it
    // Otherwise timeout detection won't work (sending keeps connection "alive")
    
    // Advance tail
    ipcDriver.txQueueTail = (ipcDriver.txQueueTail + 1) % IPC_TX_QUEUE_SIZE;
    
    return true;
}

// ============================================================================
// PACKET RECEPTION
// ============================================================================

void ipc_processRxByte(uint8_t byte) {
    uint32_t now = millis();
    
    switch (ipcDriver.state) {
        case IPC_STATE_IDLE:
            if (byte == IPC_START_BYTE) {
                // Start of new packet
                ipcDriver.rxBufferPos = 0;
                ipcDriver.escapeNext = false;
                ipcDriver.rxStartTime = now;
                ipcDriver.state = IPC_STATE_RECEIVING;
            }
            break;
            
        case IPC_STATE_RECEIVING:
            // Check for timeout
            if ((now - ipcDriver.rxStartTime) > IPC_TIMEOUT_MS) {
                Serial.printf("[IPC RX] ERROR: TIMEOUT after %lu ms, %u bytes buffered\n", 
                             now - ipcDriver.rxStartTime, ipcDriver.rxBufferPos);
                ipcDriver.state = IPC_STATE_ERROR;
                ipcDriver.rxErrorCount++;
                strcpy(ipcDriver.message, "IPC RX: Timeout");
                ipcDriver.newMessage = true;
                return;
            }
            
            // Check for buffer overflow
            if (ipcDriver.rxBufferPos >= IPC_RX_BUFFER_SIZE) {
                Serial.printf("[IPC RX] ERROR: Buffer overflow at %u bytes\n", ipcDriver.rxBufferPos);
                ipcDriver.state = IPC_STATE_ERROR;
                ipcDriver.rxErrorCount++;
                strcpy(ipcDriver.message, "IPC RX: Buffer overflow");
                ipcDriver.newMessage = true;
                return;
            }
            
            // Handle end byte
            if (byte == IPC_END_BYTE && !ipcDriver.escapeNext) {
                ipcDriver.state = IPC_STATE_PROCESSING;
                return;
            }
            
            // Handle escape sequences
            if (byte == IPC_ESCAPE_BYTE && !ipcDriver.escapeNext) {
                ipcDriver.escapeNext = true;
                return;
            }
            
            // Store byte (with escape handling)
            if (ipcDriver.escapeNext) {
                ipcDriver.rxBuffer[ipcDriver.rxBufferPos++] = byte ^ IPC_ESCAPE_XOR;
                ipcDriver.escapeNext = false;
            } else {
                ipcDriver.rxBuffer[ipcDriver.rxBufferPos++] = byte;
            }
            break;
            
        default:
            break;
    }
}

void ipc_processReceivedPacket(void) {
    
    // Need at least 5 bytes: LENGTH(2) + MSG_TYPE(1) + CRC(2)
    if (ipcDriver.rxBufferPos < 5) {
        ipcDriver.state = IPC_STATE_ERROR;
        ipcDriver.rxErrorCount++;
        strcpy(ipcDriver.message, "IPC RX: Packet too short");
        ipcDriver.newMessage = true;
        return;
    }
    
    // Extract LENGTH (big-endian)
    // LENGTH = size of (MSG_TYPE (1) + PAYLOAD (N))
    // Note: LENGTH field itself and CRC are NOT included in the length value
    uint16_t packetLength = ((uint16_t)ipcDriver.rxBuffer[0] << 8) | ipcDriver.rxBuffer[1];
    
    // Extract MSG_TYPE
    uint8_t msgType = ipcDriver.rxBuffer[2];
    
    // Calculate payload length: packetLength - 1 (MSG_TYPE)
    uint16_t payloadLen = packetLength - 1;
    
    // Check buffer length: LENGTH(2) + packetLength + CRC(2)
    uint16_t expectedLen = 2 + packetLength + 2;
    if (ipcDriver.rxBufferPos != expectedLen) {
        Serial.printf("[IPC] ERROR: Length mismatch (buffer %u bytes, expected %u)\n",
                      ipcDriver.rxBufferPos, expectedLen);
        ipcDriver.state = IPC_STATE_ERROR;
        ipcDriver.rxErrorCount++;
        sprintf(ipcDriver.message, "IPC RX: Length mismatch (exp %u, got %u)", 
                expectedLen, ipcDriver.rxBufferPos);
        ipcDriver.newMessage = true;
        return;
    }
    
    // Extract CRC (big-endian)
    uint16_t rxCRC = ((uint16_t)ipcDriver.rxBuffer[ipcDriver.rxBufferPos - 2] << 8) | 
                     ipcDriver.rxBuffer[ipcDriver.rxBufferPos - 1];
    
    // Calculate CRC (over LENGTH + MSG_TYPE + PAYLOAD)
    uint16_t calcCRC = ipc_calcCRC16(ipcDriver.rxBuffer, ipcDriver.rxBufferPos - 2);
    
    // Verify CRC
    if (rxCRC != calcCRC) {
        Serial.printf("[IPC] ERROR: CRC mismatch (0x%04X != 0x%04X)\n", rxCRC, calcCRC);
        ipcDriver.state = IPC_STATE_ERROR;
        ipcDriver.crcErrorCount++;
        ipcDriver.rxErrorCount++;
        sprintf(ipcDriver.message, "IPC RX: CRC error (exp 0x%04X, got 0x%04X)", 
                calcCRC, rxCRC);
        ipcDriver.newMessage = true;
        return;
    }
    
    // Packet validated successfully (logging handled by message handlers)
    
    // Extract payload
    ipcDriver.rxMsgType = msgType;
    ipcDriver.rxPayloadLen = payloadLen;
    if (payloadLen > 0) {
        memcpy(ipcDriver.rxPayload, &ipcDriver.rxBuffer[3], payloadLen);
    }
    
    // Update statistics
    ipcDriver.rxPacketCount++;
    ipcDriver.lastActivity = millis();
    
    // Process message
    ipc_handleMessage(msgType, ipcDriver.rxPayload, payloadLen);
    
    // Return to idle
    ipcDriver.state = IPC_STATE_IDLE;
}

// ============================================================================
// UPDATE FUNCTION
// ============================================================================

void ipc_update(void) {
    uint32_t now = millis();
    
    // Handle robust startup - broadcast HELLO when disconnected and hardware ready
    if (ipcDriver.hardwareReady && 
        (ipcDriver.connectionState == IPC_CONN_DISCONNECTED || 
         ipcDriver.connectionState == IPC_CONN_HANDSHAKE_SENT)) {
        if (now - ipcDriver.lastHelloBroadcast > 2000) {  // Broadcast every 2 seconds
            if (ipc_sendHello()) {
                ipcDriver.lastHelloBroadcast = now;
                // Do NOT update lastActivity - only RX updates it for timeout detection
                ipcDriver.connectionState = IPC_CONN_HANDSHAKE_SENT;
                Serial.println("[IPC] Broadcasting HELLO for connection");
            }
        }
    }
    
    // Process RX bytes
    while (ipcDriver.uart->available()) {
        uint8_t byte = ipcDriver.uart->read();
        ipc_processRxByte(byte);
        
        // If we're in PROCESSING state, handle the packet
        if (ipcDriver.state == IPC_STATE_PROCESSING) {
            ipc_processReceivedPacket();
        }
        
        // If we're in ERROR state, clear and return to IDLE
        if (ipcDriver.state == IPC_STATE_ERROR) {
            ipc_clearRxBuffer();
            ipcDriver.state = IPC_STATE_IDLE;
        }
    }
    
    // Process TX queue
    if (ipc_txQueueCount() > 0) {
        ipc_processTxQueue();
    }
    
    // Send keepalive ping if connected
    if (ipcDriver.connectionState == IPC_CONN_CONNECTED) {
        if ((now - ipcDriver.lastKeepalive) > IPC_KEEPALIVE_MS) {
            ipc_sendPing();
            ipcDriver.lastKeepalive = now;
            //Serial.println("[IPC] Sent PING keepalive");
        }
    }
    
    // Check connection timeout and reset to disconnected state
    // Use 5-second timeout (5 missed PING/PONG cycles) to detect dead connection
    if (ipcDriver.connectionState == IPC_CONN_CONNECTED || 
        ipcDriver.connectionState == IPC_CONN_HANDSHAKE_SENT) {
        // Skip check if lastActivity is in the future (just processed a message in this cycle)
        if (ipcDriver.lastActivity <= now) {
            uint32_t timeSinceActivity = now - ipcDriver.lastActivity;
            if (timeSinceActivity > 5000) {  // 5 seconds
                Serial.printf("[IPC] Connection timeout (%lu ms since last activity), returning to disconnected state\n", timeSinceActivity);
                ipcDriver.connectionState = IPC_CONN_DISCONNECTED;
                ipcDriver.connected = false;
                ipcDriver.lastHelloBroadcast = 0;  // Reset to trigger immediate broadcast
                ipcDriver.lastActivity = now;      // Reset activity timestamp for fresh start
            }
        }
    }
}

// ============================================================================
// HELPER FUNCTIONS
// ============================================================================

bool ipc_sendPing(void) {
    return ipc_sendPacket(IPC_MSG_PING, nullptr, 0);
}

bool ipc_sendPong(void) {
    return ipc_sendPacket(IPC_MSG_PONG, nullptr, 0);
}

bool ipc_sendHello(void) {
    IPC_Hello_t hello;
    hello.protocolVersion = IPC_PROTOCOL_VERSION;
    hello.firmwareVersion = 0x00010000;  // v1.0.0 - TODO: Get from build system
    strcpy(hello.deviceName, "SAME51-IO-MCU");
    
    return ipc_sendPacket(IPC_MSG_HELLO, (uint8_t*)&hello, sizeof(hello));
}

bool ipc_sendError(uint8_t errorCode, const char *message) {
    IPC_Error_t error;
    error.errorCode = errorCode;
    strncpy(error.message, message, sizeof(error.message) - 1);
    error.message[sizeof(error.message) - 1] = '\0';
    
    return ipc_sendPacket(IPC_MSG_ERROR, (uint8_t*)&error, sizeof(error));
}

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================

bool ipc_txQueueHasSpace(void) {
    uint8_t nextHead = (ipcDriver.txQueueHead + 1) % IPC_TX_QUEUE_SIZE;
    return (nextHead != ipcDriver.txQueueTail);
}

uint8_t ipc_txQueueCount(void) {
    if (ipcDriver.txQueueHead >= ipcDriver.txQueueTail) {
        return ipcDriver.txQueueHead - ipcDriver.txQueueTail;
    } else {
        return IPC_TX_QUEUE_SIZE - ipcDriver.txQueueTail + ipcDriver.txQueueHead;
    }
}

void ipc_clearTxQueue(void) {
    ipcDriver.txQueueHead = 0;
    ipcDriver.txQueueTail = 0;
}

void ipc_clearRxBuffer(void) {
    ipcDriver.rxBufferPos = 0;
    ipcDriver.escapeNext = false;
}

bool ipc_isConnected(void) {
    return ipcDriver.connected;
}

void ipc_printStats(void) {
    Serial.println("\n=== IPC Statistics ===");
    Serial.printf("State: %s\n", 
                  ipcDriver.state == IPC_STATE_IDLE ? "IDLE" :
                  ipcDriver.state == IPC_STATE_RECEIVING ? "RECEIVING" :
                  ipcDriver.state == IPC_STATE_PROCESSING ? "PROCESSING" : "ERROR");
    Serial.printf("Connected: %s\n", ipcDriver.connected ? "YES" : "NO");
    Serial.printf("RX Packets: %u\n", ipcDriver.rxPacketCount);
    Serial.printf("TX Packets: %u\n", ipcDriver.txPacketCount);
    Serial.printf("RX Errors: %u\n", ipcDriver.rxErrorCount);
    Serial.printf("TX Errors: %u\n", ipcDriver.txErrorCount);
    Serial.printf("CRC Errors: %u\n", ipcDriver.crcErrorCount);
    Serial.printf("TX Queue: %u/%u\n", ipc_txQueueCount(), IPC_TX_QUEUE_SIZE);
    Serial.printf("Last Activity: %u ms ago\n", millis() - ipcDriver.lastActivity);
    if (ipcDriver.newMessage) {
        Serial.printf("Last Message: %s\n", ipcDriver.message);
    }
    Serial.println("======================");
}
