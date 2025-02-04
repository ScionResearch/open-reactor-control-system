#include "IPCProtocol.h"
#include <string.h>


IPCProtocol::IPCProtocol(HardwareSerial& serialPort) : _serial(serialPort) {}

void IPCProtocol::begin(long baudrate) {
    _serial.begin(baudrate);
}

// Function to calculate CRC-16 (CCITT standard)
uint16_t IPCProtocol::calculateCRC(const Message& msg) const
{
    uint16_t crc = 0xFFFF; // Initial value
    uint8_t* dataPtr = (uint8_t*)&msg;
    
    // Calculate the size of the structure up to the CRC.
    size_t size = sizeof(Message) - sizeof(msg.crc);

    for (size_t i = 0; i < size ; i++)
    {
        uint8_t byte = dataPtr[i];
        for (int j = 0; j < 8; j++)
        {
            bool bit = ((crc ^ byte) & 0x01);
            crc >>= 1;
            if (bit)
            {
                crc ^= 0xA001;
            }
            byte >>= 1;
        }
    }
    return crc;
}

bool IPCProtocol::sendMessage(const Message& msg) {
    uint8_t buffer[sizeof(Message) + 3];
    int bufferIndex = 0;
    
    buffer[bufferIndex++] = _startByte;
    
    // Copy the message into the buffer.
    memcpy(buffer + bufferIndex, &msg, sizeof(Message)-sizeof(msg.crc));
    bufferIndex += sizeof(Message)-sizeof(msg.crc);

    // Calculate CRC
    uint16_t crc = calculateCRC(msg);
    memcpy(buffer+bufferIndex, &crc, sizeof(crc));
    bufferIndex += sizeof(crc);
    
    buffer[bufferIndex++] = _endByte;


    _serial.write(buffer, bufferIndex);
    
    return true;
}

void IPCProtocol::registerCallback(uint8_t msgId, std::function<void(const Message&)> callback) {
  if (_numCallbacks < 10) {
      _callbacks[_numCallbacks].msgId = msgId;
      _callbacks[_numCallbacks].callback = callback;
      _numCallbacks++;
  }
}

void IPCProtocol::update() {
    while (_serial.available() > 0) {
       
        int bytesRead = _serial.read();

        if (_rxBufferIndex == 0 && bytesRead != _startByte) {
           //Waiting for start byte, ignore this message.
           continue;
        }
        _rxBuffer[_rxBufferIndex++] = bytesRead;

        // Check for end of message and that the buffer is big enough.
        if (_rxBufferIndex >= 7 && bytesRead == _endByte)
        {
          if (_rxBufferIndex > (int)(sizeof(Message) + 2)) {
            // message is too large, error.
            _rxBufferIndex = 0; // reset buffer for next message
            continue;
          }
          
            // Message is likely complete, try and decode.
            Message msg;
            memcpy(&msg, _rxBuffer+1, sizeof(Message)-sizeof(msg.crc)); // copy data excluding start and end bytes, and CRC
            uint16_t receivedCrc;
            memcpy(&receivedCrc, _rxBuffer + 1 + (sizeof(Message)-sizeof(msg.crc)), sizeof(receivedCrc)); // copy the CRC

            uint16_t calculatedCrc = calculateCRC(msg);

            if (calculatedCrc == receivedCrc)
            {
               for (int i = 0; i < _numCallbacks; i++) {
                    if (_callbacks[i].msgId == msg.msgId) {
                    _callbacks[i].callback(msg);
                    break; // Exit loop once the handler is found
                }
              }
            }
            _rxBufferIndex = 0; // reset buffer for next message
        }
       if(_rxBufferIndex >= (int)(sizeof(Message)+2)) {
            // If we exceed the maximum size of the buffer then reset for the next message.
           _rxBufferIndex = 0;
        }
    }
}