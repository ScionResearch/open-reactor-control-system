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

// Global IPC object
extern IPCProtocol ipc;