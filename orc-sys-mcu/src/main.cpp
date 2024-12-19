#include "sys_init.h"

// ---------------------- RTOS tasks ---------------------- //

// Core 0 tasks

// Core 1 tasks
void statusLEDs(void *param);
void manageRTC(void *param);

// -------------------- Non-RTOS tasks -------------------- //

// Forward declarations for web server functions
void setupEthernet(void);
void manageEthernet(void);
void setupWebServer(void);
void handleWebServer(void);
void handleRoot(void);
void handleFile(const char *path);

// RTC threadsafe functions
bool updateGlobalDateTime(const DateTime &dt);
bool getGlobalDateTime(DateTime &dt);

// Debug functions
void osDebugPrint(HardwareSerial &serialDebug);

NetworkConfig networkConfig;
char deviceMacAddress[18];
SemaphoreHandle_t dateTimeMutex = NULL;
DateTime globalDateTime = {0};
QueueHandle_t ntpUpdateQueue = NULL;

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

// NTP time sync task - only queues update requests
void ntpSyncTask(void *parameter)
{
  const TickType_t ntpSyncInterval = pdMS_TO_TICKS(3600000); // Sync every hour
  bool updateRequested = false;

  for (;;)
  {
    if (networkConfig.ntpEnabled && !updateRequested && eth.linkStatus() == LinkON)
    {
      // Queue an NTP update request
      uint8_t dummy = 1;
      xQueueSend(ntpUpdateQueue, &dummy, 0);
      updateRequested = true;
    }

    // Wait for the sync interval
    vTaskDelay(ntpSyncInterval);
    updateRequested = false;
  }
}

// Handle NTP updates in the main loop
void handleNTPUpdates()
{
  static WiFiUDP udp;
  static NTPClient timeClient(udp, networkConfig.ntpServer);
  static bool clientInitialized = false;
  uint8_t dummy;

  // Check if there's an NTP update request
  if (xQueueReceive(ntpUpdateQueue, &dummy, 0) == pdTRUE)
  {
    if (!clientInitialized)
    {
      timeClient.begin();
      clientInitialized = true;
    }

    if (eth.linkStatus() == LinkON && timeClient.update())
    {
      // Get NTP time
      time_t epochTime = timeClient.getEpochTime();

      // Apply timezone offset
      int tzHours = 0, tzMinutes = 0;
      sscanf(networkConfig.timezone, "%d:%d", &tzHours, &tzMinutes);
      epochTime += (tzHours * 3600 + tzMinutes * 60);

      // Convert to DateTime and update using thread-safe function
      DateTime newTime = epochToDateTime(epochTime);
      if (!updateGlobalDateTime(newTime))
      {
        Serial.println("Failed to update time from NTP");
      }
      else
      {
        Serial.println("Time updated from NTP server");
      }
    }
    else
    {
      Serial.println("Failed to get time from NTP server");
    }
  }
}

void debugPrintNetConfig(NetworkConfig config)
{
  Serial.printf("Mode: %s\n", config.useDHCP ? "DHCP" : "Static");
  Serial.printf("IP: %s\n", config.ip.toString().c_str());
  Serial.printf("Subnet: %s\n", config.subnet.toString().c_str());
  Serial.printf("Gateway: %s\n", config.gateway.toString().c_str());
  Serial.printf("DNS: %s\n", config.dns.toString().c_str());
  Serial.printf("Timezone: %s\n", config.timezone);
  Serial.printf("NTP Server: %s\n", config.ntpServer);
  Serial.printf("NTP Enabled: %s\n", config.ntpEnabled ? "true" : "false");
}

void checkTerminal(void)
{
  if (Serial.available())
  {
    Serial.print("Command received: ");
    char serialString[10];  // Buffer for incoming serial data
    Serial.readBytesUntil('\n', serialString, sizeof(serialString));
    Serial.println(serialString);
    if (strcmp(serialString, "ps") == 0) {
      osDebugPrint(Serial);
    }
  }
}

// Function to load network configuration from EEPROM
bool loadNetworkConfig()
{
  Serial.println("Loading network configuration:");
  EEPROM.begin(512);
  uint8_t magicNumber = EEPROM.read(0);
  Serial.printf("Magic number: %x\n", magicNumber);
  if (magicNumber != EE_MAGIC_NUMBER) return false;
  EEPROM.get(EE_NETWORK_CONFIG_ADDRESS, networkConfig);
  EEPROM.end();
  debugPrintNetConfig(networkConfig);
  return true;
}

