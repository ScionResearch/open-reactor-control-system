#include "ledManager.h"

// Object definition
Adafruit_NeoPixel leds(4, PIN_LED_DAT, NEO_GRB + NEO_KHZ800);

// Status variables
StatusVariables status;

// LED state tracking variables
uint32_t loopCounter = 0;
uint32_t ledRefreshInterval = 20;
uint32_t loopCountsPerHalfSec = 500 / ledRefreshInterval;
bool blinkState = false;
unsigned long lastLEDBlinkTime = 0;

void init_ledManager() {
    leds.begin();
    leds.setBrightness(50);
    leds.fill(LED_COLOR_OFF, 0, 4);
    leds.show();
    log(LOG_INFO, false, "LED manager initialized\n");
}

// LED colour setter
bool setLEDcolour(uint8_t led, uint32_t colour) {
    if (led > 3) {
      log(LOG_ERROR, false,"Invalid LED number: %d\n", led);
      return false;
    }
    status.LEDcolour[led] = colour;
    return true;
}

void handleLEDManager() {
    unsigned long currentMillis = millis();
    uint32_t statusLEDcolour = STATUS_WARNING;

    // Update all normal LEDs
    for (int i = 0; i < 3; i++) {
        leds.setPixelColor(i, status.LEDcolour[i]);
    }
    
    // Check status items to determine main status LED colour
    if (!status.IPCOK || !status.RTCOK) {
        statusLEDcolour = LED_STATUS_ERROR;
    } else if (!status.psuOK || !status.V20OK || !status.V5OK || !status.sdCardOK) {
        statusLEDcolour = LED_STATUS_WARNING;
    } else {
        statusLEDcolour = LED_STATUS_OK;
    }
    
    // Handle blinking for system status LED (every 500ms)
    if (currentMillis - lastLEDBlinkTime >= 500) {
        blinkState = !blinkState;
        lastLEDBlinkTime = currentMillis;
        
        if (blinkState) {
            leds.setPixelColor(LED_SYSTEM_STATUS, statusLEDcolour);
        } else {
            leds.setPixelColor(LED_SYSTEM_STATUS, LED_COLOR_OFF);
        }
    }
    
    // Update LEDs
    leds.show();
}