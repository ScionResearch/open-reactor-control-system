#pragma once

#include "../sys_init.h"

void init_ipcManager(void);
void manageIPC(void);

void registerIpcCallbacks(void); // <-- ADDED FOR SENSOR IPC TO MQTT ROUTING