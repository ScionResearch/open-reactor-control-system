/**
 * @file networkManager.cpp
 * @brief Network management implementation - Ethernet, NTP, and network configuration
 */

#include "networkManager.h"
#include "../config/ioConfig.h"
#include "../utils/logger.h"
#include "../utils/timeManager.h"
#include "../utils/statusManager.h"

// Forward declaration for web server setup (defined in webAPI/webServer.cpp)
extern void setupWebServer(void);

// =============================================================================
// Global Variables
// =============================================================================

NetworkConfig networkConfig;

Wiznet5500lwIP eth(PIN_ETH_CS, SPI, PIN_ETH_IRQ);
WebServer server(80);

// NTP update tracking
bool ntpUpdateRequested = false;
uint32_t ntpUpdateTimestamp = 0 - NTP_MIN_SYNC_INTERVAL;
uint32_t lastNTPUpdateTime = 0;

// Device MAC address (stored as string)
char deviceMacAddress[18];

bool ethernetConnected = false;
unsigned long lastNetworkCheckTime = 0;

// =============================================================================
// Network Initialization
// =============================================================================

void init_network() {
    setupEthernet();
    setupWebServer();
}

void manageNetwork(void) {
    manageEthernet();
    if (networkConfig.ntpEnabled) {
        handleNTPUpdates(false);
    }
}

// =============================================================================
// Ethernet Setup and Management
// =============================================================================

