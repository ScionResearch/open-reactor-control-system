#include "network.h"

// Global variables
NetworkConfig networkConfig;

Wiznet5500lwIP eth(PIN_ETH_CS, SPI, PIN_ETH_IRQ);
WebServer server(80);

// NTP update queue
QueueHandle_t ntpUpdateQueue;
uint32_t ntpUpdateTimestamp = 0 - NTP_MIN_SYNC_INTERVAL;

// Device MAC address (stored as string)
char deviceMacAddress[18];

bool ethernetConnected = false;

// Network component initialisation functions ------------------------------>
void init_network() {
    setupEthernet();
    setupWebServer();
    setupNetworkAPI();
    setupMqttAPI();
    setupTimeAPI();
}
void setupEthernet()
{
  // Load network configuration
  if (!loadNetworkConfig())
  {
    // Set default configuration if load fails
    debug_printf(LOG_INFO, "Invalid network configuration, using defaults\n");
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
    debug_printf(LOG_WARNING, "Failed to apply network configuration\n");
  }

  else {
    // Get and store MAC address
    uint8_t mac[6];
    eth.macAddress(mac);
    snprintf(deviceMacAddress, sizeof(deviceMacAddress), "%02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    debug_printf(LOG_INFO, "MAC Address: %s\n", deviceMacAddress);
  }

  // Wait for Ethernet to connect
  delay(2000);

  if (eth.linkStatus() == LinkOFF) {
    debug_printf(LOG_WARNING, "Ethernet not connected\n");
    ethernetConnected = false;
  }
  else {
    debug_printf(LOG_INFO, "Ethernet connected, IP address: %s, Gateway: %s\n",
                eth.localIP().toString().c_str(),
                eth.gatewayIP().toString().c_str());
    ethernetConnected = true;
  }
}

bool loadNetworkConfig()
{
  debug_printf(LOG_INFO, "Loading network configuration:\n");
  
  // Check if LittleFS is mounted
  if (!LittleFS.begin()) {
    debug_printf(LOG_WARNING, "Failed to mount LittleFS\n");
    return false;
  }

  // Check if config file exists
  if (!LittleFS.exists(CONFIG_FILENAME)) {
    debug_printf(LOG_WARNING, "Config file not found\n");
    LittleFS.end();
    return false;
  }

  // Open config file
  File configFile = LittleFS.open(CONFIG_FILENAME, "r");
  if (!configFile) {
    debug_printf(LOG_WARNING, "Failed to open config file\n");
    LittleFS.end();
    return false;
  }

  // Allocate a buffer to store contents of the file
  StaticJsonDocument<512> doc;
  DeserializationError error = deserializeJson(doc, configFile);
  configFile.close();

  if (error) {
    debug_printf(LOG_WARNING, "Failed to parse config file: %s\n", error.c_str());
    LittleFS.end();
    return false;
  }

  // Check magic number
  uint8_t magicNumber = doc["magic_number"] | 0;
  debug_printf(LOG_INFO, "Magic number: %x\n", magicNumber);
  if (magicNumber != CONFIG_MAGIC_NUMBER) {
    debug_printf(LOG_WARNING, "Invalid magic number\n");
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
  debug_printf(LOG_INFO, "Saving network configuration:\n");
  debugPrintNetConfig(networkConfig);
  
  // Check if LittleFS is mounted
  if (!LittleFS.begin()) {
    debug_printf(LOG_WARNING, "Failed to mount LittleFS\n");
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
    debug_printf(LOG_WARNING, "Failed to open config file for writing\n");
    LittleFS.end();
    return;
  }
  
  // Write to file
  if (serializeJson(doc, configFile) == 0) {
    debug_printf(LOG_WARNING, "Failed to write config file\n");
  }
  
  // Close file
  configFile.close();
  LittleFS.end();
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
      debug_printf(LOG_INFO, "Failed to configure Ethernet using DHCP, falling back to 192.168.1.10\n");
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

              // Update DST setting
              networkConfig.dstEnabled = doc["dst"] | false;

              // Save configuration to storage
              saveNetworkConfig();

              // Send success response before applying changes
              server.send(200, "application/json", "{\"status\":\"success\",\"message\":\"Configuration saved\"}");

              // Apply new configuration after a short delay
              delay(100);
              rp2040.reboot(); // Use proper RP2040 reset function
            });
}

