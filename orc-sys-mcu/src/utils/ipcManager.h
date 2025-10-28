#pragma once

#include "../sys_init.h"

void init_ipcManager(void);
void manageIPC(void);

// IPC message handlers
void registerIpcCallbacks(void);
void handleSensorData(uint8_t messageType, const uint8_t *payload, uint16_t length);
void handlePing(uint8_t messageType, const uint8_t *payload, uint16_t length);
void handlePong(uint8_t messageType, const uint8_t *payload, uint16_t length);
void handleHello(uint8_t messageType, const uint8_t *payload, uint16_t length);
void handleHelloAck(uint8_t messageType, const uint8_t *payload, uint16_t length);
void handleError(uint8_t messageType, const uint8_t *payload, uint16_t length);
void handleFaultNotify(uint8_t messageType, const uint8_t *payload, uint16_t length);
void handleControlAck(uint8_t messageType, const uint8_t *payload, uint16_t length);
void handleDeviceStatus(uint8_t messageType, const uint8_t *payload, uint16_t length);

// Output control command senders
bool sendDigitalOutputCommand(uint16_t index, uint8_t command, bool state, float pwmDuty);
bool sendAnalogOutputCommand(uint16_t index, uint8_t command, float value);
bool sendStepperCommand(uint8_t command, float rpm, bool direction);
bool sendDCMotorCommand(uint16_t index, uint8_t command, float power, bool direction);

// Device management command senders
bool sendDeviceCreateCommand(uint8_t startIndex, const IPC_DeviceConfig_t* config);
bool sendDeviceDeleteCommand(uint8_t startIndex);
bool sendDeviceConfigCommand(uint8_t startIndex, const IPC_DeviceConfig_t* config);
bool sendDeviceQueryCommand(uint8_t startIndex);

// Global IPC object
extern IPCProtocol ipc;