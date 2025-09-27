#pragma once

#include "../sys_init.h"

void init_timeManager(void);
void handleTimeManager(void);
DateTime epochToDateTime(time_t epochTime);
void manageTime(void);
bool updateGlobalDateTime(const DateTime &dt);
bool getGlobalDateTime(DateTime &dt, uint32_t timeout = 1000);

String getISO8601Timestamp(uint32_t timeout = 100); // <-- ADDED FOR MQTT SENSOR PUBLISHING

extern MCP79410 rtc;

// Global DateTime protection
extern volatile bool dateTimeLocked;
extern volatile bool dateTimeWriteLocked;
extern DateTime globalDateTime;

// Update timing
#define TIME_UPDATE_INTERVAL 1000
