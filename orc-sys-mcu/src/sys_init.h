#pragma once

#define VERSION "1.0.1"

// Include libraries
#include <Arduino.h>
#include <W5500lwIP.h>
#include <WebServer.h>
#include <NTPClient.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <PubSubClient.h>
#include <SPI.h>
#include "Adafruit_NeoPixel.h"
#include "MCP79410.h"
#include "IPCProtocol.h"
#include "IPCDataStructs.h"

// Include program files
#include "hardware/pins.h"

#include "config/ioConfig.h"

#include "network/networkManager.h"
#include "webAPI/webServer.h"
#include "mqtt/mqttManager.h"

#include "utils/ipcManager.h"
#include "utils/logger.h"
#include "utils/objectCache.h"
#include "utils/powerManager.h"
#include "utils/statusManager.h"
#include "utils/terminalManager.h"
#include "utils/timeManager.h"

#include "storage/sdManager.h"


void init_core0(void);
void init_core1(void);
void manage_core0(void);
void manage_core1(void);

// Control manager prototype
void init_controlManager(void);

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