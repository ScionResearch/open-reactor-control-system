#ifndef IPC_PROTOCOL_H
#define IPC_PROTOCOL_H

#include <Arduino.h>
#include <stdint.h>
#include <functional>

#define MAX_PAYLOAD_SIZE 128

struct Message {
  uint8_t msgId;
  uint8_t objId;
  uint8_t dataLength;
  uint8_t data[MAX_PAYLOAD_SIZE];
  uint16_t crc;
};

class IPCProtocol {
public:
    IPCProtocol(HardwareSerial& serialPort);
    void begin(long baudrate);
    bool sendMessage(const Message& msg);
    uint16_t calculateCRC(const Message& msg) const;
private:
    HardwareSerial& _serial;
    const uint8_t _startByte = 0xAA;
    const uint8_t _endByte = 0x55;
};

#endif /* IPC_PROTOCOL_H */
