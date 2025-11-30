#pragma once

/**
 * @file networkManager.h
 * @brief Network management - Ethernet, NTP, and network configuration
 * 
 * This module handles:
 * - Ethernet initialization and management (W5500)
 * - Network configuration (DHCP/Static IP)
 * - NTP time synchronization
 * - Network config persistence (LittleFS)
 */

#include <Arduino.h>
#include <W5500lwIP.h>
#include <WebServer.h>
#include <NTPClient.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <WiFiUdp.h>

// Forward declarations
#include "../hardware/pins.h"

// =============================================================================
// Configuration Constants
// =============================================================================

// LittleFS configuration
#define CONFIG_FILENAME "/system_config.json"
#define CONFIG_MAGIC_NUMBER 0x55

// NTP timing defines
#define NTP_MIN_SYNC_INTERVAL 70000   // Too frequent NTP requests will cause failed connections - 70s minimum
#define NTP_UPDATE_INTERVAL 86400000  // 1 day = 86400000ms
//#define NTP_UPDATE_INTERVAL 100000  // 100 seconds (for testing!)

// NTP status defines
#define NTP_STATUS_CURRENT 0
#define NTP_STATUS_STALE 1
#define NTP_STATUS_FAILED 2

// Maximum file size for downloads (5MB to be safe)
#define MAX_DOWNLOAD_SIZE 5242880

// =============================================================================
// Network Configuration Structure
// =============================================================================

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
    bool mqttEnabled;
    char mqttBroker[64];
    uint16_t mqttPort;
    char mqttUsername[32];
    char mqttPassword[32];
    // Optional: device topic prefix override and publish interval (ms)
    // If empty, defaults to "orcs/dev/<MAC>"; if set, used as-is
    char mqttDevicePrefix[64];
    uint32_t mqttPublishIntervalMs;
};

// =============================================================================
// Function Declarations
// =============================================================================

// Initialization and main loop
void init_network(void);
void manageNetwork(void);

// Ethernet setup and management
void setupEthernet(void);
void manageEthernet(void);
bool applyNetworkConfig(void);

// Configuration persistence
bool loadNetworkConfig(void);
void saveNetworkConfig(void);

// NTP management
void handleNTPUpdates(bool forceUpdate);
void ntpUpdate(void);

// Debug/utility
void printNetConfig(NetworkConfig config);

// =============================================================================
// Global Variables (extern declarations)
// =============================================================================

extern NetworkConfig networkConfig;
extern Wiznet5500lwIP eth;
extern WebServer server;

// Device MAC address (stored as string)
extern char deviceMacAddress[18];

// NTP tracking
extern uint32_t ntpUpdateTimestamp;
extern uint32_t lastNTPUpdateTime;

// Connection state
extern bool ethernetConnected;
