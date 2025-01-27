#include "sys_init.h"

// ---------------------- RTOS tasks ---------------------- //

// Core 0 tasks

// Core 1 tasks
void statusLEDs(void *param);
void manageRTC(void *param);
void manageTerminal(void *param);
void managePower(void *param);

// -------------------- Non-RTOS tasks -------------------- //

// Forward declarations for web server functions
void setupEthernet(void);
void manageEthernet(void);
void setupWebServer(void);
void handleWebServer(void);
void handleRoot(void);
void handleFile(const char *path);

// Global threadsafe functions
bool updateGlobalDateTime(const DateTime &dt);
bool getGlobalDateTime(DateTime &dt);
bool setLEDcolour(uint8_t led, uint32_t colour);

// Debug functions
void debug_printf(const char* format, ...);
void osDebugPrint(void);

// Function to convert epoch time to DateTime
DateTime epochToDateTime(time_t epochTime)
{
  struct tm *timeinfo = gmtime(&epochTime);
  DateTime dt = {
      .year = (uint16_t)(timeinfo->tm_year + 1900),
      .month = (uint8_t)(timeinfo->tm_mon + 1),
      .day = (uint8_t)timeinfo->tm_mday,
      .hour = (uint8_t)timeinfo->tm_hour,
      .minute = (uint8_t)timeinfo->tm_min,
      .second = (uint8_t)timeinfo->tm_sec};
  return dt;
}

// Carry out an NTP update
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
    debug_printf("Failed to get time from NTP server, retrying\n");
    bool updateSuccessful = false;
    for (int i = 0; i < 3; i++) {
      if (timeClient.update()) {
        updateSuccessful = true;
        break;
      }
      delay(10);
   }
    if (!updateSuccessful) {
      debug_printf("Failed to get time from NTP server, giving up\n");
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
    debug_printf("Failed to update time from NTP\n");
  }
  else
  {
    debug_printf("Time updated from NTP server\n");
  }
}

void handleNTPUpdates(bool forceUpdate = false)
{
  if (!networkConfig.ntpEnabled) return;
  uint32_t timeSinceLastUpdate = millis() - ntpUpdateTimestamp;

  // Check if there's an NTP update request
  if (timeSinceLastUpdate > NTP_UPDATE_INTERVAL || forceUpdate)
  {
    if (timeSinceLastUpdate < NTP_MIN_SYNC_INTERVAL) {
      debug_printf("Time since last NTP update: %ds - skipping\n", timeSinceLastUpdate/1000);
      return;
    }
    ntpUpdate();
    ntpUpdateTimestamp = millis();
  }
}

void debugPrintNetConfig(NetworkConfig config)
{
  debug_printf("Mode: %s\n", config.useDHCP ? "DHCP" : "Static");
  debug_printf("IP: %s\n", config.ip.toString().c_str());
  debug_printf("Subnet: %s\n", config.subnet.toString().c_str());
  debug_printf("Gateway: %s\n", config.gateway.toString().c_str());
  debug_printf("DNS: %s\n", config.dns.toString().c_str());
  debug_printf("Timezone: %s\n", config.timezone);
  debug_printf("Hostname: %s\n", config.hostname);
  debug_printf("NTP Server: %s\n", config.ntpServer);
  debug_printf("NTP Enabled: %s\n", config.ntpEnabled ? "true" : "false");
  debug_printf("DST Enabled: %s\n", config.dstEnabled ? "true" : "false");
}

// Function to load network configuration from EEPROM
bool loadNetworkConfig()
{
  debug_printf("Loading network configuration:\n");
  EEPROM.begin(512);
  uint8_t magicNumber = EEPROM.read(0);
  debug_printf("Magic number: %x\n", magicNumber);
  if (magicNumber != EE_MAGIC_NUMBER) return false;
  EEPROM.get(EE_NETWORK_CONFIG_ADDRESS, networkConfig);
  EEPROM.end();
  debugPrintNetConfig(networkConfig);
  return true;
}

