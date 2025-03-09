#include "ipcManager.h"

// Inter-processor communication
void init_ipcManager(void) {
    Serial1.setRX(PIN_SI_RX);
    Serial1.setTX(PIN_SI_TX);
    ipc.begin(115200);
    // Add in handshaking checks here...
    log(LOG_INFO, false, "Inter-processor communication setup complete\n");
  }