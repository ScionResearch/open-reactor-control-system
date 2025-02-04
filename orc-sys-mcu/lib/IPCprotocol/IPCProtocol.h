#ifndef IPC_PROTOCOL_H
#define IPC_PROTOCOL_H

#include <Arduino.h>
#include <stdint.h>
#include <functional>

// Define the maximum size of the data payload (adjust as needed)
#define MAX_PAYLOAD_SIZE 128

// Define a generic Message struct
struct Message {
  uint8_t msgId;
  uint8_t objId;
  uint8_t dataLength;
  uint8_t data[MAX_PAYLOAD_SIZE];
  uint16_t crc;
};

// Define a structure for mapping a message ID to a callback function
struct MessageCallback {
  uint8_t msgId;
  std::function<void(const Message&)> callback;
};

class IPCProtocol {
public:
    // Constructor: takes the Serial port instance.
    IPCProtocol(HardwareSerial& serialPort);
    
    // Initialise the library
    void begin(long baudrate);

    // Send data
    bool sendMessage(const Message& msg);

    // Register a callback function for a specific message ID
    void registerCallback(uint8_t msgId, std::function<void(const Message&)> callback);

    // Poll for and process any incoming messages
    void update();
    
    // Calculate the CRC value from the message (without the CRC field).
    uint16_t calculateCRC(const Message& msg) const;

private:
  HardwareSerial& _serial;

  // Array to store registered callbacks
  MessageCallback _callbacks[10]; // Store 10 message handlers
  int _numCallbacks = 0; // Track the number of callbacks
    
    // Start and end delimiter constants
    const uint8_t _startByte = 0xAA;
    const uint8_t _endByte = 0x55;
    
    // Receive buffer
    uint8_t _rxBuffer[MAX_PAYLOAD_SIZE + 7];
    int _rxBufferIndex = 0;
};

#endif /* IPC_PROTOCOL_H */