void setupWebServer()
{
  // Initialize LittleFS for serving web files
  if (!LittleFS.begin())
  {
    debug_printf(LOG_ERROR, "LittleFS Mount Failed\n");
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

  // System status endpoints
  server.on("/api/power", HTTP_GET, []() {
        StaticJsonDocument<200> doc;
        
        xSemaphoreTake(statusMutex, portMAX_DELAY);
        doc["mainVoltage"] = status.Vpsu;
        doc["v20Voltage"] = status.V20;
        doc["v5Voltage"] = status.V5;
        doc["mainVoltageOK"] = status.psuOK;
        doc["v20VoltageOK"] = status.V20OK;
        doc["v5VoltageOK"] = status.V5OK;
        xSemaphoreGive(statusMutex);
        
        String response;
        serializeJson(doc, response);
        server.send(200, "application/json", response);
    });

  // Handle static files
  server.onNotFound([]()
                    { handleFile(server.uri().c_str()); });

  server.begin();
  debug_printf(LOG_INFO, "HTTP server started\n");
  
  // Set Webserver Status LED
  setLEDcolour(LED_WEBSERVER_STATUS, LED_STATUS_OK);
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
        StaticJsonDocument<200> doc;
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

        debug_printf(LOG_INFO, "Received JSON: %s\n", json.c_str());
        
        if (error) {
            server.send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
            debug_printf(LOG_ERROR, "JSON parsing error: %s\n", error.c_str());
            return;
        }

        // Validate required fields
        if (!doc.containsKey("date") || !doc.containsKey("time")) {
            server.send(400, "application/json", "{\"error\":\"Missing required fields\"}");
            debug_printf(LOG_ERROR, "Missing required fields in JSON\n");
            return;
        }

        // Update timezone if provided
        if (doc.containsKey("timezone")) {
          const char* tz = doc["timezone"];
          debug_printf(LOG_INFO, "Received timezone: %s\n", tz);
          // Basic timezone format validation (+/-HH:MM)
          int tzHour, tzMin;
          if (sscanf(tz, "%d:%d", &tzHour, &tzMin) != 2 ||
              tzHour < -12 || tzHour > 14 || tzMin < 0 || tzMin > 59) {
              server.send(400, "application/json", "{\"error\":\"Invalid timezone format\"}");
              return;
          }
          strncpy(networkConfig.timezone, tz, sizeof(networkConfig.timezone) - 1);
          networkConfig.timezone[sizeof(networkConfig.timezone) - 1] = '\0';
          debug_printf(LOG_INFO, "Updated timezone: %s\n", networkConfig.timezone);
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
            debug_printf(LOG_ERROR, "Invalid date format or values in JSON\n");
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
      setLEDcolour(LED_WEBSERVER_STATUS, LED_STATUS_OFF);
      setLEDcolour(LED_MQTT_STATUS, LED_STATUS_OFF);
      debug_printf(LOG_INFO, "Ethernet disconnected, waiting for reconnect\n");
    }
    else {
      handleWebServer();
      handleNTPUpdates(false);  // Process any pending NTP updates
    }
  }
  else if (eth.linkStatus() == LinkON) {
    ethernetConnected = true;
    if(!applyNetworkConfig()) {
      debug_printf(LOG_ERROR, "Failed to apply network configuration!\n");
    }
    else {
      debug_printf(LOG_INFO, "Ethernet re-connected, IP address: %s, Gateway: %s\n",
                eth.localIP().toString().c_str(),
                eth.gatewayIP().toString().c_str());
    }
  }
}

// Handle web server requests (from manageEthernet)
void handleWebServer()
{
  if(!ethernetConnected) {
    return;
  }
  server.handleClient();
  setLEDcolour(LED_WEBSERVER_STATUS, LED_STATUS_OK);
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
    setLEDcolour(LED_WEBSERVER_STATUS, LED_STATUS_OFF);
    return;
  }
  setLEDcolour(LED_WEBSERVER_STATUS, LED_STATUS_BUSY);
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
  setLEDcolour(LED_WEBSERVER_STATUS, LED_STATUS_OK);
}

// NTP management functions ------------------------------------------------>
void ntpUpdate(void)
{
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
    debug_printf(LOG_WARNING, "Failed to get time from NTP server, retrying\n");
    bool updateSuccessful = false;
    for (int i = 0; i < 3; i++) {
      if (timeClient.update()) {
        updateSuccessful = true;
        break;
      }
      delay(10);
   }
    if (!updateSuccessful) {
      debug_printf(LOG_ERROR, "Failed to get time from NTP server, giving up\n");
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
    debug_printf(LOG_ERROR, "Failed to update time from NTP\n");
  }
  else
  {
    debug_printf(LOG_INFO, "Time updated from NTP server\n");
  }
}

void handleNTPUpdates(bool forceUpdate)
{
  if (!networkConfig.ntpEnabled) return;
  uint32_t timeSinceLastUpdate = millis() - ntpUpdateTimestamp;

  // Check if there's an NTP update request
  if (timeSinceLastUpdate > NTP_UPDATE_INTERVAL || forceUpdate)
  {
    if (timeSinceLastUpdate < NTP_MIN_SYNC_INTERVAL) {
      debug_printf(LOG_INFO, "Time since last NTP update: %ds - skipping\n", timeSinceLastUpdate/1000);
      return;
    }
    ntpUpdate();
    ntpUpdateTimestamp = millis();
  }
}

// Debug functions --------------------------------------------------------->
void debugPrintNetConfig(NetworkConfig config)
{
  debug_printf(LOG_INFO, "Mode: %s\n", config.useDHCP ? "DHCP" : "Static");
  debug_printf(LOG_INFO, "IP: %s\n", config.ip.toString().c_str());
  debug_printf(LOG_INFO, "Subnet: %s\n", config.subnet.toString().c_str());
  debug_printf(LOG_INFO, "Gateway: %s\n", config.gateway.toString().c_str());
  debug_printf(LOG_INFO, "DNS: %s\n", config.dns.toString().c_str());
  debug_printf(LOG_INFO, "Timezone: %s\n", config.timezone);
  debug_printf(LOG_INFO, "Hostname: %s\n", config.hostname);
  debug_printf(LOG_INFO, "NTP Server: %s\n", config.ntpServer);
  debug_printf(LOG_INFO, "NTP Enabled: %s\n", config.ntpEnabled ? "true" : "false");
  debug_printf(LOG_INFO, "DST Enabled: %s\n", config.dstEnabled ? "true" : "false");
}