// Function to save network configuration to EEPROM
void saveNetworkConfig()
{
  debug_printf("Saving network configuration:\n");
  debugPrintNetConfig(networkConfig);
  EEPROM.begin(512);
  EEPROM.put(EE_NETWORK_CONFIG_ADDRESS, networkConfig);
  EEPROM.update(0, EE_MAGIC_NUMBER);
  EEPROM.commit();
  EEPROM.end();
}

// Function to apply network configuration
bool applyNetworkConfig()
{
  if (networkConfig.useDHCP)
  {
    if (!eth.begin())
    {
      debug_printf("Failed to configure Ethernet using DHCP, falling back to 192.168.1.10\n");
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

void setupEthernet()
{
  // Load network configuration
  if (!loadNetworkConfig())
  {
    // Set default configuration if load fails
    debug_printf("Invalid network configuration, using defaults\n");
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
    debug_printf("Failed to apply network configuration\n");
  }

  else {
    // Get and store MAC address
    uint8_t mac[6];
    eth.macAddress(mac);
    snprintf(deviceMacAddress, sizeof(deviceMacAddress), "%02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    debug_printf("MAC Address: %s\n", deviceMacAddress);
  }

  // Wait for Ethernet to connect
  delay(2000);

  if (eth.linkStatus() == LinkOFF) {
    debug_printf("Ethernet not connected\n");
    ethernetConnected = false;
  }
  else {
    debug_printf("Ethernet connected, IP address: %s, Gateway: %s\n",
                eth.localIP().toString().c_str(),
                eth.gatewayIP().toString().c_str());
    ethernetConnected = true;
  }
}

// API endpoint to get current network settings
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

              // Save configuration to EEPROM
              saveNetworkConfig();

              // Send success response before applying changes
              server.send(200, "application/json", "{\"status\":\"success\",\"message\":\"Configuration saved\"}");

              // Apply new configuration after a short delay
              delay(100);
              rp2040.reboot(); // Use proper RP2040 reset function
            });
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

        // Save configuration to EEPROM
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

        debug_printf("Received JSON: %s\n", json.c_str());
        
        if (error) {
            server.send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
            debug_printf("JSON parsing error: %s\n", error.c_str());
            return;
        }

        // Validate required fields
        if (!doc.containsKey("date") || !doc.containsKey("time")) {
            server.send(400, "application/json", "{\"error\":\"Missing required fields\"}");
            debug_printf("Missing required fields in JSON\n");
            return;
        }

        // Update timezone if provided
        if (doc.containsKey("timezone")) {
          const char* tz = doc["timezone"];
          debug_printf("Received timezone: %s\n", tz);
          // Basic timezone format validation (+/-HH:MM)
          int tzHour, tzMin;
          if (sscanf(tz, "%d:%d", &tzHour, &tzMin) != 2 ||
              tzHour < -12 || tzHour > 14 || tzMin < 0 || tzMin > 59) {
              server.send(400, "application/json", "{\"error\":\"Invalid timezone format\"}");
              return;
          }
          strncpy(networkConfig.timezone, tz, sizeof(networkConfig.timezone) - 1);
          networkConfig.timezone[sizeof(networkConfig.timezone) - 1] = '\0';
          debug_printf("Updated timezone: %s\n", networkConfig.timezone);
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
            saveNetworkConfig(); // Save to EEPROM when NTP settings change
            return;
          }
          if (ntpWasEnabled) {
            server.send(200, "application/json", "{\"status\": \"success\", \"message\": \"NTP disabled, manual time update required\"}");
            saveNetworkConfig(); // Save to EEPROM when NTP settings change
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
            debug_printf("Invalid date format or values in JSON\n");
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

void setup()
{
  Serial.begin(115200);
  while (!Serial)
    ;

  Serial.println("Core 0 setup started");

  // Create serial debug mutex
  serialMutex = xSemaphoreCreateMutex();
  if (serialMutex == NULL) {
    Serial.println("Failed to create serial mutex");
  }
  else serialReady = true;

  // Initialize NTP update queue
  /*ntpUpdateQueue = xQueueCreate(1, sizeof(uint8_t));
  if (ntpUpdateQueue == NULL)
  {
    debug_printf("Failed to create NTP update queue\n");
    return;
  }*/

  // Initialize hardware
  setupEthernet();
  setupWebServer();
  setupNetworkAPI();
  setupMqttAPI();
  setupTimeAPI();

  // Start NTP sync task
  //xTaskCreate(ntpSyncTask, "NTP Sync", 4096, NULL, 1, NULL);

  debug_printf("Core 0 setup complete\n");
  core0setupComplete = true;
  while (!core1setupComplete) delay(100);
  debug_printf("<---System initialisation complete --->\n\n");
}

void loop()
{
  // Do network tasks if ethernet is connected
  if (ethernetConnected) {
    if (eth.linkStatus() == LinkOFF) {
      ethernetConnected = false;
      debug_printf("Ethernet disconnected, waiting for reconnect\n");
    }
    else {
      handleWebServer();
      handleNTPUpdates(false);  // Process any pending NTP updates
    }
  }
  else if (eth.linkStatus() == LinkON) {
    ethernetConnected = true;
    debug_printf("Ethernet re-connected, IP address: %s, Gateway: %s\n",
                eth.localIP().toString().c_str(),
                eth.gatewayIP().toString().c_str());
  }
}

void setup1()
{
  while (!serialReady) delay(100);
  debug_printf("Core 1 setup started\n");

  // Create synchronization primitives
  dateTimeMutex = xSemaphoreCreateMutex();
  if (dateTimeMutex == NULL) {
    debug_printf("Failed to create dateTimeMutex!\n");
    while (1);
  }
  statusMutex = xSemaphoreCreateMutex();
  if (statusMutex == NULL) {
    debug_printf("Failed to create statusMutex!\n");
    while (1);
  }

   // Set System Status
  setLEDcolour(LED_SYSTEM_STATUS, LED_STATUS_STARTUP);
  // Initialize Core 1 tasks
  xTaskCreate(statusLEDs, "LED stat", configMINIMAL_STACK_SIZE, NULL, 1, NULL);
  xTaskCreate(manageRTC, "RTC updt", configMINIMAL_STACK_SIZE, NULL, 1, NULL);
  xTaskCreate(manageTerminal, "Term updt", configMINIMAL_STACK_SIZE, NULL, 1, NULL);
  xTaskCreate(managePower, "Pwr updt", configMINIMAL_STACK_SIZE, NULL, 1, NULL);

  // Modbus not yet implemented
  setLEDcolour(LED_MODBUS_STATUS, LED_STATUS_OFF);
  // MQTT not yet implemented
  setLEDcolour(LED_MQTT_STATUS, LED_STATUS_OFF); 

  debug_printf("Core 1 setup complete\n");
  core1setupComplete = true;
  while (!core0setupComplete) delay(100);
}

void loop1() {
  delay(100);
}

// ---------------------- Core 0 tasks ---------------------- //



// ---------------------- Core 1 tasks ---------------------- //
void statusLEDs(void *param)
{
  (void)param;

  uint32_t loopCounter = 0;
  uint32_t ledRefreshInterval = 20;
  uint32_t loopCountsPerHalfSec = 500 / ledRefreshInterval;
  bool blinkState = false;

  leds.begin();
  leds.setBrightness(50);
  leds.fill(LED_COLOR_OFF, 0, 4);
  leds.show();
  debug_printf("LED status task started\n");

  // Task loop
  while (1) {
    uint32_t statusLEDcolour = STATUS_WARNING;
    if (xSemaphoreTake(statusMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
      for (int i = 0; i < 3; i++) {
        leds.setPixelColor(i, status.LEDcolour[i]);
      }
      statusLEDcolour = status.LEDcolour[3];
      xSemaphoreGive(statusMutex);
    }
    leds.show();
    vTaskDelay(pdMS_TO_TICKS(ledRefreshInterval));
    loopCounter++;
    // Things to do every half second
    if (loopCounter >= loopCountsPerHalfSec) {
      loopCounter = 0;
      blinkState = !blinkState;

      if (blinkState) {
        leds.setPixelColor(LED_SYSTEM_STATUS, statusLEDcolour);
        leds.show();
      } else {
        leds.setPixelColor(LED_SYSTEM_STATUS, LED_COLOR_OFF);
        leds.show();
      }
    }
  }
}

void manageRTC(void *param)
{
  (void)param;
  Wire1.setSDA(PIN_RTC_SDA);
  Wire1.setSCL(PIN_RTC_SCL);

  if (!rtc.begin())
  {
    debug_printf("RTC initialization failed!\n");
    return;
  }

  // Set initial time (24-hour format)
  DateTime now;
  rtc.getDateTime(&now);
  memcpy(&globalDateTime, &now, sizeof(DateTime)); // Initialize global DateTime directly
  debug_printf("Current date and time is: %04d-%02d-%02d %02d:%02d:%02d\n",
                now.year, now.month, now.day, now.hour, now.minute, now.second);
                
  debug_printf("RTC update task started\n");

  // Task loop
  while (1)
  {
    DateTime currentTime;
    if (rtc.getDateTime(&currentTime))
    {
      // Only update global time if it hasn't been modified recently
      if (xSemaphoreTake(dateTimeMutex, pdMS_TO_TICKS(100)) == pdTRUE)
      {
        memcpy(&globalDateTime, &currentTime, sizeof(DateTime));
        xSemaphoreGive(dateTimeMutex);
      }
    }
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

void manageTerminal(void *param)
{
  (void)param;
  while (!core1setupComplete || !core0setupComplete) vTaskDelay(pdMS_TO_TICKS(100));

  debug_printf("Terminal task started\n");

  // Task loop
  while (1) {
    if (Serial.available())
    {
      debug_printf("Command received: ");
      char serialString[10];  // Buffer for incoming serial data
      Serial.readBytesUntil('\n', serialString, sizeof(serialString));
      debug_printf("%s\n", serialString);
      if (strcmp(serialString, "ps") == 0) {
        osDebugPrint();
      }
    }
    vTaskDelay(pdMS_TO_TICKS(100));
  }
}

void managePower(void *param) {
  (void)param;
  while (!core1setupComplete || !core0setupComplete) vTaskDelay(pdMS_TO_TICKS(100));

  analogReadResolution(12);
  debug_printf("Power monitoring task started\n");

  float Vpsu, V20, V5;
  bool psuOK, V20OK, V5OK;

  // Task loop
  while (1) {
    Vpsu = V20 = V5 = 0.0;
    for (int i = 0; i < 10; i++) {
      Vpsu += (float)analogRead(PIN_PS_24V_FB) * V_PSU_MUL_V;
      V20 += (float)analogRead(PIN_PS_20V_FB) * V_PSU_MUL_V;
      V5 += (float)analogRead(PIN_PS_5V_FB) * V_5V_MUL_V;
      vTaskDelay(pdMS_TO_TICKS(10));
    }
    Vpsu /= 10.0;
    V20 /= 10.0;
    V5 /= 10.0;
    if (Vpsu > V_PSU_MAX || Vpsu < V_PSU_MIN) {
      if (psuOK) debug_printf("PSU voltage out of range: %.2f V\n", Vpsu);
      psuOK = false;
    }
    else psuOK = true;
    if (V20 > V_20V_MAX || V20 < V_20V_MIN) {
      if (V20OK) debug_printf("20V voltage out of range: %.2f V\n", V20);
      V20OK = false;
    }
    else V20OK = true;
    if (V5 > V_5V_MAX || V5 < V_5V_MIN) {
      if (V5OK) debug_printf("5V voltage out of range: %.2f V\n", V5);
      V5OK = false;
    }
    else V5OK = true;
    if (!psuOK || !V20OK || !V5OK) {
      setLEDcolour(LED_SYSTEM_STATUS, LED_STATUS_WARNING);
    }
    else setLEDcolour(LED_SYSTEM_STATUS, LED_STATUS_OK);
    if (xSemaphoreTake(statusMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
      status.Vpsu = Vpsu;
      status.V20 = V20;
      status.V5 = V5;
      status.psuOK = psuOK;
      status.V20OK = V20OK;
      status.V5OK = V5OK;
      xSemaphoreGive(statusMutex);
    };
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

// ---------------------- Web Server Implementation ---------------------- //

void setupWebServer()
{
  // Initialize LittleFS for serving web files
  if (!LittleFS.begin())
  {
    debug_printf("LittleFS Mount Failed\n");
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

  // Handle static files
  server.onNotFound([]()
                    { handleFile(server.uri().c_str()); });

  server.begin();
  debug_printf("HTTP server started\n");
  
  // Set Webserver Status LED
  setLEDcolour(LED_WEBSERVER_STATUS, LED_STATUS_OK);
}

void handleWebServer()
{
  if(eth.status() != WL_CONNECTED) {
    setLEDcolour(LED_WEBSERVER_STATUS, LED_STATUS_OFF);
    return;
  }
  server.handleClient();
  setLEDcolour(LED_WEBSERVER_STATUS, LED_STATUS_OK);
}

void handleRoot()
{
  handleFile("/index.html");
}

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

// ---------------------- Utility functions ---------------------- //

// Function to safely get the current DateTime
bool getGlobalDateTime(DateTime &dt)
{
  if (xSemaphoreTake(dateTimeMutex, pdMS_TO_TICKS(100)) == pdTRUE)
  {
    memcpy(&dt, &globalDateTime, sizeof(DateTime));
    xSemaphoreGive(dateTimeMutex);
    return true;
  }
  return false;
}

// Function to safely update the DateTime
/*bool updateGlobalDateTime(const DateTime &dt)
{
  if (xSemaphoreTake(dateTimeMutex, pdMS_TO_TICKS(100)) == pdTRUE)
  {
    // Update the RTC first
    debug_printf("Setting RTC to: %04d-%02d-%02d %02d:%02d:%02d\n",
                  dt.year, dt.month, dt.day, dt.hour, dt.minute, dt.second);

    rtc.setDateTime(dt); // Ignore return value since RTC appears to update anyway

    // Verify the time was set by reading it back
    DateTime currentTime;
    if (rtc.getDateTime(&currentTime))
    {
      debug_printf("RTC read back: %04d-%02d-%02d %02d:%02d:%02d\n",
                    currentTime.year, currentTime.month, currentTime.day,
                    currentTime.hour, currentTime.minute, currentTime.second);
    }

    // Update the global variable
    memcpy(&globalDateTime, &dt, sizeof(DateTime));
    xSemaphoreGive(dateTimeMutex);

    // Signal that time was manually updated
    debug_printf("Time successfully set to: %04d-%02d-%02d %02d:%02d:%02d\n",
                  dt.year, dt.month, dt.day, dt.hour, dt.minute, dt.second);
    return true; // Return true since we know the RTC actually updates
  }
  debug_printf("Failed to take dateTimeMutex in updateGlobalDateTime\n");
  return false;
}*/

// Modified updateGlobalDateTime function with retry
bool updateGlobalDateTime(const DateTime &dt) {
    const int maxRetries = 3; // Maximum number of retries
    const int retryDelayMs = 100; // Delay between retries (milliseconds)

    if (xSemaphoreTake(dateTimeMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        bool success = false;
        for (int retry = 0; retry < maxRetries; ++retry) {
            Serial.printf("Attempt %d: Setting RTC to: %04d-%02d-%02d %02d:%02d:%02d\n",
                          retry + 1, dt.year, dt.month, dt.day, dt.hour, dt.minute, dt.second);

            rtc.setDateTime(dt); // Set RTC time

            // Verify the time was set by reading it back
            DateTime currentTime;
            if (rtc.getDateTime(&currentTime)) {
              // Check that time is correct before proceeding
              if (currentTime.year == dt.year &&
                  currentTime.month == dt.month &&
                  currentTime.day == dt.day &&
                  currentTime.hour == dt.hour &&
                  currentTime.minute == dt.minute &&
                  currentTime.second == dt.second) {
                    Serial.printf("RTC verification successful after %d retries.\n", retry + 1);
                    memcpy(&globalDateTime, &dt, sizeof(DateTime)); // Update global time after successful write
                    success = true;
                    break; // Exit retry loop on success
              } else {
                Serial.printf("RTC verification failed, current time: %04d-%02d-%02d %02d:%02d:%02d, expected time: %04d-%02d-%02d %02d:%02d:%02d\n", 
                        currentTime.year, currentTime.month, currentTime.day,
                        currentTime.hour, currentTime.minute, currentTime.second,
                        dt.year, dt.month, dt.day, dt.hour, dt.minute, dt.second);
              }
            } else {
                Serial.println("Failed to read time from RTC during verification.");
            }
             
            if(retry < maxRetries - 1) {
              vTaskDelay(pdMS_TO_TICKS(retryDelayMs)); // Delay if retrying
            }
        }

        xSemaphoreGive(dateTimeMutex); // Release the mutex
        if(success) {
           Serial.printf("Time successfully set to: %04d-%02d-%02d %02d:%02d:%02d\n",
                          dt.year, dt.month, dt.day, dt.hour, dt.minute, dt.second);
           return true;
        } else {
            Serial.println("Failed to set RTC time after maximum retries.");
            return false;
        }

    } else {
        Serial.println("Failed to take dateTimeMutex in updateGlobalDateTime");
        return false;
    }
}
// Thread-safe printf-like function
void debug_printf(const char* format, ...) {
    // Create a buffer to store the output
    static char buffer[DEBUG_PRINTF_BUFFER_SIZE];
    
    // Acquire the Mutex: Blocks until the Mutex becomes available.
    if (xSemaphoreTake(serialMutex, portMAX_DELAY) == pdTRUE) {
        // Format the string into the buffer
        va_list args;
        va_start(args, format);
        int len = vsnprintf(buffer, DEBUG_PRINTF_BUFFER_SIZE, format, args);
        va_end(args);

        // Check for errors or truncation.
        if(len > 0) {
            Serial.print(buffer);
        } else {
            Serial.println("Error during debug_printf: formatting error or buffer overflow");
        }


        // Release the mutex
        xSemaphoreGive(serialMutex);
    } else {
        // Error: Failed to acquire the Mutex. Print error message on the unprotected port
        Serial.println("Error: Failed to acquire Serial Mutex for debug_printf!");
    }
}

// Thread-safe LED colour setter
bool setLEDcolour(uint8_t led, uint32_t colour) {
  if (led > 3) {
    debug_printf("Invalid LED number: %d\n", led);
    return false;
  }
  if (xSemaphoreTake(statusMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
    status.LEDcolour[led] = colour;
    xSemaphoreGive(statusMutex);
    return true;
  }
  return false;
}
void osDebugPrint(void)
{
  const char *taskStateName[5] = {
      "Ready",
      "Blocked",
      "Suspended",
      "Deleted",
      "Invalid"};
  int numberOfTasks = uxTaskGetNumberOfTasks();

  DateTime current;
  if (getGlobalDateTime(current))
  {
    // Use the current time safely
    debug_printf("Time: %02d:%02d:%02d\n",
                  current.hour, current.minute, current.second);
  }

  TaskStatus_t *pxTaskStatusArray = new TaskStatus_t[numberOfTasks];
  uint32_t runtime;
  numberOfTasks = uxTaskGetSystemState(pxTaskStatusArray, numberOfTasks, &runtime);
  debug_printf("Tasks: %d\n", numberOfTasks);
  for (int i = 0; i < numberOfTasks; i++)
  {
    debug_printf("ID: %d %s", i, pxTaskStatusArray[i].pcTaskName);
    int currentState = pxTaskStatusArray[i].eCurrentState;
    debug_printf(" Current state: %s", taskStateName[currentState]);
    debug_printf(" Priority: %u\n", pxTaskStatusArray[i].uxBasePriority);
    debug_printf(" Free stack: %u\n", pxTaskStatusArray[i].usStackHighWaterMark);
    debug_printf(" Runtime: %u\n", pxTaskStatusArray[i].ulRunTimeCounter);
  }
  delete[] pxTaskStatusArray;
}