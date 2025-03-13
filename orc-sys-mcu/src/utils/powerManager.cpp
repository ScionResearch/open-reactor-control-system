#include "powerManager.h"

// Power monitoring variables
float Vpsu = 0.0, V20 = 0.0, V5 = 0.0;
bool psuOK = true, V20OK = true, V5OK = true;
unsigned long lastPowerCheck = 0;
int sampleCount = 0;
float VpsuSum = 0.0, V20Sum = 0.0, V5Sum = 0.0;

void init_powerManager(void) {
  analogReadResolution(12);
  log(LOG_INFO, false, "Power manager initialized\n");
}

void handlePowerManager(void) {
  unsigned long currentMillis = millis();
  bool statusChanged = false;
  
  // Sample voltages every 100ms, average over 10 samples (1 second total)
  if (currentMillis - lastPowerCheck >= 100) {
    lastPowerCheck = currentMillis;
    
    // Take a sample
    VpsuSum += (float)analogRead(PIN_PS_24V_FB) * V_PSU_MUL_V;
    V20Sum += (float)analogRead(PIN_PS_20V_FB) * V_PSU_MUL_V;
    V5Sum += (float)analogRead(PIN_PS_5V_FB) * V_5V_MUL_V;
    sampleCount++;
    
    // Process readings after collecting 10 samples
    if (sampleCount >= 10) {
      Vpsu = VpsuSum / 10.0;
      V20 = V20Sum / 10.0;
      V5 = V5Sum / 10.0;
      
      // Reset for next cycle
      VpsuSum = V20Sum = V5Sum = 0.0;
      sampleCount = 0;
      
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
      status.Vpsu = Vpsu;
      status.V20 = V20;
      status.V5 = V5;
      status.psuOK = psuOK;
      status.V20OK = V20OK;
      status.V5OK = V5OK;
    }
  }
}