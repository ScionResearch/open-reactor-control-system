#include "network.h"

// Global variables
NetworkConfig networkConfig;

Wiznet5500lwIP eth(PIN_ETH_CS, SPI, PIN_ETH_IRQ);
WebServer server(80);

// NTP update tracking
bool ntpUpdateRequested = false;
uint32_t ntpUpdateTimestamp = 0 - NTP_MIN_SYNC_INTERVAL;
uint32_t lastNTPUpdateTime = 0; // Last successful NTP update time

// Device MAC address (stored as string)
char deviceMacAddress[18];

bool ethernetConnected = false;
unsigned long lastNetworkCheckTime = 0;

// Network component initialisation functions ------------------------------>
void init_network() {
    setupEthernet();
    setupWebServer();
    setupNetworkAPI();
    setupMqttAPI();
    setupTimeAPI();
}

void manageNetwork(void) {
    manageEthernet();
}

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

  // Allocate a buffer to store contents of the file
  StaticJsonDocument<512> doc;
  DeserializationError error = deserializeJson(doc, configFile);
  configFile.close();

  if (error) {
    log(LOG_WARNING, true, "Failed to parse config file: %s\n", error.c_str());
    LittleFS.end();
    return false;
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

  LittleFS.end();
  //debugPrintNetConfig(networkConfig);
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

  // Create JSON document
  StaticJsonDocument<512> doc;
  
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

bool applyNetworkConfig()
{
  if (networkConfig.useDHCP)
  {
    // Call eth.end() to release the DHCP lease if we already had one since last boot (handles changing networks on the fly)
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

void setupNetworkAPI()
{
  server.on("/api/network", HTTP_GET, []()
            {
        StaticJsonDocument<512> doc;
        doc["mode"] = networkConfig.useDHCP ? "dhcp" : "static";
        
        // Get current IP configuration
        IPAddress ip = eth.localIP();
        IPAddress subnet = eth.subnetMask();
        IPAddress gateway = eth.gatewayIP();
        IPAddress dns = eth.dnsIP();
        
        char ipStr[16];
        snprintf(ipStr, sizeof(ipStr), "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
        doc["ip"] = ipStr;
        
        char subnetStr[16];
        snprintf(subnetStr, sizeof(subnetStr), "%d.%d.%d.%d", subnet[0], subnet[1], subnet[2], subnet[3]);
        doc["subnet"] = subnetStr;
        
        char gatewayStr[16];
        snprintf(gatewayStr, sizeof(gatewayStr), "%d.%d.%d.%d", gateway[0], gateway[1], gateway[2], gateway[3]);
        doc["gateway"] = gatewayStr;
        
        char dnsStr[16];
        snprintf(dnsStr, sizeof(dnsStr), "%d.%d.%d.%d", dns[0], dns[1], dns[2], dns[3]);
        doc["dns"] = dnsStr;

        doc["mac"] = deviceMacAddress;
        doc["hostname"] = networkConfig.hostname;
        doc["ntp"] = networkConfig.ntpServer;
        doc["dst"] = networkConfig.dstEnabled;
        
        String response;
        serializeJson(doc, response);
        server.send(200, "application/json", response); });

  server.on("/api/network", HTTP_POST, []()
            {
              if (!server.hasArg("plain"))
              {
                server.send(400, "application/json", "{\"error\":\"No data received\"}");
                return;
              }

              StaticJsonDocument<512> doc;
              DeserializationError error = deserializeJson(doc, server.arg("plain"));

              if (error)
              {
                server.send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
                return;
              }

              // Update network configuration
              networkConfig.useDHCP = doc["mode"] == "dhcp";

              if (!networkConfig.useDHCP)
              {
                // Validate and parse IP addresses
                if (!networkConfig.ip.fromString(doc["ip"] | ""))
                {
                  server.send(400, "application/json", "{\"error\":\"Invalid IP address\"}");
                  return;
                }
                if (!networkConfig.subnet.fromString(doc["subnet"] | ""))
                {
                  server.send(400, "application/json", "{\"error\":\"Invalid subnet mask\"}");
                  return;
                }
                if (!networkConfig.gateway.fromString(doc["gateway"] | ""))
                {
                  server.send(400, "application/json", "{\"error\":\"Invalid gateway\"}");
                  return;
                }
                if (!networkConfig.dns.fromString(doc["dns"] | ""))
                {
                  server.send(400, "application/json", "{\"error\":\"Invalid DNS server\"}");
                  return;
                }
              }

              // Update hostname
              strlcpy(networkConfig.hostname, doc["hostname"] | "open-reactor", sizeof(networkConfig.hostname));

              // Update NTP server
              strlcpy(networkConfig.ntpServer, doc["ntp"] | "pool.ntp.org", sizeof(networkConfig.ntpServer));

              // Update DST setting if provided
              if (doc.containsKey("dst")) {
                networkConfig.dstEnabled = doc["dst"];
              }

              // Save configuration to storage
              saveNetworkConfig();

              // Send success response before applying changes
              server.send(200, "application/json", "{\"status\":\"success\",\"message\":\"Configuration saved\"}");

              // Apply new configuration after a short delay
              delay(1000);
              rp2040.reboot();
            });
}

void setupWebServer()
{
  // Initialize LittleFS for serving web files
  if (!LittleFS.begin())
  {
    log(LOG_ERROR, true, "LittleFS Mount Failed\n");
    return;
  }

  // Route handlers
  server.on("/", HTTP_GET, handleRoot);
  server.on("/api/sensors", HTTP_GET, []()
            {
        DateTime dt;
        if (!getGlobalDateTime(dt)) {
            server.send(500, "application/json", "{\"error\":\"Failed to get time\"}");
            return;
        }

        char json[128];
        snprintf(json, sizeof(json),
                "{\"temp\":25.5,\"ph\":7.2,\"do\":6.8,\"timestamp\":\"%04d-%02d-%02dT%02d:%02d:%02d\"}",
                dt.year, dt.month, dt.day, dt.hour, dt.minute, dt.second);
        server.send(200, "application/json", json); });

  // Comprehensive system status endpoint
  server.on("/api/system/status", HTTP_GET, []() {
    StaticJsonDocument<768> doc;
    
    if (!statusLocked) {
      statusLocked = true;
      
      // Power supplies
      JsonObject power = doc.createNestedObject("power");
      power["mainVoltage"] = status.Vpsu;
      power["v20Voltage"] = status.V20;
      power["v5Voltage"] = status.V5;
      power["mainVoltageOK"] = status.psuOK;
      power["v20VoltageOK"] = status.V20OK;
      power["v5VoltageOK"] = status.V5OK;
      
      // RTC status
      JsonObject rtc = doc.createNestedObject("rtc");
      rtc["ok"] = status.rtcOK;
      
      // Get current time
      DateTime now;
      if (getGlobalDateTime(now, 10)) {
        char timeStr[32];
        snprintf(timeStr, sizeof(timeStr), "%04d-%02d-%02d %02d:%02d:%02d", 
                 now.year, now.month, now.day,
                 now.hour, now.minute, now.second);
        rtc["time"] = timeStr;
      } else {
        rtc["time"] = "Unknown";
      }
      
      // IPC status
      doc["ipc"] = status.ipcOK;
      
      // SD card info
      JsonObject sd = doc.createNestedObject("sd");
      if (!sdLocked) {
        sdLocked = true;
        sd["inserted"] = sdInfo.inserted;
        sd["ready"] = sdInfo.ready;
        
        // Only include these if SD card is ready
        if (sdInfo.ready) {
          sd["capacityGB"] = sdInfo.cardSizeBytes / 1000000000.0;
          sd["freeSpaceGB"] = sdInfo.cardFreeBytes / 1000000000.0;
          sd["logFileSizeKB"] = sdInfo.logSizeBytes / 1000.0;
          sd["sensorFileSizeKB"] = sdInfo.sensorSizeBytes / 1000.0;
        }
        sdLocked = false;
      }
      
      // Network connections
      doc["mqtt"] = status.mqttConnected;
      doc["modbus"] = status.modbusConnected;
      
      statusLocked = false;
      
      String response;
      serializeJson(doc, response);
      server.send(200, "application/json", response);
    }
    else {
      server.send(500, "application/json", "{\"error\":\"Status locked\"}");
    }
  });

  // System reboot endpoint
  server.on("/api/system/reboot", HTTP_POST, []() {
    // Send response first before rebooting
    server.send(200, "application/json", "{\"success\":true,\"message\":\"System is rebooting...\"}");
    
    // Add a small delay to ensure response is sent
    delay(500);
    
    // Log the reboot
    log(LOG_INFO, true, "System reboot requested via web interface\n");

    delay(1000);
    
    // Perform system reboot
    rp2040.reboot();
  });

  // Handle static files
  server.onNotFound([]()
                    { handleFile(server.uri().c_str()); });

  server.begin();
  log(LOG_INFO, true, "HTTP server started\n");
  
  // Set Webserver Status
  if (!statusLocked) {
    statusLocked = true;
    status.webserverUp = true;
    status.webserverBusy = false;
    status.updated = true;
    statusLocked = false;
  }
}

void setupMqttAPI()
{
    server.on("/api/mqtt", HTTP_GET, []() {
        StaticJsonDocument<512> doc;
        
        doc["mqttBroker"] = networkConfig.mqttBroker;
        doc["mqttPort"] = networkConfig.mqttPort;
        doc["mqttUsername"] = networkConfig.mqttUsername;
        // Don't send the password back for security
        doc["mqttPassword"] = "";
        
        String response;
        serializeJson(doc, response);
        server.send(200, "application/json", response);
    });

    server.on("/api/mqtt", HTTP_POST, []() {
        if (!server.hasArg("plain")) {
            server.send(400, "application/json", "{\"error\":\"No data received\"}");
            return;
        }

        StaticJsonDocument<512> doc;
        DeserializationError error = deserializeJson(doc, server.arg("plain"));

        if (error) {
            server.send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
            return;
        }

        // Update MQTT configuration
        strlcpy(networkConfig.mqttBroker, doc["mqttBroker"] | "", sizeof(networkConfig.mqttBroker));
        networkConfig.mqttPort = doc["mqttPort"] | 1883;
        strlcpy(networkConfig.mqttUsername, doc["mqttUsername"] | "", sizeof(networkConfig.mqttUsername));
        
        // Only update password if one is provided
        const char* newPassword = doc["mqttPassword"] | "";
        if (strlen(newPassword) > 0) {
            strlcpy(networkConfig.mqttPassword, newPassword, sizeof(networkConfig.mqttPassword));
        }

        // Save configuration to storage
        saveNetworkConfig();

        // Send success response
        server.send(200, "application/json", "{\"status\":\"success\",\"message\":\"MQTT configuration saved\"}");
        
        // TODO: Trigger MQTT reconnect here if needed
    });
}

void setupTimeAPI()
{
  server.on("/api/time", HTTP_GET, []()
            {
        StaticJsonDocument<256> doc;
        DateTime dt;
        if (getGlobalDateTime(dt)) {
            char dateStr[11];
            snprintf(dateStr, sizeof(dateStr), "%04d-%02d-%02d", 
                    dt.year, dt.month, dt.day);
            doc["date"] = dateStr;
            
            char timeStr[9];  // Increased size to accommodate seconds
            snprintf(timeStr, sizeof(timeStr), "%02d:%02d:%02d", 
                    dt.hour, dt.minute, dt.second);  // Added seconds
            doc["time"] = timeStr;
            
            doc["timezone"] = networkConfig.timezone;
            doc["ntpEnabled"] = networkConfig.ntpEnabled;
            doc["dst"] = networkConfig.dstEnabled;
            
            // Add NTP status information
            if (networkConfig.ntpEnabled) {
                // Calculate NTP status
                uint8_t ntpStatus = NTP_STATUS_FAILED;
                uint32_t timeSinceLastUpdate = 0;
                
                if (lastNTPUpdateTime > 0) {
                    timeSinceLastUpdate = millis() - lastNTPUpdateTime;
                    if (timeSinceLastUpdate < (NTP_UPDATE_INTERVAL * 3)) {
                        ntpStatus = NTP_STATUS_CURRENT;
                    } else {
                        ntpStatus = NTP_STATUS_STALE;
                    }
                }
                
                doc["ntpStatus"] = ntpStatus;
                
                // Format last update time
                if (lastNTPUpdateTime > 0) {
                    char lastUpdateStr[20];
                    uint32_t seconds = timeSinceLastUpdate / 1000;
                    uint32_t minutes = seconds / 60;
                    uint32_t hours = minutes / 60;
                    uint32_t days = hours / 24;
                    
                    if (days > 0) {
                        snprintf(lastUpdateStr, sizeof(lastUpdateStr), "%d days ago", days);
                    } else if (hours > 0) {
                        snprintf(lastUpdateStr, sizeof(lastUpdateStr), "%d hours ago", hours);
                    } else if (minutes > 0) {
                        snprintf(lastUpdateStr, sizeof(lastUpdateStr), "%d minutes ago", minutes);
                    } else {
                        snprintf(lastUpdateStr, sizeof(lastUpdateStr), "%d seconds ago", seconds);
                    }
                    doc["lastNtpUpdate"] = lastUpdateStr;
                } else {
                    doc["lastNtpUpdate"] = "Never";
                }
            }
            
            String response;
            serializeJson(doc, response);
            server.send(200, "application/json", response);
        } else {
            server.send(500, "application/json", "{\"error\": \"Failed to get current time\"}");
        }
    });

  server.on("/api/time", HTTP_POST, []() {
        StaticJsonDocument<200> doc;
        String json = server.arg("plain");
        DeserializationError error = deserializeJson(doc, json);

        log(LOG_INFO, true, "Received JSON: %s\n", json.c_str());
        
        if (error) {
            server.send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
            log(LOG_ERROR, true, "JSON parsing error: %s\n", error.c_str());
            return;
        }

        // Validate required fields
        if (!doc.containsKey("date") || !doc.containsKey("time")) {
            server.send(400, "application/json", "{\"error\":\"Missing required fields\"}");
            log(LOG_ERROR, true, "Missing required fields in JSON\n");
            return;
        }

        // Update timezone if provided
        if (doc.containsKey("timezone")) {
          const char* tz = doc["timezone"];
          log(LOG_INFO, true, "Received timezone: %s\n", tz);
          // Basic timezone format validation (+/-HH:MM)
          int tzHour, tzMin;
          if (sscanf(tz, "%d:%d", &tzHour, &tzMin) != 2 ||
              tzHour < -12 || tzHour > 14 || tzMin < 0 || tzMin > 59) {
              server.send(400, "application/json", "{\"error\":\"Invalid timezone format\"}");
              return;
          }
          strncpy(networkConfig.timezone, tz, sizeof(networkConfig.timezone) - 1);
          networkConfig.timezone[sizeof(networkConfig.timezone) - 1] = '\0';
          log(LOG_INFO, true, "Updated timezone: %s\n", networkConfig.timezone);
        }

        // Update NTP enabled status if provided
        if (doc.containsKey("ntpEnabled")) {
          bool ntpWasEnabled = networkConfig.ntpEnabled;
          networkConfig.ntpEnabled = doc["ntpEnabled"];
          if (networkConfig.ntpEnabled) {
            // Update DST setting if provided
            if (doc.containsKey("dstEnabled")) {
              networkConfig.dstEnabled = doc["dstEnabled"];
            }
            handleNTPUpdates(true);
            server.send(200, "application/json", "{\"status\": \"success\", \"message\": \"NTP enabled, manual time update ignored\"}");
            saveNetworkConfig(); // Save to storage when NTP settings change
            return;
          }
          if (ntpWasEnabled) {
            server.send(200, "application/json", "{\"status\": \"success\", \"message\": \"NTP disabled, manual time update required\"}");
            saveNetworkConfig(); // Save to storage when NTP settings change
          }
        }

        // Validate and parse date and time
        const char* dateStr = doc["date"];
        uint16_t year;
        uint8_t month, day;
        const char* timeStr = doc["time"];
        uint8_t hour, minute;

        // Parse date string (format: YYYY-MM-DD)
        if (sscanf(dateStr, "%hu-%hhu-%hhu", &year, &month, &day) != 3 ||
            year < 2000 || year > 2099 || month < 1 || month > 12 || day < 1 || day > 31) {
            server.send(400, "application/json", "{\"error\": \"Invalid date format or values\"}");
            log(LOG_ERROR, true, "Invalid date format or values in JSON\n");
            return;
        }

        // Parse time string (format: HH:MM)          
        if (sscanf(timeStr, "%hhu:%hhu", &hour, &minute) != 2 ||
            hour > 23 || minute > 59) {
            server.send(400, "application/json", "{\"error\": \"Invalid time format or values\"}");
            return;
        }

        DateTime newDateTime = {year, month, day, hour, minute, 0};
        if (updateGlobalDateTime(newDateTime)) {
                server.send(200, "application/json", "{\"status\": \"success\"}");
        } else {
                server.send(500, "application/json", "{\"error\": \"Failed to update time\"}");
        }
  } );
}

// Network management functions --------------------------------------------->
// Handle ethernet plug and unplug events (from main loop)
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
      // Ethernet is still connected
      handleWebServer();
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

// Handle web server requests
void handleWebServer() {
  if(!ethernetConnected) {
    return;
  }
  server.handleClient();
  if (!statusLocked) {
    statusLocked = true;
    status.webserverBusy = false;
    status.webserverUp = true;
    status.updated = true;
    statusLocked = false;
  }
}

// Webserver callbacks ----------------------------------------------------->
void handleRoot()
{
  handleFile("/index.html");
}

// Handle file requests - retrieve from LittleFS and send to client
void handleFile(const char *path)
{
  if(eth.status() != WL_CONNECTED) {
    if (!statusLocked) {
      statusLocked = true;
      status.webserverBusy = false;
      status.webserverUp = false;
      status.updated = true;
      statusLocked = false;
    }
    return;
  }
  if (!statusLocked) {
    statusLocked = true;
    status.webserverBusy = true;
    statusLocked = false;
  }
  String contentType;
  if (strstr(path, ".html"))
    contentType = "text/html";
  else if (strstr(path, ".css"))
    contentType = "text/css";
  else if (strstr(path, ".js"))
    contentType = "application/javascript";
  else if (strstr(path, ".json"))
    contentType = "application/json";
  else if (strstr(path, ".ico"))
    contentType = "image/x-icon";
  else
    contentType = "text/plain";

  String filePath = path;
  if (filePath.endsWith("/"))
    filePath += "index.html";
  if (!filePath.startsWith("/"))
    filePath = "/" + filePath;

  if (LittleFS.exists(filePath))
  {
    File file = LittleFS.open(filePath, "r");
    server.streamFile(file, contentType);
    file.close();
  }
  else
  {
    server.send(404, "text/plain", "File not found");
  }
  if (!statusLocked) {
    statusLocked = true;
    status.webserverBusy = false;
    status.webserverUp = true;
    status.updated = true;
    statusLocked = false;
  }
}

// NTP management functions ------------------------------------------------>
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

// Debug functions --------------------------------------------------------->
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