// Function to save network configuration to EEPROM
void saveNetworkConfig()
{
  Serial.println("Saving network configuration:");
  debugPrintNetConfig(networkConfig);
  EEPROM.begin(512);
  EEPROM.put(EE_NETWORK_CONFIG_ADDRESS, networkConfig);
  EEPROM.update(0, EE_MAGIC_NUMBER);
  EEPROM.commit();
  Serial.println("Configuration saved, checking stored values:");
  NetworkConfig config;
  EEPROM.get(EE_NETWORK_CONFIG_ADDRESS, config);
  debugPrintNetConfig(config);
  EEPROM.end();
}

// Function to apply network configuration
bool applyNetworkConfig()
{
  if (networkConfig.useDHCP)
  {
    if (!eth.begin())
    {
      Serial.println("Failed to configure Ethernet using DHCP, falling back to 192.168.1.10");
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
    Serial.println("Invalid network configuration, using defaults");
    networkConfig.ntpEnabled = false;
    networkConfig.useDHCP = true;
    networkConfig.ip = IPAddress(192, 168, 1, 100);
    networkConfig.subnet = IPAddress(255, 255, 255, 0);
    networkConfig.gateway = IPAddress(192, 168, 1, 1);
    networkConfig.dns = IPAddress(8, 8, 8, 8);
    strcpy(networkConfig.timezone, "+13:00");
    strcpy(networkConfig.ntpServer, "pool.ntp.org");
    saveNetworkConfig();
  }

  SPI.setMOSI(PIN_ETH_MOSI);
  SPI.setMISO(PIN_ETH_MISO);
  SPI.setSCK(PIN_ETH_SCK);
  SPI.setCS(PIN_ETH_CS);

  eth.setSPISpeed(30000000);

  // Apply network configuration
  if (!applyNetworkConfig())
  {
    Serial.println("Failed to apply network configuration");
  }

  else {
    // Get and store MAC address
    uint8_t mac[6];
    eth.macAddress(mac);
    snprintf(deviceMacAddress, sizeof(deviceMacAddress), "%02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    Serial.print("MAC Address: ");
    Serial.println(deviceMacAddress);
  }

  // Wait until Ethernet is connected
  while (eth.linkStatus() == LinkOFF)
  {
    Serial.println("Ethernet cable is not connected.");
    delay(1000);
  }
  Serial.printf("Ethernet connected, IP address: %s, Gateway: %s\n",
                eth.localIP().toString().c_str(),
                eth.gatewayIP().toString().c_str());
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
        doc["ntp"] = networkConfig.ntpServer;
        
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

              // Update NTP server
              strlcpy(networkConfig.ntpServer, doc["ntp"] | "pool.ntp.org", sizeof(networkConfig.ntpServer));

              // Save configuration to EEPROM
              saveNetworkConfig();

              // Send success response before applying changes
              server.send(200, "application/json", "{\"status\":\"success\",\"message\":\"Configuration saved\"}");

              // Apply new configuration after a short delay
              delay(100);
              rp2040.reboot(); // Use proper RP2040 reset function
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
            
            String response;
            serializeJson(doc, response);
            server.send(200, "application/json", response);
        } else {
            server.send(500, "application/json", "{\"error\": \"Failed to get current time\"}");
        } });

  server.on("/api/time", HTTP_POST, []()
            {
        StaticJsonDocument<200> doc;
        String json = server.arg("plain");
        DeserializationError error = deserializeJson(doc, json);
        
        if (error) {
            server.send(400, "application/json", "{\"error\": \"Invalid JSON\"}");
            return;
        }

        // Validate required fields
        if (!doc.containsKey("date") || !doc.containsKey("time")) {
            server.send(400, "application/json", "{\"error\": \"Missing required fields\"}");
            return;
        }

        // Parse date string (format: YYYY-MM-DD)
        const char* dateStr = doc["date"];
        uint16_t year;
        uint8_t month, day;
        if (sscanf(dateStr, "%hu-%hhu-%hhu", &year, &month, &day) != 3 ||
            year < 2000 || year > 2099 || month < 1 || month > 12 || day < 1 || day > 31) {
            server.send(400, "application/json", "{\"error\": \"Invalid date format or values\"}");
            return;
        }

        // Parse time string (format: HH:MM)
        const char* timeStr = doc["time"];
        uint8_t hour, minute;
        if (sscanf(timeStr, "%hhu:%hhu", &hour, &minute) != 2 ||
            hour > 23 || minute > 59) {
            server.send(400, "application/json", "{\"error\": \"Invalid time format or values\"}");
            return;
        }

        // Update timezone if provided
        if (doc.containsKey("timezone")) {
            const char* tz = doc["timezone"];
            // Basic timezone format validation (+/-HH:MM)
            int tzHour, tzMin;
            if (sscanf(tz, "%d:%d", &tzHour, &tzMin) != 2 ||
                tzHour < -12 || tzHour > 14 || tzMin < 0 || tzMin > 59) {
                server.send(400, "application/json", "{\"error\": \"Invalid timezone format\"}");
                return;
            }
            strncpy(networkConfig.timezone, tz, sizeof(networkConfig.timezone) - 1);
            networkConfig.timezone[sizeof(networkConfig.timezone) - 1] = '\0';
        }

        // Update NTP enabled status if provided
        if (doc.containsKey("ntpEnabled")) {
            networkConfig.ntpEnabled = doc["ntpEnabled"];
            saveNetworkConfig(); // Save to EEPROM when NTP settings change
        }

        // Only update time if NTP is disabled
        if (!networkConfig.ntpEnabled) {
            DateTime newDateTime = {year, month, day, hour, minute, 0};
            if (updateGlobalDateTime(newDateTime)) {
                server.send(200, "application/json", "{\"status\": \"success\"}");
            } else {
                server.send(500, "application/json", "{\"error\": \"Failed to update time\"}");
            }
        } else {
            server.send(200, "application/json", "{\"status\": \"success\", \"message\": \"NTP enabled, manual time update ignored\"}");
        } });
}

