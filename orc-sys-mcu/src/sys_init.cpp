#include "sys_init.h"

// Object definitions
IPCProtocol ipc(Serial1);

bool core0setupComplete = false;
bool core1setupComplete = false;

bool debug = true;

void init_core0(void);

void init_core1(void);

void init_core0(void) {
    log(LOG_DEBUG, false, "[Core0] init_core0() start\n");
    init_logger();
    init_network();
    init_mqttManager();
}

void init_core1(void) {
    init_statusManager();
    init_timeManager();
    init_powerManager();
    init_terminalManager();
    init_ipcManager();
    init_controlManager(); // <-- Add control manager init
    registerIpcCallbacks(); // <-- Register IPC-to-MQTT callbacks
    while (!core0setupComplete) delay(100); // 
    init_sdManager();   
}

void manage_core0(void) {
    log(LOG_DEBUG, false, "[Core0] manage_core0() start\n");
    log(LOG_DEBUG, false, "[Core0] manageNetwork\n");
    manageNetwork();
    log(LOG_DEBUG, false, "[Core0] manageMqtt\n");
    manageMqtt();
}

void manage_core1(void) {
    log(LOG_DEBUG, false, "[Core1] manageStatus\n");
    manageStatus();
    log(LOG_DEBUG, false, "[Core1] manageTime\n");
    manageTime();
    log(LOG_DEBUG, false, "[Core1] managePower\n");
    managePower();
    log(LOG_DEBUG, false, "[Core1] manageTerminal\n");
    manageTerminal();
    log(LOG_DEBUG, false, "[Core1] manageIPC\n");
    manageIPC();
    log(LOG_DEBUG, false, "[Core1] manageSD\n");
    manageSD();
}
