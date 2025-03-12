#include "ledManager.h"

// Object definition
Adafruit_NeoPixel leds(4, PIN_LED_DAT, NEO_GRB + NEO_KHZ800);

// Status variables
StatusVariables status;
SemaphoreHandle_t statusMutex = NULL;

void init_ledManager() {
    // Create synchronization primitives
    statusMutex = xSemaphoreCreateMutex();
    if (statusMutex == NULL) {
        log(LOG_ERROR, false, "Failed to create statusMutex!\n");
        while (1);
    }
    xTaskCreate(statusLEDs, "LED stat", configMINIMAL_STACK_SIZE, NULL, 1, NULL);
}

// Thread-safe LED colour setter
bool setLEDcolour(uint8_t led, uint32_t colour) {
    if (led > 3) {
      log(LOG_ERROR, false,"Invalid LED number: %d\n", led);
      return false;
    }
    if (xSemaphoreTake(statusMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
      status.LEDcolour[led] = colour;
      xSemaphoreGive(statusMutex);
      return true;
    }
    return false;
}

void statusLEDs(void *param)
{
  (void)param;

  uint32_t loopCounter = 0;
  uint32_t ledRefreshInterval = 20;
  uint32_t loopCountsPerHalfSec = 500 / ledRefreshInterval;
  bool blinkState = false;

  leds.begin();
  leds.setBrightness(50);
  leds.fill(LED_COLOR_OFF, 0, 4);
  leds.show();
  log(LOG_INFO, false, "LED status task started\n");

  // Task loop
  while (1) {
    uint32_t statusLEDcolour = STATUS_WARNING;
    if (xSemaphoreTake(statusMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
      for (int i = 0; i < 3; i++) {
        leds.setPixelColor(i, status.LEDcolour[i]);
      }
      // Check status items to determin main status LED colour
      if (!status.IPCOK || !status.RTCOK) {
        statusLEDcolour = LED_STATUS_ERROR;
      } else if (!status.psuOK || !status.V20OK || !status.V5OK || !status.sdCardOK) {
        statusLEDcolour = LED_STATUS_WARNING;
      } else {
        statusLEDcolour = LED_STATUS_OK;
      }
      xSemaphoreGive(statusMutex);
    }
    leds.show();
    vTaskDelay(pdMS_TO_TICKS(ledRefreshInterval));
    loopCounter++;
    // Things to do every half second
    if (loopCounter >= loopCountsPerHalfSec) {
      loopCounter = 0;
      blinkState = !blinkState;

      if (blinkState) {
        leds.setPixelColor(LED_SYSTEM_STATUS, statusLEDcolour);
        leds.show();
      } else {
        leds.setPixelColor(LED_SYSTEM_STATUS, LED_COLOR_OFF);
        leds.show();
      }
    }
  }
}