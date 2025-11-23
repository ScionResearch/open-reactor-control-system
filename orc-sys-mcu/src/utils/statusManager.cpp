#include "statusManager.h"
#include "objectCache.h"
#include "config/ioConfig.h"

// Object definition
Adafruit_NeoPixel leds(4, PIN_LED_DAT, NEO_GRB + NEO_KHZ800);

// Status variables
StatusVariables status;
bool statusLocked = false;
static bool blinkState = false;
static uint32_t ledTS = 0;

void init_statusManager() {
  leds.begin();
  leds.setBrightness(50);
  leds.fill(LED_COLOR_OFF, 0, 4);
  leds.setPixelColor(LED_SYSTEM_STATUS, LED_STATUS_STARTUP);
  leds.show();
  status.ledPulseTS = millis();
  
  // Initialize modbus status flags
  status.modbusConfigured = false;
  status.modbusConnected = false;
  status.modbusFault = false;
  
  log(LOG_INFO, false, "Status manager started\n");
  ledTS = millis();
}

/**
 * @brief Aggregate device status from object cache
 * Updates modbusConfigured, modbusConnected, and modbusFault flags
 */
void updateDeviceStatus(void) {
  // Check if any devices are configured
  uint8_t deviceCount = getActiveDeviceControlCount();
  status.modbusConfigured = (deviceCount > 0);
  
  if (!status.modbusConfigured) {
    // No devices configured
    status.modbusConnected = false;
    status.modbusFault = false;
    return;
  }
  
  // Scan device control objects (indices 50-69)
  uint8_t connectedCount = 0;
  uint8_t faultCount = 0;
  uint8_t validDeviceCount = 0;
  
  for (uint8_t i = 50; i < 70; i++) {
    ObjectCache::CachedObject* obj = objectCache.getObject(i);
    if (obj && obj->valid && obj->lastUpdate > 0) {
      validDeviceCount++;
      
      // Check connected flag (bit 1 of flags)
      bool connected = (obj->flags & IPC_SENSOR_FLAG_CONNECTED) ? true : false;
      if (connected) {
        connectedCount++;
      }
      
      // Check fault flag (bit 0 of flags)
      bool fault = (obj->flags & IPC_SENSOR_FLAG_FAULT) ? true : false;
      if (fault) {
        faultCount++;
      }
    }
  }
  
  // Update status flags
  status.modbusFault = (faultCount > 0);
  status.modbusConnected = (validDeviceCount > 0 && connectedCount == validDeviceCount && faultCount == 0);
}

void manageStatus(void)
{
  if (millis() - ledTS < LED_UPDATE_PERIOD) return;
  if (statusLocked) return;
  statusLocked = true;
  
  // Update device status from object cache (always check, not gated by status.updated)
  updateDeviceStatus();
  
  // Update Modbus LED based on device status (always update)
  uint32_t newModbusColor;
  if (!status.modbusConfigured) {
    // No devices configured - LED off
    newModbusColor = LED_STATUS_OFF;
  } else if (status.modbusFault) {
    // Any device in fault state - LED red
    newModbusColor = LED_STATUS_ERROR;
  } else if (status.modbusConnected) {
    // All devices connected - LED green
    newModbusColor = LED_STATUS_OK;
  } else {
    // Devices configured but not all connected - LED yellow
    newModbusColor = LED_STATUS_WARNING;
  }
  
  // Only update LED if color changed to avoid unnecessary SPI writes
  if (status.LEDcolour[LED_MODBUS_STATUS] != newModbusColor) {
    status.LEDcolour[LED_MODBUS_STATUS] = newModbusColor;
    leds.setPixelColor(LED_MODBUS_STATUS, status.LEDcolour[LED_MODBUS_STATUS]);
    leds.show();
  }
  
  // Check for status change and update other LED colours accordingly
  if (status.updated) {
    // System status LED colours
    if (!status.ipcOK || !status.rtcOK) {
      status.LEDcolour[LED_SYSTEM_STATUS] = LED_STATUS_ERROR;
    } else if (!status.psuOK || !status.V20OK || !status.V5OK || !status.sdCardOK) {
      status.LEDcolour[LED_SYSTEM_STATUS] = LED_STATUS_WARNING;
    } else {
      status.LEDcolour[LED_SYSTEM_STATUS] = LED_STATUS_OK;
    }
    // Webserver status LED colours
    if (status.webserverBusy) {
      status.LEDcolour[LED_WEBSERVER_STATUS] = LED_STATUS_BUSY;
    } else if (status.webserverUp) {
      status.LEDcolour[LED_WEBSERVER_STATUS] = LED_STATUS_OK;
    } else {
      status.LEDcolour[LED_WEBSERVER_STATUS] = LED_STATUS_OFF;
    }
    // MQTT status LED colours
    if (status.mqttConnected) {
      status.LEDcolour[LED_MQTT_STATUS] = LED_STATUS_OK;
    } else if (status.mqttBusy) {
      status.LEDcolour[LED_MQTT_STATUS] = LED_STATUS_BUSY;
    } else {
      status.LEDcolour[LED_MQTT_STATUS] = LED_STATUS_OFF;
    }
    leds.setPixelColor(LED_WEBSERVER_STATUS, status.LEDcolour[LED_WEBSERVER_STATUS]);
    leds.setPixelColor(LED_MQTT_STATUS, status.LEDcolour[LED_MQTT_STATUS]);
    // Reset the update flag
    status.updated = false;
  }  
  // Status LED blink updater  
  if (millis() - status.ledPulseTS >= LED_BLINK_PERIOD) {
    blinkState = !blinkState;
    status.ledPulseTS += LED_BLINK_PERIOD;
    if (blinkState) {
      leds.setPixelColor(LED_SYSTEM_STATUS, status.LEDcolour[LED_SYSTEM_STATUS]);
    } else {
      leds.setPixelColor(LED_SYSTEM_STATUS, LED_COLOR_OFF);
    }
    leds.show();
  }
  statusLocked = false;
}