void setup()
{
  Serial.begin(115200);
  while (!Serial)
    ;

  // Initialize NTP update queue
  ntpUpdateQueue = xQueueCreate(1, sizeof(uint8_t));
  if (ntpUpdateQueue == NULL)
  {
    Serial.println("Failed to create NTP update queue");
    return;
  }

  // Initialize hardware
  setupEthernet();
  setupWebServer();
  setupNetworkAPI();
  setupTimeAPI();

  // Start NTP sync task if enabled
  if (networkConfig.ntpEnabled)
  {
    xTaskCreate(ntpSyncTask, "NTP Sync", 4096, NULL, 1, NULL);
    Serial.println("NTP sync task started");
  }

  Serial.println("Core 0 setup complete");
}

void loop()
{
  handleWebServer();
  handleNTPUpdates();  // Process any pending NTP updates
}

void setup1()
{
  while (!Serial)
    ;

  // Create synchronization primitives
  dateTimeMutex = xSemaphoreCreateMutex();
  // Initialize Core 1 tasks
  xTaskCreate(statusLEDs, "LED stat", configMINIMAL_STACK_SIZE, NULL, 1, NULL);
  Serial.println("LED status task started");
  xTaskCreate(manageRTC, "RTC updt", configMINIMAL_STACK_SIZE, NULL, 1, NULL);

  Serial.println("Core 1 setup complete");

  // Set System Status OK
  statusColours[LED_SYSTEM_STATUS] = LED_STATUS_OK;

  // Modbus not yet implemented
  statusColours[LED_MODBUS_STATUS] = LED_STATUS_OFF;

  // MQTT not yet implemented
  statusColours[LED_MQTT_STATUS] = LED_STATUS_OFF;

  Serial.printf("Set status colours to: %d, modbus: %d, mqtt: %d\n", statusColours[3], statusColours[2], statusColours[0]);
}

