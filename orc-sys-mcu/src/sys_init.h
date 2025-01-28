// Include libraries
#include <Arduino.h>
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "Adafruit_Neopixel.h"
#include "MCP79410.h"
#include <W5500lwIP.h>
#include <WebServer.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <EEPROM.h>
#include <NTPClient.h>

// Hardware pin definitions

#define PIN_ETH_MISO 0
#define PIN_ETH_CS 1
#define PIN_ETH_SCK 2
#define PIN_ETH_MOSI 3
#define PIN_ETH_RST 4
#define PIN_ETH_IRQ 5
#define PIN_RTC_SDA 6
#define PIN_RTC_SCL 7
#define PIN_I2C_EXT_SDA 8
#define PIN_I2C_EXT_SCL 9
#define PIN_SD_SCK 10
#define PIN_SD_MOSI 11
#define PIN_SD_MISO 12
#define PIN_SD_D1 13
#define PIN_SD_D2 14
#define PIN_SD_CS 15
#define PIN_SI_TX 16
#define PIN_SI_RX 17
#define PIN_SD_CD 18
#define PIN_LED_DAT 19
#define PIN_SP_IO_0 20
#define PIN_SP_IO_1 21
#define PIN_SP_IO_2 22
#define PIN_SP_IO_3 23
#define PIN_SP_IO_4 24
#define PIN_SP_IO_5 25
#define PIN_PS_24V_FB 26
#define PIN_PS_20V_FB 27
#define PIN_PS_5V_FB 28
#define PIN_SP_IO_6 29

// Voltage divider constants
#define V_PSU_MUL_V      0.01726436769
#define V_5V_MUL_V       0.00240673828

// Limits
#define V_PSU_MIN        22.0
#define V_PSU_MAX        29.0
#define V_20V_MIN        19.5
#define V_20V_MAX        20.5
#define V_5V_MIN         4.5
#define V_5V_MAX         5.5

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

// EEPROM addresses
#define EE_MAGIC_NUMBER 0x55
#define EE_NETWORK_CONFIG_ADDRESS 1

// Timing defines
#define NTP_MIN_SYNC_INTERVAL 70000
#define NTP_UPDATE_INTERVAL 600000  // 10 minutes - 1 day = 86400000ms

// Buffer sizes
#define DEBUG_PRINTF_BUFFER_SIZE 256

// Log entry types
#define LOG_INFO 0
#define LOG_WARNING 1
#define LOG_ERROR 2
#define LOG_DEBUG 3

// Object definitions
Adafruit_NeoPixel leds(4, PIN_LED_DAT, NEO_GRB + NEO_KHZ800);
MCP79410 rtc(Wire1);
Wiznet5500lwIP eth(PIN_ETH_CS, SPI, PIN_ETH_IRQ);
WebServer server(80);

// FreeRTOS defines

// Global variables

// Network configuration structure
struct NetworkConfig
{
    IPAddress ip;
    IPAddress subnet;
    IPAddress gateway;
    IPAddress dns;
    bool useDHCP;
    char hostname[32];
    char ntpServer[64];
    bool ntpEnabled;
    char timezone[8]; // Format: "+13:00"
    bool dstEnabled;  // Daylight Saving Time enabled
    // MQTT Configuration
    char mqttBroker[64];
    uint16_t mqttPort;
    char mqttUsername[32];
    char mqttPassword[32];
};

// Status variables structure
struct StatusVariables
{
    uint32_t LEDcolour[4]; // 0 = MQTT, 1 = Webserver, 2 = Modbus, 3 = System
    float Vpsu;
    float V20;
    float V5;
    bool psuOK;
    bool V20OK;
    bool V5OK;
};

// Status variables
StatusVariables status;
SemaphoreHandle_t statusMutex = NULL;

// Serial port mutex
SemaphoreHandle_t serialMutex = NULL;

// Global DateTime protection
SemaphoreHandle_t dateTimeMutex = NULL;
DateTime globalDateTime;

// NTP update queue
QueueHandle_t ntpUpdateQueue;
uint32_t ntpUpdateTimestamp = 0 - NTP_MIN_SYNC_INTERVAL;

// Global variables
NetworkConfig networkConfig;

// Device MAC address (stored as string)
char deviceMacAddress[18];

bool ethernetConnected = false;
bool serialReady = false;
bool core0setupComplete = false, core1setupComplete = false;

// Log entry types
const char *logType[] = {"INFO", "WARNING", "ERROR", "DEBUG"};