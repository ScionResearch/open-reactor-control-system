#include "sys_init.h"

// Object definitions
IPCProtocol ipc(Serial1);

bool core0setupComplete = false;
bool core1setupComplete = false;

bool debug = true;

void init_core0(void);

void init_core1(void);

void init_core0(void) {
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
    registerIpcCallbacks(); // <-- Register IPC-to-MQTT callbacks
    while (!core0setupComplete) delay(100); // 
    init_sdManager();   
}

void manage_core0(void) {
    manageNetwork();
    manageMqtt();
}

void manage_core1(void) {
    manageStatus();
    manageTime();
    managePower();
    manageTerminal();
    manageIPC();
    manageSD();
}
