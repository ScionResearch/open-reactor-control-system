#include "IPCProtocol.h"
#include <string.h>

IPCProtocol::IPCProtocol(HardwareSerial& serialPort) : _serial(serialPort) {}

void IPCProtocol::begin(long baudrate) {
    _serial.begin(baudrate);
}

uint16_t IPCProtocol::calculateCRC(const Message& msg) const {
    uint16_t crc = 0xFFFF;
    uint8_t* dataPtr = (uint8_t*)&msg;
    size_t size = sizeof(Message) - sizeof(msg.crc);
    for (size_t i = 0; i < size ; i++) {
        uint8_t byte = dataPtr[i];
        for (int j = 0; j < 8; j++) {
            bool bit = ((crc ^ byte) & 0x01);
            crc >>= 1;
            if (bit) {
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
    memcpy(buffer + bufferIndex, &msg, sizeof(Message)-sizeof(msg.crc));
    bufferIndex += sizeof(Message)-sizeof(msg.crc);
    uint16_t crc = calculateCRC(msg);
    memcpy(buffer+bufferIndex, &crc, sizeof(crc));
    bufferIndex += sizeof(crc);
    buffer[bufferIndex++] = _endByte;
    _serial.write(buffer, bufferIndex);
    return true;
}
