#include "powerManager.h"

void init_powerManager(void) {
  xTaskCreate(managePower, "Pwr updt", configMINIMAL_STACK_SIZE, NULL, 1, NULL);
}

void managePower(void *param) {
    (void)param;
    while (!core1setupComplete || !core0setupComplete) vTaskDelay(pdMS_TO_TICKS(100));
  
    analogReadResolution(12);
    log(LOG_INFO, false, "Power monitoring task started\n");
  
    float Vpsu, V20, V5;
    bool psuOK, V20OK, V5OK;
  
    // Task loop
    while (1) {
      bool statusChanged = false;

      // Read voltage values
      Vpsu = V20 = V5 = 0.0;
      for (int i = 0; i < 10; i++) {
        Vpsu += (float)analogRead(PIN_PS_24V_FB) * V_PSU_MUL_V;
        V20 += (float)analogRead(PIN_PS_20V_FB) * V_PSU_MUL_V;
        V5 += (float)analogRead(PIN_PS_5V_FB) * V_5V_MUL_V;
        vTaskDelay(pdMS_TO_TICKS(10));
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
      if (xSemaphoreTake(statusMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        status.Vpsu = Vpsu;
        status.V20 = V20;
        status.V5 = V5;
        status.psuOK = psuOK;
        status.V20OK = V20OK;
        status.V5OK = V5OK;
        xSemaphoreGive(statusMutex);
      };
      vTaskDelay(pdMS_TO_TICKS(1000));
    }
  }