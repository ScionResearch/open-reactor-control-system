#pragma once

#include "../sys_init.h"

// LittleFS configuration
#define CONFIG_FILENAME "/network_config.json"
#define CONFIG_MAGIC_NUMBER 0x55

// Timing defines
#define NTP_MIN_SYNC_INTERVAL 70000   // Too frequent NTP requests will cause failed connections - 70s minimum
#define NTP_UPDATE_INTERVAL 86400000  // 1 day = 86400000ms

// NTP status defines
#define NTP_STATUS_CURRENT 0
#define NTP_STATUS_STALE 1
#define NTP_STATUS_FAILED 2

void init_network(void);
void setupEthernet(void);
bool loadNetworkConfig(void);
void saveNetworkConfig(void);
bool applyNetworkConfig(void);

void setupNetworkAPI(void);
void setupWebServer(void);
void setupMqttAPI(void);
void setupTimeAPI(void);

void manageEthernet(void);

void handleWebServer(void);
void handleRoot(void);
void handleFile(const char *path);
void handleNTPUpdates(bool forceUpdate);
void ntpUpdate(void);

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

void debugPrintNetConfig(NetworkConfig config);

// Global variables
extern NetworkConfig networkConfig;

extern Wiznet5500lwIP eth;
extern WebServer server;

// NTP update queue
extern QueueHandle_t ntpUpdateQueue;
extern uint32_t ntpUpdateTimestamp;
extern uint32_t lastNTPUpdateTime; // Last successful NTP update time

extern bool ethernetConnected;