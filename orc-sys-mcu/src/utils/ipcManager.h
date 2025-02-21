#pragma once

#include <Arduino.h>
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "logger.h"
#include "ledManager.h"
#include "../sys_init.h"

void init_ipcManager(void);