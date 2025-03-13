#pragma once

// Include libraries
#include <Arduino.h>
#include <W5500lwIP.h>
#include <WebServer.h>
#include <NTPClient.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <SPI.h>
#include "Adafruit_Neopixel.h"
#include "MCP79410.h"
#include "IPCProtocol.h"
#include "IPCDataStructs.h"

// Include program files
#include "hardware/pins.h"

#include "network/network.h"

#include "utils/logger.h"
#include "utils/ledManager.h"
#include "utils/timeManager.h"
#include "utils/powerManager.h"
#include "utils/terminalManager.h"
#include "utils/ipcManager.h"

#include "storage/sdManager.h"

// Timing for cooperative multitasking
#define TASK_INTERVAL_SD_MANAGER    500  // ms
#define TASK_INTERVAL_LED_MANAGER   100  // ms
#define TASK_INTERVAL_TIME_MANAGER  1000 // ms
#define TASK_INTERVAL_POWER_MANAGER 1000 // ms
#define TASK_INTERVAL_TERMINAL      100  // ms
#define TASK_INTERVAL_NETWORK       50   // ms
#define TASK_INTERVAL_IPC           10   // ms - IPC needs faster updates for responsiveness

void init_core0(void);
void init_core1(void);

// Task handler prototypes
void handleSDManager(void);
void handleLEDManager(void);
void handleTimeManager(void);
void handlePowerManager(void);
void handleTerminalManager(void);
void handleIpcManager(void);
void handleNetworkManager(void);

// Object definitions
extern IPCProtocol ipc;

extern bool core0setupComplete;
extern bool core1setupComplete;

extern bool debug;