#pragma once

#include "../sys_init.h"

void init_timeManager(void);
DateTime epochToDateTime(time_t epochTime);
void manageRTC(void *param);
bool updateGlobalDateTime(const DateTime &dt);
bool getGlobalDateTime(DateTime &dt);


extern MCP79410 rtc;

// Global DateTime protection
extern SemaphoreHandle_t dateTimeMutex;
extern DateTime globalDateTime;