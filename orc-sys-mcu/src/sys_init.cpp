#include "sys_init.h"

// Object definitions
IPCProtocol ipc(Serial1);

bool core0setupComplete = false;
bool core1setupComplete = false;

void init_core0(void);

void init_core1(void);

void init_core0(void) {
    init_logger();
    init_network();
}

void init_core1(void) {
    init_timeManager();
    init_ledManager();
    init_powerManager();
    init_terminalManager();
    init_ipcManager();
}