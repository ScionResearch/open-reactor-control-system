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

// Buffer sizes
#define DEBUG_PRINTF_BUFFER_SIZE 256

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

// Serial port mutex
SemaphoreHandle_t serialMutex = NULL;

// Global DateTime protection
SemaphoreHandle_t dateTimeMutex = NULL;
DateTime globalDateTime;

// NTP update queue
QueueHandle_t ntpUpdateQueue;
uint32_t ntpUpdateTimestamp = 0;

// Global variables
NetworkConfig networkConfig;
uint8_t systemStatus[] = {STATUS_STARTUP, STATUS_STARTUP, STATUS_STARTUP, STATUS_STARTUP};
uint32_t statusColours[] = {
    LED_STATUS_STARTUP,
    LED_STATUS_OK,
    LED_STATUS_ERROR,
    LED_STATUS_WARNING,
    LED_STATUS_BUSY,
    LED_STATUS_OFF
};

// Device MAC address (stored as string)
char deviceMacAddress[18];

bool ethernetConnected = false;
bool serialReady = false;