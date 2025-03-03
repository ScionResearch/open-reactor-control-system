#pragma once

// Include libraries
#include <Arduino.h>
#include <W5500lwIP.h>
#include <WebServer.h>
#include <NTPClient.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <SdFat.h>
#include <SPI.h>
#include <sdios.h>
#include "Adafruit_Neopixel.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
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

void init_core0(void);

void init_core1(void);

// Object definitions
extern IPCProtocol ipc;

extern bool core0setupComplete;
extern bool core1setupComplete;

extern bool debug;