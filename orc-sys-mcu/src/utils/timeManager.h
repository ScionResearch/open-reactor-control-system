#pragma once

#include "../sys_init.h"

void init_timeManager(void);
void handleTimeManager(void);
DateTime epochToDateTime(time_t epochTime);
bool updateGlobalDateTime(const DateTime &dt);
bool getGlobalDateTime(DateTime &dt);

extern MCP79410 rtc;
extern DateTime globalDateTime;