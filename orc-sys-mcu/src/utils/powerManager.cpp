#include "powerManager.h"

uint32_t powerTS = 0;

void init_powerManager(void) {
  analogReadResolution(12);
  log(LOG_INFO, false, "Power monitoring task started\n");
}

void managePower(void) {
  if (millis() - powerTS < POWER_UPDATE_INTERVAL) return;
  powerTS += POWER_UPDATE_INTERVAL;
  static float Vpsu, V20, V5;
  static bool psuOK, V20OK, V5OK;
  bool statusChanged = false;

  // Read voltage values
  Vpsu = V20 = V5 = 0.0;
  for (int i = 0; i < 10; i++) {
    Vpsu += (float)analogRead(PIN_PS_24V_FB) * V_PSU_MUL_V;
    V20 += (float)analogRead(PIN_PS_20V_FB) * V_PSU_MUL_V;
    V5 += (float)analogRead(PIN_PS_5V_FB) * V_5V_MUL_V;
  }
  Vpsu /= 10.0;
  V20 /= 10.0;
  V5 /= 10.0;

  // Check PSU voltage
  if ((Vpsu > V_PSU_MAX || Vpsu < V_PSU_MIN) && psuOK) {
    log(LOG_WARNING, false, "PSU voltage out of range: %.2f V\n", Vpsu);
    statusChanged = true;
    psuOK = false;
  } else if (Vpsu <= V_PSU_MAX && Vpsu >= V_PSU_MIN && !psuOK) {
    log(LOG_INFO, false, "PSU voltage OK: %.2f V\n", Vpsu);
    statusChanged = true;
    psuOK = true;
  }

  // Check 20V voltage
  if ((V20 > V_20V_MAX || V20 < V_20V_MIN) && V20OK) {
    log(LOG_WARNING, false, "20V voltage out of range: %.2f V\n", V20);
    statusChanged = true;
    V20OK = false;
  } else if (V20 <= V_20V_MAX && V20 >= V_20V_MIN && !V20OK) {
    log(LOG_INFO, false, "20V voltage OK: %.2f V\n", V20);
    statusChanged = true;
    V20OK = true;
  }

  // Check 5V voltage
  if ((V5 > V_5V_MAX || V5 < V_5V_MIN) && V5OK) {
    log(LOG_WARNING, false, "5V voltage out of range: %.2f V\n", V5);
    statusChanged = true;
    V5OK = false;
  } else if (V5 <= V_5V_MAX && V5 >= V_5V_MIN && !V5OK) {
    log(LOG_INFO, false, "5V voltage OK: %.2f V\n", V5);
    statusChanged = true;
    V5OK = true;
  }
  
  // Update global status
  if (statusLocked) return;
  statusLocked = true;
  status.Vpsu = Vpsu;
  status.V20 = V20;
  status.V5 = V5;
  if (statusChanged) {
    status.psuOK = psuOK;
    status.V20OK = V20OK;
    status.V5OK = V5OK;
  }
  status.updated = true;
  statusLocked = false;
}