void setupEthernet()
{
    // Load network configuration
    if (!loadNetworkConfig())
    {
        // Set default configuration if load fails
        log(LOG_INFO, false, "Invalid network configuration, using defaults\n");
        networkConfig.ntpEnabled = false;
        networkConfig.useDHCP = true;
        networkConfig.ip = IPAddress(192, 168, 1, 100);
        networkConfig.subnet = IPAddress(255, 255, 255, 0);
        networkConfig.gateway = IPAddress(192, 168, 1, 1);
        networkConfig.dns = IPAddress(8, 8, 8, 8);
        strcpy(networkConfig.timezone, "+13:00");
        strcpy(networkConfig.hostname, "open-reactor");
        strcpy(networkConfig.ntpServer, "pool.ntp.org");
        networkConfig.dstEnabled = false;
        saveNetworkConfig();
    }
    
    // Load IO configuration (Core 0 only accesses LittleFS)
    // Note: loadIOConfig() now initializes defaults first, then overwrites with file data
    if (!loadIOConfig())
    {
        // File doesn't exist or couldn't be parsed - save the defaults that were initialized
        log(LOG_INFO, false, "IO config not found or invalid, saving defaults\n");
        saveIOConfig();
    }
    
    // Print loaded IO configuration for verification
    printIOConfig();

    SPI.setMOSI(PIN_ETH_MOSI);
    SPI.setMISO(PIN_ETH_MISO);
    SPI.setSCK(PIN_ETH_SCK);
    SPI.setCS(PIN_ETH_CS);

    eth.setSPISpeed(30000000);

    eth.hostname(networkConfig.hostname);

    // Apply network configuration
    if (!applyNetworkConfig())
    {
        log(LOG_WARNING, false, "Failed to apply network configuration\n");
    }
    else {
        // Get and store MAC address
        uint8_t mac[6];
        eth.macAddress(mac);
        snprintf(deviceMacAddress, sizeof(deviceMacAddress), "%02X:%02X:%02X:%02X:%02X:%02X",
                 mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
        log(LOG_INFO, false, "MAC Address: %s\n", deviceMacAddress);
    }

    // Wait for Ethernet to connect
    uint32_t startTime = millis();
    uint32_t timeout = 10000;
    while (eth.linkStatus() == LinkOFF) {
        if (millis() - startTime > timeout) {
            break;
        }
    }

    if (eth.linkStatus() == LinkOFF) {
        log(LOG_WARNING, false, "Ethernet not connected\n");
        ethernetConnected = false;
    }
    else {
        log(LOG_INFO, false, "Ethernet connected, IP address: %s, Gateway: %s\n",
                    eth.localIP().toString().c_str(),
                    eth.gatewayIP().toString().c_str());
        ethernetConnected = true;
    }
}

bool applyNetworkConfig()
{
    if (networkConfig.useDHCP)
    {
        // Call eth.end() to release the DHCP lease if we already had one since last boot
        // NOTE: requires modification of end function in LwipIntDev.h, added dhcp_release_and_stop(&_netif); before netif_remove(&_netif);)
        eth.end();
        
        if (!eth.begin())
        {
            log(LOG_WARNING, true, "Failed to configure Ethernet using DHCP, falling back to 192.168.1.10\n");
            IPAddress defaultIP = {192, 168, 1, 10};
            eth.config(defaultIP);
            if (!eth.begin()) return false;
        }
    }
    else
    {
        eth.config(networkConfig.ip, networkConfig.gateway, networkConfig.subnet, networkConfig.dns);
        if (!eth.begin()) return false;
    }
    return true;
}

void manageEthernet(void)
{
    // Do network tasks if ethernet is connected
    if (ethernetConnected) {
        if (eth.linkStatus() == LinkOFF) {
            ethernetConnected = false;
            // Set Webserver Status
            if (!statusLocked) {
                statusLocked = true;
                status.webserverUp = false;
                status.webserverBusy = false;
                status.mqttConnected = false;
                status.mqttBusy = false;
                status.updated = true;
                statusLocked = false;
            }
            log(LOG_INFO, true, "Ethernet disconnected, waiting for reconnect\n");
        } else {
            // Ethernet is still connected - web server handled elsewhere
            server.handleClient();
            if (!statusLocked) {
                statusLocked = true;
                status.webserverBusy = false;
                status.webserverUp = true;
                status.updated = true;
                statusLocked = false;
            }
        }
    }
    else if (eth.linkStatus() == LinkON) {
        ethernetConnected = true;
        if(!applyNetworkConfig()) {
            log(LOG_ERROR, true, "Failed to apply network configuration!\n");
        }
        else {
            log(LOG_INFO, true, "Ethernet re-connected, IP address: %s, Gateway: %s\n",
                        eth.localIP().toString().c_str(),
                        eth.gatewayIP().toString().c_str());
        }
    }
}

// =============================================================================
// Configuration Persistence
// =============================================================================

bool loadNetworkConfig()
{
    log(LOG_INFO, true, "Loading network configuration:\n");
    
    // Check if LittleFS is mounted
    if (!LittleFS.begin()) {
        log(LOG_WARNING, true, "Failed to mount LittleFS\n");
        return false;
    }

    // Check if config file exists
    if (!LittleFS.exists(CONFIG_FILENAME)) {
        log(LOG_WARNING, true, "Config file not found\n");
        LittleFS.end();
        return false;
    }

    // Open config file
    File configFile = LittleFS.open(CONFIG_FILENAME, "r");
    if (!configFile) {
        log(LOG_WARNING, true, "Failed to open config file\n");
        LittleFS.end();
        return false;
    }

    // Allocate a buffer to store contents of the file (increased for recording config)
    StaticJsonDocument<1536> doc;
    DeserializationError error = deserializeJson(doc, configFile);
    configFile.close();

    if (error) {
        log(LOG_WARNING, true, "Failed to parse config file: %s\n", error.c_str());
        LittleFS.end();
        return false;
    } else {
        log(LOG_INFO, false, "Deserialized network config file: %d bytes\n", doc.memoryUsage());
    }

    // Check magic number
    uint8_t magicNumber = doc["magic_number"] | 0;
    log(LOG_INFO, true, "Magic number: %x\n", magicNumber);
    if (magicNumber != CONFIG_MAGIC_NUMBER) {
        log(LOG_WARNING, true, "Invalid magic number\n");
        LittleFS.end();
        return false;
    }

    // Parse network configuration
    networkConfig.useDHCP = doc["use_dhcp"] | true;
    
    // Parse IP addresses
    IPAddress ip, subnet, gateway, dns;
    if (ip.fromString(doc["ip"] | "192.168.1.100")) networkConfig.ip = ip;
    if (subnet.fromString(doc["subnet"] | "255.255.255.0")) networkConfig.subnet = subnet;
    if (gateway.fromString(doc["gateway"] | "192.168.1.1")) networkConfig.gateway = gateway;
    if (dns.fromString(doc["dns"] | "8.8.8.8")) networkConfig.dns = dns;
    
    // Parse strings
    strlcpy(networkConfig.hostname, doc["hostname"] | "open-reactor", sizeof(networkConfig.hostname));
    strlcpy(networkConfig.ntpServer, doc["ntp_server"] | "pool.ntp.org", sizeof(networkConfig.ntpServer));
    strlcpy(networkConfig.timezone, doc["timezone"] | "+13:00", sizeof(networkConfig.timezone));
    
    // Parse booleans
    networkConfig.ntpEnabled = doc["ntp_enabled"] | false;
    networkConfig.dstEnabled = doc["dst_enabled"] | false;
    
    // Parse MQTT configuration
    strlcpy(networkConfig.mqttBroker, doc["mqtt_broker"] | "", sizeof(networkConfig.mqttBroker));
    networkConfig.mqttPort = doc["mqtt_port"] | 1883;
    strlcpy(networkConfig.mqttUsername, doc["mqtt_username"] | "", sizeof(networkConfig.mqttUsername));
    strlcpy(networkConfig.mqttPassword, doc["mqtt_password"] | "", sizeof(networkConfig.mqttPassword));
    // Optional fields
    strlcpy(networkConfig.mqttDevicePrefix, doc["mqtt_device_prefix"] | "", sizeof(networkConfig.mqttDevicePrefix));
    networkConfig.mqttPublishIntervalMs = doc["mqtt_publish_interval_ms"] | 10000;

    // Parse recording configuration
    JsonObject recording = doc["recording"];
    if (!recording.isNull()) {
        recordingConfig.enabled = recording["enabled"] | false;
        
        JsonObject inputs = recording["inputs"];
        recordingConfig.inputs.enabled = inputs["enabled"] | false;
        recordingConfig.inputs.interval = inputs["interval"] | 60;
        if (recordingConfig.inputs.interval < RECORDING_MIN_INTERVAL) 
            recordingConfig.inputs.interval = RECORDING_MIN_INTERVAL;
        
        JsonObject outputs = recording["outputs"];
        recordingConfig.outputs.enabled = outputs["enabled"] | false;
        recordingConfig.outputs.interval = outputs["interval"] | 60;
        if (recordingConfig.outputs.interval < RECORDING_MIN_INTERVAL) 
            recordingConfig.outputs.interval = RECORDING_MIN_INTERVAL;
        
        JsonObject motors = recording["motors"];
        recordingConfig.motors.enabled = motors["enabled"] | false;
        recordingConfig.motors.interval = motors["interval"] | 60;
        if (recordingConfig.motors.interval < RECORDING_MIN_INTERVAL) 
            recordingConfig.motors.interval = RECORDING_MIN_INTERVAL;
        
        JsonObject sensors = recording["sensors"];
        recordingConfig.sensors.enabled = sensors["enabled"] | false;
        recordingConfig.sensors.interval = sensors["interval"] | 60;
        if (recordingConfig.sensors.interval < RECORDING_MIN_INTERVAL) 
            recordingConfig.sensors.interval = RECORDING_MIN_INTERVAL;
        
        JsonObject energy = recording["energy"];
        recordingConfig.energy.enabled = energy["enabled"] | false;
        recordingConfig.energy.interval = energy["interval"] | 60;
        if (recordingConfig.energy.interval < RECORDING_MIN_INTERVAL) 
            recordingConfig.energy.interval = RECORDING_MIN_INTERVAL;
        
        JsonObject controllers = recording["controllers"];
        recordingConfig.controllers.enabled = controllers["enabled"] | false;
        recordingConfig.controllers.interval = controllers["interval"] | 60;
        if (recordingConfig.controllers.interval < RECORDING_MIN_INTERVAL) 
            recordingConfig.controllers.interval = RECORDING_MIN_INTERVAL;
        
        JsonObject devices = recording["devices"];
        recordingConfig.devices.enabled = devices["enabled"] | false;
        recordingConfig.devices.interval = devices["interval"] | 60;
        if (recordingConfig.devices.interval < RECORDING_MIN_INTERVAL) 
            recordingConfig.devices.interval = RECORDING_MIN_INTERVAL;
        
        log(LOG_INFO, false, "Loaded recording config: master=%s\n", 
            recordingConfig.enabled ? "enabled" : "disabled");
    }

    LittleFS.end();
    return true;
}

void saveNetworkConfig()
{
    log(LOG_INFO, true, "Saving network configuration:\n");
    printNetConfig(networkConfig);
    
    // Check if LittleFS is mounted
    if (!LittleFS.begin()) {
        log(LOG_WARNING, true, "Failed to mount LittleFS\n");
        return;
    }

    // Create JSON document on heap to avoid stack overflow
    DynamicJsonDocument doc(1536);
    
    // Store magic number
    doc["magic_number"] = CONFIG_MAGIC_NUMBER;
    
    // Store network configuration
    doc["use_dhcp"] = networkConfig.useDHCP;
    
    // Store IP addresses as strings
    doc["ip"] = networkConfig.ip.toString();
    doc["subnet"] = networkConfig.subnet.toString();
    doc["gateway"] = networkConfig.gateway.toString();
    doc["dns"] = networkConfig.dns.toString();
    
    // Store strings
    doc["hostname"] = networkConfig.hostname;
    doc["ntp_server"] = networkConfig.ntpServer;
    doc["timezone"] = networkConfig.timezone;
    
    // Store booleans
    doc["ntp_enabled"] = networkConfig.ntpEnabled;
    doc["dst_enabled"] = networkConfig.dstEnabled;
    
    // Store MQTT configuration
    doc["mqtt_broker"] = networkConfig.mqttBroker;
    doc["mqtt_port"] = networkConfig.mqttPort;
    doc["mqtt_username"] = networkConfig.mqttUsername;
    doc["mqtt_password"] = networkConfig.mqttPassword;
    // Optional fields
    doc["mqtt_device_prefix"] = networkConfig.mqttDevicePrefix;
    doc["mqtt_publish_interval_ms"] = networkConfig.mqttPublishIntervalMs;
    
    // Store recording configuration
    JsonObject recording = doc.createNestedObject("recording");
    recording["enabled"] = recordingConfig.enabled;
    
    JsonObject inputs = recording.createNestedObject("inputs");
    inputs["enabled"] = recordingConfig.inputs.enabled;
    inputs["interval"] = recordingConfig.inputs.interval;
    
    JsonObject outputs = recording.createNestedObject("outputs");
    outputs["enabled"] = recordingConfig.outputs.enabled;
    outputs["interval"] = recordingConfig.outputs.interval;
    
    JsonObject motors = recording.createNestedObject("motors");
    motors["enabled"] = recordingConfig.motors.enabled;
    motors["interval"] = recordingConfig.motors.interval;
    
    JsonObject sensors = recording.createNestedObject("sensors");
    sensors["enabled"] = recordingConfig.sensors.enabled;
    sensors["interval"] = recordingConfig.sensors.interval;
    
    JsonObject energy = recording.createNestedObject("energy");
    energy["enabled"] = recordingConfig.energy.enabled;
    energy["interval"] = recordingConfig.energy.interval;
    
    JsonObject controllers = recording.createNestedObject("controllers");
    controllers["enabled"] = recordingConfig.controllers.enabled;
    controllers["interval"] = recordingConfig.controllers.interval;
    
    JsonObject devices = recording.createNestedObject("devices");
    devices["enabled"] = recordingConfig.devices.enabled;
    devices["interval"] = recordingConfig.devices.interval;
    
    // Open file for writing
    File configFile = LittleFS.open(CONFIG_FILENAME, "w");
    if (!configFile) {
        log(LOG_WARNING, true, "Failed to open config file for writing\n");
        LittleFS.end();
        return;
    }
    
    // Write to file
    if (serializeJson(doc, configFile) == 0) {
        log(LOG_WARNING, true, "Failed to write config file\n");
    }
    
    // Close file
    configFile.close();
    // Don't end LittleFS here as it will prevent serving web files
    // LittleFS.end();
}

// =============================================================================
// NTP Management
// =============================================================================

void ntpUpdate(void) {
    static WiFiUDP udp;
    static NTPClient timeClient(udp, networkConfig.ntpServer);
    static bool clientInitialized = false;

    if (!clientInitialized)
    {
        timeClient.begin();
        clientInitialized = true;
    }

    if (!eth.linkStatus()) return;

    if (!timeClient.update()) {
        log(LOG_WARNING, true, "Failed to get time from NTP server, retrying\n");
        bool updateSuccessful = false;
        for (int i = 0; i < 3; i++) {
            if (timeClient.update()) {
                updateSuccessful = true;
                break;
            }
            delay(10);
        }
        if (!updateSuccessful) {
            log(LOG_ERROR, true, "Failed to get time from NTP server, giving up\n");
            return;
        }
    }
    
    // Get NTP time
    time_t epochTime = timeClient.getEpochTime();

    // Apply timezone offset
    int tzHours = 0, tzMinutes = 0, tzDSToffset = 0;
    if (networkConfig.dstEnabled) {
        tzDSToffset = 3600;
    }
    sscanf(networkConfig.timezone, "%d:%d", &tzHours, &tzMinutes);
    epochTime += (tzHours * 3600 + tzMinutes * 60 + tzDSToffset);

    // Convert to DateTime and update using thread-safe function
    DateTime newTime = epochToDateTime(epochTime);
    if (!updateGlobalDateTime(newTime))
    {
        log(LOG_ERROR, true, "Failed to update time from NTP\n");
    }
    else
    {
        log(LOG_INFO, true, "Time updated from NTP server\n");
        lastNTPUpdateTime = millis(); // Record the time of successful update
    }
}

void handleNTPUpdates(bool forceUpdate) {
    if (!networkConfig.ntpEnabled) return;
    uint32_t timeSinceLastUpdate = millis() - ntpUpdateTimestamp;

    // Check if there's an NTP update request or if it's time for a scheduled update
    if (ntpUpdateRequested || timeSinceLastUpdate > NTP_UPDATE_INTERVAL || forceUpdate)
    {
        if (timeSinceLastUpdate < NTP_MIN_SYNC_INTERVAL) {
            log(LOG_INFO, true, "Time since last NTP update: %ds - skipping\n", timeSinceLastUpdate/1000);
            return;
        }
        ntpUpdate();
        ntpUpdateTimestamp = millis();
        ntpUpdateRequested = false; // Clear the request flag
    }
}

// =============================================================================
// Debug Functions
// =============================================================================

void printNetConfig(NetworkConfig config)
{
    log(LOG_INFO, true, "Mode: %s\n", config.useDHCP ? "DHCP" : "Static");
    if (config.useDHCP) {
        log(LOG_INFO, true, "IP: %s\n", eth.localIP().toString().c_str());
        log(LOG_INFO, true, "Subnet: %s\n", eth.subnetMask().toString().c_str());
        log(LOG_INFO, true, "Gateway: %s\n", eth.gatewayIP().toString().c_str());
        log(LOG_INFO, true, "DNS: %s\n", eth.dnsIP().toString().c_str());
    } else {
        log(LOG_INFO, true, "IP: %s\n", config.ip.toString().c_str());
        log(LOG_INFO, true, "Subnet: %s\n", config.subnet.toString().c_str());
        log(LOG_INFO, true, "Gateway: %s\n", config.gateway.toString().c_str());
        log(LOG_INFO, true, "DNS: %s\n", config.dns.toString().c_str());
    }
    log(LOG_INFO, true, "Timezone: %s\n", config.timezone);
    log(LOG_INFO, true, "Hostname: %s\n", config.hostname);
    log(LOG_INFO, true, "NTP Server: %s\n", config.ntpServer);
    log(LOG_INFO, true, "NTP Enabled: %s\n", config.ntpEnabled ? "true" : "false");
    log(LOG_INFO, true, "DST Enabled: %s\n", config.dstEnabled ? "true" : "false");
}