void loop1()
{
  checkTerminal();
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
  while (1) {
    for (int i = 0; i < 4; i++) {
      if (!(i == 3 && statusColours[i] == LED_STATUS_OK)) leds.setPixelColor(i, statusColours[i]);
    }
    leds.show();
    vTaskDelay(pdMS_TO_TICKS(ledRefreshInterval));
    loopCounter++;
    // Things to do every half second
    if (loopCounter >= loopCountsPerHalfSec) {
      loopCounter = 0;
      blinkState = !blinkState;
      if (blinkState) {
        leds.setPixelColor(3, LED_STATUS_OK);
        leds.show();
      } else {
        leds.setPixelColor(3, LED_COLOR_OFF);
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
    Serial.println("RTC initialization failed!");
    return;
  }

  // Set initial time (24-hour format)
  DateTime now;
  rtc.getDateTime(&now);
  memcpy(&globalDateTime, &now, sizeof(DateTime)); // Initialize global DateTime directly
  Serial.printf("Current date and time is: %04d-%02d-%02d %02d:%02d:%02d\n",
                now.year, now.month, now.day, now.hour, now.minute, now.second);

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

void osDebugPrint(HardwareSerial &serialDebug)
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
    Serial.printf("Time: %02d:%02d:%02d\n",
                  current.hour, current.minute, current.second);
  }

  TaskStatus_t *pxTaskStatusArray = new TaskStatus_t[numberOfTasks];
  uint32_t runtime;
  numberOfTasks = uxTaskGetSystemState(pxTaskStatusArray, numberOfTasks, &runtime);
  serialDebug.printf("Tasks: %d\n", numberOfTasks);
  for (int i = 0; i < numberOfTasks; i++)
  {
    serialDebug.printf("ID: %d %s", i, pxTaskStatusArray[i].pcTaskName);
    int currentState = pxTaskStatusArray[i].eCurrentState;
    serialDebug.printf(" Current state: %s", taskStateName[currentState]);
    serialDebug.printf(" Priority: %u\n", pxTaskStatusArray[i].uxBasePriority);
    serialDebug.printf(" Free stack: %u\n", pxTaskStatusArray[i].usStackHighWaterMark);
    serialDebug.printf(" Runtime: %u\n", pxTaskStatusArray[i].ulRunTimeCounter);
  }
  delete[] pxTaskStatusArray;
}

// ---------------------- Web Server Implementation ---------------------- //

void setupWebServer()
{
  // Initialize LittleFS for serving web files
  if (!LittleFS.begin())
  {
    Serial.println("LittleFS Mount Failed");
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
                "{\"temperature\":25.5,\"ph\":7.2,\"dissolvedOxygen\":6.8,\"timestamp\":\"%04d-%02d-%02dT%02d:%02d:%02d\"}",
                dt.year, dt.month, dt.day, dt.hour, dt.minute, dt.second);
        server.send(200, "application/json", json); });

  // Handle static files
  server.onNotFound([]()
                    { handleFile(server.uri().c_str()); });

  server.begin();
  Serial.println("HTTP server started");
  
  // Set Webserver Status LED
  statusColours[LED_WEBSERVER_STATUS] = LED_STATUS_OK;
}

void handleWebServer()
{
  if(eth.status() != WL_CONNECTED) {
    statusColours[LED_WEBSERVER_STATUS] = LED_STATUS_OFF;
    return;
  }
  server.handleClient();
  statusColours[LED_WEBSERVER_STATUS] = LED_STATUS_OK;
}

void handleRoot()
{
  handleFile("/index.html");
}

void handleFile(const char *path)
{
  if(eth.status() != WL_CONNECTED) {
    statusColours[LED_WEBSERVER_STATUS] = LED_STATUS_OFF;
    return;
  }
  statusColours[LED_WEBSERVER_STATUS] = LED_STATUS_BUSY;
  String contentType;
  if (strstr(path, ".html"))
    contentType = "text/html";
  else if (strstr(path, ".css"))
    contentType = "text/css";
  else if (strstr(path, ".js"))
    contentType = "application/javascript";
  else if (strstr(path, ".json"))
    contentType = "application/json";
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
  statusColours[LED_WEBSERVER_STATUS] = LED_STATUS_OK;
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
bool updateGlobalDateTime(const DateTime &dt)
{
  if (xSemaphoreTake(dateTimeMutex, pdMS_TO_TICKS(100)) == pdTRUE)
  {
    // Update the RTC first
    Serial.printf("Setting RTC to: %04d-%02d-%02d %02d:%02d:%02d\n",
                  dt.year, dt.month, dt.day, dt.hour, dt.minute, dt.second);

    rtc.setDateTime(dt); // Ignore return value since RTC appears to update anyway

    // Verify the time was set by reading it back
    DateTime currentTime;
    if (rtc.getDateTime(&currentTime))
    {
      Serial.printf("RTC read back: %04d-%02d-%02d %02d:%02d:%02d\n",
                    currentTime.year, currentTime.month, currentTime.day,
                    currentTime.hour, currentTime.minute, currentTime.second);
    }

    // Update the global variable
    memcpy(&globalDateTime, &dt, sizeof(DateTime));
    xSemaphoreGive(dateTimeMutex);

    // Signal that time was manually updated
    Serial.printf("Time successfully set to: %04d-%02d-%02d %02d:%02d:%02d\n",
                  dt.year, dt.month, dt.day, dt.hour, dt.minute, dt.second);
    return true; // Return true since we know the RTC actually updates
  }
  Serial.println("Failed to take dateTimeMutex in updateGlobalDateTime");
  return false;
}