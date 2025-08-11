/* Description: Holds the global status struct and LED manager functions
 * Call manageStatus() in the main loop frequently to keep the LEDs updated
 * Use the status struct to update the status of the system from other functions,
 * ensure that the status struct is only accessed after checking the statusLocked flag.
 * Set statusLocked to true before updating the status struct and set it to false after updating.
 * Set status.updated to true after updating the status struct if LED colours need to change.
 */

#pragma once

#include "../sys_init.h"

// LED colours
#define LED_COLOR_GREEN 0x00FF00
#define LED_COLOR_YELLOW 0xFFFF00
#define LED_COLOR_RED 0xFF0000
#define LED_COLOR_BLUE 0x0000FF
#define LED_COLOR_WHITE 0xFFFFFF
#define LED_COLOR_OFF 0x000000
#define LED_COLOR_PURPLE 0x8800FF
#define LED_COLOR_CYAN 0x00FFFF
#define LED_COLOR_ORANGE 0xFFA500
#define LED_COLOR_PINK 0xFFC0CB
#define LED_COLOR_MAGENTA 0xFF00FF

// LED indexes
#define LED_MQTT_STATUS 0
#define LED_WEBSERVER_STATUS 1
#define LED_MODBUS_STATUS 2
#define LED_SYSTEM_STATUS 3

// LED status numbers
#define STATUS_STARTUP 0
#define STATUS_OK 1
#define STATUS_ERROR 2
#define STATUS_WARNING 3
#define STATUS_BUSY 4

// LED status colors
#define LED_STATUS_STARTUP LED_COLOR_ORANGE
#define LED_STATUS_OK LED_COLOR_GREEN
#define LED_STATUS_ERROR LED_COLOR_RED
#define LED_STATUS_WARNING LED_COLOR_YELLOW
#define LED_STATUS_BUSY LED_COLOR_BLUE
#define LED_STATUS_OFF LED_COLOR_OFF

#define LED_UPDATE_PERIOD 100
#define LED_BLINK_PERIOD 500

void init_statusManager(void);
void manageStatus(void);

/**
 * @brief Holds the global status of the system, including sensors and controls.
 */
struct StatusVariables
{
    bool updated;
    // System status variables
    uint32_t ledPulseTS;
    uint32_t LEDcolour[4]; // 0 = MQTT, 1 = Webserver, 2 = Modbus, 3 = System
    bool psuOK;
    bool V20OK;
    bool V5OK;
    bool sdCardOK;
    bool ipcOK;
    bool rtcOK;

    // Modbus status variables
    bool modbusConnected;
    bool modbusBusy;

    // Webserver status variables
    bool webserverUp;
    bool webserverBusy;

    // MQTT status variables
    bool mqttConnected;
    bool mqttBusy;

    // Power supply voltage variables
    float Vpsu;
    float V20;
    float V5;

    /** Sensor readings (from I/O Controller) */
    PowerSensor powerSensor;
    TemperatureSensor temperatureSensor;
    PHSensor phSensor;
    DissolvedOxygenSensor doSensor;
    OpticalDensitySensor odSensor;
    GasFlowSensor gasFlowSensor;
    PressureSensor pressureSensor;
    StirrerSpeedSensor stirrerSpeedSensor;
    WeightSensor weightSensor;

    /** Control setpoints */
    TemperatureControl temperatureControl;
    PHControl phControl;
    DissolvedOxygenControl doControl;
    GasFlowControl gasFlowControl;
    StirrerSpeedControl stirrerSpeedControl;
    PumpSpeedControl pumpSpeedControl;
    FeedControl feedControl;
    WasteControl wasteControl;
};

// Object definition
extern Adafruit_NeoPixel leds;

// Status variables
extern StatusVariables status;
extern bool statusLocked;