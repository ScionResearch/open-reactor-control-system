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
      if (Vpsu > V_PSU_MAX || Vpsu < V_PSU_MIN) {
        if (psuOK) log(LOG_WARNING, true, "PSU voltage out of range: %.2f V\n", Vpsu);
        psuOK = false;
      }
      else psuOK = true;
      if (V20 > V_20V_MAX || V20 < V_20V_MIN) {
        if (V20OK) log(LOG_WARNING, true, "20V voltage out of range: %.2f V\n", V20);
        V20OK = false;
      }
      else V20OK = true;
      if (V5 > V_5V_MAX || V5 < V_5V_MIN) {
        if (V5OK) log(LOG_WARNING, true, "5V voltage out of range: %.2f V\n", V5);
        V5OK = false;
      }
      else V5OK = true;
      if (!psuOK || !V20OK || !V5OK) {
        setLEDcolour(LED_SYSTEM_STATUS, LED_STATUS_WARNING);
      }
      else setLEDcolour(LED_SYSTEM_STATUS, LED_STATUS_OK);
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