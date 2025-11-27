#include "network.h"

// Global variables
NetworkConfig networkConfig;

// Wiznet5500lwIP eth(PIN_ETH_CS, SPI, PIN_ETH_IRQ);

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
}

void manageNetwork(void) {
  manageEthernet();
  if (networkConfig.ntpEnabled) {
    handleNTPUpdates(false);
  }
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
  StaticJsonDocument<1024> doc;
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
  // Optional fields
  doc["mqtt_device_prefix"] = networkConfig.mqttDevicePrefix;
  doc["mqtt_publish_interval_ms"] = networkConfig.mqttPublishIntervalMs;
  
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
        Serial.println("[WEB] /api/network GET request received");
        StaticJsonDocument<512> doc;
        doc["mode"] = networkConfig.useDHCP ? "dhcp" : "static";
        
        // Get current IP configuration
        IPAddress ip = eth.localIP();
        IPAddress subnet = eth.subnetMask();
        IPAddress gateway = eth.gatewayIP();
        IPAddress dns = eth.dnsIP();
        
        Serial.printf("[WEB] IP: %d.%d.%d.%d\n", ip[0], ip[1], ip[2], ip[3]);
        
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
        Serial.printf("[WEB] Sending /api/network response (%d bytes)\n", response.length());
        server.send(200, "application/json", response);
        Serial.println("[WEB] /api/network response sent successfully"); });

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

// --- New API Handlers for UI Dashboard ---

void handleGetAllStatus() {
  if (statusLocked) {
    server.send(503, "application/json", "{\"error\":\"Status temporarily unavailable\"}");
    return;
  }
  statusLocked = true;

  StaticJsonDocument<2048> doc; // Increased size for all data

  // System info
  doc["hostname"] = networkConfig.hostname;
  doc["mac"] = deviceMacAddress;

  // Internal Status
  JsonObject internal = doc.createNestedObject("internal");
  internal["psuOK"] = status.psuOK;
  internal["v20OK"] = status.V20OK;
  internal["v5OK"] = status.V5OK;
  internal["sdCardOK"] = status.sdCardOK;
  internal["ipcOK"] = status.ipcOK;
  internal["ipcConnected"] = status.ipcConnected;
  internal["ipcTimeout"] = status.ipcTimeout;
  internal["rtcOK"] = status.rtcOK;
  internal["mqttConnected"] = status.mqttConnected;

  // Sensor Readings
  JsonObject sensors = doc.createNestedObject("sensors");
  sensors["temperature"] = status.temperatureSensor.celcius;
  sensors["ph"] = status.phSensor.pH;
  sensors["do"] = status.doSensor.oxygen;
  // ... add other sensor values as needed ...

  // Control Setpoints
  JsonObject controls = doc.createNestedObject("controls");
  JsonObject tempControl = controls.createNestedObject("temperature");
  tempControl["setpoint"] = status.temperatureControl.sp_celcius;
  tempControl["enabled"] = status.temperatureControl.enabled;

  JsonObject phControl = controls.createNestedObject("ph");
  phControl["setpoint"] = status.phControl.sp_pH;
  phControl["enabled"] = status.phControl.enabled;
  // ... add other control values as needed ...

  statusLocked = false;

  String response;
  serializeJson(doc, response);
  server.send(200, "application/json", response);
}

void handleUpdateControl() {
  if (!server.hasArg("plain")) {
    server.send(400, "application/json", "{\"error\":\"No data received\"}");
    return;
  }

  StaticJsonDocument<256> doc;
  DeserializationError error = deserializeJson(doc, server.arg("plain"));
  if (error) {
    server.send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
    return;
  }

  const char* type = doc["type"];
  JsonObject config = doc["config"];

  if (!type || config.isNull()) {
    server.send(400, "application/json", "{\"error\":\"Invalid payload structure\"}");
    return;
  }

  bool success = false;
  if (strcmp(type, "temperature") == 0) {
    success = updateTemperatureControl(config);
  } else if (strcmp(type, "ph") == 0) {
    success = updatePhControl(config);
  }
  // ... add else-if for other control types as needed ...

  if (success) {
    server.send(200, "application/json", "{\"success\":true}");
  } else {
    server.send(500, "application/json", "{\"success\":false, \"error\":\"Failed to apply control update\"}");
  }
}

// --- Add this handler for /api/system/status ---
void handleSystemStatus() {
  // NOTE: This handler only READS from status/sdInfo structs, it doesn't write.
  // Therefore we don't need to acquire statusLocked - reads are safe without it.
  // This prevents 503 errors when other processes are briefly updating status.

  StaticJsonDocument<1024> doc;

  // Power info
  JsonObject power = doc.createNestedObject("power");
  power["mainVoltage"] = status.Vpsu;
  power["mainVoltageOK"] = status.psuOK;
  power["v20Voltage"] = status.V20;
  power["v20VoltageOK"] = status.V20OK;
  power["v5Voltage"] = status.V5;
  power["v5VoltageOK"] = status.V5OK;

  // RTC info
  JsonObject rtc = doc.createNestedObject("rtc");
  rtc["ok"] = status.rtcOK;
  rtc["time"] = getISO8601Timestamp(100);

  // Subsystem status
  doc["mqtt"] = status.mqttConnected;
  
  // IPC status with detailed state
  JsonObject ipc = doc.createNestedObject("ipc");
  ipc["ok"] = status.ipcOK;           // Legacy overall status
  ipc["connected"] = status.ipcConnected;
  ipc["timeout"] = status.ipcTimeout;
  
  // Modbus status with detailed state
  JsonObject modbus = doc.createNestedObject("modbus");
  modbus["configured"] = status.modbusConfigured;
  modbus["connected"] = status.modbusConnected;
  modbus["fault"] = status.modbusFault;

  // SD card info - read from cached sdInfo struct (updated by SD manager)
  JsonObject sd = doc.createNestedObject("sd");
  sd["inserted"] = sdInfo.inserted;
  sd["ready"] = sdInfo.ready;
  sd["capacityGB"] = sdInfo.cardSizeBytes * 0.000000001;
  sd["freeSpaceGB"] = sdInfo.cardFreeBytes * 0.000000001;
  sd["logFileSizeKB"] = sdInfo.logSizeBytes * 0.001;
  sd["sensorFileSizeKB"] = sdInfo.sensorSizeBytes * 0.001;

  String response;
  serializeJson(doc, response);
  server.send(200, "application/json", response);
}

// --- Sensors API Handler for Control Tab ---
void handleGetSensors() {
  if (statusLocked) {
    server.send(503, "application/json", "{\"error\":\"Status temporarily unavailable\"}");
    return;
  }
  statusLocked = true;

  StaticJsonDocument<1024> doc;

  // Current sensor readings from status struct
  doc["temp"] = status.temperatureSensor.celcius;
  doc["ph"] = status.phSensor.pH;
  doc["do"] = status.doSensor.oxygen;
  doc["stirrer"] = status.stirrerSpeedSensor.rpm;
  doc["pressure"] = status.pressureSensor.kPa;
  doc["gasFlow"] = status.gasFlowSensor.mlPerMinute;
  doc["weight"] = status.weightSensor.grams;
  doc["opticalDensity"] = status.odSensor.OD;
  
  // Power sensor readings
  doc["powerVolts"] = status.powerSensor.voltage;
  doc["powerAmps"] = status.powerSensor.current;
  doc["powerWatts"] = status.powerSensor.power;
  
  // Online status for each sensor
  doc["tempOnline"] = status.temperatureSensor.online;
  doc["phOnline"] = status.phSensor.online;
  doc["doOnline"] = status.doSensor.online;
  doc["stirrerOnline"] = status.stirrerSpeedSensor.online;
  doc["pressureOnline"] = status.pressureSensor.online;
  doc["gasFlowOnline"] = status.gasFlowSensor.online;
  doc["weightOnline"] = status.weightSensor.online;
  doc["odOnline"] = status.odSensor.online;
  doc["powerOnline"] = status.powerSensor.online;

  statusLocked = false;

  String response;
  serializeJson(doc, response);
  server.send(200, "application/json", response);
}

// --- Object Index API Handlers ---

void handleGetInputs() {
  // Compact JSON response for inputs tab
  // Data is served from cache, which is continuously updated by pollSensors()
  // Use heap allocation - 6KB is too large for stack
  DynamicJsonDocument* doc = new DynamicJsonDocument(6144);
  if (!doc) {
    server.send(500, "application/json", "{\"error\":\"Memory allocation failed\"}");
    return;
  }
  
  // Analog Inputs (ADC) - Indices 0-7
  JsonArray adc = doc->createNestedArray("adc");
  for (uint8_t i = 0; i < 8; i++) {
    ObjectCache::CachedObject* obj = objectCache.getObject(i);
    if (obj && obj->valid) {
      JsonObject o = adc.createNestedObject();
      o["i"] = i;  // Compact key names
      o["v"] = obj->value;
      o["n"] = ioConfig.adcInputs[i].name;  // Custom name from config
      // Ensure unit string is null-terminated and clean
      char cleanUnit[8];
      strncpy(cleanUnit, obj->unit, sizeof(cleanUnit) - 1);
      cleanUnit[sizeof(cleanUnit) - 1] = '\0';
      o["u"] = cleanUnit;
      o["d"] = ioConfig.adcInputs[i].showOnDashboard;  // Dashboard flag
      if (obj->flags & IPC_SENSOR_FLAG_FAULT) o["f"] = 1;
    }
  }
  
  // RTD Temperature Sensors - Indices 10-12
  JsonArray rtd = doc->createNestedArray("rtd");
  for (uint8_t i = 10; i < 13; i++) {
    ObjectCache::CachedObject* obj = objectCache.getObject(i);
    if (obj && obj->valid) {
      JsonObject o = rtd.createNestedObject();
      o["i"] = i;
      o["v"] = obj->value;
      o["n"] = ioConfig.rtdSensors[i - 10].name;  // Custom name from config
      // Ensure unit string is null-terminated and clean
      char cleanUnit[8];
      strncpy(cleanUnit, obj->unit, sizeof(cleanUnit) - 1);
      cleanUnit[sizeof(cleanUnit) - 1] = '\0';
      o["u"] = cleanUnit;
      o["d"] = ioConfig.rtdSensors[i - 10].showOnDashboard;  // Dashboard flag
      if (obj->flags & IPC_SENSOR_FLAG_FAULT) o["f"] = 1;
    }
  }
  
  // Digital GPIO - Indices 13-20
  JsonArray gpio = doc->createNestedArray("gpio");
  for (uint8_t i = 13; i < 21; i++) {
    ObjectCache::CachedObject* obj = objectCache.getObject(i);
    if (obj && obj->valid) {
      JsonObject o = gpio.createNestedObject();
      o["i"] = i;
      o["n"] = ioConfig.gpio[i - 13].name;  // Custom name from config
      o["s"] = (obj->value > 0.5) ? 1 : 0;  // State as boolean
      o["d"] = ioConfig.gpio[i - 13].showOnDashboard;  // Dashboard flag
      if (obj->flags & IPC_SENSOR_FLAG_FAULT) o["f"] = 1;
    }
  }
  
  // Energy Sensors - Indices 31-32
  JsonArray energy = doc->createNestedArray("energy");
  for (uint8_t i = 31; i < 33; i++) {
    ObjectCache::CachedObject* obj = objectCache.getObject(i);
    if (obj && obj->valid) {
      JsonObject o = energy.createNestedObject();
      o["i"] = i;
      o["n"] = ioConfig.energySensors[i - 31].name;  // Custom name from config
      o["v"] = obj->value;  // Voltage (V)
      // Multi-value data: current (A) and power (W)
      if (obj->valueCount >= 2) {
        o["c"] = obj->additionalValues[0];  // Current (A)
        o["p"] = obj->additionalValues[1];  // Power (W)
      } else {
        o["c"] = 0.0f;
        o["p"] = 0.0f;
      }
      o["d"] = ioConfig.energySensors[i - 31].showOnDashboard;  // Dashboard flag
      if (obj->flags & IPC_SENSOR_FLAG_FAULT) o["f"] = 1;
    }
  }
  
  // Dynamic Device Sensors - Indices 70-99
  // These are created by dynamic devices (pH probes, MFCs, etc.)
  JsonArray devices = doc->createNestedArray("devices");
  for (uint8_t i = 70; i <= 99; i++) {
    ObjectCache::CachedObject* obj = objectCache.getObject(i);
    if (obj && obj->valid) {
      JsonObject o = devices.createNestedObject();
      o["i"] = i;
      o["v"] = obj->value;
      
      // Use custom name from config if set, otherwise use IO MCU name
      uint8_t sensorIndex = i - 70;
      char cleanName[40];
      if (ioConfig.deviceSensors[sensorIndex].nameOverridden && 
          strlen(ioConfig.deviceSensors[sensorIndex].name) > 0) {
        // Use custom name from SYS MCU config
        strncpy(cleanName, ioConfig.deviceSensors[sensorIndex].name, sizeof(cleanName) - 1);
      } else {
        // Use default name from IO MCU
        strncpy(cleanName, obj->name, sizeof(cleanName) - 1);
      }
      cleanName[sizeof(cleanName) - 1] = '\0';
      o["n"] = cleanName;
      
      // Ensure unit string is null-terminated and clean
      char cleanUnit[8];
      strncpy(cleanUnit, obj->unit, sizeof(cleanUnit) - 1);
      cleanUnit[sizeof(cleanUnit) - 1] = '\0';
      o["u"] = cleanUnit;
      
      // Object type for display (pH, DO, Temperature, Flow, Pressure, etc.)
      o["t"] = obj->objectType;
      
      // Control index for devices that have both sensor and control objects
      // Devices with controls are typically at sensor indices 70-89, control indices 50-69
      if (i >= 70 && i < 90) {
        uint8_t controlIdx = i - 20;  // Control index = sensor index - 20
        o["c"] = controlIdx;  // Add control index for frontend use
      }
      
      // Show on dashboard flag from config
      o["d"] = ioConfig.deviceSensors[sensorIndex].showOnDashboard;
      
      if (obj->flags & IPC_SENSOR_FLAG_FAULT) o["f"] = 1;
    }
  }
  
  String response;
  serializeJson(*doc, response);
  
  // Check for overflow
  if (doc->overflowed()) {
    log(LOG_ERROR, true, "JSON document overflow in /api/inputs! Increase buffer size.\n");
  }
  
  // Debug: Log the JSON response size
  //log(LOG_DEBUG, false, "API /api/inputs response (%d bytes)\n", response.length());
  
  delete doc;  // Clean up heap allocation
  server.send(200, "application/json", response);
}

// --- ADC Configuration API Handlers ---

void handleGetADCConfig(uint8_t index) {
  if (index >= MAX_ADC_INPUTS) {
    server.send(400, "application/json", "{\"error\":\"Invalid ADC index\"}");
    return;
  }
  
  StaticJsonDocument<256> doc;
  doc["index"] = index;
  doc["name"] = ioConfig.adcInputs[index].name;
  doc["unit"] = ioConfig.adcInputs[index].unit;
  doc["enabled"] = ioConfig.adcInputs[index].enabled;
  doc["showOnDashboard"] = ioConfig.adcInputs[index].showOnDashboard;
  
  JsonObject cal = doc.createNestedObject("cal");
  cal["scale"] = ioConfig.adcInputs[index].cal.scale;
  cal["offset"] = ioConfig.adcInputs[index].cal.offset;
  
  String response;
  serializeJson(doc, response);
  server.send(200, "application/json", response);
}

void handleSaveADCConfig(uint8_t index) {
  log(LOG_DEBUG, false, "handleSaveADCConfig: START index=%d\n", index);
  
  if (index >= MAX_ADC_INPUTS) {
    server.send(400, "application/json", "{\"error\":\"Invalid ADC index\"}");
    return;
  }
  
  if (!server.hasArg("plain")) {
    server.send(400, "application/json", "{\"error\":\"No data received\"}");
    return;
  }
  
  log(LOG_DEBUG, false, "handleSaveADCConfig: Parsing JSON\n");
  StaticJsonDocument<256> doc;
  DeserializationError error = deserializeJson(doc, server.arg("plain"));
  
  if (error) {
    log(LOG_DEBUG, false, "handleSaveADCConfig: JSON parse error\n");
    server.send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
    return;
  }
  
  log(LOG_DEBUG, false, "handleSaveADCConfig: Updating config\n");
  
  // Update name
  if (doc.containsKey("name")) {
    strncpy(ioConfig.adcInputs[index].name, doc["name"], sizeof(ioConfig.adcInputs[index].name) - 1);
    ioConfig.adcInputs[index].name[sizeof(ioConfig.adcInputs[index].name) - 1] = '\0';
  }
  
  // Update unit
  if (doc.containsKey("unit")) {
    strncpy(ioConfig.adcInputs[index].unit, doc["unit"], sizeof(ioConfig.adcInputs[index].unit) - 1);
    ioConfig.adcInputs[index].unit[sizeof(ioConfig.adcInputs[index].unit) - 1] = '\0';
  }
  
  // Update calibration values
  if (doc.containsKey("cal")) {
    JsonObject cal = doc["cal"];
    if (cal.containsKey("scale")) {
      ioConfig.adcInputs[index].cal.scale = cal["scale"];
    }
    if (cal.containsKey("offset")) {
      ioConfig.adcInputs[index].cal.offset = cal["offset"];
    }
  }
  
  // Update showOnDashboard flag
  if (doc.containsKey("showOnDashboard")) {
    ioConfig.adcInputs[index].showOnDashboard = doc["showOnDashboard"];
  }
  
  log(LOG_DEBUG, false, "handleSaveADCConfig: Calling saveIOConfig\n");
  
  // Save configuration to flash
  saveIOConfig();
  
  log(LOG_DEBUG, false, "handleSaveADCConfig: saveIOConfig complete, preparing IPC\n");
  
  // Send updated calibration to IO MCU
  IPC_ConfigAnalogInput_t cfg;
  cfg.transactionId = generateTransactionId();
  cfg.index = index;
  strncpy(cfg.unit, ioConfig.adcInputs[index].unit, sizeof(cfg.unit) - 1);
  cfg.unit[sizeof(cfg.unit) - 1] = '\0';
  cfg.calScale = ioConfig.adcInputs[index].cal.scale;
  cfg.calOffset = ioConfig.adcInputs[index].cal.offset;
  
  log(LOG_DEBUG, false, "handleSaveADCConfig: Sending IPC packet\n");
  
  bool sent = ipc.sendPacket(IPC_MSG_CONFIG_ANALOG_INPUT, (uint8_t*)&cfg, sizeof(cfg));
  
  // Track transaction for ACK validation
  if (sent) {
    addPendingTransaction(cfg.transactionId, IPC_MSG_CONFIG_ANALOG_INPUT, IPC_MSG_CONTROL_ACK, 1, cfg.index);
  }
  
  if (sent) {
    log(LOG_INFO, false, "Updated ADC[%d] config: %s, unit=%s, scale=%.4f, offset=%.4f\n",
        index, ioConfig.adcInputs[index].name, cfg.unit, cfg.calScale, cfg.calOffset);
    log(LOG_DEBUG, false, "handleSaveADCConfig: Sending response\n");
    server.send(200, "application/json", "{\"success\":true}");
    log(LOG_DEBUG, false, "handleSaveADCConfig: COMPLETE\n");
  } else {
    log(LOG_WARNING, false, "Failed to send ADC[%d] config to IO MCU\n", index);
    server.send(500, "application/json", "{\"success\":false,\"error\":\"Failed to update IO MCU\"}");
  }
}

// --- DAC Configuration API Handlers ---

void handleGetDACConfig(uint8_t index) {
  if (index < 8 || index > 9) {
    server.send(400, "application/json", "{\"error\":\"Invalid DAC index\"}");
    return;
  }
  
  uint8_t dacIndex = index - 8;  // Convert to 0-1
  
  StaticJsonDocument<256> doc;
  doc["index"] = index;
  doc["name"] = ioConfig.dacOutputs[dacIndex].name;
  doc["unit"] = ioConfig.dacOutputs[dacIndex].unit;
  doc["enabled"] = ioConfig.dacOutputs[dacIndex].enabled;
  doc["showOnDashboard"] = ioConfig.dacOutputs[dacIndex].showOnDashboard;
  
  JsonObject cal = doc.createNestedObject("cal");
  cal["scale"] = ioConfig.dacOutputs[dacIndex].cal.scale;
  cal["offset"] = ioConfig.dacOutputs[dacIndex].cal.offset;
  
  String response;
  serializeJson(doc, response);
  server.send(200, "application/json", response);
}

void handleSaveDACConfig(uint8_t index) {
  log(LOG_DEBUG, false, "handleSaveDACConfig: START index=%d\n", index);
  
  if (index < 8 || index > 9) {
    server.send(400, "application/json", "{\"error\":\"Invalid DAC index\"}");
    return;
  }
  
  uint8_t dacIndex = index - 8;  // Convert to 0-1
  
  if (!server.hasArg("plain")) {
    server.send(400, "application/json", "{\"error\":\"No data received\"}");
    return;
  }
  
  log(LOG_DEBUG, false, "handleSaveDACConfig: Parsing JSON\n");
  StaticJsonDocument<256> doc;
  DeserializationError error = deserializeJson(doc, server.arg("plain"));
  
  if (error) {
    log(LOG_DEBUG, false, "handleSaveDACConfig: JSON parse error\n");
    server.send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
    return;
  }
  
  log(LOG_DEBUG, false, "handleSaveDACConfig: Updating config\n");
  
  // Update name
  if (doc.containsKey("name")) {
    strncpy(ioConfig.dacOutputs[dacIndex].name, doc["name"], sizeof(ioConfig.dacOutputs[dacIndex].name) - 1);
    ioConfig.dacOutputs[dacIndex].name[sizeof(ioConfig.dacOutputs[dacIndex].name) - 1] = '\0';
  }
  
  // Update calibration values
  if (doc.containsKey("cal")) {
    JsonObject cal = doc["cal"];
    if (cal.containsKey("scale")) {
      ioConfig.dacOutputs[dacIndex].cal.scale = cal["scale"];
    }
    if (cal.containsKey("offset")) {
      ioConfig.dacOutputs[dacIndex].cal.offset = cal["offset"];
    }
  }
  
  // Update showOnDashboard flag
  if (doc.containsKey("showOnDashboard")) {
    ioConfig.dacOutputs[dacIndex].showOnDashboard = doc["showOnDashboard"];
  }
  
  log(LOG_DEBUG, false, "handleSaveDACConfig: Calling saveIOConfig\n");
  
  // Save configuration to flash
  saveIOConfig();
  
  log(LOG_DEBUG, false, "handleSaveDACConfig: saveIOConfig complete, preparing IPC\n");
  
  // Send updated calibration to IO MCU
  IPC_ConfigAnalogOutput_t cfg;
  cfg.transactionId = generateTransactionId();
  cfg.index = index;
  strncpy(cfg.unit, ioConfig.dacOutputs[dacIndex].unit, sizeof(cfg.unit) - 1);
  cfg.unit[sizeof(cfg.unit) - 1] = '\0';
  cfg.calScale = ioConfig.dacOutputs[dacIndex].cal.scale;
  cfg.calOffset = ioConfig.dacOutputs[dacIndex].cal.offset;
  
  log(LOG_DEBUG, false, "handleSaveDACConfig: Sending IPC packet\n");
  
  bool sent = ipc.sendPacket(IPC_MSG_CONFIG_ANALOG_OUTPUT, (uint8_t*)&cfg, sizeof(cfg));
  
  // Track transaction for ACK validation
  if (sent) {
    addPendingTransaction(cfg.transactionId, IPC_MSG_CONFIG_ANALOG_OUTPUT, IPC_MSG_CONTROL_ACK, 1, cfg.index);
  }
  
  if (sent) {
    log(LOG_INFO, false, "Updated DAC[%d] config: %s, unit=%s, scale=%.4f, offset=%.4f\n",
        index, ioConfig.dacOutputs[dacIndex].name, cfg.unit, cfg.calScale, cfg.calOffset);
    log(LOG_DEBUG, false, "handleSaveDACConfig: Sending response\n");
    server.send(200, "application/json", "{\"success\":true}");
    log(LOG_DEBUG, false, "handleSaveDACConfig: COMPLETE\n");
  } else {
    log(LOG_WARNING, false, "Failed to send DAC[%d] config to IO MCU\n", index);
    server.send(500, "application/json", "{\"success\":false,\"error\":\"Failed to update IO MCU\"}");
  }
}

// --- RTD Configuration API Handlers ---

void handleGetRTDConfig(uint8_t index) {
  if (index < 10 || index >= 10 + MAX_RTD_SENSORS) {
    server.send(400, "application/json", "{\"error\":\"Invalid RTD index\"}");
    return;
  }
  
  uint8_t rtdIndex = index - 10;
  StaticJsonDocument<256> doc;
  doc["index"] = index;
  doc["name"] = ioConfig.rtdSensors[rtdIndex].name;
  doc["unit"] = ioConfig.rtdSensors[rtdIndex].unit;
  doc["wires"] = ioConfig.rtdSensors[rtdIndex].wireConfig;
  doc["type"] = ioConfig.rtdSensors[rtdIndex].nominalOhms;
  doc["showOnDashboard"] = ioConfig.rtdSensors[rtdIndex].showOnDashboard;
  
  JsonObject cal = doc.createNestedObject("cal");
  cal["scale"] = ioConfig.rtdSensors[rtdIndex].cal.scale;
  cal["offset"] = ioConfig.rtdSensors[rtdIndex].cal.offset;
  
  String response;
  serializeJson(doc, response);
  server.send(200, "application/json", response);
}

void handleSaveRTDConfig(uint8_t index) {
  log(LOG_DEBUG, false, "handleSaveRTDConfig: START index=%d\n", index);
  
  if (index < 10 || index >= 10 + MAX_RTD_SENSORS) {
    server.send(400, "application/json", "{\"error\":\"Invalid RTD index\"}");
    return;
  }
  
  if (!server.hasArg("plain")) {
    server.send(400, "application/json", "{\"error\":\"No data received\"}");
    return;
  }
  
  log(LOG_DEBUG, false, "handleSaveRTDConfig: Parsing JSON\n");
  StaticJsonDocument<256> doc;
  DeserializationError error = deserializeJson(doc, server.arg("plain"));
  
  if (error) {
    log(LOG_DEBUG, false, "handleSaveRTDConfig: JSON parse error\n");
    server.send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
    return;
  }
  
  log(LOG_DEBUG, false, "handleSaveRTDConfig: Updating config\n");
  
  uint8_t rtdIndex = index - 10;
  
  // Update name
  if (doc.containsKey("name")) {
    strncpy(ioConfig.rtdSensors[rtdIndex].name, doc["name"], sizeof(ioConfig.rtdSensors[rtdIndex].name) - 1);
    ioConfig.rtdSensors[rtdIndex].name[sizeof(ioConfig.rtdSensors[rtdIndex].name) - 1] = '\0';
  }
  
  // Update unit
  if (doc.containsKey("unit")) {
    strncpy(ioConfig.rtdSensors[rtdIndex].unit, doc["unit"], sizeof(ioConfig.rtdSensors[rtdIndex].unit) - 1);
    ioConfig.rtdSensors[rtdIndex].unit[sizeof(ioConfig.rtdSensors[rtdIndex].unit) - 1] = '\0';
  }
  
  // Update wire configuration
  if (doc.containsKey("wires")) {
    ioConfig.rtdSensors[rtdIndex].wireConfig = doc["wires"];
  }
  
  // Update RTD type
  if (doc.containsKey("type")) {
    ioConfig.rtdSensors[rtdIndex].nominalOhms = doc["type"];
  }
  
  // Update calibration values
  if (doc.containsKey("cal")) {
    JsonObject cal = doc["cal"];
    if (cal.containsKey("scale")) {
      ioConfig.rtdSensors[rtdIndex].cal.scale = cal["scale"];
    }
    if (cal.containsKey("offset")) {
      ioConfig.rtdSensors[rtdIndex].cal.offset = cal["offset"];
    }
  }
  
  // Update showOnDashboard flag
  if (doc.containsKey("showOnDashboard")) {
    ioConfig.rtdSensors[rtdIndex].showOnDashboard = doc["showOnDashboard"];
  }
  
  log(LOG_DEBUG, false, "handleSaveRTDConfig: Calling saveIOConfig\n");
  
  // Save configuration to flash
  saveIOConfig();
  
  log(LOG_DEBUG, false, "handleSaveRTDConfig: saveIOConfig complete, preparing IPC\n");
  
  // Send updated configuration to IO MCU
  IPC_ConfigRTD_t cfg;
  cfg.transactionId = generateTransactionId();
  cfg.index = index;
  strncpy(cfg.unit, ioConfig.rtdSensors[rtdIndex].unit, sizeof(cfg.unit) - 1);
  cfg.unit[sizeof(cfg.unit) - 1] = '\0';
  cfg.calScale = ioConfig.rtdSensors[rtdIndex].cal.scale;
  cfg.calOffset = ioConfig.rtdSensors[rtdIndex].cal.offset;
  cfg.wireConfig = ioConfig.rtdSensors[rtdIndex].wireConfig;
  cfg.nominalOhms = ioConfig.rtdSensors[rtdIndex].nominalOhms;
  
  log(LOG_DEBUG, false, "handleSaveRTDConfig: Sending IPC packet\n");
  
  bool sent = ipc.sendPacket(IPC_MSG_CONFIG_RTD, (uint8_t*)&cfg, sizeof(cfg));
  
  // Track transaction for ACK validation
  if (sent) {
    addPendingTransaction(cfg.transactionId, IPC_MSG_CONFIG_RTD, IPC_MSG_CONTROL_ACK, 1, cfg.index);
  }
  
  if (sent) {
    log(LOG_INFO, false, "Updated RTD[%d] config: %s, unit=%s, %d-wire PT%d, scale=%.4f, offset=%.4f\n",
        index, ioConfig.rtdSensors[rtdIndex].name, cfg.unit, cfg.wireConfig, cfg.nominalOhms, 
        cfg.calScale, cfg.calOffset);
    log(LOG_DEBUG, false, "handleSaveRTDConfig: Sending response\n");
    server.send(200, "application/json", "{\"success\":true}");
    log(LOG_DEBUG, false, "handleSaveRTDConfig: COMPLETE\n");
  } else {
    log(LOG_WARNING, false, "Failed to send RTD[%d] config to IO MCU\n", index);
    server.send(500, "application/json", "{\"success\":false,\"error\":\"Failed to update IO MCU\"}");
  }
}

// --- GPIO Configuration API Handlers ---

void handleGetGPIOConfig(uint8_t index) {
  // GPIO indices are 13-20, convert to array index 0-7
  if (index < 13 || index >= 13 + MAX_GPIO) {
    server.send(400, "application/json", "{\"error\":\"Invalid GPIO index\"}");
    return;
  }
  
  uint8_t gpioIndex = index - 13;
  
  StaticJsonDocument<256> doc;
  doc["index"] = index;
  doc["name"] = ioConfig.gpio[gpioIndex].name;
  doc["pullMode"] = (uint8_t)ioConfig.gpio[gpioIndex].pullMode;
  doc["enabled"] = ioConfig.gpio[gpioIndex].enabled;
  doc["showOnDashboard"] = ioConfig.gpio[gpioIndex].showOnDashboard;
  
  String response;
  serializeJson(doc, response);
  server.send(200, "application/json", response);
}

void handleSaveGPIOConfig(uint8_t index) {
  log(LOG_DEBUG, false, "handleSaveGPIOConfig: START index=%d\n", index);
  
  if (index < 13 || index >= 13 + MAX_GPIO) {
    server.send(400, "application/json", "{\"error\":\"Invalid GPIO index\"}");
    return;
  }
  
  if (!server.hasArg("plain")) {
    server.send(400, "application/json", "{\"error\":\"No data received\"}");
    return;
  }
  
  log(LOG_DEBUG, false, "handleSaveGPIOConfig: Parsing JSON\n");
  StaticJsonDocument<256> doc;
  DeserializationError error = deserializeJson(doc, server.arg("plain"));
  
  if (error) {
    log(LOG_DEBUG, false, "handleSaveGPIOConfig: JSON parse error\n");
    server.send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
    return;
  }
  
  log(LOG_DEBUG, false, "handleSaveGPIOConfig: Updating config\n");
  
  uint8_t gpioIndex = index - 13;
  
  // Update configuration from JSON
  if (doc.containsKey("name")) {
    strlcpy(ioConfig.gpio[gpioIndex].name, doc["name"] | "", 
            sizeof(ioConfig.gpio[gpioIndex].name));
  }
  
  if (doc.containsKey("pullMode")) {
    ioConfig.gpio[gpioIndex].pullMode = (GPIOPullMode)(doc["pullMode"] | GPIO_PULL_UP);
  }
  
  if (doc.containsKey("enabled")) {
    ioConfig.gpio[gpioIndex].enabled = doc["enabled"] | true;
  }
  
  // Update showOnDashboard flag
  if (doc.containsKey("showOnDashboard")) {
    ioConfig.gpio[gpioIndex].showOnDashboard = doc["showOnDashboard"];
  }
  
  log(LOG_DEBUG, false, "handleSaveGPIOConfig: Calling saveIOConfig\n");
  
  // Save configuration to flash
  saveIOConfig();
  
  log(LOG_DEBUG, false, "handleSaveGPIOConfig: saveIOConfig complete, preparing IPC\n");
  
  // Send updated configuration to IO MCU
  IPC_ConfigGPIO_t cfg;
  cfg.transactionId = generateTransactionId();
  cfg.index = index;
  strlcpy(cfg.name, ioConfig.gpio[gpioIndex].name, sizeof(cfg.name));
  cfg.pullMode = (uint8_t)ioConfig.gpio[gpioIndex].pullMode;
  cfg.enabled = ioConfig.gpio[gpioIndex].enabled;
  
  log(LOG_DEBUG, false, "handleSaveGPIOConfig: Sending IPC packet\n");
  
  bool sent = ipc.sendPacket(IPC_MSG_CONFIG_GPIO, (uint8_t*)&cfg, sizeof(cfg));
  
  // Track transaction for ACK validation
  if (sent) {
    addPendingTransaction(cfg.transactionId, IPC_MSG_CONFIG_GPIO, IPC_MSG_CONTROL_ACK, 1, cfg.index);
  }
  
  if (sent) {
    log(LOG_INFO, false, "Updated GPIO[%d] config: %s, pullMode=%d, enabled=%d\n",
        index, cfg.name, cfg.pullMode, cfg.enabled);
    log(LOG_DEBUG, false, "handleSaveGPIOConfig: Sending response\n");
    server.send(200, "application/json", "{\"success\":true}");
    log(LOG_DEBUG, false, "handleSaveGPIOConfig: COMPLETE\n");
  } else {
    log(LOG_WARNING, false, "Failed to send GPIO[%d] config to IO MCU\n", index);
    server.send(500, "application/json", "{\"success\":false,\"error\":\"Failed to update IO MCU\"}");
  }
}

// --- Energy Sensor Configuration API Handlers ---

void handleGetEnergySensorConfig(uint8_t index) {
  // Energy sensor indices are 31-32, convert to array index 0-1
  if (index < 31 || index >= 31 + MAX_ENERGY_SENSORS) {
    server.send(400, "application/json", "{\"error\":\"Invalid energy sensor index\"}");
    return;
  }
  
  uint8_t sensorIndex = index - 31;
  StaticJsonDocument<256> doc;
  doc["index"] = index;
  doc["name"] = ioConfig.energySensors[sensorIndex].name;
  doc["showOnDashboard"] = ioConfig.energySensors[sensorIndex].showOnDashboard;
  
  String response;
  serializeJson(doc, response);
  server.send(200, "application/json", response);
}

void handleSaveEnergySensorConfig(uint8_t index) {
  log(LOG_DEBUG, false, "handleSaveEnergySensorConfig: START index=%d\n", index);
  
  if (index < 31 || index >= 31 + MAX_ENERGY_SENSORS) {
    server.send(400, "application/json", "{\"error\":\"Invalid energy sensor index\"}");
    return;
  }
  
  if (!server.hasArg("plain")) {
    server.send(400, "application/json", "{\"error\":\"No data received\"}");
    return;
  }
  
  log(LOG_DEBUG, false, "handleSaveEnergySensorConfig: Parsing JSON\n");
  StaticJsonDocument<256> doc;
  DeserializationError error = deserializeJson(doc, server.arg("plain"));
  
  if (error) {
    log(LOG_DEBUG, false, "handleSaveEnergySensorConfig: JSON parse error\n");
    server.send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
    return;
  }
  
  log(LOG_DEBUG, false, "handleSaveEnergySensorConfig: Updating config\n");
  
  uint8_t sensorIndex = index - 31;
  
  // Update name
  if (doc.containsKey("name")) {
    strncpy(ioConfig.energySensors[sensorIndex].name, doc["name"], 
            sizeof(ioConfig.energySensors[sensorIndex].name) - 1);
    ioConfig.energySensors[sensorIndex].name[sizeof(ioConfig.energySensors[sensorIndex].name) - 1] = '\0';
  }
  
  // Update showOnDashboard flag
  if (doc.containsKey("showOnDashboard")) {
    ioConfig.energySensors[sensorIndex].showOnDashboard = doc["showOnDashboard"];
  }
  
  log(LOG_DEBUG, false, "handleSaveEnergySensorConfig: Calling saveIOConfig\n");
  
  // Save configuration to flash
  saveIOConfig();
  
  log(LOG_INFO, false, "Updated Energy Sensor[%d] config: %s, dashboard=%d\n",
      index, ioConfig.energySensors[sensorIndex].name, 
      ioConfig.energySensors[sensorIndex].showOnDashboard);
  
  server.send(200, "application/json", "{\"success\":true}");
  log(LOG_DEBUG, false, "handleSaveEnergySensorConfig: COMPLETE\n");
}

// --- Device Sensor Configuration API Handlers ---

void handleGetDeviceSensorConfig(uint8_t index) {
  // Device sensor indices are 70-99, convert to array index 0-29
  if (index < 70 || index >= 70 + MAX_DEVICE_SENSORS) {
    server.send(400, "application/json", "{\"error\":\"Invalid device sensor index\"}");
    return;
  }
  
  uint8_t sensorIndex = index - 70;
  
  StaticJsonDocument<256> doc;
  doc["index"] = index;
  doc["name"] = ioConfig.deviceSensors[sensorIndex].name;
  doc["showOnDashboard"] = ioConfig.deviceSensors[sensorIndex].showOnDashboard;
  doc["nameOverridden"] = ioConfig.deviceSensors[sensorIndex].nameOverridden;
  
  String response;
  serializeJson(doc, response);
  server.send(200, "application/json", response);
}

void handleSaveDeviceSensorConfig(uint8_t index) {
  log(LOG_DEBUG, false, "handleSaveDeviceSensorConfig: START index=%d\n", index);
  
  if (index < 60 || index >= 60 + MAX_DEVICE_SENSORS) {
    server.send(400, "application/json", "{\"error\":\"Invalid device sensor index\"}");
    return;
  }
  
  if (!server.hasArg("plain")) {
    server.send(400, "application/json", "{\"error\":\"No data received\"}");
    return;
  }
  
  log(LOG_DEBUG, false, "handleSaveDeviceSensorConfig: Parsing JSON\n");
  StaticJsonDocument<256> doc;
  DeserializationError error = deserializeJson(doc, server.arg("plain"));
  
  if (error) {
    log(LOG_DEBUG, false, "handleSaveDeviceSensorConfig: JSON parse error\n");
    server.send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
    return;
  }
  
  log(LOG_DEBUG, false, "handleSaveDeviceSensorConfig: Updating config\n");
  
  uint8_t sensorIndex = index - 70;
  
  // Update name
  if (doc.containsKey("name")) {
    strncpy(ioConfig.deviceSensors[sensorIndex].name, doc["name"], 
            sizeof(ioConfig.deviceSensors[sensorIndex].name) - 1);
    ioConfig.deviceSensors[sensorIndex].name[sizeof(ioConfig.deviceSensors[sensorIndex].name) - 1] = '\0';
    ioConfig.deviceSensors[sensorIndex].nameOverridden = (strlen(ioConfig.deviceSensors[sensorIndex].name) > 0);
  }
  
  // Update showOnDashboard flag
  if (doc.containsKey("showOnDashboard")) {
    ioConfig.deviceSensors[sensorIndex].showOnDashboard = doc["showOnDashboard"];
  }
  
  log(LOG_DEBUG, false, "handleSaveDeviceSensorConfig: Calling saveIOConfig\n");
  
  // Save configuration to flash
  saveIOConfig();
  
  log(LOG_INFO, false, "Updated device sensor[%d] config: name='%s', showOnDashboard=%d\n", 
      index, ioConfig.deviceSensors[sensorIndex].name,
      ioConfig.deviceSensors[sensorIndex].showOnDashboard);
  
  server.send(200, "application/json", "{\"success\":true}");
  log(LOG_DEBUG, false, "handleSaveDeviceSensorConfig: COMPLETE\n");
}

// --- COM Port Configuration API Handlers ---

void handleGetComPortConfig(uint8_t index) {
  if (index >= MAX_COM_PORTS) {
    server.send(400, "application/json", "{\"error\":\"Invalid COM port index\"}");
    return;
  }
  
  StaticJsonDocument<256> doc;
  doc["index"] = index;
  doc["name"] = ioConfig.comPorts[index].name;
  doc["baudRate"] = ioConfig.comPorts[index].baudRate;
  doc["dataBits"] = ioConfig.comPorts[index].dataBits;
  doc["stopBits"] = ioConfig.comPorts[index].stopBits;
  doc["parity"] = ioConfig.comPorts[index].parity;
  doc["showOnDashboard"] = ioConfig.comPorts[index].showOnDashboard;
  
  String response;
  serializeJson(doc, response);
  server.send(200, "application/json", response);
}

void handleSaveComPortConfig(uint8_t index) {
  log(LOG_DEBUG, false, "handleSaveComPortConfig: START index=%d\n", index);
  
  if (index >= MAX_COM_PORTS) {
    server.send(400, "application/json", "{\"error\":\"Invalid COM port index\"}");
    return;
  }
  
  if (!server.hasArg("plain")) {
    server.send(400, "application/json", "{\"error\":\"No data received\"}");
    return;
  }
  
  log(LOG_DEBUG, false, "handleSaveComPortConfig: Parsing JSON\n");
  StaticJsonDocument<256> doc;
  DeserializationError error = deserializeJson(doc, server.arg("plain"));
  
  if (error) {
    log(LOG_DEBUG, false, "handleSaveComPortConfig: JSON parse error\n");
    server.send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
    return;
  }
  
  log(LOG_DEBUG, false, "handleSaveComPortConfig: Updating config\n");
  
  // Update name
  if (doc.containsKey("name")) {
    strncpy(ioConfig.comPorts[index].name, doc["name"], sizeof(ioConfig.comPorts[index].name) - 1);
    ioConfig.comPorts[index].name[sizeof(ioConfig.comPorts[index].name) - 1] = '\0';
  }
  
  // Update baud rate
  if (doc.containsKey("baudRate")) {
    ioConfig.comPorts[index].baudRate = doc["baudRate"];
  }
  
  // Update data bits
  if (doc.containsKey("dataBits")) {
    ioConfig.comPorts[index].dataBits = doc["dataBits"];
  }
  
  // Update stop bits
  if (doc.containsKey("stopBits")) {
    ioConfig.comPorts[index].stopBits = doc["stopBits"];
  }
  
  // Update parity
  if (doc.containsKey("parity")) {
    ioConfig.comPorts[index].parity = doc["parity"];
  }
  
  // Update showOnDashboard flag
  if (doc.containsKey("showOnDashboard")) {
    ioConfig.comPorts[index].showOnDashboard = doc["showOnDashboard"];
  }
  
  log(LOG_DEBUG, false, "handleSaveComPortConfig: Calling saveIOConfig\n");
  
  // Save configuration to flash
  saveIOConfig();
  
  log(LOG_DEBUG, false, "handleSaveComPortConfig: saveIOConfig complete, preparing IPC\n");
  
  // Send updated config to IO MCU
  IPC_ConfigComPort_t cfg;
  cfg.transactionId = generateTransactionId();
  cfg.index = index;
  cfg.baudRate = ioConfig.comPorts[index].baudRate;
  cfg.dataBits = ioConfig.comPorts[index].dataBits;
  cfg.stopBits = ioConfig.comPorts[index].stopBits;
  cfg.parity = ioConfig.comPorts[index].parity;
  
  log(LOG_DEBUG, false, "handleSaveComPortConfig: Sending IPC packet\n");
  
  bool sent = ipc.sendPacket(IPC_MSG_CONFIG_COMPORT, (uint8_t*)&cfg, sizeof(cfg));
  
  // Track transaction for ACK validation
  if (sent) {
    addPendingTransaction(cfg.transactionId, IPC_MSG_CONFIG_COMPORT, IPC_MSG_CONTROL_ACK, 1, cfg.index);
  }
  
  if (sent) {
    log(LOG_INFO, false, "Updated COM port %d config: baud=%d, parity=%d, stop=%.1f\n",
        index, cfg.baudRate, cfg.parity, cfg.stopBits);
    log(LOG_DEBUG, false, "handleSaveComPortConfig: Sending response\n");
    server.send(200, "application/json", "{\"success\":true}");
    log(LOG_DEBUG, false, "handleSaveComPortConfig: COMPLETE\n");
  } else {
    log(LOG_WARNING, false, "Failed to send COM port %d config to IO MCU\n", index);
    server.send(500, "application/json", "{\"success\":false,\"error\":\"Failed to update IO MCU\"}");
  }
}

// --- COM Ports Status API Handler ---

void handleGetComPorts() {
  StaticJsonDocument<1024> doc;
  
  JsonArray ports = doc.createNestedArray("ports");
  for (int i = 0; i < MAX_COM_PORTS; i++) {
    JsonObject port = ports.createNestedObject();
    port["index"] = i;
    port["name"] = ioConfig.comPorts[i].name;
    port["baud"] = ioConfig.comPorts[i].baudRate;
    port["dataBits"] = ioConfig.comPorts[i].dataBits;
    port["parity"] = ioConfig.comPorts[i].parity;
    port["stopBits"] = ioConfig.comPorts[i].stopBits;
    port["d"] = ioConfig.comPorts[i].showOnDashboard;
    
    // Get runtime status from object cache (if implemented)
    // For now, check if port has communication errors
    // This will be populated from IO MCU status updates
    port["error"] = false;  // TODO: Get actual status from object cache
  }
  
  String response;
  serializeJson(doc, response);
  server.send(200, "application/json", response);
}

// ============================================================================
// DEVICES API HANDLERS
// ============================================================================

/**
 * @brief Helper function to convert DeviceConfig to IPC_DeviceConfig_t
 */
static void deviceConfigToIPC(const DeviceConfig* device, IPC_DeviceConfig_t* ipcConfig) {
  memset(ipcConfig, 0, sizeof(IPC_DeviceConfig_t));
  
  // Map driver type to IPC device type
  switch (device->driverType) {
    case DEVICE_DRIVER_HAMILTON_PH:  ipcConfig->deviceType = IPC_DEV_HAMILTON_PH; break;
    case DEVICE_DRIVER_HAMILTON_DO:  ipcConfig->deviceType = IPC_DEV_HAMILTON_DO; break;
    case DEVICE_DRIVER_HAMILTON_OD:  ipcConfig->deviceType = IPC_DEV_HAMILTON_OD; break;
    case DEVICE_DRIVER_ALICAT_MFC:   ipcConfig->deviceType = IPC_DEV_ALICAT_MFC; break;
    case DEVICE_DRIVER_PRESSURE_CONTROLLER: ipcConfig->deviceType = IPC_DEV_PRESSURE_CTRL; break;
    default: ipcConfig->deviceType = IPC_DEV_NONE; break;
  }
  
  // Map interface type to IPC bus type
  switch (device->interfaceType) {
    case DEVICE_INTERFACE_MODBUS_RTU:
      ipcConfig->busType = IPC_BUS_MODBUS_RTU;
      ipcConfig->busIndex = device->modbus.portIndex;
      ipcConfig->address = device->modbus.slaveID;
      break;
      
    case DEVICE_INTERFACE_ANALOGUE_IO:
      ipcConfig->busType = IPC_BUS_ANALOG;
      ipcConfig->busIndex = device->analogueIO.dacOutputIndex;
      ipcConfig->address = 0;  // Not used for analog
      break;
      
    case DEVICE_INTERFACE_MOTOR_DRIVEN:
      ipcConfig->busType = IPC_BUS_DIGITAL;
      ipcConfig->busIndex = device->motorDriven.motorIndex;
      ipcConfig->address = 0;  // Not used for motors
      break;
      
    default:
      ipcConfig->busType = IPC_BUS_NONE;
      break;
  }
  
  // Object count will be determined by IO MCU based on device type
  ipcConfig->objectCount = 0;
}

/**
 * @brief Get all configured devices
 */
void handleGetDevices() {
  DynamicJsonDocument doc(4096);  // Larger size for device list
  
  JsonArray devices = doc.createNestedArray("devices");
  
  for (int i = 0; i < MAX_DEVICES; i++) {
    if (!ioConfig.devices[i].isActive) continue;  // Skip inactive slots
    
    JsonObject device = devices.createNestedObject();
    device["dynamicIndex"] = ioConfig.devices[i].dynamicIndex;
    device["interfaceType"] = (uint8_t)ioConfig.devices[i].interfaceType;
    device["driverType"] = (uint8_t)ioConfig.devices[i].driverType;
    device["name"] = ioConfig.devices[i].name;
    
    // Get control object data from cache using centralized index calculation
    uint8_t controlIndex = getDeviceControlIndex(&ioConfig.devices[i]);
    ObjectCache::CachedObject* controlObj = objectCache.getObject(controlIndex);
    
    if (controlObj && controlObj->valid && controlObj->lastUpdate > 0) {
      // Device has valid control data
      device["connected"] = (controlObj->flags & IPC_SENSOR_FLAG_CONNECTED) ? true : false;
      device["fault"] = (controlObj->flags & IPC_SENSOR_FLAG_FAULT) ? true : false;
      device["setpoint"] = controlObj->value;  // Control object value is the setpoint
      device["unit"] = controlObj->unit;
      
      // Check if there's an actual value (might be in sensor object or additional values)
      if (controlObj->valueCount > 0) {
        device["actualValue"] = controlObj->additionalValues[0];
      } else {
        device["actualValue"] = controlObj->value;  // Fallback to same as setpoint
      }
      
      if (strlen(controlObj->message) > 0) {
        device["message"] = controlObj->message;
      }
    } else {
      // No control data available yet
      device["connected"] = false;
      device["fault"] = false;
      device["setpoint"] = 0.0f;
      device["actualValue"] = 0.0f;
      device["unit"] = "";
    }
    
    // Add interface-specific parameters
    if (ioConfig.devices[i].interfaceType == DEVICE_INTERFACE_MODBUS_RTU) {
      device["portIndex"] = ioConfig.devices[i].modbus.portIndex;
      device["slaveID"] = ioConfig.devices[i].modbus.slaveID;
    } else if (ioConfig.devices[i].interfaceType == DEVICE_INTERFACE_ANALOGUE_IO) {
      device["dacOutputIndex"] = ioConfig.devices[i].analogueIO.dacOutputIndex;
      device["unit"] = ioConfig.devices[i].analogueIO.unit;
      device["scale"] = ioConfig.devices[i].analogueIO.scale;
      device["offset"] = ioConfig.devices[i].analogueIO.offset;
    } else if (ioConfig.devices[i].interfaceType == DEVICE_INTERFACE_MOTOR_DRIVEN) {
      device["usesStepper"] = ioConfig.devices[i].motorDriven.usesStepper;
      device["motorIndex"] = ioConfig.devices[i].motorDriven.motorIndex;
    }
  }
  
  String response;
  serializeJson(doc, response);
  server.send(200, "application/json", response);
}

/**
 * @brief Get specific device by dynamic index
 */
void handleGetDevice() {
  // Extract index from URI: /api/devices/{index}
  String uri = server.uri();
  String indexStr = uri.substring(13);  // Skip "/api/devices/"
  
  // Remove any query parameters
  int queryPos = indexStr.indexOf('?');
  if (queryPos > 0) {
    indexStr = indexStr.substring(0, queryPos);
  }
  
  uint8_t dynamicIndex = indexStr.toInt();
  
  // Validate index range
  if (dynamicIndex < DYNAMIC_INDEX_START || dynamicIndex > DYNAMIC_INDEX_END) {
    server.send(400, "application/json", "{\"error\":\"Invalid device index\"}");
    return;
  }
  
  // Find device in config
  int8_t deviceIdx = findDeviceByIndex(dynamicIndex);
  if (deviceIdx < 0) {
    server.send(404, "application/json", "{\"error\":\"Device not found\"}");
    return;
  }
  
  // Build response
  StaticJsonDocument<512> doc;
  doc["dynamicIndex"] = ioConfig.devices[deviceIdx].dynamicIndex;
  doc["interfaceType"] = (uint8_t)ioConfig.devices[deviceIdx].interfaceType;
  doc["driverType"] = (uint8_t)ioConfig.devices[deviceIdx].driverType;
  doc["name"] = ioConfig.devices[deviceIdx].name;
  doc["online"] = false;  // TODO: Get actual status
  
  // Add interface-specific parameters
  if (ioConfig.devices[deviceIdx].interfaceType == DEVICE_INTERFACE_MODBUS_RTU) {
    doc["portIndex"] = ioConfig.devices[deviceIdx].modbus.portIndex;
    doc["slaveID"] = ioConfig.devices[deviceIdx].modbus.slaveID;
  } else if (ioConfig.devices[deviceIdx].interfaceType == DEVICE_INTERFACE_ANALOGUE_IO) {
    doc["dacOutputIndex"] = ioConfig.devices[deviceIdx].analogueIO.dacOutputIndex;
    doc["unit"] = ioConfig.devices[deviceIdx].analogueIO.unit;
    doc["scale"] = ioConfig.devices[deviceIdx].analogueIO.scale;
    doc["offset"] = ioConfig.devices[deviceIdx].analogueIO.offset;
  } else if (ioConfig.devices[deviceIdx].interfaceType == DEVICE_INTERFACE_MOTOR_DRIVEN) {
    doc["usesStepper"] = ioConfig.devices[deviceIdx].motorDriven.usesStepper;
    doc["motorIndex"] = ioConfig.devices[deviceIdx].motorDriven.motorIndex;
  }
  
  String response;
  serializeJson(doc, response);
  server.send(200, "application/json", response);
}

/**
 * @brief Create new device
 */
void handleCreateDevice() {
  // Parse JSON body
  DynamicJsonDocument doc(1024);
  DeserializationError error = deserializeJson(doc, server.arg("plain"));
  
  if (error) {
    server.send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
    return;
  }
  
  // Validate required fields
  if (!doc.containsKey("interfaceType") || !doc.containsKey("driverType") || !doc.containsKey("name")) {
    server.send(400, "application/json", "{\"error\":\"Missing required fields\"}");
    return;
  }
  
  uint8_t interfaceType = doc["interfaceType"];
  uint8_t driverType = doc["driverType"];
  String name = doc["name"].as<String>();
  
  // Validate name length
  if (name.length() == 0 || name.length() > 39) {
    server.send(400, "application/json", "{\"error\":\"Device name must be 1-39 characters\"}");
    return;
  }
  
  // Allocate dynamic index (reserves consecutive slots based on device type)
  int8_t dynamicIndex = allocateDynamicIndex((DeviceDriverType)driverType);
  if (dynamicIndex < 0) {
    server.send(400, "application/json", "{\"error\":\"No available consecutive device slots for this device type\"}");
    return;
  }
  
  // Find empty slot in device array
  int emptySlot = -1;
  for (int i = 0; i < MAX_DEVICES; i++) {
    if (!ioConfig.devices[i].isActive) {
      emptySlot = i;
      break;
    }
  }
  
  if (emptySlot < 0) {
    server.send(500, "application/json", "{\"error\":\"Internal error: no device slot available\"}");
    return;
  }
  
  // Configure device
  ioConfig.devices[emptySlot].isActive = true;
  ioConfig.devices[emptySlot].dynamicIndex = dynamicIndex;
  ioConfig.devices[emptySlot].interfaceType = (DeviceInterfaceType)interfaceType;
  ioConfig.devices[emptySlot].driverType = (DeviceDriverType)driverType;
  strncpy(ioConfig.devices[emptySlot].name, name.c_str(), sizeof(ioConfig.devices[emptySlot].name) - 1);
  ioConfig.devices[emptySlot].name[sizeof(ioConfig.devices[emptySlot].name) - 1] = '\0';
  
  // Set interface-specific parameters
  if (interfaceType == DEVICE_INTERFACE_MODBUS_RTU) {
    ioConfig.devices[emptySlot].modbus.portIndex = doc["portIndex"] | 0;
    ioConfig.devices[emptySlot].modbus.slaveID = doc["slaveID"] | 1;
  } else if (interfaceType == DEVICE_INTERFACE_ANALOGUE_IO) {
    ioConfig.devices[emptySlot].analogueIO.dacOutputIndex = doc["dacOutputIndex"] | 0;
    strncpy(ioConfig.devices[emptySlot].analogueIO.unit, doc["unit"] | "bar", 
            sizeof(ioConfig.devices[emptySlot].analogueIO.unit) - 1);
    ioConfig.devices[emptySlot].analogueIO.scale = doc["scale"] | 100.0f;
    ioConfig.devices[emptySlot].analogueIO.offset = doc["offset"] | 0.0f;
  } else if (interfaceType == DEVICE_INTERFACE_MOTOR_DRIVEN) {
    ioConfig.devices[emptySlot].motorDriven.usesStepper = doc["usesStepper"] | false;
    ioConfig.devices[emptySlot].motorDriven.motorIndex = doc["motorIndex"] | 27;
  }
  
  // Save configuration to LittleFS
  saveIOConfig();
  
  // Convert to IPC config and send to IO MCU
  IPC_DeviceConfig_t ipcConfig;
  deviceConfigToIPC(&ioConfig.devices[emptySlot], &ipcConfig);
  
  bool sent = sendDeviceCreateCommand(dynamicIndex, &ipcConfig);
  if (!sent) {
    log(LOG_WARNING, true, "Failed to send device create command to IO MCU\n");
    // Don't fail the request - config is saved, device will be created on next boot
  }
  
  log(LOG_INFO, true, "Device created: %s (index %d, driver %d)\n", 
      name.c_str(), dynamicIndex, driverType);
  
  // Send success response
  StaticJsonDocument<256> response;
  response["success"] = true;
  response["dynamicIndex"] = dynamicIndex;
  response["message"] = "Device created successfully";
  
  String responseStr;
  serializeJson(response, responseStr);
  server.send(201, "application/json", responseStr);
}

/**
 * @brief Delete device
 */
void handleDeleteDevice() {
  // Extract index from URI: /api/devices/{index}
  String uri = server.uri();
  String indexStr = uri.substring(13);  // Skip "/api/devices/"
  
  // Remove any query parameters
  int queryPos = indexStr.indexOf('?');
  if (queryPos > 0) {
    indexStr = indexStr.substring(0, queryPos);
  }
  
  uint8_t dynamicIndex = indexStr.toInt();
  
  // Validate index range
  if (dynamicIndex < DYNAMIC_INDEX_START || dynamicIndex > DYNAMIC_INDEX_END) {
    server.send(400, "application/json", "{\"error\":\"Invalid device index\"}");
    return;
  }
  
  // Find device in config
  int8_t deviceIdx = findDeviceByIndex(dynamicIndex);
  if (deviceIdx < 0) {
    server.send(404, "application/json", "{\"error\":\"Device not found\"}");
    return;
  }
  
  // Get device name for logging
  String deviceName = String(ioConfig.devices[deviceIdx].name);
  
  // Free the dynamic index and mark slot as inactive
  freeDynamicIndex(dynamicIndex);
  
  // Invalidate sensor cache entries for this device
  // Devices can have up to 4 sensor objects (e.g., pH probe has pH + temp)
  // Invalidate the maximum possible range to ensure all sensors are cleared
  objectCache.invalidateRange(dynamicIndex, 4);
  
  // Send delete command to IO MCU
  bool sent = sendDeviceDeleteCommand(dynamicIndex);
  if (!sent) {
    log(LOG_WARNING, true, "Failed to send device delete command to IO MCU\n");
  }
  
  // Save configuration
  saveIOConfig();
  
  log(LOG_INFO, true, "Device deleted: %s (index %d), cache invalidated\n", deviceName.c_str(), dynamicIndex);
  
  // Send success response
  server.send(200, "application/json", "{\"success\":true,\"message\":\"Device deleted successfully\"}");
}

/**
 * @brief Update device configuration
 */
void handleUpdateDevice() {
  // Extract index from URI: /api/devices/{index}
  String uri = server.uri();
  String indexStr = uri.substring(13);  // Skip "/api/devices/"
  
  // Remove any query parameters
  int queryPos = indexStr.indexOf('?');
  if (queryPos > 0) {
    indexStr = indexStr.substring(0, queryPos);
  }
  
  uint8_t dynamicIndex = indexStr.toInt();
  
  // Validate index range
  if (dynamicIndex < DYNAMIC_INDEX_START || dynamicIndex > DYNAMIC_INDEX_END) {
    server.send(400, "application/json", "{\"error\":\"Invalid device index\"}");
    return;
  }
  
  // Find device in config
  int8_t deviceIdx = findDeviceByIndex(dynamicIndex);
  if (deviceIdx < 0) {
    server.send(404, "application/json", "{\"error\":\"Device not found\"}");
    return;
  }
  
  // Parse request body
  String body = server.arg("plain");
  StaticJsonDocument<512> doc;
  DeserializationError error = deserializeJson(doc, body);
  
  if (error) {
    server.send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
    return;
  }
  
  // Update device name
  if (doc.containsKey("name")) {
    const char* name = doc["name"];
    if (strlen(name) == 0 || strlen(name) >= sizeof(ioConfig.devices[deviceIdx].name)) {
      server.send(400, "application/json", "{\"error\":\"Invalid device name\"}");
      return;
    }
    strlcpy(ioConfig.devices[deviceIdx].name, name, sizeof(ioConfig.devices[deviceIdx].name));
  }
  
  // Update interface-specific parameters based on interface type
  DeviceInterfaceType interfaceType = ioConfig.devices[deviceIdx].interfaceType;
  
  if (interfaceType == DEVICE_INTERFACE_MODBUS_RTU) {
    if (doc.containsKey("portIndex")) {
      uint8_t portIndex = doc["portIndex"];
      if (portIndex > 3) {
        server.send(400, "application/json", "{\"error\":\"Invalid port index\"}");
        return;
      }
      ioConfig.devices[deviceIdx].modbus.portIndex = portIndex;
    }
    
    if (doc.containsKey("slaveID")) {
      uint8_t slaveID = doc["slaveID"];
      if (slaveID < 1 || slaveID > 247) {
        server.send(400, "application/json", "{\"error\":\"Invalid slave ID\"}");
        return;
      }
      ioConfig.devices[deviceIdx].modbus.slaveID = slaveID;
    }
  } 
  else if (interfaceType == DEVICE_INTERFACE_ANALOGUE_IO) {
    if (doc.containsKey("dacOutputIndex")) {
      ioConfig.devices[deviceIdx].analogueIO.dacOutputIndex = doc["dacOutputIndex"];
    }
    if (doc.containsKey("unit")) {
      strlcpy(ioConfig.devices[deviceIdx].analogueIO.unit, doc["unit"], 
              sizeof(ioConfig.devices[deviceIdx].analogueIO.unit));
    }
    if (doc.containsKey("scale")) {
      ioConfig.devices[deviceIdx].analogueIO.scale = doc["scale"];
    }
    if (doc.containsKey("offset")) {
      ioConfig.devices[deviceIdx].analogueIO.offset = doc["offset"];
    }
  } 
  else if (interfaceType == DEVICE_INTERFACE_MOTOR_DRIVEN) {
    if (doc.containsKey("usesStepper")) {
      ioConfig.devices[deviceIdx].motorDriven.usesStepper = doc["usesStepper"];
    }
    if (doc.containsKey("motorIndex")) {
      uint8_t motorIndex = doc["motorIndex"];
      // Validate: 26 for stepper, 27-30 for DC motors
      if (motorIndex != 26 && (motorIndex < 27 || motorIndex > 30)) {
        server.send(400, "application/json", "{\"error\":\"Invalid motor index\"}");
        return;
      }
      ioConfig.devices[deviceIdx].motorDriven.motorIndex = motorIndex;
    }
  }
  
  // Save configuration
  saveIOConfig();

  // Convert to IPC config and send update to IO MCU
  IPC_DeviceConfig_t ipcConfig;
  deviceConfigToIPC(&ioConfig.devices[deviceIdx], &ipcConfig);

  bool sent = sendDeviceConfigCommand(dynamicIndex, &ipcConfig);
  if (!sent) {
    log(LOG_WARNING, true, "Failed to send device config update to IO MCU\n");
  }

  // If this is a pressure controller with analogue IO, also send calibration update
  if (ioConfig.devices[deviceIdx].driverType == DEVICE_DRIVER_PRESSURE_CONTROLLER &&
      ioConfig.devices[deviceIdx].interfaceType == DEVICE_INTERFACE_ANALOGUE_IO) {
    
    IPC_ConfigPressureCtrl_t calibCfg;
    calibCfg.controlIndex = getDeviceControlIndex(&ioConfig.devices[deviceIdx]);
    calibCfg.dacIndex = ioConfig.devices[deviceIdx].analogueIO.dacOutputIndex;
    strncpy(calibCfg.unit, ioConfig.devices[deviceIdx].analogueIO.unit, sizeof(calibCfg.unit) - 1);
    calibCfg.unit[sizeof(calibCfg.unit) - 1] = '\0';
    calibCfg.scale = ioConfig.devices[deviceIdx].analogueIO.scale;
    calibCfg.offset = ioConfig.devices[deviceIdx].analogueIO.offset;
    
    bool calibSent = ipc.sendPacket(IPC_MSG_CONFIG_PRESSURE_CTRL, (uint8_t*)&calibCfg, sizeof(calibCfg));
    if (calibSent) {
      log(LOG_INFO, false, "Sent pressure controller calibration update: scale=%.6f, offset=%.2f, unit=%s\n",
          calibCfg.scale, calibCfg.offset, calibCfg.unit);
    } else {
      log(LOG_WARNING, true, "Failed to send pressure controller calibration update\n");
    }
  }

  log(LOG_INFO, true, "Device updated: %s (index %d)\n", 
      ioConfig.devices[deviceIdx].name, dynamicIndex);

  // Send success response
  StaticJsonDocument<256> response;
  response["success"] = true;
  response["message"] = "Device updated successfully";
  response["dynamicIndex"] = dynamicIndex;
  
  String responseStr;
  serializeJson(response, responseStr);
  server.send(200, "application/json", responseStr);
}

// ============================================================================
// OUTPUTS API HANDLERS
// ============================================================================

/**
 * @brief Get all outputs status for monitoring
 */
void handleGetOutputs() {
  StaticJsonDocument<2048> doc;
  
  // DAC Analog Outputs (indices 8-9)
  JsonArray dacOutputs = doc.createNestedArray("dacOutputs");
  for (int i = 0; i < MAX_DAC_OUTPUTS; i++) {
    JsonObject dac = dacOutputs.createNestedObject();
    uint16_t index = 8 + i;
    dac["index"] = index;
    dac["name"] = ioConfig.dacOutputs[i].name;
    dac["unit"] = ioConfig.dacOutputs[i].unit;
    dac["d"] = ioConfig.dacOutputs[i].showOnDashboard;
    
    // Get actual runtime state from object cache
    ObjectCache::CachedObject* dacObj = objectCache.getObject(index);
    if (dacObj && dacObj->valid && dacObj->lastUpdate > 0) {
      dac["value"] = dacObj->value;  // Current output value in mV (0-10240)
    } else {
      dac["value"] = 0.0f;
    }
  }
  
  // Digital Outputs (indices 21-25)
  JsonArray digitalOutputs = doc.createNestedArray("digitalOutputs");
  for (int i = 0; i < MAX_DIGITAL_OUTPUTS; i++) {
    JsonObject output = digitalOutputs.createNestedObject();
    uint16_t index = 21 + i;
    output["index"] = index;
    output["name"] = ioConfig.digitalOutputs[i].name;
    output["mode"] = (uint8_t)ioConfig.digitalOutputs[i].mode;
    output["d"] = ioConfig.digitalOutputs[i].showOnDashboard;
    
    // Get actual runtime state from object cache
    ObjectCache::CachedObject* cachedObj = objectCache.getObject(index);
    if (cachedObj && cachedObj->valid && cachedObj->lastUpdate > 0) {
      output["value"] = cachedObj->value;  // PWM duty cycle or digital state (0-100%)
      output["state"] = cachedObj->value > 0;  // True if output is on
    } else {
      output["state"] = false;
      output["value"] = 0;
    }
  }
  
  // Stepper Motor (index 26)
  JsonObject stepper = doc.createNestedObject("stepperMotor");
  stepper["name"] = ioConfig.stepperMotor.name;
  stepper["d"] = ioConfig.stepperMotor.showOnDashboard;
  stepper["maxRPM"] = ioConfig.stepperMotor.maxRPM;
  
  // Get actual runtime state from object cache
  ObjectCache::CachedObject* stepperObj = objectCache.getObject(26);
  if (stepperObj && stepperObj->valid && stepperObj->lastUpdate > 0) {
    stepper["rpm"] = stepperObj->value;  // Current RPM
    stepper["running"] = (stepperObj->flags & IPC_SENSOR_FLAG_RUNNING) ? true : false;
    stepper["direction"] = (stepperObj->flags & IPC_SENSOR_FLAG_DIRECTION) ? true : false;
  } else {
    stepper["running"] = false;
    stepper["rpm"] = 0;
    stepper["direction"] = true;
  }
  
  // DC Motors (indices 27-30)
  JsonArray dcMotors = doc.createNestedArray("dcMotors");
  for (int i = 0; i < MAX_DC_MOTORS; i++) {
    JsonObject motor = dcMotors.createNestedObject();
    uint16_t index = 27 + i;
    motor["index"] = index;
    motor["name"] = ioConfig.dcMotors[i].name;
    motor["d"] = ioConfig.dcMotors[i].showOnDashboard;
    
    // Get actual runtime state from object cache
    ObjectCache::CachedObject* motorObj = objectCache.getObject(index);
    if (motorObj && motorObj->valid && motorObj->lastUpdate > 0) {
      motor["power"] = motorObj->value;  // Current power %
      motor["running"] = (motorObj->flags & IPC_SENSOR_FLAG_RUNNING) ? true : false;
      motor["direction"] = (motorObj->flags & IPC_SENSOR_FLAG_DIRECTION) ? true : false;
      
      // Add current reading from additional values (if available)
      if (motorObj->valueCount > 0) {
        motor["current"] = motorObj->additionalValues[0];  // Motor current in Amps
      } else {
        motor["current"] = 0.0f;
      }
    } else {
      motor["running"] = false;
      motor["power"] = 0;
      motor["direction"] = true;
      motor["current"] = 0.0f;
    }
  }
  
  String response;
  serializeJson(doc, response);
  server.send(200, "application/json", response);
}

// --- Digital Output Configuration Handlers ---

void handleGetDigitalOutputConfig(uint8_t index) {
  if (index < 21 || index >= 21 + MAX_DIGITAL_OUTPUTS) {
    server.send(400, "application/json", "{\"error\":\"Invalid output index\"}");
    return;
  }
  
  int outputIdx = index - 21;
  StaticJsonDocument<256> doc;
  doc["index"] = index;
  doc["name"] = ioConfig.digitalOutputs[outputIdx].name;
  doc["mode"] = (uint8_t)ioConfig.digitalOutputs[outputIdx].mode;
  doc["enabled"] = ioConfig.digitalOutputs[outputIdx].enabled;
  doc["showOnDashboard"] = ioConfig.digitalOutputs[outputIdx].showOnDashboard;
  
  String response;
  serializeJson(doc, response);
  server.send(200, "application/json", response);
}

void handleSaveDigitalOutputConfig(uint8_t index) {
  if (index < 21 || index >= 21 + MAX_DIGITAL_OUTPUTS) {
    server.send(400, "application/json", "{\"error\":\"Invalid output index\"}");
    return;
  }
  
  if (!server.hasArg("plain")) {
    server.send(400, "application/json", "{\"error\":\"No data provided\"}");
    return;
  }
  
  StaticJsonDocument<256> doc;
  DeserializationError error = deserializeJson(doc, server.arg("plain"));
  
  if (error) {
    server.send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
    return;
  }
  
  int outputIdx = index - 21;
  
  // Update configuration
  if (doc.containsKey("name")) {
    strlcpy(ioConfig.digitalOutputs[outputIdx].name, doc["name"] | "", 
            sizeof(ioConfig.digitalOutputs[outputIdx].name));
  }
  
  if (doc.containsKey("mode")) {
    ioConfig.digitalOutputs[outputIdx].mode = (OutputMode)(doc["mode"] | 0);
  }
  
  if (doc.containsKey("enabled")) {
    ioConfig.digitalOutputs[outputIdx].enabled = doc["enabled"] | true;
  }
  
  if (doc.containsKey("showOnDashboard")) {
    ioConfig.digitalOutputs[outputIdx].showOnDashboard = doc["showOnDashboard"] | false;
  }
  
  // Save configuration to file
  saveIOConfig();
  
  // Send updated config to IO MCU via IPC
  IPC_ConfigDigitalOutput_t cfg;
  cfg.index = index;
  strncpy(cfg.name, ioConfig.digitalOutputs[outputIdx].name, sizeof(cfg.name) - 1);
  cfg.name[sizeof(cfg.name) - 1] = '\0';
  cfg.mode = (uint8_t)ioConfig.digitalOutputs[outputIdx].mode;
  cfg.enabled = ioConfig.digitalOutputs[outputIdx].enabled;
  
  bool sent = ipc.sendPacket(IPC_MSG_CONFIG_DIGITAL_OUTPUT, (uint8_t*)&cfg, sizeof(cfg));
  
  if (sent) {
    log(LOG_INFO, false, "Pushed DigitalOutput[%d] config to IO MCU\n", index);
    server.send(200, "application/json", "{\"success\":true,\"message\":\"Config saved and pushed\"}");
  } else {
    log(LOG_WARNING, false, "Failed to push DigitalOutput[%d] config (queue full)\n", index);
    server.send(200, "application/json", "{\"success\":true,\"warning\":\"Saved but IPC queue full\"}");
  }
}

// --- Digital Output Runtime Control Handlers ---

void handleSetOutputState(uint8_t index) {
  if (index < 21 || index >= 21 + MAX_DIGITAL_OUTPUTS) {
    server.send(400, "application/json", "{\"error\":\"Invalid output index\"}");
    return;
  }
  
  if (!server.hasArg("plain")) {
    server.send(400, "application/json", "{\"error\":\"No data provided\"}");
    return;
  }
  
  StaticJsonDocument<128> doc;
  DeserializationError error = deserializeJson(doc, server.arg("plain"));
  
  if (error || !doc.containsKey("state")) {
    server.send(400, "application/json", "{\"error\":\"Invalid request\"}");
    return;
  }
  
  bool state = doc["state"];
  
  // Send SET_STATE command to IO MCU via IPC
  bool sent = sendDigitalOutputCommand(index, DOUT_CMD_SET_STATE, state, 0);
  
  if (sent) {
    log(LOG_INFO, false, "Set output %d state: %s\n", index, state ? "ON" : "OFF");
    server.send(200, "application/json", "{\"success\":true,\"message\":\"Command sent\"}");
  } else {
    log(LOG_WARNING, false, "Failed to set output %d: IPC queue full\n", index);
    server.send(503, "application/json", "{\"error\":\"IPC queue full, try again\"}");
  }
}

void handleSetOutputValue(uint8_t index) {
  if (index < 21 || index >= 21 + MAX_DIGITAL_OUTPUTS) {
    server.send(400, "application/json", "{\"error\":\"Invalid output index\"}");
    return;
  }
  
  if (!server.hasArg("plain")) {
    server.send(400, "application/json", "{\"error\":\"No data provided\"}");
    return;
  }
  
  StaticJsonDocument<128> doc;
  DeserializationError error = deserializeJson(doc, server.arg("plain"));
  
  if (error || !doc.containsKey("value")) {
    server.send(400, "application/json", "{\"error\":\"Invalid request\"}");
    return;
  }
  
  float value = doc["value"] | 0.0f;
  
  // Validate PWM value (0-100%)
  if (value < 0.0f || value > 100.0f) {
    server.send(400, "application/json", "{\"error\":\"Value must be 0-100%\"}");
    return;
  }
  
  // Send SET_PWM command to IO MCU via IPC
  bool sent = sendDigitalOutputCommand(index, DOUT_CMD_SET_PWM, false, value);
  
  if (sent) {
    log(LOG_INFO, false, "Set output %d PWM value: %.1f%%\n", index, value);
    server.send(200, "application/json", "{\"success\":true,\"message\":\"Command sent\"}");
  } else {
    log(LOG_WARNING, false, "Failed to set output %d PWM: IPC queue full\n", index);
    server.send(503, "application/json", "{\"error\":\"IPC queue full, try again\"}");
  }
}

// --- Analog Output (DAC) Control Handlers ---

void handleSetAnalogOutputValue(uint8_t index) {
  if (index < 8 || index > 9) {
    server.send(400, "application/json", "{\"error\":\"Invalid DAC index\"}");
    return;
  }
  
  if (!server.hasArg("plain")) {
    server.send(400, "application/json", "{\"error\":\"No data provided\"}");
    return;
  }
  
  StaticJsonDocument<128> doc;
  DeserializationError error = deserializeJson(doc, server.arg("plain"));
  
  if (error || !doc.containsKey("value")) {
    server.send(400, "application/json", "{\"error\":\"Invalid request\"}");
    return;
  }
  
  float value = doc["value"] | 0.0f;
  
  // Validate value range (0-10240 mV)
  if (value < 0.0f || value > 10240.0f) {
    server.send(400, "application/json", "{\"error\":\"Value must be 0-10240 mV\"}");
    return;
  }
  
  // Send SET_VALUE command to IO MCU via IPC
  bool sent = sendAnalogOutputCommand(index, AOUT_CMD_SET_VALUE, value);
  
  if (sent) {
    log(LOG_INFO, false, "Set DAC %d value: %.1f mV\n", index, value);
    server.send(200, "application/json", "{\"success\":true,\"message\":\"Command sent\"}");
  } else {
    log(LOG_WARNING, false, "Failed to set DAC %d: IPC queue full\n", index);
    server.send(503, "application/json", "{\"error\":\"IPC queue full, try again\"}");
  }
}

// --- Stepper Motor Configuration & Control Handlers ---

void handleGetStepperConfig() {
  StaticJsonDocument<512> doc;
  doc["name"] = ioConfig.stepperMotor.name;
  doc["stepsPerRev"] = ioConfig.stepperMotor.stepsPerRev;
  doc["maxRPM"] = ioConfig.stepperMotor.maxRPM;
  doc["holdCurrent_mA"] = ioConfig.stepperMotor.holdCurrent_mA;
  doc["runCurrent_mA"] = ioConfig.stepperMotor.runCurrent_mA;
  doc["acceleration"] = ioConfig.stepperMotor.acceleration;
  doc["invertDirection"] = ioConfig.stepperMotor.invertDirection;
  doc["enabled"] = ioConfig.stepperMotor.enabled;
  doc["showOnDashboard"] = ioConfig.stepperMotor.showOnDashboard;
  
  // Include TMC5130 advanced features
  doc["stealthChopEnabled"] = ioConfig.stepperMotor.stealthChopEnabled;
  doc["coolStepEnabled"] = ioConfig.stepperMotor.coolStepEnabled;
  doc["fullStepEnabled"] = ioConfig.stepperMotor.fullStepEnabled;
  doc["stealthChopMaxRPM"] = ioConfig.stepperMotor.stealthChopMaxRPM;
  doc["coolStepMinRPM"] = ioConfig.stepperMotor.coolStepMinRPM;
  doc["fullStepMinRPM"] = ioConfig.stepperMotor.fullStepMinRPM;
  
  String response;
  serializeJson(doc, response);
  server.send(200, "application/json", response);
}

void handleSaveStepperConfig() {
  if (!server.hasArg("plain")) {
    server.send(400, "application/json", "{\"error\":\"No data provided\"}");
    return;
  }
  
  StaticJsonDocument<512> doc;
  DeserializationError error = deserializeJson(doc, server.arg("plain"));
  
  if (error) {
    server.send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
    return;
  }
  
  // Update configuration
  if (doc.containsKey("name")) {
    strlcpy(ioConfig.stepperMotor.name, doc["name"] | "", 
            sizeof(ioConfig.stepperMotor.name));
  }
  
  if (doc.containsKey("stepsPerRev")) {
    ioConfig.stepperMotor.stepsPerRev = doc["stepsPerRev"] | 200;
  }
  
  if (doc.containsKey("maxRPM")) {
    ioConfig.stepperMotor.maxRPM = doc["maxRPM"] | 500;
  }
  
  if (doc.containsKey("holdCurrent_mA")) {
    ioConfig.stepperMotor.holdCurrent_mA = doc["holdCurrent_mA"] | 50;   // Safe default: 50mA
  }
  
  if (doc.containsKey("runCurrent_mA")) {
    ioConfig.stepperMotor.runCurrent_mA = doc["runCurrent_mA"] | 100;    // Safe default: 100mA
  }
  
  if (doc.containsKey("acceleration")) {
    ioConfig.stepperMotor.acceleration = doc["acceleration"] | 100;
  }
  
  if (doc.containsKey("invertDirection")) {
    ioConfig.stepperMotor.invertDirection = doc["invertDirection"] | false;
  }
  
  if (doc.containsKey("enabled")) {
    ioConfig.stepperMotor.enabled = doc["enabled"] | true;
  }
  
  if (doc.containsKey("showOnDashboard")) {
    ioConfig.stepperMotor.showOnDashboard = doc["showOnDashboard"] | false;
  }
  
  // Handle TMC5130 advanced features
  if (doc.containsKey("stealthChopEnabled")) {
    ioConfig.stepperMotor.stealthChopEnabled = doc["stealthChopEnabled"] | false;
  }
  
  if (doc.containsKey("coolStepEnabled")) {
    ioConfig.stepperMotor.coolStepEnabled = doc["coolStepEnabled"] | false;
  }
  
  if (doc.containsKey("fullStepEnabled")) {
    ioConfig.stepperMotor.fullStepEnabled = doc["fullStepEnabled"] | false;
  }
  
  if (doc.containsKey("stealthChopMaxRPM")) {
    ioConfig.stepperMotor.stealthChopMaxRPM = doc["stealthChopMaxRPM"] | 100.0;
  }
  
  if (doc.containsKey("coolStepMinRPM")) {
    ioConfig.stepperMotor.coolStepMinRPM = doc["coolStepMinRPM"] | 200.0;
  }
  
  if (doc.containsKey("fullStepMinRPM")) {
    ioConfig.stepperMotor.fullStepMinRPM = doc["fullStepMinRPM"] | 300.0;
  }
  
  // Validate configuration before saving
  // TMC5130 Constraints:
  // - holdCurrent: 1-1000 mA
  // - runCurrent: 1-1800 mA  
  // - maxRPM: reasonable upper limit (e.g., 5000 RPM)
  // - acceleration: <= maxRPM
  
  if (ioConfig.stepperMotor.holdCurrent_mA < 1 || ioConfig.stepperMotor.holdCurrent_mA > 1000) {
    log(LOG_WARNING, false, "Stepper hold current out of range: %d mA (valid: 1-1000 mA)\n", 
        ioConfig.stepperMotor.holdCurrent_mA);
    server.send(400, "application/json", 
                "{\"error\":\"Hold current must be 1-1000 mA\"}");
    return;
  }
  
  if (ioConfig.stepperMotor.runCurrent_mA < 1 || ioConfig.stepperMotor.runCurrent_mA > 1800) {
    log(LOG_WARNING, false, "Stepper run current out of range: %d mA (valid: 1-1800 mA)\n",
        ioConfig.stepperMotor.runCurrent_mA);
    server.send(400, "application/json", 
                "{\"error\":\"Run current must be 1-1800 mA\"}");
    return;
  }
  
  if (ioConfig.stepperMotor.maxRPM < 1 || ioConfig.stepperMotor.maxRPM > 3000) {
    log(LOG_WARNING, false, "Stepper max RPM out of range: %d (valid: 1-3000 RPM)\n",
        ioConfig.stepperMotor.maxRPM);
    return;
  }
  
  if (ioConfig.stepperMotor.acceleration < 1 || 
      ioConfig.stepperMotor.acceleration > ioConfig.stepperMotor.maxRPM) {
    log(LOG_WARNING, false, "Stepper acceleration out of range: %d (valid: 1-%d RPM/s)\n",
        ioConfig.stepperMotor.acceleration, ioConfig.stepperMotor.maxRPM);
    server.send(400, "application/json", 
                "{\"error\":\"Acceleration must be 1-maxRPM RPM/s\"}");
    return;
  }
  
  if (ioConfig.stepperMotor.stepsPerRev < 1 || ioConfig.stepperMotor.stepsPerRev > 10000) {
    log(LOG_WARNING, false, "Stepper steps/rev out of range: %d (valid: 1-10000)\n",
        ioConfig.stepperMotor.stepsPerRev);
    server.send(400, "application/json", 
                "{\"error\":\"Steps per revolution must be 1-10000\"}");
    return;
  }
  
  // Validate RPM thresholds: StealthChopMaxRPM < CoolStepMinRPM < FullStepMinRPM < MaxRPM
  if (ioConfig.stepperMotor.stealthChopMaxRPM >= ioConfig.stepperMotor.coolStepMinRPM) {
    log(LOG_WARNING, false, "Invalid RPM thresholds: stealthChopMaxRPM (%.1f) must be < coolStepMinRPM (%.1f)\n",
        ioConfig.stepperMotor.stealthChopMaxRPM, ioConfig.stepperMotor.coolStepMinRPM);
    server.send(400, "application/json", 
                "{\"error\":\"StealthChop Max RPM must be less than CoolStep Min RPM\"}");
    return;
  }
  
  if (ioConfig.stepperMotor.coolStepMinRPM >= ioConfig.stepperMotor.fullStepMinRPM) {
    log(LOG_WARNING, false, "Invalid RPM thresholds: coolStepMinRPM (%.1f) must be < fullStepMinRPM (%.1f)\n",
        ioConfig.stepperMotor.coolStepMinRPM, ioConfig.stepperMotor.fullStepMinRPM);
    server.send(400, "application/json", 
                "{\"error\":\"CoolStep Min RPM must be less than FullStep Min RPM\"}");
    return;
  }
  
  if (ioConfig.stepperMotor.fullStepMinRPM >= ioConfig.stepperMotor.maxRPM) {
    log(LOG_WARNING, false, "Invalid RPM thresholds: fullStepMinRPM (%.1f) must be < maxRPM (%.1f)\n",
        ioConfig.stepperMotor.fullStepMinRPM, (float)ioConfig.stepperMotor.maxRPM);
    server.send(400, "application/json", 
                "{\"error\":\"FullStep Min RPM must be less than Max RPM\"}");
    return;
  }
  
  // Save configuration to file
  saveIOConfig();
  
  // Send IPC config packet to IO MCU
  IPC_ConfigStepper_t cfg;
  cfg.index = 26;
  strncpy(cfg.name, ioConfig.stepperMotor.name, sizeof(cfg.name) - 1);
  cfg.name[sizeof(cfg.name) - 1] = '\0';
  cfg.stepsPerRev = ioConfig.stepperMotor.stepsPerRev;
  cfg.maxRPM = ioConfig.stepperMotor.maxRPM;
  cfg.holdCurrent_mA = ioConfig.stepperMotor.holdCurrent_mA;
  cfg.runCurrent_mA = ioConfig.stepperMotor.runCurrent_mA;
  cfg.acceleration = ioConfig.stepperMotor.acceleration;
  cfg.invertDirection = ioConfig.stepperMotor.invertDirection;
  cfg.enabled = ioConfig.stepperMotor.enabled;
  
  // Include TMC5130 advanced features in IPC packet
  cfg.stealthChopEnabled = ioConfig.stepperMotor.stealthChopEnabled;
  cfg.coolStepEnabled = ioConfig.stepperMotor.coolStepEnabled;
  cfg.fullStepEnabled = ioConfig.stepperMotor.fullStepEnabled;
  cfg.stealthChopMaxRPM = ioConfig.stepperMotor.stealthChopMaxRPM;
  cfg.coolStepMinRPM = ioConfig.stepperMotor.coolStepMinRPM;
  cfg.fullStepMinRPM = ioConfig.stepperMotor.fullStepMinRPM;
  
  log(LOG_DEBUG, false, "Sending stepper config: size=%d bytes (TYPE=0x%02X)\n", 
      sizeof(cfg), IPC_MSG_CONFIG_STEPPER);
  
  bool sent = ipc.sendPacket(IPC_MSG_CONFIG_STEPPER, (uint8_t*)&cfg, sizeof(cfg));
  
  if (sent) {
    log(LOG_INFO, false, "Pushed Stepper config to IO MCU\n");
    server.send(200, "application/json", "{\"success\":true,\"message\":\"Config saved and pushed\"}");
  } else {
    log(LOG_WARNING, false, "Failed to push Stepper config (queue full)\n");
    server.send(200, "application/json", "{\"success\":true,\"warning\":\"Saved but IPC queue full\"}");
  }
}

void handleSetStepperRPM() {
  if (!server.hasArg("plain")) {
    server.send(400, "application/json", "{\"error\":\"No data provided\"}");
    return;
  }
  
  StaticJsonDocument<128> doc;
  DeserializationError error = deserializeJson(doc, server.arg("plain"));
  
  if (error || !doc.containsKey("rpm")) {
    server.send(400, "application/json", "{\"error\":\"Invalid request\"}");
    return;
  }
  
  float rpm = doc["rpm"] | 0.0f;
  
  // Validate RPM
  if (rpm > ioConfig.stepperMotor.maxRPM) {
    server.send(400, "application/json", "{\"error\":\"RPM exceeds maximum\"}");
    return;
  }
  
  // Send SET_RPM command via IPC
  bool sent = sendStepperCommand(STEPPER_CMD_SET_RPM, rpm, true);
  
  if (sent) {
    log(LOG_INFO, false, "Set stepper RPM: %.1f\n", rpm);
    server.send(200, "application/json", "{\"success\":true,\"message\":\"Command sent\"}");
  } else {
    server.send(503, "application/json", "{\"error\":\"IPC queue full, try again\"}");
  }
}

void handleSetStepperDirection() {
  if (!server.hasArg("plain")) {
    server.send(400, "application/json", "{\"error\":\"No data provided\"}");
    return;
  }
  
  StaticJsonDocument<128> doc;
  DeserializationError error = deserializeJson(doc, server.arg("plain"));
  
  if (error || !doc.containsKey("forward")) {
    server.send(400, "application/json", "{\"error\":\"Invalid request\"}");
    return;
  }
  
  bool forward = doc["forward"];
  
  // Send SET_DIR command via IPC
  bool sent = sendStepperCommand(STEPPER_CMD_SET_DIR, 0, forward);
  
  if (sent) {
    log(LOG_INFO, false, "Set stepper direction: %s\n", forward ? "Forward" : "Reverse");
    server.send(200, "application/json", "{\"success\":true,\"message\":\"Command sent\"}");
  } else {
    server.send(503, "application/json", "{\"error\":\"IPC queue full, try again\"}");
  }
}

void handleStartStepper() {
  if (!server.hasArg("plain")) {
    server.send(400, "application/json", "{\"error\":\"No data provided\"}");
    return;
  }
  
  StaticJsonDocument<128> doc;
  DeserializationError error = deserializeJson(doc, server.arg("plain"));
  
  if (error) {
    server.send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
    return;
  }
  
  float rpm = doc["rpm"] | 0.0f;
  bool forward = doc["forward"] | true;
  
  // Validate RPM against configured max
  if (rpm > ioConfig.stepperMotor.maxRPM) {
    server.send(400, "application/json", "{\"error\":\"RPM exceeds maximum\"}");
    return;
  }
  
  // Check if motor is currently running from cache
  ObjectCache::CachedObject* stepperObj = objectCache.getObject(26);
  bool isRunning = false;
  if (stepperObj && stepperObj->valid) {
    isRunning = (stepperObj->flags & IPC_SENSOR_FLAG_RUNNING) ? true : false;
  }
  
  // If already running, use UPDATE command, otherwise START
  uint8_t command = isRunning ? STEPPER_CMD_UPDATE : STEPPER_CMD_START;
  bool sent = sendStepperCommand(command, rpm, forward);
  
  if (sent) {
    log(LOG_INFO, false, "%s stepper: RPM=%.1f, Direction=%s\n", 
        isRunning ? "Update" : "Start", rpm, forward ? "Forward" : "Reverse");
    server.send(200, "application/json", "{\"success\":true,\"message\":\"Command sent\"}");
  } else {
    log(LOG_WARNING, false, "Failed to %s stepper: IPC queue full\n", 
        isRunning ? "update" : "start");
    server.send(503, "application/json", "{\"error\":\"IPC queue full, try again\"}");
  }
}

void handleStopStepper() {
  // Send STOP command via IPC
  bool sent = sendStepperCommand(STEPPER_CMD_STOP, 0, false);
  
  if (sent) {
    log(LOG_INFO, false, "Stop stepper motor\n");
    server.send(200, "application/json", "{\"success\":true,\"message\":\"Command sent\"}");
  } else {
    log(LOG_WARNING, false, "Failed to stop stepper: IPC queue full\n");
    server.send(503, "application/json", "{\"error\":\"IPC queue full, try again\"}");
  }
}

// --- DC Motor Configuration & Control Handlers ---

void handleGetDCMotorConfig(uint8_t index) {
  if (index < 27 || index >= 27 + MAX_DC_MOTORS) {
    server.send(400, "application/json", "{\"error\":\"Invalid motor index\"}");
    return;
  }
  
  int motorIdx = index - 27;
  StaticJsonDocument<256> doc;
  doc["index"] = index;
  doc["name"] = ioConfig.dcMotors[motorIdx].name;
  doc["invertDirection"] = ioConfig.dcMotors[motorIdx].invertDirection;
  doc["enabled"] = ioConfig.dcMotors[motorIdx].enabled;
  doc["showOnDashboard"] = ioConfig.dcMotors[motorIdx].showOnDashboard;
  
  String response;
  serializeJson(doc, response);
  server.send(200, "application/json", response);
}

void handleSaveDCMotorConfig(uint8_t index) {
  if (index < 27 || index >= 27 + MAX_DC_MOTORS) {
    server.send(400, "application/json", "{\"error\":\"Invalid motor index\"}");
    return;
  }
  
  if (!server.hasArg("plain")) {
    server.send(400, "application/json", "{\"error\":\"No data provided\"}");
    return;
  }
  
  StaticJsonDocument<256> doc;
  DeserializationError error = deserializeJson(doc, server.arg("plain"));
  
  if (error) {
    server.send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
    return;
  }
  
  int motorIdx = index - 27;
  
  // Update configuration
  if (doc.containsKey("name")) {
    strlcpy(ioConfig.dcMotors[motorIdx].name, doc["name"] | "", 
            sizeof(ioConfig.dcMotors[motorIdx].name));
  }
  
  if (doc.containsKey("invertDirection")) {
    ioConfig.dcMotors[motorIdx].invertDirection = doc["invertDirection"] | false;
  }
  
  if (doc.containsKey("enabled")) {
    ioConfig.dcMotors[motorIdx].enabled = doc["enabled"] | true;
  }
  
  if (doc.containsKey("showOnDashboard")) {
    ioConfig.dcMotors[motorIdx].showOnDashboard = doc["showOnDashboard"] | false;
  }
  
  // Save configuration to file
  saveIOConfig();
  
  // Send updated config to IO MCU via IPC
  IPC_ConfigDCMotor_t cfg;
  cfg.index = index;
  strncpy(cfg.name, ioConfig.dcMotors[motorIdx].name, sizeof(cfg.name) - 1);
  cfg.name[sizeof(cfg.name) - 1] = '\0';
  cfg.invertDirection = ioConfig.dcMotors[motorIdx].invertDirection;
  cfg.enabled = ioConfig.dcMotors[motorIdx].enabled;
  
  bool sent = ipc.sendPacket(IPC_MSG_CONFIG_DCMOTOR, (uint8_t*)&cfg, sizeof(cfg));
  
  if (sent) {
    log(LOG_INFO, false, "Pushed DCMotor[%d] config to IO MCU\n", index);
    server.send(200, "application/json", "{\"success\":true,\"message\":\"Config saved and pushed\"}");
  } else {
    log(LOG_WARNING, false, "Failed to push DCMotor[%d] config (queue full)\n", index);
    server.send(200, "application/json", "{\"success\":true,\"warning\":\"Saved but IPC queue full\"}");
  }
}

void handleSetDCMotorPower(uint8_t index) {
  if (index < 27 || index >= 27 + MAX_DC_MOTORS) {
    server.send(400, "application/json", "{\"error\":\"Invalid motor index\"}");
    return;
  }
  
  if (!server.hasArg("plain")) {
    server.send(400, "application/json", "{\"error\":\"No data provided\"}");
    return;
  }
  
  StaticJsonDocument<128> doc;
  DeserializationError error = deserializeJson(doc, server.arg("plain"));
  
  if (error || !doc.containsKey("power")) {
    server.send(400, "application/json", "{\"error\":\"Invalid request\"}");
    return;
  }
  
  float power = doc["power"] | 0.0f;
  
  // Validate power (0-100%)
  if (power < 0.0f || power > 100.0f) {
    server.send(400, "application/json", "{\"error\":\"Power must be 0-100%\"}");
    return;
  }
  
  // Send SET_POWER command via IPC
  bool sent = sendDCMotorCommand(index, DCMOTOR_CMD_SET_POWER, power, true);
  
  if (sent) {
    log(LOG_INFO, false, "Set DC motor %d power: %.1f%%\n", index, power);
    server.send(200, "application/json", "{\"success\":true,\"message\":\"Command sent\"}");
  } else {
    server.send(503, "application/json", "{\"error\":\"IPC queue full, try again\"}");
  }
}

void handleSetDCMotorDirection(uint8_t index) {
  if (index < 27 || index >= 27 + MAX_DC_MOTORS) {
    server.send(400, "application/json", "{\"error\":\"Invalid motor index\"}");
    return;
  }
  
  if (!server.hasArg("plain")) {
    server.send(400, "application/json", "{\"error\":\"No data provided\"}");
    return;
  }
  
  StaticJsonDocument<128> doc;
  DeserializationError error = deserializeJson(doc, server.arg("plain"));
  
  if (error || !doc.containsKey("forward")) {
    server.send(400, "application/json", "{\"error\":\"Invalid request\"}");
    return;
  }
  
  bool forward = doc["forward"];
  
  // Send SET_DIR command via IPC
  bool sent = sendDCMotorCommand(index, DCMOTOR_CMD_SET_DIR, 0, forward);
  
  if (sent) {
    log(LOG_INFO, false, "Set DC motor %d direction: %s\n", 
        index, forward ? "Forward" : "Reverse");
    server.send(200, "application/json", "{\"success\":true,\"message\":\"Command sent\"}");
  } else {
    server.send(503, "application/json", "{\"error\":\"IPC queue full, try again\"}");
  }
}

void handleStartDCMotor(uint8_t index) {
  if (index < 27 || index >= 27 + MAX_DC_MOTORS) {
    server.send(400, "application/json", "{\"error\":\"Invalid motor index\"}");
    return;
  }
  
  if (!server.hasArg("plain")) {
    server.send(400, "application/json", "{\"error\":\"No data provided\"}");
    return;
  }
  
  StaticJsonDocument<128> doc;
  DeserializationError error = deserializeJson(doc, server.arg("plain"));
  
  if (error) {
    server.send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
    return;
  }
  
  float power = doc["power"] | 0.0f;
  bool forward = doc["forward"] | true;
  
  // Validate power
  if (power < 0.0f || power > 100.0f) {
    server.send(400, "application/json", "{\"error\":\"Power must be 0-100%\"}");
    return;
  }
  
  // Send START command via IPC
  bool sent = sendDCMotorCommand(index, DCMOTOR_CMD_START, power, forward);
  
  if (sent) {
    log(LOG_INFO, false, "Start DC motor %d: %.1f%%, %s\n", 
        index, power, forward ? "Forward" : "Reverse");
    server.send(200, "application/json", "{\"success\":true,\"message\":\"Command sent\"}");
  } else {
    log(LOG_WARNING, false, "Failed to start DC motor %d: IPC queue full\n", index);
    server.send(503, "application/json", "{\"error\":\"IPC queue full, try again\"}");
  }
}

void handleStopDCMotor(uint8_t index) {
  if (index < 27 || index >= 27 + MAX_DC_MOTORS) {
    server.send(400, "application/json", "{\"error\":\"Invalid motor index\"}");
    return;
  }
  
  // Send STOP command via IPC
  bool sent = sendDCMotorCommand(index, DCMOTOR_CMD_STOP, 0, false);
  
  if (sent) {
    log(LOG_INFO, false, "Stop DC motor %d\n", index);
    server.send(200, "application/json", "{\"success\":true,\"message\":\"Command sent\"}");
  } else {
    log(LOG_WARNING, false, "Failed to stop DC motor %d: IPC queue full\n", index);
    server.send(503, "application/json", "{\"error\":\"IPC queue full, try again\"}");
  }
}
// Controllers API (indices 40-49) - Phase 1
// ============================================================================

void handleGetControllers() {
  // Use DynamicJsonDocument for larger responses - 8KB supports up to 10+ controllers
  // Each controller can add 300-500 bytes of JSON data
  DynamicJsonDocument doc(8192);
  JsonArray controllers = doc.createNestedArray("controllers");
  
  for (int i = 0; i < MAX_TEMP_CONTROLLERS; i++) {    
    if (!ioConfig.tempControllers[i].isActive) continue;
    
    uint8_t index = 40 + i;
    JsonObject ctrl = controllers.createNestedObject();
    
    ctrl["index"] = index;
    ctrl["name"] = ioConfig.tempControllers[i].name;
    ctrl["showOnDashboard"] = ioConfig.tempControllers[i].showOnDashboard;
    ctrl["unit"] = ioConfig.tempControllers[i].unit;
    ctrl["setpoint"] = ioConfig.tempControllers[i].setpoint;
    ctrl["controlMethod"] = (uint8_t)ioConfig.tempControllers[i].controlMethod;
    ctrl["hysteresis"] = ioConfig.tempControllers[i].hysteresis;
    ctrl["kP"] = ioConfig.tempControllers[i].kP;
    ctrl["kI"] = ioConfig.tempControllers[i].kI;
    ctrl["kD"] = ioConfig.tempControllers[i].kD;
    
    // Get runtime values from object cache for controller status
    ObjectCache::CachedObject* obj = objectCache.getObject(index);
    bool enabled = false;
    
    if (obj && obj->valid && obj->lastUpdate > 0) {
      // Controller object exists - check if enabled
      enabled = (obj->flags & IPC_SENSOR_FLAG_RUNNING) ? true : false;
      ctrl["enabled"] = enabled;
      ctrl["fault"] = (obj->flags & IPC_SENSOR_FLAG_FAULT) ? true : false;
      ctrl["message"] = obj->message;
      ctrl["tuning"] = (obj->flags & 0x10) ? true : false;  // Bit 4 for autotune flag
      
      // Update in-memory config with runtime PID gains (may have been updated by autotune)
      // additionalValues: [0]=output%, [1]=kP, [2]=kI, [3]=kD
      if (obj->valueCount >= 4) {
        ioConfig.tempControllers[i].kP = obj->additionalValues[1];
        ioConfig.tempControllers[i].kI = obj->additionalValues[2];
        ioConfig.tempControllers[i].kD = obj->additionalValues[3];
      }
      
      if (enabled) {
        // Controller is running - use controller's process value and output
        ctrl["processValue"] = obj->value;  // Process value (temperature)
        ctrl["output"] = obj->valueCount > 0 ? obj->additionalValues[0] : 0.0f;  // Output %
      }
    }
    
    // If controller is disabled (or doesn't exist), read process value from source sensor
    if (!enabled) {
      uint8_t pvSourceIndex = ioConfig.tempControllers[i].pvSourceIndex;
      ObjectCache::CachedObject* sensorObj = objectCache.getObject(pvSourceIndex);
      
      if (sensorObj && sensorObj->valid && sensorObj->lastUpdate > 0) {
        ctrl["processValue"] = sensorObj->value;  // Read from configured sensor
      } else {
        ctrl["processValue"] = nullptr;
      }
      
      // Read output state/value from the output object cache
      uint8_t outputIndex = ioConfig.tempControllers[i].outputIndex;
      ObjectCache::CachedObject* outputObj = objectCache.getObject(outputIndex);
      
      if (outputObj && outputObj->valid && outputObj->lastUpdate > 0) {
        ctrl["output"] = outputObj->value;  // Output state (0/1 for digital, 0-100 for PWM/DAC)
      } else {
        ctrl["output"] = nullptr;
      }
    }
  }
  
  if (ioConfig.phController.isActive) {
    uint8_t index = 43;
    JsonObject ctrl = controllers.createNestedObject();
    
    ctrl["index"] = index;
    ctrl["name"] = ioConfig.phController.name;
    ctrl["showOnDashboard"] = ioConfig.phController.showOnDashboard;
    ctrl["unit"] = "pH";  // pH units
    ctrl["setpoint"] = ioConfig.phController.setpoint;
    ctrl["controlMethod"] = 2;  // Use 2 for pH controller type
    ctrl["deadband"] = ioConfig.phController.deadband;
    
    // Dosing configuration info
    ctrl["acidEnabled"] = ioConfig.phController.acidDosing.enabled;
    ctrl["alkalineEnabled"] = ioConfig.phController.alkalineDosing.enabled;
    ctrl["acidOutputType"] = ioConfig.phController.acidDosing.outputType;
    ctrl["alkalineOutputType"] = ioConfig.phController.alkalineDosing.outputType;
    ctrl["acidDosingTime_ms"] = ioConfig.phController.acidDosing.dosingTime_ms;
    ctrl["alkalineDosingTime_ms"] = ioConfig.phController.alkalineDosing.dosingTime_ms;
    ctrl["acidMfcFlowRate_mL_min"] = ioConfig.phController.acidDosing.mfcFlowRate_mL_min;
    ctrl["alkalineMfcFlowRate_mL_min"] = ioConfig.phController.alkalineDosing.mfcFlowRate_mL_min;
    ctrl["acidVolumePerDose_mL"] = ioConfig.phController.acidDosing.volumePerDose_mL;
    ctrl["alkalineVolumePerDose_mL"] = ioConfig.phController.alkalineDosing.volumePerDose_mL;
    
    // Get runtime values from object cache for controller status
    ObjectCache::CachedObject* obj = objectCache.getObject(index);
    bool enabled = false;
    
    if (obj && obj->valid && obj->lastUpdate > 0) {
      // Controller object exists - check if enabled
      enabled = (obj->flags & IPC_SENSOR_FLAG_RUNNING) ? true : false;
      ctrl["enabled"] = enabled;
      ctrl["fault"] = (obj->flags & IPC_SENSOR_FLAG_FAULT) ? true : false;
      ctrl["message"] = obj->message;
      
      if (enabled) {
        // Controller is running - use controller's process value and output
        ctrl["processValue"] = obj->value;  // Process value (pH)
        // additionalValues: [0]=output, [1]=acidVol, [2]=alkalineVol
        ctrl["output"] = obj->valueCount > 0 ? obj->additionalValues[0] : 0.0f;  // Dosing state (0=off, 1=acid, 2=alkaline)
        ctrl["acidVolumeTotal_mL"] = obj->valueCount > 1 ? obj->additionalValues[1] : 0.0f;
        ctrl["alkalineVolumeTotal_mL"] = obj->valueCount > 2 ? obj->additionalValues[2] : 0.0f;
      }
    }
    
    // If controller is disabled (or doesn't exist), read process value from source sensor
    if (!enabled) {
      uint8_t pvSourceIndex = ioConfig.phController.pvSourceIndex;
      ObjectCache::CachedObject* sensorObj = objectCache.getObject(pvSourceIndex);
      
      if (sensorObj && sensorObj->valid && sensorObj->lastUpdate > 0) {
        ctrl["processValue"] = sensorObj->value;  // Read from configured pH sensor
      } else {
        ctrl["processValue"] = nullptr;
      }
      
      ctrl["output"] = 0;  // No dosing when disabled
      // Still show cumulative volumes even when disabled (they persist across enable/disable)
      ctrl["acidVolumeTotal_mL"] = obj->valueCount > 1 ? obj->additionalValues[1] : 0.0f;
      ctrl["alkalineVolumeTotal_mL"] = obj->valueCount > 2 ? obj->additionalValues[2] : 0.0f;
    }
  }
  
  // Flow Controllers (indices 44-47)
  for (int i = 0; i < MAX_FLOW_CONTROLLERS; i++) {
    if (!ioConfig.flowControllers[i].isActive) continue;
    
    uint8_t index = 44 + i;
    JsonObject ctrl = controllers.createNestedObject();
    
    ctrl["index"] = index;
    ctrl["name"] = ioConfig.flowControllers[i].name;
    ctrl["showOnDashboard"] = ioConfig.flowControllers[i].showOnDashboard;
    ctrl["unit"] = "mL/min";  // Flow rate unit
    ctrl["setpoint"] = ioConfig.flowControllers[i].flowRate_mL_min;  // Flow rate setpoint
    ctrl["controlMethod"] = 3;  // Use 3 for flow controller type
    
    // Flow configuration info
    ctrl["outputType"] = ioConfig.flowControllers[i].outputType;
    ctrl["outputIndex"] = ioConfig.flowControllers[i].outputIndex;
    ctrl["motorPower"] = ioConfig.flowControllers[i].motorPower;
    ctrl["calibrationVolume_mL"] = ioConfig.flowControllers[i].calibrationVolume_mL;
    ctrl["calibrationDoseTime_ms"] = ioConfig.flowControllers[i].calibrationDoseTime_ms;
    
    // Get runtime values from object cache for controller status
    ObjectCache::CachedObject* obj = objectCache.getObject(index);
    bool enabled = false;
    
    if (obj && obj->valid && obj->lastUpdate > 0) {
      // Controller object exists - check if enabled
      enabled = (obj->flags & IPC_SENSOR_FLAG_RUNNING) ? true : false;
      ctrl["enabled"] = enabled;
      ctrl["fault"] = (obj->flags & IPC_SENSOR_FLAG_FAULT) ? true : false;
      ctrl["message"] = obj->message;
      
      // Always report values regardless of enabled state
      ctrl["processValue"] = obj->value;  // Current flow rate (mL/min)
      // additionalValues: [0]=currentOutput (0=off, 1=dosing), [1]=calculatedInterval_ms, [2]=cumulativeVolume_mL
      ctrl["output"] = obj->valueCount > 0 ? obj->additionalValues[0] : 0.0f;  // Dosing state
      ctrl["dosingInterval_ms"] = obj->valueCount > 1 ? obj->additionalValues[1] : 0.0f;
      ctrl["cumulativeVolume_mL"] = obj->valueCount > 2 ? obj->additionalValues[2] : 0.0f;
    } else {
      // Controller doesn't exist yet or no data
      ctrl["enabled"] = false;
      ctrl["processValue"] = 0.0f;
      ctrl["output"] = 0.0f;
      ctrl["dosingInterval_ms"] = 0.0f;
      ctrl["cumulativeVolume_mL"] = 0.0f;
    }
  }
  
  // DO Controller (index 48)
  if (ioConfig.doController.isActive) {
    uint8_t index = 48;
    JsonObject ctrl = controllers.createNestedObject();
    
    ctrl["index"] = index;
    ctrl["name"] = ioConfig.doController.name;
    ctrl["showOnDashboard"] = ioConfig.doController.showOnDashboard;
    ctrl["unit"] = "mg/L";  // Dissolved oxygen unit
    ctrl["setpoint"] = ioConfig.doController.setpoint_mg_L;
    ctrl["controlMethod"] = 4;  // Use 4 for DO controller type
    
    // DO configuration info
    ctrl["activeProfileIndex"] = ioConfig.doController.activeProfileIndex;
    if (ioConfig.doController.activeProfileIndex < MAX_DO_PROFILES) {
      ctrl["activeProfileName"] = ioConfig.doProfiles[ioConfig.doController.activeProfileIndex].name;
    } else {
      ctrl["activeProfileName"] = "None";
    }
    ctrl["stirrerEnabled"] = ioConfig.doController.stirrerEnabled;
    ctrl["stirrerType"] = ioConfig.doController.stirrerType;
    ctrl["stirrerIndex"] = ioConfig.doController.stirrerIndex;
    ctrl["stirrerMaxRPM"] = ioConfig.doController.stirrerMaxRPM;
    ctrl["mfcEnabled"] = ioConfig.doController.mfcEnabled;
    ctrl["mfcDeviceIndex"] = ioConfig.doController.mfcDeviceIndex;
    
    // Get runtime values from object cache for controller status
    ObjectCache::CachedObject* obj = objectCache.getObject(index);
    bool enabled = false;
    
    if (obj && obj->valid && obj->lastUpdate > 0) {
      // Controller object exists - read all values from object cache
      ctrl["enabled"] = (obj->flags & IPC_SENSOR_FLAG_RUNNING) ? true : false;
      ctrl["fault"] = (obj->flags & IPC_SENSOR_FLAG_FAULT) ? true : false;
      ctrl["message"] = obj->message;
      
      // Always read values from object cache when it exists
      ctrl["processValue"] = obj->value;  // Process value (DO in mg/L)
      // additionalValues: [0]=stirrerOutput, [1]=mfcOutput, [2]=error, [3]=setpoint
      ctrl["stirrerOutput"] = obj->valueCount > 0 ? obj->additionalValues[0] : 0.0f;
      ctrl["mfcOutput"] = obj->valueCount > 1 ? obj->additionalValues[1] : 0.0f;
      ctrl["error"] = obj->valueCount > 2 ? obj->additionalValues[2] : 0.0f;
      float runtimeSetpoint = obj->valueCount > 3 ? obj->additionalValues[3] : ioConfig.doController.setpoint_mg_L;
      ctrl["setpoint"] = runtimeSetpoint;  // Use runtime setpoint (may have been updated)
      ctrl["output"] = ctrl["error"];  // Use error as primary output for card display
      
      // Stirrer unit based on motor type (0=DC %, 1=Stepper RPM)
      ctrl["stirrerUnit"] = (ioConfig.doController.stirrerType == 0) ? "%" : "RPM";
    } else {
      // Controller doesn't exist yet or no data - scan for DO sensor
      ctrl["enabled"] = false;
      ctrl["fault"] = false;
      ctrl["message"] = "";
      ctrl["output"] = 0.0f;
      ctrl["stirrerOutput"] = 0.0f;
      ctrl["mfcOutput"] = 0.0f;
      ctrl["stirrerUnit"] = (ioConfig.doController.stirrerType == 0) ? "%" : "RPM";
      
      // Try to find DO sensor in object cache to show process value
      bool foundDOSensor = false;
      for (uint8_t i = 0; i < 100; i++) {
        ObjectCache::CachedObject* sensorObj = objectCache.getObject(i);
        if (sensorObj && sensorObj->valid && sensorObj->objectType == OBJ_T_DISSOLVED_OXYGEN_SENSOR && sensorObj->lastUpdate > 0) {
          ctrl["processValue"] = sensorObj->value;
          foundDOSensor = true;
          break;
        }
      }
      
      if (!foundDOSensor) {
        ctrl["processValue"] = 0.0f;
      }
    }
  }
  
  String response;
  serializeJson(doc, response);
  server.send(200, "application/json", response);
}

void handleGetTempControllerConfig(uint8_t index) {
  if (index < 40 || index >= 40 + MAX_TEMP_CONTROLLERS) {
    server.send(400, "application/json", "{\"error\":\"Invalid controller index\"}");
    return;
  }
  
  int ctrlIdx = index - 40;
  StaticJsonDocument<1024> doc;
  
  doc["index"] = index;
  doc["isActive"] = ioConfig.tempControllers[ctrlIdx].isActive;
  doc["name"] = ioConfig.tempControllers[ctrlIdx].name;
  doc["enabled"] = ioConfig.tempControllers[ctrlIdx].enabled;
  doc["showOnDashboard"] = ioConfig.tempControllers[ctrlIdx].showOnDashboard;
  doc["unit"] = ioConfig.tempControllers[ctrlIdx].unit;
  doc["pvSourceIndex"] = ioConfig.tempControllers[ctrlIdx].pvSourceIndex;
  doc["outputIndex"] = ioConfig.tempControllers[ctrlIdx].outputIndex;
  doc["controlMethod"] = (uint8_t)ioConfig.tempControllers[ctrlIdx].controlMethod;
  doc["setpoint"] = ioConfig.tempControllers[ctrlIdx].setpoint;
  doc["hysteresis"] = ioConfig.tempControllers[ctrlIdx].hysteresis;
  doc["kP"] = ioConfig.tempControllers[ctrlIdx].kP;
  doc["kI"] = ioConfig.tempControllers[ctrlIdx].kI;
  doc["kD"] = ioConfig.tempControllers[ctrlIdx].kD;
  doc["integralWindup"] = ioConfig.tempControllers[ctrlIdx].integralWindup;
  doc["outputMin"] = ioConfig.tempControllers[ctrlIdx].outputMin;
  doc["outputMax"] = ioConfig.tempControllers[ctrlIdx].outputMax;
  
  String response;
  serializeJson(doc, response);
  server.send(200, "application/json", response);
}

void handleSaveTempControllerConfig(uint8_t index) {
  if (index < 40 || index >= 40 + MAX_TEMP_CONTROLLERS) {
    server.send(400, "application/json", "{\"error\":\"Invalid controller index\"}");
    return;
  }
  
  if (!server.hasArg("plain")) {
    server.send(400, "application/json", "{\"error\":\"No data provided\"}");
    return;
  }
  
  StaticJsonDocument<1024> doc;
  DeserializationError error = deserializeJson(doc, server.arg("plain"));
  
  if (error) {
    server.send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
    return;
  }
  
  int ctrlIdx = index - 40;
  
  // DEBUG: Log received setpoint to track down the 7.0 bug
  float receivedSetpoint = doc["setpoint"] | -999.0f;
  log(LOG_INFO, false, "[TEMP CTRL %d] Save config: received setpoint=%.2f\n", index, receivedSetpoint);
  
  // Check for output conflicts
  uint8_t newOutputIndex = doc["outputIndex"] | 0;
  if (newOutputIndex > 0) {
    for (int j = 0; j < MAX_TEMP_CONTROLLERS; j++) {
      if (j != ctrlIdx && ioConfig.tempControllers[j].isActive && 
          ioConfig.tempControllers[j].outputIndex == newOutputIndex) {
        server.send(400, "application/json", 
                   "{\"error\":\"Output already in use by another controller\"}");
        return;
      }
    }
  }
  
  // Update configuration
  ioConfig.tempControllers[ctrlIdx].isActive = doc["isActive"] | true;
  strlcpy(ioConfig.tempControllers[ctrlIdx].name, doc["name"] | "", 
          sizeof(ioConfig.tempControllers[ctrlIdx].name));
  // DO NOT save enabled state - runtime only (avoid flash wear)
  ioConfig.tempControllers[ctrlIdx].enabled = false;
  
  // Update showOnDashboard flag
  if (doc.containsKey("showOnDashboard")) {
    ioConfig.tempControllers[ctrlIdx].showOnDashboard = doc["showOnDashboard"];
  }
  
  strlcpy(ioConfig.tempControllers[ctrlIdx].unit, doc["unit"] | "C", 
          sizeof(ioConfig.tempControllers[ctrlIdx].unit));
  
  ioConfig.tempControllers[ctrlIdx].pvSourceIndex = doc["pvSourceIndex"] | 0;
  ioConfig.tempControllers[ctrlIdx].outputIndex = doc["outputIndex"] | 0;
  
  ioConfig.tempControllers[ctrlIdx].controlMethod = (ControlMethod)(doc["controlMethod"] | CONTROL_METHOD_PID);
  ioConfig.tempControllers[ctrlIdx].setpoint = doc["setpoint"] | 25.0f;
  
  ioConfig.tempControllers[ctrlIdx].hysteresis = doc["hysteresis"] | 0.5f;
  
  ioConfig.tempControllers[ctrlIdx].kP = doc["kP"] | 2.0f;
  ioConfig.tempControllers[ctrlIdx].kI = doc["kI"] | 0.5f;
  ioConfig.tempControllers[ctrlIdx].kD = doc["kD"] | 0.1f;
  ioConfig.tempControllers[ctrlIdx].integralWindup = doc["integralWindup"] | 100.0f;
  ioConfig.tempControllers[ctrlIdx].outputMin = doc["outputMin"] | 0.0f;
  ioConfig.tempControllers[ctrlIdx].outputMax = doc["outputMax"] | 100.0f;
  
  // Set output mode based on control method
  uint8_t outputIdx = ioConfig.tempControllers[ctrlIdx].outputIndex;
  if (outputIdx >= 21 && outputIdx <= 25) {
    int digitalIdx = outputIdx - 21;
    if (ioConfig.tempControllers[ctrlIdx].controlMethod == CONTROL_METHOD_ON_OFF) {
      ioConfig.digitalOutputs[digitalIdx].mode = OUTPUT_MODE_ON_OFF;
    } else {
      ioConfig.digitalOutputs[digitalIdx].mode = OUTPUT_MODE_PWM;
    }
  }
  
  // Save configuration
  saveIOConfig();
  
  // Send IPC config packet to IO MCU
  IPC_ConfigTempController_t cfg;
  memset(&cfg, 0, sizeof(cfg));
  
  // Generate transaction ID for ACK tracking
  cfg.transactionId = generateTransactionId();
  cfg.index = index;
  cfg.isActive = ioConfig.tempControllers[ctrlIdx].isActive;
  strncpy(cfg.name, ioConfig.tempControllers[ctrlIdx].name, sizeof(cfg.name) - 1);
  cfg.enabled = ioConfig.tempControllers[ctrlIdx].enabled;
  cfg.pvSourceIndex = ioConfig.tempControllers[ctrlIdx].pvSourceIndex;
  cfg.outputIndex = ioConfig.tempControllers[ctrlIdx].outputIndex;
  cfg.controlMethod = (uint8_t)ioConfig.tempControllers[ctrlIdx].controlMethod;
  cfg.setpoint = ioConfig.tempControllers[ctrlIdx].setpoint;
  cfg.hysteresis = ioConfig.tempControllers[ctrlIdx].hysteresis;
  cfg.kP = ioConfig.tempControllers[ctrlIdx].kP;
  cfg.kI = ioConfig.tempControllers[ctrlIdx].kI;
  cfg.kD = ioConfig.tempControllers[ctrlIdx].kD;
  cfg.integralWindup = ioConfig.tempControllers[ctrlIdx].integralWindup;
  cfg.outputMin = ioConfig.tempControllers[ctrlIdx].outputMin;
  cfg.outputMax = ioConfig.tempControllers[ctrlIdx].outputMax;
  
  bool sent = ipc.sendPacket(IPC_MSG_CONFIG_TEMP_CONTROLLER, (uint8_t*)&cfg, sizeof(cfg));
  
  // Track transaction for ACK validation
  if (sent) {
    addPendingTransaction(cfg.transactionId, IPC_MSG_CONFIG_TEMP_CONTROLLER, IPC_MSG_CONTROL_ACK, 1, cfg.index);
  }
  
  if (sent) {
    log(LOG_INFO, false, "Saved and sent temperature controller %d configuration to IO MCU\n", index);
    server.send(200, "application/json", "{\"success\":true,\"message\":\"Configuration saved and applied\"}");
  } else {
    log(LOG_WARNING, false, "Saved temperature controller %d config but failed to send to IO MCU\n", index);
    server.send(200, "application/json", "{\"success\":true,\"message\":\"Configuration saved but IO MCU update failed\"}");
  }
}

// Temperature controller runtime control
void handleUpdateControllerSetpoint(uint8_t index) {
  if (index < 40 || index >= 40 + MAX_TEMP_CONTROLLERS) {
    server.send(400, "application/json", "{\"error\":\"Invalid controller index\"}");
    return;
  }
  
  if (!server.hasArg("plain")) {
    server.send(400, "application/json", "{\"error\":\"No data provided\"}");
    return;
  }
  
  StaticJsonDocument<256> doc;
  DeserializationError error = deserializeJson(doc, server.arg("plain"));
  
  if (error) {
    server.send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
    return;
  }
  
  int ctrlIdx = index - 40;
  float setpoint = doc["setpoint"] | ioConfig.tempControllers[ctrlIdx].setpoint;
  
  // Send IPC command to IO MCU (DO NOT save to config - avoid flash wear)
  IPC_TempControllerControl_t cmd;
  memset(&cmd, 0, sizeof(cmd));
  cmd.transactionId = generateTransactionId();
  cmd.index = index;
  cmd.objectType = OBJ_T_TEMPERATURE_CONTROL;
  cmd.command = TEMP_CTRL_CMD_SET_SETPOINT;
  cmd.setpoint = setpoint;
  
  bool sent = ipc.sendPacket(IPC_MSG_CONTROL_WRITE, (uint8_t*)&cmd, sizeof(cmd));
  
  if (sent) {
    addPendingTransaction(cmd.transactionId, IPC_MSG_CONTROL_WRITE, IPC_MSG_CONTROL_ACK, 1, index);
    // Update in-memory config (DO NOT save to flash)
    // This allows the API to return the correct setpoint when web UI polls
    ioConfig.tempControllers[ctrlIdx].setpoint = setpoint;
    
    log(LOG_INFO, false, "Controller %d setpoint updated to %.1f (txn=%d)\n", index, setpoint, cmd.transactionId);
    server.send(200, "application/json", "{\"success\":true,\"message\":\"Setpoint updated\"}");
  } else {
    log(LOG_WARNING, false, "Failed to send setpoint command to controller %d\n", index);
    server.send(500, "application/json", "{\"error\":\"Failed to communicate with IO MCU\"}");
  }
}

void handleEnableController(uint8_t index) {
  if (index < 40 || index >= 40 + MAX_TEMP_CONTROLLERS) {
    server.send(400, "application/json", "{\"error\":\"Invalid controller index\"}");
    return;
  }
  
  // Send IPC command to IO MCU (DO NOT save to config - avoid flash wear)
  IPC_TempControllerControl_t cmd;
  memset(&cmd, 0, sizeof(cmd));
  cmd.transactionId = generateTransactionId();
  cmd.index = index;
  cmd.objectType = OBJ_T_TEMPERATURE_CONTROL;
  cmd.command = TEMP_CTRL_CMD_ENABLE;
  
  bool sent = ipc.sendPacket(IPC_MSG_CONTROL_WRITE, (uint8_t*)&cmd, sizeof(cmd));
  
  if (sent) {
    addPendingTransaction(cmd.transactionId, IPC_MSG_CONTROL_WRITE, IPC_MSG_CONTROL_ACK, 1, index);
    log(LOG_INFO, false, "Controller %d enabled (txn=%d)\n", index, cmd.transactionId);
    server.send(200, "application/json", "{\"success\":true,\"message\":\"Controller enabled\"}");
  } else {
    log(LOG_WARNING, false, "Failed to send enable command to controller %d\n", index);
    server.send(500, "application/json", "{\"error\":\"Failed to communicate with IO MCU\"}");
  }
}

void handleDisableController(uint8_t index) {
  if (index < 40 || index >= 40 + MAX_TEMP_CONTROLLERS) {
    server.send(400, "application/json", "{\"error\":\"Invalid controller index\"}");
    return;
  }
  
  // Send IPC command to IO MCU (DO NOT save to config - avoid flash wear)
  IPC_TempControllerControl_t cmd;
  memset(&cmd, 0, sizeof(cmd));
  cmd.transactionId = generateTransactionId();
  cmd.index = index;
  cmd.objectType = OBJ_T_TEMPERATURE_CONTROL;
  cmd.command = TEMP_CTRL_CMD_DISABLE;
  
  bool sent = ipc.sendPacket(IPC_MSG_CONTROL_WRITE, (uint8_t*)&cmd, sizeof(cmd));
  
  if (sent) {
    addPendingTransaction(cmd.transactionId, IPC_MSG_CONTROL_WRITE, IPC_MSG_CONTROL_ACK, 1, index);
    log(LOG_INFO, false, "Controller %d disabled (txn=%d)\n", index, cmd.transactionId);
    server.send(200, "application/json", "{\"success\":true,\"message\":\"Controller disabled\"}");
  } else {
    log(LOG_WARNING, false, "Failed to send disable command to controller %d\n", index);
    server.send(500, "application/json", "{\"error\":\"Failed to communicate with IO MCU\"}");
  }
}

void handleStartController(uint8_t index) {
  handleEnableController(index);  // Same as enable
}

void handleStopController(uint8_t index) {
  handleDisableController(index);  // Same as disable
}

void handleStartAutotune(uint8_t index) {
  if (index < 40 || index >= 40 + MAX_TEMP_CONTROLLERS) {
    server.send(400, "application/json", "{\"error\":\"Invalid controller index\"}");
    return;
  }
  
  int ctrlIdx = index - 40;
  
  // Verify controller is PID mode
  if (ioConfig.tempControllers[ctrlIdx].controlMethod != CONTROL_METHOD_PID) {
    server.send(400, "application/json", "{\"error\":\"Autotune only available for PID controllers\"}");
    return;
  }
  
  // Get autotune parameters from request body
  float targetSetpoint = ioConfig.tempControllers[ctrlIdx].setpoint;
  float outputStep = 100.0f;  // Default 100% output step for aggressive relay
  
  if (server.hasArg("plain")) {
    StaticJsonDocument<256> doc;
    DeserializationError error = deserializeJson(doc, server.arg("plain"));
    if (!error) {
      targetSetpoint = doc["setpoint"] | targetSetpoint;
      outputStep = doc["outputStep"] | outputStep;
    }
  }
  
  // Send IPC command to IO MCU
  IPC_TempControllerControl_t cmd;
  memset(&cmd, 0, sizeof(cmd));
  cmd.transactionId = generateTransactionId();
  cmd.index = index;
  cmd.objectType = OBJ_T_TEMPERATURE_CONTROL;
  cmd.command = TEMP_CTRL_CMD_START_AUTOTUNE;
  cmd.setpoint = targetSetpoint;
  cmd.autotuneOutputStep = outputStep;
  
  bool sent = ipc.sendPacket(IPC_MSG_CONTROL_WRITE, (uint8_t*)&cmd, sizeof(cmd));
  
  if (sent) {
    addPendingTransaction(cmd.transactionId, IPC_MSG_CONTROL_WRITE, IPC_MSG_CONTROL_ACK, 1, index);
    log(LOG_INFO, false, "Controller %d autotune started (setpoint=%.1f, step=%.1f%%, txn=%d)\n", 
        index, targetSetpoint, outputStep, cmd.transactionId);
    server.send(200, "application/json", "{\"success\":true,\"message\":\"Autotune started\"}");
  } else {
    log(LOG_WARNING, false, "Failed to send autotune command to controller %d\n", index);
    server.send(500, "application/json", "{\"error\":\"Failed to communicate with IO MCU\"}");
  }
}

void handleDeleteController(uint8_t index) {
  if (index < 40 || index >= 40 + MAX_TEMP_CONTROLLERS) {
    server.send(400, "application/json", "{\"error\":\"Invalid controller index\"}");
    return;
  }
  
  int ctrlIdx = index - 40;
  
  // Disable and mark inactive
  ioConfig.tempControllers[ctrlIdx].isActive = false;
  ioConfig.tempControllers[ctrlIdx].enabled = false;
  memset(ioConfig.tempControllers[ctrlIdx].name, 0, sizeof(ioConfig.tempControllers[ctrlIdx].name));
  
  saveIOConfig();
  
  // Send IPC delete command to IO MCU
  IPC_ConfigTempController_t cfg;
  memset(&cfg, 0, sizeof(cfg));
  cfg.transactionId = generateTransactionId();
  cfg.index = index;
  cfg.isActive = false;  // This signals deletion
  
  bool sent = ipc.sendPacket(IPC_MSG_CONFIG_TEMP_CONTROLLER, (uint8_t*)&cfg, sizeof(cfg));
  
  // Track transaction for ACK validation
  if (sent) {
    addPendingTransaction(cfg.transactionId, IPC_MSG_CONFIG_TEMP_CONTROLLER, IPC_MSG_CONTROL_ACK, 1, cfg.index);
  }
  
  if (sent) {
    log(LOG_INFO, false, "Controller %d deleted and removed from IO MCU\n", index);
    server.send(200, "application/json", "{\"success\":true,\"message\":\"Controller deleted\"}");
  } else {
    log(LOG_WARNING, false, "Controller %d deleted from config but failed to remove from IO MCU\n", index);
    server.send(200, "application/json", "{\"success\":true,\"message\":\"Controller deleted but IO MCU update failed\"}");
  }
}

// ============================================================================
// pH Controller Configuration and Control (Index 43)
// ============================================================================

void handleGetpHControllerConfig() {
  StaticJsonDocument<1024> doc;
  
  doc["index"] = 43;
  doc["isActive"] = ioConfig.phController.isActive;
  doc["name"] = ioConfig.phController.name;
  doc["enabled"] = ioConfig.phController.enabled;
  doc["showOnDashboard"] = ioConfig.phController.showOnDashboard;
  doc["pvSourceIndex"] = ioConfig.phController.pvSourceIndex;
  doc["setpoint"] = ioConfig.phController.setpoint;
  doc["deadband"] = ioConfig.phController.deadband;
  
  // Acid dosing configuration
  JsonObject acid = doc.createNestedObject("acidDosing");
  acid["enabled"] = ioConfig.phController.acidDosing.enabled;
  acid["outputType"] = ioConfig.phController.acidDosing.outputType;
  acid["outputIndex"] = ioConfig.phController.acidDosing.outputIndex;
  acid["motorPower"] = ioConfig.phController.acidDosing.motorPower;
  acid["dosingTime_ms"] = ioConfig.phController.acidDosing.dosingTime_ms;
  acid["dosingInterval_ms"] = ioConfig.phController.acidDosing.dosingInterval_ms;
  acid["volumePerDose_mL"] = ioConfig.phController.acidDosing.volumePerDose_mL;
  acid["mfcFlowRate_mL_min"] = ioConfig.phController.acidDosing.mfcFlowRate_mL_min;
  
  // Alkaline dosing configuration
  JsonObject alkaline = doc.createNestedObject("alkalineDosing");
  alkaline["enabled"] = ioConfig.phController.alkalineDosing.enabled;
  alkaline["outputType"] = ioConfig.phController.alkalineDosing.outputType;
  alkaline["outputIndex"] = ioConfig.phController.alkalineDosing.outputIndex;
  alkaline["motorPower"] = ioConfig.phController.alkalineDosing.motorPower;
  alkaline["dosingTime_ms"] = ioConfig.phController.alkalineDosing.dosingTime_ms;
  alkaline["dosingInterval_ms"] = ioConfig.phController.alkalineDosing.dosingInterval_ms;
  alkaline["volumePerDose_mL"] = ioConfig.phController.alkalineDosing.volumePerDose_mL;
  alkaline["mfcFlowRate_mL_min"] = ioConfig.phController.alkalineDosing.mfcFlowRate_mL_min;
  
  String response;
  serializeJson(doc, response);
  server.send(200, "application/json", response);
}

void handleSavepHControllerConfig() {
  if (!server.hasArg("plain")) {
    server.send(400, "application/json", "{\"error\":\"No data provided\"}");
    return;
  }
  
  StaticJsonDocument<1024> doc;
  DeserializationError error = deserializeJson(doc, server.arg("plain"));
  
  if (error) {
    server.send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
    return;
  }
  
  // Validate dosing configuration - at least one must be enabled
  bool acidEnabled = doc["acidDosing"]["enabled"] | false;
  bool alkalineEnabled = doc["alkalineDosing"]["enabled"] | false;
  
  if (!acidEnabled && !alkalineEnabled) {
    server.send(400, "application/json", "{\"error\":\"At least one dosing direction must be enabled\"}");
    return;
  }
  
  // Update configuration
  ioConfig.phController.isActive = doc["isActive"] | true;
  strlcpy(ioConfig.phController.name, doc["name"] | "", sizeof(ioConfig.phController.name));
  // DO NOT save enabled state - runtime only (avoid flash wear)
  ioConfig.phController.enabled = false;
  
  if (doc.containsKey("showOnDashboard")) {
    ioConfig.phController.showOnDashboard = doc["showOnDashboard"];
  }
  
  ioConfig.phController.pvSourceIndex = doc["pvSourceIndex"] | 0;
  ioConfig.phController.setpoint = doc["setpoint"] | 7.0f;
  ioConfig.phController.deadband = doc["deadband"] | 0.2f;
  
  // Acid dosing configuration
  JsonObject acid = doc["acidDosing"];
  ioConfig.phController.acidDosing.enabled = acid["enabled"] | false;
  ioConfig.phController.acidDosing.outputType = acid["outputType"] | 0;
  ioConfig.phController.acidDosing.outputIndex = acid["outputIndex"] | 21;
  ioConfig.phController.acidDosing.motorPower = acid["motorPower"] | 50;
  ioConfig.phController.acidDosing.dosingTime_ms = acid["dosingTime_ms"] | 1000;
  ioConfig.phController.acidDosing.dosingInterval_ms = acid["dosingInterval_ms"] | 60000;
  ioConfig.phController.acidDosing.volumePerDose_mL = acid["volumePerDose_mL"] | 0.5f;
  ioConfig.phController.acidDosing.mfcFlowRate_mL_min = acid["mfcFlowRate_mL_min"] | 100.0f;
  
  // Alkaline dosing configuration
  JsonObject alkaline = doc["alkalineDosing"];
  ioConfig.phController.alkalineDosing.enabled = alkaline["enabled"] | false;
  ioConfig.phController.alkalineDosing.outputType = alkaline["outputType"] | 0;
  ioConfig.phController.alkalineDosing.outputIndex = alkaline["outputIndex"] | 22;
  ioConfig.phController.alkalineDosing.motorPower = alkaline["motorPower"] | 50;
  ioConfig.phController.alkalineDosing.dosingTime_ms = alkaline["dosingTime_ms"] | 1000;
  ioConfig.phController.alkalineDosing.dosingInterval_ms = alkaline["dosingInterval_ms"] | 60000;
  ioConfig.phController.alkalineDosing.volumePerDose_mL = alkaline["volumePerDose_mL"] | 0.5f;
  ioConfig.phController.alkalineDosing.mfcFlowRate_mL_min = alkaline["mfcFlowRate_mL_min"] | 100.0f;
  
  // Save configuration
  saveIOConfig();
  
  // Send IPC config packet to IO MCU
  IPC_ConfigpHController_t cfg;
  memset(&cfg, 0, sizeof(cfg));
  
  cfg.transactionId = generateTransactionId();
  cfg.index = 43;
  cfg.isActive = ioConfig.phController.isActive;
  strncpy(cfg.name, ioConfig.phController.name, sizeof(cfg.name) - 1);
  cfg.enabled = ioConfig.phController.enabled;
  cfg.pvSourceIndex = ioConfig.phController.pvSourceIndex;
  cfg.setpoint = ioConfig.phController.setpoint;
  cfg.deadband = ioConfig.phController.deadband;
  
  cfg.acidEnabled = ioConfig.phController.acidDosing.enabled;
  cfg.acidOutputType = ioConfig.phController.acidDosing.outputType;
  cfg.acidOutputIndex = ioConfig.phController.acidDosing.outputIndex;
  cfg.acidMotorPower = ioConfig.phController.acidDosing.motorPower;
  cfg.acidDosingTime_ms = ioConfig.phController.acidDosing.dosingTime_ms;
  cfg.acidDosingInterval_ms = ioConfig.phController.acidDosing.dosingInterval_ms;
  cfg.acidVolumePerDose_mL = ioConfig.phController.acidDosing.volumePerDose_mL;
  cfg.acidMfcFlowRate_mL_min = ioConfig.phController.acidDosing.mfcFlowRate_mL_min;
  
  cfg.alkalineEnabled = ioConfig.phController.alkalineDosing.enabled;
  cfg.alkalineOutputType = ioConfig.phController.alkalineDosing.outputType;
  cfg.alkalineOutputIndex = ioConfig.phController.alkalineDosing.outputIndex;
  cfg.alkalineMotorPower = ioConfig.phController.alkalineDosing.motorPower;
  cfg.alkalineDosingTime_ms = ioConfig.phController.alkalineDosing.dosingTime_ms;
  cfg.alkalineDosingInterval_ms = ioConfig.phController.alkalineDosing.dosingInterval_ms;
  cfg.alkalineVolumePerDose_mL = ioConfig.phController.alkalineDosing.volumePerDose_mL;
  cfg.alkalineMfcFlowRate_mL_min = ioConfig.phController.alkalineDosing.mfcFlowRate_mL_min;
  
  bool sent = ipc.sendPacket(IPC_MSG_CONFIG_PH_CONTROLLER, (uint8_t*)&cfg, sizeof(cfg));
  
  if (sent) {
    addPendingTransaction(cfg.transactionId, IPC_MSG_CONFIG_PH_CONTROLLER, IPC_MSG_CONTROL_ACK, 1, cfg.index);
    log(LOG_INFO, false, "Saved and sent pH controller configuration to IO MCU\n");
    server.send(200, "application/json", "{\"success\":true,\"message\":\"Configuration saved and applied\"}");
  } else {
    log(LOG_WARNING, false, "Saved pH controller config but failed to send to IO MCU\n");
    server.send(200, "application/json", "{\"success\":true,\"message\":\"Configuration saved but IO MCU update failed\"}");
  }
}

void handleDeletepHController() {
  // Disable and mark inactive
  ioConfig.phController.isActive = false;
  ioConfig.phController.enabled = false;
  memset(ioConfig.phController.name, 0, sizeof(ioConfig.phController.name));
  
  saveIOConfig();
  
  // Send IPC delete command to IO MCU
  IPC_ConfigpHController_t cfg;
  memset(&cfg, 0, sizeof(cfg));
  cfg.transactionId = generateTransactionId();
  cfg.index = 43;
  cfg.isActive = false;  // This signals deletion
  
  bool sent = ipc.sendPacket(IPC_MSG_CONFIG_PH_CONTROLLER, (uint8_t*)&cfg, sizeof(cfg));
  
  if (sent) {
    addPendingTransaction(cfg.transactionId, IPC_MSG_CONFIG_PH_CONTROLLER, IPC_MSG_CONTROL_ACK, 1, cfg.index);
    log(LOG_INFO, false, "pH controller deleted and removed from IO MCU\n");
    server.send(200, "application/json", "{\"success\":true,\"message\":\"pH controller deleted\"}");
  } else {
    log(LOG_WARNING, false, "pH controller deleted from config but failed to remove from IO MCU\n");
    server.send(200, "application/json", "{\"success\":true,\"message\":\"pH controller deleted but IO MCU update failed\"}");
  }
}

void handleUpdatepHSetpoint() {
  if (!server.hasArg("plain")) {
    server.send(400, "application/json", "{\"error\":\"No data provided\"}");
    return;
  }
  
  StaticJsonDocument<256> doc;
  DeserializationError error = deserializeJson(doc, server.arg("plain"));
  
  if (error) {
    server.send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
    return;
  }
  
  float setpoint = doc["setpoint"] | 7.0f;
  
  // Send runtime control command to IO MCU (does NOT recreate controller)
  IPC_pHControllerControl_t cmd;
  memset(&cmd, 0, sizeof(cmd));
  
  cmd.transactionId = generateTransactionId();
  cmd.index = 43;
  cmd.objectType = OBJ_T_PH_CONTROL;
  cmd.command = PH_CMD_SET_SETPOINT;
  cmd.setpoint = setpoint;
  
  bool sent = ipc.sendPacket(IPC_MSG_CONTROL_WRITE, (uint8_t*)&cmd, sizeof(cmd));
  
  if (sent) {
    addPendingTransaction(cmd.transactionId, IPC_MSG_CONTROL_WRITE, IPC_MSG_CONTROL_ACK, 1, 43);
    // Update in-memory config (DO NOT save to flash)
    // This allows the API to return the correct setpoint when web UI polls
    ioConfig.phController.setpoint = setpoint;
    log(LOG_INFO, false, "pH setpoint updated to %.2f (txn=%d)\n", setpoint, cmd.transactionId);
    server.send(200, "application/json", "{\"success\":true}");
  } else {
    log(LOG_WARNING, false, "Failed to send pH setpoint update\n");
    server.send(500, "application/json", "{\"error\":\"Failed to send IPC command\"}");
  }
}

void handleEnablepHController() {
  IPC_pHControllerControl_t cmd;
  memset(&cmd, 0, sizeof(cmd));
  cmd.transactionId = generateTransactionId();
  cmd.index = 43;
  cmd.objectType = OBJ_T_PH_CONTROL;
  cmd.command = PH_CMD_ENABLE;
  
  bool sent = ipc.sendPacket(IPC_MSG_CONTROL_WRITE, (uint8_t*)&cmd, sizeof(cmd));
  
  if (sent) {
    addPendingTransaction(cmd.transactionId, IPC_MSG_CONTROL_WRITE, IPC_MSG_CONTROL_ACK, 1, 43);
    log(LOG_INFO, false, "pH controller enabled (txn=%d)\n", cmd.transactionId);
    server.send(200, "application/json", "{\"success\":true}");
  } else {
    server.send(500, "application/json", "{\"error\":\"Failed to send command to IO MCU\"}");
  }
}

void handleDisablepHController() {
  IPC_pHControllerControl_t cmd;
  memset(&cmd, 0, sizeof(cmd));
  cmd.transactionId = generateTransactionId();
  cmd.index = 43;
  cmd.objectType = OBJ_T_PH_CONTROL;
  cmd.command = PH_CMD_DISABLE;
  
  bool sent = ipc.sendPacket(IPC_MSG_CONTROL_WRITE, (uint8_t*)&cmd, sizeof(cmd));
  
  if (sent) {
    addPendingTransaction(cmd.transactionId, IPC_MSG_CONTROL_WRITE, IPC_MSG_CONTROL_ACK, 1, 43);
    log(LOG_INFO, false, "pH controller disabled (txn=%d)\n", cmd.transactionId);
    server.send(200, "application/json", "{\"success\":true}");
  } else {
    server.send(500, "application/json", "{\"error\":\"Failed to send command to IO MCU\"}");
  }
}

void handleManualpHAcidDose() {
  IPC_pHControllerControl_t cmd;
  memset(&cmd, 0, sizeof(cmd));
  cmd.transactionId = generateTransactionId();
  cmd.index = 43;
  cmd.objectType = OBJ_T_PH_CONTROL;
  cmd.command = PH_CMD_DOSE_ACID;
  
  bool sent = ipc.sendPacket(IPC_MSG_CONTROL_WRITE, (uint8_t*)&cmd, sizeof(cmd));
  
  if (sent) {
    addPendingTransaction(cmd.transactionId, IPC_MSG_CONTROL_WRITE, IPC_MSG_CONTROL_ACK, 1, 43);
    log(LOG_INFO, false, "pH manual acid dose started (txn=%d)\n", cmd.transactionId);
    server.send(200, "application/json", "{\"success\":true}");
  } else {
    server.send(500, "application/json", "{\"error\":\"Failed to send command to IO MCU\"}");
  }
}

void handleManualpHAlkalineDose() {
  IPC_pHControllerControl_t cmd;
  memset(&cmd, 0, sizeof(cmd));
  cmd.transactionId = generateTransactionId();
  cmd.index = 43;
  cmd.objectType = OBJ_T_PH_CONTROL;
  cmd.command = PH_CMD_DOSE_ALKALINE;
  
  bool sent = ipc.sendPacket(IPC_MSG_CONTROL_WRITE, (uint8_t*)&cmd, sizeof(cmd));
  
  if (sent) {
    addPendingTransaction(cmd.transactionId, IPC_MSG_CONTROL_WRITE, IPC_MSG_CONTROL_ACK, 1, 43);
    log(LOG_INFO, false, "pH manual alkaline dose started (txn=%d)\n", cmd.transactionId);
    server.send(200, "application/json", "{\"success\":true}");
  } else {
    server.send(500, "application/json", "{\"error\":\"Failed to send command to IO MCU\"}");
  }
}

void handleResetpHAcidVolume() {
  IPC_pHControllerControl_t cmd;
  memset(&cmd, 0, sizeof(cmd));
  cmd.transactionId = generateTransactionId();
  cmd.index = 43;
  cmd.objectType = OBJ_T_PH_CONTROL;
  cmd.command = PH_CMD_RESET_ACID_VOLUME;
  
  bool sent = ipc.sendPacket(IPC_MSG_CONTROL_WRITE, (uint8_t*)&cmd, sizeof(cmd));
  
  if (sent) {
    addPendingTransaction(cmd.transactionId, IPC_MSG_CONTROL_WRITE, IPC_MSG_CONTROL_ACK, 1, 43);
    log(LOG_INFO, false, "pH acid volume reset (txn=%d)\n", cmd.transactionId);
    server.send(200, "application/json", "{\"success\":true}");
  } else {
    server.send(500, "application/json", "{\"error\":\"Failed to send command to IO MCU\"}");
  }
}

void handleResetpHAlkalineVolume() {
  IPC_pHControllerControl_t cmd;
  memset(&cmd, 0, sizeof(cmd));
  cmd.transactionId = generateTransactionId();
  cmd.index = 43;
  cmd.objectType = OBJ_T_PH_CONTROL;
  cmd.command = PH_CMD_RESET_BASE_VOLUME;
  
  bool sent = ipc.sendPacket(IPC_MSG_CONTROL_WRITE, (uint8_t*)&cmd, sizeof(cmd));
  
  if (sent) {
    addPendingTransaction(cmd.transactionId, IPC_MSG_CONTROL_WRITE, IPC_MSG_CONTROL_ACK, 1, 43);
    log(LOG_INFO, false, "pH alkaline volume reset (txn=%d)\n", cmd.transactionId);
    server.send(200, "application/json", "{\"success\":true}");
  } else {
    server.send(500, "application/json", "{\"error\":\"Failed to send command to IO MCU\"}");
  }
}

void handleDosepHAcid() {
  IPC_pHControllerControl_t cmd;
  memset(&cmd, 0, sizeof(cmd));
  cmd.transactionId = generateTransactionId();
  cmd.index = 43;
  cmd.objectType = OBJ_T_PH_CONTROL;
  cmd.command = PH_CMD_DOSE_ACID;
  
  bool sent = ipc.sendPacket(IPC_MSG_CONTROL_WRITE, (uint8_t*)&cmd, sizeof(cmd));
  
  if (sent) {
    addPendingTransaction(cmd.transactionId, IPC_MSG_CONTROL_WRITE, IPC_MSG_CONTROL_ACK, 1, 43);
    log(LOG_INFO, false, "pH manual acid dose started (txn=%d)\n", cmd.transactionId);
    server.send(200, "application/json", "{\"success\":true}");
  } else {
    server.send(500, "application/json", "{\"error\":\"Failed to send command to IO MCU\"}");
  }
}

void handleDosepHAlkaline() {
  IPC_pHControllerControl_t cmd;
  memset(&cmd, 0, sizeof(cmd));
  cmd.transactionId = generateTransactionId();
  cmd.index = 43;
  cmd.objectType = OBJ_T_PH_CONTROL;
  cmd.command = PH_CMD_DOSE_ALKALINE;
  
  bool sent = ipc.sendPacket(IPC_MSG_CONTROL_WRITE, (uint8_t*)&cmd, sizeof(cmd));
  
  if (sent) {
    addPendingTransaction(cmd.transactionId, IPC_MSG_CONTROL_WRITE, IPC_MSG_CONTROL_ACK, 1, 43);
    log(LOG_INFO, false, "pH manual alkaline dose started (txn=%d)\n", cmd.transactionId);
    server.send(200, "application/json", "{\"success\":true}");
  } else {
    server.send(500, "application/json", "{\"error\":\"Failed to send command to IO MCU\"}");
  }
}

// ============================================================================
// Flow Controller Configuration and Control (Indices 44-47)
// ============================================================================

void handleGetFlowControllerConfig(uint8_t index) {
  // Validate index (44-47)
  if (index < 44 || index >= 44 + MAX_FLOW_CONTROLLERS) {
    server.send(400, "application/json", "{\"error\":\"Invalid flow controller index\"}");
    return;
  }
  
  int arrIdx = index - 44;
  
  StaticJsonDocument<512> doc;
  doc["index"] = index;
  doc["isActive"] = ioConfig.flowControllers[arrIdx].isActive;
  doc["name"] = ioConfig.flowControllers[arrIdx].name;
  doc["enabled"] = ioConfig.flowControllers[arrIdx].enabled;
  doc["showOnDashboard"] = ioConfig.flowControllers[arrIdx].showOnDashboard;
  doc["flowRate_mL_min"] = ioConfig.flowControllers[arrIdx].flowRate_mL_min;
  doc["outputType"] = ioConfig.flowControllers[arrIdx].outputType;
  doc["outputIndex"] = ioConfig.flowControllers[arrIdx].outputIndex;
  doc["motorPower"] = ioConfig.flowControllers[arrIdx].motorPower;
  doc["calibrationDoseTime_ms"] = ioConfig.flowControllers[arrIdx].calibrationDoseTime_ms;
  doc["calibrationMotorPower"] = ioConfig.flowControllers[arrIdx].calibrationMotorPower;
  doc["calibrationVolume_mL"] = ioConfig.flowControllers[arrIdx].calibrationVolume_mL;
  doc["minDosingInterval_ms"] = ioConfig.flowControllers[arrIdx].minDosingInterval_ms;
  doc["maxDosingTime_ms"] = ioConfig.flowControllers[arrIdx].maxDosingTime_ms;
  
  String response;
  serializeJson(doc, response);
  server.send(200, "application/json", response);
}

void handleSaveFlowControllerConfig(uint8_t index) {
  // Validate index (44-47)
  if (index < 44 || index >= 44 + MAX_FLOW_CONTROLLERS) {
    server.send(400, "application/json", "{\"error\":\"Invalid flow controller index\"}");
    return;
  }
  
  if (!server.hasArg("plain")) {
    server.send(400, "application/json", "{\"error\":\"No data provided\"}");
    return;
  }
  
  StaticJsonDocument<1024> doc;
  DeserializationError error = deserializeJson(doc, server.arg("plain"));
  
  if (error) {
    server.send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
    return;
  }
  
  int arrIdx = index - 44;
  
  // Validate calibration data
  float calibVol = doc["calibrationVolume_mL"] | 1.0f;
  if (calibVol <= 0.0f) {
    server.send(400, "application/json", "{\"error\":\"Calibration volume must be > 0\"}");
    return;
  }
  
  // Update configuration
  ioConfig.flowControllers[arrIdx].isActive = doc["isActive"] | true;
  strlcpy(ioConfig.flowControllers[arrIdx].name, doc["name"] | "", sizeof(ioConfig.flowControllers[arrIdx].name));
  // DO NOT save enabled state - runtime only
  ioConfig.flowControllers[arrIdx].enabled = false;
  
  if (doc.containsKey("showOnDashboard")) {
    ioConfig.flowControllers[arrIdx].showOnDashboard = doc["showOnDashboard"];
  }
  
  ioConfig.flowControllers[arrIdx].flowRate_mL_min = doc["flowRate_mL_min"] | 10.0f;
  
  ioConfig.flowControllers[arrIdx].outputType = doc["outputType"] | 1;
  ioConfig.flowControllers[arrIdx].outputIndex = doc["outputIndex"] | (27 + arrIdx);
  ioConfig.flowControllers[arrIdx].motorPower = doc["motorPower"] | 50;
  
  ioConfig.flowControllers[arrIdx].calibrationDoseTime_ms = doc["calibrationDoseTime_ms"] | 1000;
  ioConfig.flowControllers[arrIdx].calibrationMotorPower = doc["calibrationMotorPower"] | 50;
  ioConfig.flowControllers[arrIdx].calibrationVolume_mL = calibVol;
  
  ioConfig.flowControllers[arrIdx].minDosingInterval_ms = doc["minDosingInterval_ms"] | 1000;
  ioConfig.flowControllers[arrIdx].maxDosingTime_ms = doc["maxDosingTime_ms"] | 30000;
  
  // Save configuration
  saveIOConfig();
  
  // Send IPC config packet to IO MCU
  IPC_ConfigFlowController_t cfg;
  memset(&cfg, 0, sizeof(cfg));
  
  cfg.transactionId = generateTransactionId();
  cfg.index = index;
  cfg.isActive = ioConfig.flowControllers[arrIdx].isActive;
  strncpy(cfg.name, ioConfig.flowControllers[arrIdx].name, sizeof(cfg.name) - 1);
  cfg.enabled = ioConfig.flowControllers[arrIdx].enabled;
  cfg.flowRate_mL_min = ioConfig.flowControllers[arrIdx].flowRate_mL_min;
  
  cfg.outputType = ioConfig.flowControllers[arrIdx].outputType;
  cfg.outputIndex = ioConfig.flowControllers[arrIdx].outputIndex;
  cfg.motorPower = ioConfig.flowControllers[arrIdx].motorPower;
  
  cfg.calibrationDoseTime_ms = ioConfig.flowControllers[arrIdx].calibrationDoseTime_ms;
  cfg.calibrationMotorPower = ioConfig.flowControllers[arrIdx].calibrationMotorPower;
  cfg.calibrationVolume_mL = ioConfig.flowControllers[arrIdx].calibrationVolume_mL;
  
  cfg.minDosingInterval_ms = ioConfig.flowControllers[arrIdx].minDosingInterval_ms;
  cfg.maxDosingTime_ms = ioConfig.flowControllers[arrIdx].maxDosingTime_ms;
  
  bool sent = ipc.sendPacket(IPC_MSG_CONFIG_FLOW_CONTROLLER, (uint8_t*)&cfg, sizeof(cfg));
  
  if (sent) {
    addPendingTransaction(cfg.transactionId, IPC_MSG_CONFIG_FLOW_CONTROLLER, IPC_MSG_CONTROL_ACK, 1, cfg.index);
    log(LOG_INFO, false, "Saved and sent flow controller %d configuration to IO MCU\n", index);
    server.send(200, "application/json", "{\"success\":true,\"message\":\"Configuration saved and applied\"}");
  } else {
    log(LOG_WARNING, false, "Saved flow controller %d config but failed to send to IO MCU\n", index);
    server.send(200, "application/json", "{\"success\":true,\"message\":\"Configuration saved but IO MCU update failed\"}");
  }
}

void handleDeleteFlowController(uint8_t index) {
  // Validate index (44-47)
  if (index < 44 || index >= 44 + MAX_FLOW_CONTROLLERS) {
    server.send(400, "application/json", "{\"error\":\"Invalid flow controller index\"}");
    return;
  }
  
  int arrIdx = index - 44;
  
  // Disable and mark inactive
  ioConfig.flowControllers[arrIdx].isActive = false;
  ioConfig.flowControllers[arrIdx].enabled = false;
  memset(ioConfig.flowControllers[arrIdx].name, 0, sizeof(ioConfig.flowControllers[arrIdx].name));
  
  saveIOConfig();
  
  // Send IPC delete command to IO MCU
  IPC_ConfigFlowController_t cfg;
  memset(&cfg, 0, sizeof(cfg));
  cfg.transactionId = generateTransactionId();
  cfg.index = index;
  cfg.isActive = false;  // This signals deletion
  
  bool sent = ipc.sendPacket(IPC_MSG_CONFIG_FLOW_CONTROLLER, (uint8_t*)&cfg, sizeof(cfg));
  
  if (sent) {
    addPendingTransaction(cfg.transactionId, IPC_MSG_CONFIG_FLOW_CONTROLLER, IPC_MSG_CONTROL_ACK, 1, cfg.index);
    log(LOG_INFO, false, "Flow controller %d deleted and removed from IO MCU\n", index);
    server.send(200, "application/json", "{\"success\":true,\"message\":\"Flow controller deleted\"}");
  } else {
    log(LOG_WARNING, false, "Flow controller %d deleted from config but failed to remove from IO MCU\n", index);
    server.send(200, "application/json", "{\"success\":true,\"message\":\"Flow controller deleted but IO MCU update failed\"}");
  }
}

void handleSetFlowRate(uint8_t index) {
  if (!server.hasArg("plain")) {
    server.send(400, "application/json", "{\"error\":\"No data provided\"}");
    return;
  }
  
  StaticJsonDocument<256> doc;
  DeserializationError error = deserializeJson(doc, server.arg("plain"));
  
  if (error) {
    server.send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
    return;
  }
  
  float flowRate = doc["flowRate"] | 10.0f;
  
  IPC_FlowControllerControl_t cmd;
  memset(&cmd, 0, sizeof(cmd));
  cmd.transactionId = generateTransactionId();
  cmd.index = index;
  cmd.objectType = OBJ_T_FLOW_CONTROL;
  cmd.command = FLOW_CMD_SET_FLOW_RATE;
  cmd.flowRate_mL_min = flowRate;
  
  bool sent = ipc.sendPacket(IPC_MSG_CONTROL_WRITE, (uint8_t*)&cmd, sizeof(cmd));
  
  if (sent) {
    addPendingTransaction(cmd.transactionId, IPC_MSG_CONTROL_WRITE, IPC_MSG_CONTROL_ACK, 1, index);
    log(LOG_INFO, false, "Flow controller %d flow rate set to %.2f mL/min (txn=%d)\n", index, flowRate, cmd.transactionId);
    server.send(200, "application/json", "{\"success\":true}");
  } else {
    server.send(500, "application/json", "{\"error\":\"Failed to send command to IO MCU\"}");
  }
}

void handleEnableFlowController(uint8_t index) {
  IPC_FlowControllerControl_t cmd;
  memset(&cmd, 0, sizeof(cmd));
  cmd.transactionId = generateTransactionId();
  cmd.index = index;
  cmd.objectType = OBJ_T_FLOW_CONTROL;
  cmd.command = FLOW_CMD_ENABLE;
  
  bool sent = ipc.sendPacket(IPC_MSG_CONTROL_WRITE, (uint8_t*)&cmd, sizeof(cmd));
  
  if (sent) {
    addPendingTransaction(cmd.transactionId, IPC_MSG_CONTROL_WRITE, IPC_MSG_CONTROL_ACK, 1, index);
    log(LOG_INFO, false, "Flow controller %d enabled (txn=%d)\n", index, cmd.transactionId);
    server.send(200, "application/json", "{\"success\":true}");
  } else {
    server.send(500, "application/json", "{\"error\":\"Failed to send command to IO MCU\"}");
  }
}

void handleDisableFlowController(uint8_t index) {
  IPC_FlowControllerControl_t cmd;
  memset(&cmd, 0, sizeof(cmd));
  cmd.transactionId = generateTransactionId();
  cmd.index = index;
  cmd.objectType = OBJ_T_FLOW_CONTROL;
  cmd.command = FLOW_CMD_DISABLE;
  
  bool sent = ipc.sendPacket(IPC_MSG_CONTROL_WRITE, (uint8_t*)&cmd, sizeof(cmd));
  
  if (sent) {
    addPendingTransaction(cmd.transactionId, IPC_MSG_CONTROL_WRITE, IPC_MSG_CONTROL_ACK, 1, index);
    log(LOG_INFO, false, "Flow controller %d disabled (txn=%d)\n", index, cmd.transactionId);
    server.send(200, "application/json", "{\"success\":true}");
  } else {
    server.send(500, "application/json", "{\"error\":\"Failed to send command to IO MCU\"}");
  }
}

void handleManualFlowDose(uint8_t index) {
  IPC_FlowControllerControl_t cmd;
  memset(&cmd, 0, sizeof(cmd));
  cmd.transactionId = generateTransactionId();
  cmd.index = index;
  cmd.objectType = OBJ_T_FLOW_CONTROL;
  cmd.command = FLOW_CMD_MANUAL_DOSE;
  
  bool sent = ipc.sendPacket(IPC_MSG_CONTROL_WRITE, (uint8_t*)&cmd, sizeof(cmd));
  
  if (sent) {
    addPendingTransaction(cmd.transactionId, IPC_MSG_CONTROL_WRITE, IPC_MSG_CONTROL_ACK, 1, index);
    log(LOG_INFO, false, "Flow controller %d manual dose started (txn=%d)\n", index, cmd.transactionId);
    server.send(200, "application/json", "{\"success\":true}");
  } else {
    server.send(500, "application/json", "{\"error\":\"Failed to send command to IO MCU\"}");
  }
}

void handleResetFlowVolume(uint8_t index) {
  IPC_FlowControllerControl_t cmd;
  memset(&cmd, 0, sizeof(cmd));
  cmd.transactionId = generateTransactionId();
  cmd.index = index;
  cmd.objectType = OBJ_T_FLOW_CONTROL;
  cmd.command = FLOW_CMD_RESET_VOLUME;
  
  bool sent = ipc.sendPacket(IPC_MSG_CONTROL_WRITE, (uint8_t*)&cmd, sizeof(cmd));
  
  if (sent) {
    addPendingTransaction(cmd.transactionId, IPC_MSG_CONTROL_WRITE, IPC_MSG_CONTROL_ACK, 1, index);
    log(LOG_INFO, false, "Flow controller %d volume reset (txn=%d)\n", index, cmd.transactionId);
    server.send(200, "application/json", "{\"success\":true}");
  } else {
    server.send(500, "application/json", "{\"error\":\"Failed to send command to IO MCU\"}");
  }
}

// ============================================================================
// DO Controller Configuration and Control (Index 48)
// ============================================================================

void handleGetDOControllerConfig() {
  StaticJsonDocument<512> doc;
  
  doc["index"] = 48;
  doc["isActive"] = ioConfig.doController.isActive;
  doc["name"] = ioConfig.doController.name;
  doc["enabled"] = ioConfig.doController.enabled;
  doc["showOnDashboard"] = ioConfig.doController.showOnDashboard;
  doc["setpoint_mg_L"] = ioConfig.doController.setpoint_mg_L;
  doc["activeProfileIndex"] = ioConfig.doController.activeProfileIndex;
  doc["stirrerEnabled"] = ioConfig.doController.stirrerEnabled;
  doc["stirrerType"] = ioConfig.doController.stirrerType;
  doc["stirrerIndex"] = ioConfig.doController.stirrerIndex;
  doc["stirrerMaxRPM"] = ioConfig.doController.stirrerMaxRPM;
  doc["mfcEnabled"] = ioConfig.doController.mfcEnabled;
  doc["mfcDeviceIndex"] = ioConfig.doController.mfcDeviceIndex;
  
  // Include active profile name if valid
  if (ioConfig.doController.activeProfileIndex < MAX_DO_PROFILES) {
    doc["activeProfileName"] = ioConfig.doProfiles[ioConfig.doController.activeProfileIndex].name;
  }
  
  String response;
  serializeJson(doc, response);
  server.send(200, "application/json", response);
}

void handleSaveDOControllerConfig() {
  if (!server.hasArg("plain")) {
    server.send(400, "application/json", "{\"error\":\"No data provided\"}");
    return;
  }
  
  // Use heap allocation to avoid stack overflow
  DynamicJsonDocument* doc = new DynamicJsonDocument(4096);
  if (!doc) {
    server.send(500, "application/json", "{\"error\":\"Memory allocation failed\"}");
    return;
  }
  
  DeserializationError error = deserializeJson(*doc, server.arg("plain"));
  
  if (error) {
    delete doc;
    server.send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
    return;
  }
  
  // Validate profile index if provided
  if (doc->containsKey("activeProfileIndex")) {
    uint8_t profIdx = (*doc)["activeProfileIndex"];
    if (profIdx >= MAX_DO_PROFILES) {
      delete doc;
      server.send(400, "application/json", "{\"error\":\"Invalid profile index\"}");
      return;
    }
  }
  
  // Update configuration
  ioConfig.doController.isActive = (*doc)["isActive"] | true;
  strlcpy(ioConfig.doController.name, (*doc)["name"] | "DO Controller", sizeof(ioConfig.doController.name));
  // DO NOT save enabled state - runtime only
  ioConfig.doController.enabled = false;
  
  if (doc->containsKey("showOnDashboard")) {
    ioConfig.doController.showOnDashboard = (*doc)["showOnDashboard"];
  }
  
  ioConfig.doController.setpoint_mg_L = (*doc)["setpoint_mg_L"] | 8.0f;
  
  // Only update profile index if provided (preserve existing if not)
  if (doc->containsKey("activeProfileIndex")) {
    ioConfig.doController.activeProfileIndex = (*doc)["activeProfileIndex"];
  }
  
  // Only update stirrer configuration if provided (preserve existing if not)
  if (doc->containsKey("stirrerEnabled")) {
    ioConfig.doController.stirrerEnabled = (*doc)["stirrerEnabled"];
    if (ioConfig.doController.stirrerEnabled) {
      ioConfig.doController.stirrerType = (*doc)["stirrerType"] | 0;
      ioConfig.doController.stirrerIndex = (*doc)["stirrerIndex"] | 27;
      ioConfig.doController.stirrerMaxRPM = (*doc)["stirrerMaxRPM"] | 300.0f;
    } else {
      ioConfig.doController.stirrerType = 0;
      ioConfig.doController.stirrerIndex = 0;
      ioConfig.doController.stirrerMaxRPM = 0.0f;
    }
  }
  
  // Only update MFC configuration if provided (preserve existing if not)
  if (doc->containsKey("mfcEnabled")) {
    ioConfig.doController.mfcEnabled = (*doc)["mfcEnabled"];
    if (ioConfig.doController.mfcEnabled) {
      ioConfig.doController.mfcDeviceIndex = (*doc)["mfcDeviceIndex"] | 50;
    } else {
      ioConfig.doController.mfcDeviceIndex = 0;
    }
  }
  
  delete doc;  // Clean up heap allocation
  
  // Validate configuration before saving
  if (ioConfig.doController.mfcEnabled) {
    if (ioConfig.doController.mfcDeviceIndex < 50 || ioConfig.doController.mfcDeviceIndex >= 70) {
      log(LOG_ERROR, true, "Invalid MFC device index: %d (must be 50-69)\n", ioConfig.doController.mfcDeviceIndex);
      server.send(400, "application/json", "{\"error\":\"MFC device index must be between 50-69\"}");
      return;
    }
  }
  
  if (ioConfig.doController.stirrerEnabled) {
    if (ioConfig.doController.stirrerIndex < 26 || ioConfig.doController.stirrerIndex >= 31) {
      log(LOG_ERROR, true, "Invalid stirrer index: %d\n", ioConfig.doController.stirrerIndex);
      server.send(400, "application/json", "{\"error\":\"Invalid stirrer motor index\"}");
      return;
    }
  }
  
  // Save configuration
  saveIOConfig();
  // Send IPC config packet to IO MCU
  IPC_ConfigDOController_t cfg;
  memset(&cfg, 0, sizeof(cfg));
  
  cfg.transactionId = generateTransactionId();
  cfg.index = 48;
  cfg.isActive = ioConfig.doController.isActive;
  strncpy(cfg.name, ioConfig.doController.name, sizeof(cfg.name) - 1);
  cfg.enabled = false;  // Don't send enabled state in config - preserve runtime state on IO MCU
  cfg.showOnDashboard = ioConfig.doController.showOnDashboard;
  cfg.setpoint_mg_L = ioConfig.doController.setpoint_mg_L;
  
  // Get active profile and copy points
  uint8_t profileIdx = ioConfig.doController.activeProfileIndex;
  if (profileIdx < MAX_DO_PROFILES && ioConfig.doProfiles[profileIdx].isActive) {
    // Safety: limit to MAX_DO_PROFILE_POINTS to prevent overflow
    int numPoints = ioConfig.doProfiles[profileIdx].numPoints;
    cfg.numPoints = (numPoints < MAX_DO_PROFILE_POINTS) ? numPoints : MAX_DO_PROFILE_POINTS;
    
    // Copy profile points to flattened arrays
    for (int j = 0; j < cfg.numPoints; j++) {
      cfg.profileErrorValues[j] = ioConfig.doProfiles[profileIdx].points[j].error_mg_L;
      cfg.profileStirrerValues[j] = ioConfig.doProfiles[profileIdx].points[j].stirrerOutput;
      cfg.profileMFCValues[j] = ioConfig.doProfiles[profileIdx].points[j].mfcOutput_mL_min;
    }
  } else {
    cfg.numPoints = 0;
  }
  
  cfg.stirrerEnabled = ioConfig.doController.stirrerEnabled;
  cfg.stirrerType = ioConfig.doController.stirrerType;
  cfg.stirrerIndex = ioConfig.doController.stirrerIndex;
  cfg.stirrerMaxRPM = ioConfig.doController.stirrerMaxRPM;
  cfg.mfcEnabled = ioConfig.doController.mfcEnabled;
  cfg.mfcDeviceIndex = ioConfig.doController.mfcDeviceIndex;
  
  bool sent = ipc.sendPacket(IPC_MSG_CONFIG_DO_CONTROLLER, (uint8_t*)&cfg, sizeof(cfg));
  
  if (sent) {
    addPendingTransaction(cfg.transactionId, IPC_MSG_CONFIG_DO_CONTROLLER, IPC_MSG_CONTROL_ACK, 1, cfg.index);
    log(LOG_INFO, false, "Saved and sent DO controller configuration to IO MCU\n");
    server.send(200, "application/json", "{\"success\":true,\"message\":\"Configuration saved and applied\"}");
  } else {
    log(LOG_WARNING, false, "Saved DO controller config but failed to send to IO MCU\n");
    server.send(200, "application/json", "{\"success\":true,\"message\":\"Configuration saved but IO MCU update failed\"}");
  }
}

void handleSetDOSetpoint() {
  if (!server.hasArg("plain")) {
    server.send(400, "application/json", "{\"error\":\"No data provided\"}");
    return;
  }
  
  StaticJsonDocument<256> doc;
  DeserializationError error = deserializeJson(doc, server.arg("plain"));
  
  if (error) {
    server.send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
    return;
  }
  
  float setpoint = doc["setpoint"] | 8.0f;
  
  // Send runtime control command to IO MCU
  IPC_DOControllerControl_t cmd;
  memset(&cmd, 0, sizeof(cmd));
  cmd.transactionId = generateTransactionId();
  cmd.index = 48;
  cmd.objectType = OBJ_T_DISSOLVED_OXYGEN_CONTROL;
  cmd.command = DO_CMD_SET_SETPOINT;
  cmd.setpoint_mg_L = setpoint;
  
  bool sent = ipc.sendPacket(IPC_MSG_CONTROL_WRITE, (uint8_t*)&cmd, sizeof(cmd));
  
  if (sent) {
    addPendingTransaction(cmd.transactionId, IPC_MSG_CONTROL_WRITE, IPC_MSG_CONTROL_ACK, 1, 48);
    log(LOG_INFO, false, "DO setpoint changed to %.2f mg/L (txn=%d)\n", setpoint, cmd.transactionId);
    server.send(200, "application/json", "{\"success\":true}");
  } else {
    server.send(500, "application/json", "{\"error\":\"Failed to send command to IO MCU\"}");
  }
}

void handleEnableDOController() {
  IPC_DOControllerControl_t cmd;
  memset(&cmd, 0, sizeof(cmd));
  cmd.transactionId = generateTransactionId();
  cmd.index = 48;
  cmd.objectType = OBJ_T_DISSOLVED_OXYGEN_CONTROL;
  cmd.command = DO_CMD_ENABLE;
  
  bool sent = ipc.sendPacket(IPC_MSG_CONTROL_WRITE, (uint8_t*)&cmd, sizeof(cmd));
  
  if (sent) {
    addPendingTransaction(cmd.transactionId, IPC_MSG_CONTROL_WRITE, IPC_MSG_CONTROL_ACK, 1, 48);
    log(LOG_INFO, false, "DO controller enabled (txn=%d)\n", cmd.transactionId);
    server.send(200, "application/json", "{\"success\":true}");
  } else {
    server.send(500, "application/json", "{\"error\":\"Failed to send command to IO MCU\"}");
  }
}

void handleDisableDOController() {
  IPC_DOControllerControl_t cmd;
  memset(&cmd, 0, sizeof(cmd));
  cmd.transactionId = generateTransactionId();
  cmd.index = 48;
  cmd.objectType = OBJ_T_DISSOLVED_OXYGEN_CONTROL;
  cmd.command = DO_CMD_DISABLE;
  
  bool sent = ipc.sendPacket(IPC_MSG_CONTROL_WRITE, (uint8_t*)&cmd, sizeof(cmd));
  
  if (sent) {
    addPendingTransaction(cmd.transactionId, IPC_MSG_CONTROL_WRITE, IPC_MSG_CONTROL_ACK, 1, 48);
    log(LOG_INFO, false, "DO controller disabled (txn=%d)\n", cmd.transactionId);
    server.send(200, "application/json", "{\"success\":true}");
  } else {
    server.send(500, "application/json", "{\"error\":\"Failed to send command to IO MCU\"}");
  }
}

void handleDeleteDOController() {
  // Disable and mark inactive
  ioConfig.doController.isActive = false;
  ioConfig.doController.enabled = false;
  memset(ioConfig.doController.name, 0, sizeof(ioConfig.doController.name));
  
  saveIOConfig();
  
  // Send IPC delete command to IO MCU
  IPC_ConfigDOController_t cfg;
  memset(&cfg, 0, sizeof(cfg));
  cfg.index = 48;
  cfg.isActive = false;  // This signals deletion
  
  bool sent = ipc.sendPacket(IPC_MSG_CONFIG_DO_CONTROLLER, (uint8_t*)&cfg, sizeof(cfg));
  
  if (sent) {
    log(LOG_INFO, false, "DO controller deleted and removed from IO MCU\n");
    server.send(200, "application/json", "{\"success\":true,\"message\":\"DO controller deleted\"}");
  } else {
    log(LOG_WARNING, false, "DO controller deleted from config but failed to remove from IO MCU\n");
    server.send(200, "application/json", "{\"success\":true,\"message\":\"DO controller deleted but IO MCU update failed\"}");
  }
}

// ============================================================================
// DO Profile Management (Indices 0-2)
// ============================================================================

void handleGetAllDOProfiles() {
  // SAFETY: Check config magic number
  if (ioConfig.magicNumber != IO_CONFIG_MAGIC_NUMBER) {
    server.send(200, "application/json", "{\"profiles\":[]}");
    return;
  }
  
  // Use heap allocation to avoid stack overflow - 4KB is too large for stack
  DynamicJsonDocument* doc = new DynamicJsonDocument(4096);
  if (!doc) {
    server.send(500, "application/json", "{\"error\":\"Memory allocation failed\"}");
    return;
  }
  
  JsonArray profiles = doc->createNestedArray("profiles");
  
  // Iterate profiles - use intermediate variables to avoid optimization issues
  for (int i = 0; i < MAX_DO_PROFILES; i++) {
    JsonObject profile = profiles.createNestedObject();
    profile["index"] = i;
    
    // Read fields into intermediate variables (prevents crash from optimization bug)
    bool isActive = ioConfig.doProfiles[i].isActive;
    profile["isActive"] = isActive;
    
    // Direct string assignment works when fields are read separately
    profile["name"] = ioConfig.doProfiles[i].name;
    
    uint8_t numPoints = ioConfig.doProfiles[i].numPoints;
    if (numPoints > MAX_DO_PROFILE_POINTS) {
      numPoints = MAX_DO_PROFILE_POINTS;
    }
    profile["numPoints"] = numPoints;
    
    // Efficient array format
    JsonArray errors = profile.createNestedArray("errors");
    JsonArray stirrers = profile.createNestedArray("stirrers");
    JsonArray mfcs = profile.createNestedArray("mfcs");
    
    for (int j = 0; j < numPoints; j++) {
      errors.add(ioConfig.doProfiles[i].points[j].error_mg_L);
      stirrers.add(ioConfig.doProfiles[i].points[j].stirrerOutput);
      mfcs.add(ioConfig.doProfiles[i].points[j].mfcOutput_mL_min);
    }
  }
  
  // Check for overflow
  if (doc->overflowed()) {
    delete doc;
    server.send(500, "application/json", "{\"error\":\"JSON buffer overflow\"}");
    return;
  }
  
  String response;
  serializeJson(*doc, response);
  delete doc;
  
  server.send(200, "application/json", response);
}

void handleGetDOProfile(uint8_t index) {
  if (index >= MAX_DO_PROFILES) {
    server.send(400, "application/json", "{\"error\":\"Invalid profile index\"}");
    return;
  }
  
  // 1KB buffer sufficient with efficient array format
  StaticJsonDocument<1024> doc;
  doc["index"] = index;
  doc["isActive"] = ioConfig.doProfiles[index].isActive;
  doc["name"] = ioConfig.doProfiles[index].name;
  doc["numPoints"] = ioConfig.doProfiles[index].numPoints;
  
  // Efficient array format
  int maxPoints = (ioConfig.doProfiles[index].numPoints < MAX_DO_PROFILE_POINTS) 
                   ? ioConfig.doProfiles[index].numPoints : MAX_DO_PROFILE_POINTS;
  
  JsonArray errors = doc.createNestedArray("errors");
  JsonArray stirrers = doc.createNestedArray("stirrers");
  JsonArray mfcs = doc.createNestedArray("mfcs");
  
  for (int j = 0; j < maxPoints; j++) {
    errors.add(ioConfig.doProfiles[index].points[j].error_mg_L);
    stirrers.add(ioConfig.doProfiles[index].points[j].stirrerOutput);
    mfcs.add(ioConfig.doProfiles[index].points[j].mfcOutput_mL_min);
  }
  
  String response;
  serializeJson(doc, response);
  server.send(200, "application/json", response);
}

void handleSaveDOProfile(uint8_t index) {
  if (index >= MAX_DO_PROFILES) {
    server.send(400, "application/json", "{\"error\":\"Invalid profile index\"}");
    return;
  }
  
  if (!server.hasArg("plain")) {
    server.send(400, "application/json", "{\"error\":\"No data provided\"}");
    return;
  }
  
  // 1KB buffer sufficient with efficient array format
  StaticJsonDocument<1024> doc;
  DeserializationError error = deserializeJson(doc, server.arg("plain"));
  
  if (error) {
    log(LOG_ERROR, true, "DO profile JSON parse error: %s\n", error.c_str());
    server.send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
    return;
  }
  
  // Validate number of points
  uint8_t numPoints = doc["numPoints"] | 0;
  if (numPoints > MAX_DO_PROFILE_POINTS) {
    server.send(400, "application/json", "{\"error\":\"Too many profile points (max 20)\"}");
    return;
  }
  
  // Update profile
  ioConfig.doProfiles[index].isActive = doc["isActive"] | true;
  strlcpy(ioConfig.doProfiles[index].name, doc["name"] | "", sizeof(ioConfig.doProfiles[index].name));
  ioConfig.doProfiles[index].numPoints = numPoints;
  
  // Parse profile points from efficient array format
  JsonArray errors = doc["errors"];
  JsonArray stirrers = doc["stirrers"];
  JsonArray mfcs = doc["mfcs"];
  
  if (errors && stirrers && mfcs) {
    for (int j = 0; j < numPoints && j < MAX_DO_PROFILE_POINTS; j++) {
      ioConfig.doProfiles[index].points[j].error_mg_L = errors[j] | 0.0f;
      ioConfig.doProfiles[index].points[j].stirrerOutput = stirrers[j] | 0.0f;
      ioConfig.doProfiles[index].points[j].mfcOutput_mL_min = mfcs[j] | 0.0f;
    }
  }
  
  // Save to flash
  saveIOConfig();
  
  // If this profile is currently active in the DO controller, update the controller
  if (ioConfig.doController.isActive && ioConfig.doController.activeProfileIndex == index) {
    // Send updated config to IO MCU
    IPC_ConfigDOController_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    
    cfg.index = 48;
    cfg.isActive = true;
    strncpy(cfg.name, ioConfig.doController.name, sizeof(cfg.name) - 1);
    cfg.enabled = ioConfig.doController.enabled;
    cfg.setpoint_mg_L = ioConfig.doController.setpoint_mg_L;
    
    // Copy updated profile points
    // Safety: limit to MAX_DO_PROFILE_POINTS to prevent overflow
    int numPoints = ioConfig.doProfiles[index].numPoints;
    cfg.numPoints = (numPoints < MAX_DO_PROFILE_POINTS) ? numPoints : MAX_DO_PROFILE_POINTS;
    for (int j = 0; j < cfg.numPoints; j++) {
      cfg.profileErrorValues[j] = ioConfig.doProfiles[index].points[j].error_mg_L;
      cfg.profileStirrerValues[j] = ioConfig.doProfiles[index].points[j].stirrerOutput;
      cfg.profileMFCValues[j] = ioConfig.doProfiles[index].points[j].mfcOutput_mL_min;
    }
    
    cfg.stirrerEnabled = ioConfig.doController.stirrerEnabled;
    cfg.stirrerType = ioConfig.doController.stirrerType;
    cfg.stirrerIndex = ioConfig.doController.stirrerIndex;
    cfg.stirrerMaxRPM = ioConfig.doController.stirrerMaxRPM;
    cfg.mfcEnabled = ioConfig.doController.mfcEnabled;
    cfg.mfcDeviceIndex = ioConfig.doController.mfcDeviceIndex;
    
    ipc.sendPacket(IPC_MSG_CONFIG_DO_CONTROLLER, (uint8_t*)&cfg, sizeof(cfg));
    log(LOG_INFO, false, "DO profile %d updated and applied to controller\n", index);
  }
  
  server.send(200, "application/json", "{\"success\":true,\"message\":\"Profile saved\"}");
}

void handleDeleteDOProfile(uint8_t index) {
  if (index >= MAX_DO_PROFILES) {
    server.send(400, "application/json", "{\"error\":\"Invalid profile index\"}");
    return;
  }
  
  // Check if this profile is currently active
  if (ioConfig.doController.isActive && ioConfig.doController.activeProfileIndex == index) {
    server.send(400, "application/json", "{\"error\":\"Cannot delete active profile. Switch to another profile first.\"}");
    return;
  }
  
  // Mark inactive and clear data
  ioConfig.doProfiles[index].isActive = false;
  memset(ioConfig.doProfiles[index].name, 0, sizeof(ioConfig.doProfiles[index].name));
  ioConfig.doProfiles[index].numPoints = 0;
  memset(ioConfig.doProfiles[index].points, 0, sizeof(ioConfig.doProfiles[index].points));
  
  saveIOConfig();
  
  log(LOG_INFO, false, "DO profile %d deleted\n", index);
  server.send(200, "application/json", "{\"success\":true,\"message\":\"Profile deleted\"}");
}

// ============================================================================
// Device Control (Peripheral Devices like MFC, pH controllers)
// ============================================================================

bool sendDeviceControlCommand(uint16_t controlIndex, DeviceControlCommand command, float setpoint = 0.0f) {
  IPC_DeviceControlCmd_t cmd;
  cmd.transactionId = generateTransactionId();
  cmd.index = controlIndex;
  cmd.objectType = OBJ_T_DEVICE_CONTROL;
  cmd.command = command;
  cmd.setpoint = setpoint;
  memset(cmd.reserved, 0, sizeof(cmd.reserved));
  
  bool sent = ipc.sendPacket(IPC_MSG_DEVICE_CONTROL, (uint8_t*)&cmd, sizeof(cmd));
  
  if (sent) {
    addPendingTransaction(cmd.transactionId, IPC_MSG_DEVICE_CONTROL, IPC_MSG_CONTROL_ACK, 1, controlIndex);
    log(LOG_DEBUG, false, "IPC TX: DeviceControl[%d] command=%d (txn=%d)\n", controlIndex, command, cmd.transactionId);
  }
  
  return sent;
}

void handleSetDeviceSetpoint(uint16_t controlIndex) {
  // Validate control index (50-69)
  if (controlIndex < 50 || controlIndex >= 70) {
    server.send(400, "application/json", "{\"error\":\"Invalid control index\"}");
    return;
  }
  
  // Parse JSON body
  StaticJsonDocument<256> doc;
  DeserializationError error = deserializeJson(doc, server.arg("plain"));
  
  if (error) {
    server.send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
    return;
  }
  
  if (!doc.containsKey("setpoint")) {
    server.send(400, "application/json", "{\"error\":\"Missing setpoint parameter\"}");
    return;
  }
  
  float setpoint = doc["setpoint"];
  
  // Send setpoint command via IPC
  bool sent = sendDeviceControlCommand(controlIndex, DEV_CMD_SET_SETPOINT, setpoint);
  
  if (sent) {
    log(LOG_INFO, false, "Set device %d setpoint: %.2f\n", controlIndex, setpoint);
    server.send(200, "application/json", "{\"success\":true,\"message\":\"Setpoint command sent\"}");
  } else {
    log(LOG_WARNING, false, "Failed to set device %d setpoint: IPC queue full\n", controlIndex);
    server.send(503, "application/json", "{\"error\":\"IPC queue full, try again\"}");
  }
}

void setupWebServer()
{
  // Initialize LittleFS for serving web files
  if (!LittleFS.begin())
  {
    log(LOG_ERROR, true, "LittleFS Mount Failed\n");
    return;
  }


  // Setup API endpoints that are defined in separate functions
  setupNetworkAPI();
  setupMqttAPI();
  setupTimeAPI();

  // NEW: Comprehensive status endpoint for the UI
  server.on("/api/status/all", HTTP_GET, handleGetAllStatus);

  // NEW: Scalable control endpoint
  server.on("/api/controls", HTTP_POST, handleUpdateControl);

  // NEW: System status endpoint for the UI
  server.on("/api/system/status", HTTP_GET, handleSystemStatus);

  // NEW: Sensors endpoint for the control tab
  server.on("/api/sensors", HTTP_GET, handleGetSensors);

  // SD Card File Manager API endpoint
  server.on("/api/sd/list", HTTP_GET, handleSDListDirectory);
  // server.on("/api/sd/download", HTTP_GET, handleSDDownloadFile);
  // server.on("/api/sd/view", HTTP_GET, handleSDViewFile);

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

  // Object Index API endpoints
  server.on("/api/inputs", HTTP_GET, handleGetInputs);
  
  // Configuration API endpoints
  server.on("/api/config/adc/0", HTTP_GET, []() { handleGetADCConfig(0); });
  server.on("/api/config/adc/1", HTTP_GET, []() { handleGetADCConfig(1); });
  server.on("/api/config/adc/2", HTTP_GET, []() { handleGetADCConfig(2); });
  server.on("/api/config/adc/3", HTTP_GET, []() { handleGetADCConfig(3); });
  server.on("/api/config/adc/4", HTTP_GET, []() { handleGetADCConfig(4); });
  server.on("/api/config/adc/5", HTTP_GET, []() { handleGetADCConfig(5); });
  server.on("/api/config/adc/6", HTTP_GET, []() { handleGetADCConfig(6); });
  server.on("/api/config/adc/7", HTTP_GET, []() { handleGetADCConfig(7); });
  
  server.on("/api/config/adc/0", HTTP_POST, []() { handleSaveADCConfig(0); });
  server.on("/api/config/adc/1", HTTP_POST, []() { handleSaveADCConfig(1); });
  server.on("/api/config/adc/2", HTTP_POST, []() { handleSaveADCConfig(2); });
  server.on("/api/config/adc/3", HTTP_POST, []() { handleSaveADCConfig(3); });
  server.on("/api/config/adc/4", HTTP_POST, []() { handleSaveADCConfig(4); });
  server.on("/api/config/adc/5", HTTP_POST, []() { handleSaveADCConfig(5); });
  server.on("/api/config/adc/6", HTTP_POST, []() { handleSaveADCConfig(6); });
  server.on("/api/config/adc/7", HTTP_POST, []() { handleSaveADCConfig(7); });
  
  // DAC Configuration endpoints (indices 8-9)
  server.on("/api/dac/8/config", HTTP_GET, []() { handleGetDACConfig(8); });
  server.on("/api/dac/9/config", HTTP_GET, []() { handleGetDACConfig(9); });
  
  server.on("/api/dac/8/config", HTTP_POST, []() { handleSaveDACConfig(8); });
  server.on("/api/dac/9/config", HTTP_POST, []() { handleSaveDACConfig(9); });
  
  // RTD Configuration endpoints (indices 10-12)
  server.on("/api/config/rtd/10", HTTP_GET, []() { handleGetRTDConfig(10); });
  server.on("/api/config/rtd/11", HTTP_GET, []() { handleGetRTDConfig(11); });
  server.on("/api/config/rtd/12", HTTP_GET, []() { handleGetRTDConfig(12); });
  
  server.on("/api/config/rtd/10", HTTP_POST, []() { handleSaveRTDConfig(10); });
  server.on("/api/config/rtd/11", HTTP_POST, []() { handleSaveRTDConfig(11); });
  server.on("/api/config/rtd/12", HTTP_POST, []() { handleSaveRTDConfig(12); });
  
  // GPIO Configuration endpoints (indices 13-20)
  server.on("/api/config/gpio/13", HTTP_GET, []() { handleGetGPIOConfig(13); });
  server.on("/api/config/gpio/14", HTTP_GET, []() { handleGetGPIOConfig(14); });
  server.on("/api/config/gpio/15", HTTP_GET, []() { handleGetGPIOConfig(15); });
  server.on("/api/config/gpio/16", HTTP_GET, []() { handleGetGPIOConfig(16); });
  server.on("/api/config/gpio/17", HTTP_GET, []() { handleGetGPIOConfig(17); });
  server.on("/api/config/gpio/18", HTTP_GET, []() { handleGetGPIOConfig(18); });
  server.on("/api/config/gpio/19", HTTP_GET, []() { handleGetGPIOConfig(19); });
  server.on("/api/config/gpio/20", HTTP_GET, []() { handleGetGPIOConfig(20); });
  
  server.on("/api/config/gpio/13", HTTP_POST, []() { handleSaveGPIOConfig(13); });
  server.on("/api/config/gpio/14", HTTP_POST, []() { handleSaveGPIOConfig(14); });
  server.on("/api/config/gpio/15", HTTP_POST, []() { handleSaveGPIOConfig(15); });
  server.on("/api/config/gpio/16", HTTP_POST, []() { handleSaveGPIOConfig(16); });
  server.on("/api/config/gpio/17", HTTP_POST, []() { handleSaveGPIOConfig(17); });
  server.on("/api/config/gpio/18", HTTP_POST, []() { handleSaveGPIOConfig(18); });
  server.on("/api/config/gpio/19", HTTP_POST, []() { handleSaveGPIOConfig(19); });
  server.on("/api/config/gpio/20", HTTP_POST, []() { handleSaveGPIOConfig(20); });
  
  // Energy Sensor Configuration endpoints (indices 31-32)
  server.on("/api/config/energy/31", HTTP_GET, []() { handleGetEnergySensorConfig(31); });
  server.on("/api/config/energy/32", HTTP_GET, []() { handleGetEnergySensorConfig(32); });
  
  server.on("/api/config/energy/31", HTTP_POST, []() { handleSaveEnergySensorConfig(31); });
  server.on("/api/config/energy/32", HTTP_POST, []() { handleSaveEnergySensorConfig(32); });

  // Device Sensor Configuration endpoints (indices 70-99)
  for (uint8_t i = 70; i <= 99; i++) {
    String getPath = "/api/config/devicesensor/" + String(i);
    String postPath = "/api/config/devicesensor/" + String(i);
    server.on(getPath.c_str(), HTTP_GET, [i]() { handleGetDeviceSensorConfig(i); });
    server.on(postPath.c_str(), HTTP_POST, [i]() { handleSaveDeviceSensorConfig(i); });
  }

  // COM Port Configuration endpoints (indices 0-3)
  server.on("/api/config/comport/0", HTTP_GET, []() { handleGetComPortConfig(0); });
  server.on("/api/config/comport/1", HTTP_GET, []() { handleGetComPortConfig(1); });
  server.on("/api/config/comport/2", HTTP_GET, []() { handleGetComPortConfig(2); });
  server.on("/api/config/comport/3", HTTP_GET, []() { handleGetComPortConfig(3); });
  
  server.on("/api/config/comport/0", HTTP_POST, []() { handleSaveComPortConfig(0); });
  server.on("/api/config/comport/1", HTTP_POST, []() { handleSaveComPortConfig(1); });
  server.on("/api/config/comport/2", HTTP_POST, []() { handleSaveComPortConfig(2); });
  server.on("/api/config/comport/3", HTTP_POST, []() { handleSaveComPortConfig(3); });
  
  // Get all COM ports status
  server.on("/api/comports", HTTP_GET, handleGetComPorts);

  // ============================================================================
  // Devices API Endpoints
  // ============================================================================
  
  // Get all devices
  server.on("/api/devices", HTTP_GET, handleGetDevices);
  
  // Create new device
  server.on("/api/devices", HTTP_POST, handleCreateDevice);

  // ============================================================================
  // Outputs API Endpoints
  // ============================================================================
  
  // Get all outputs status (for monitoring)
  server.on("/api/outputs", HTTP_GET, handleGetOutputs);
  
  // Digital Output Configuration (indices 21-25)
  server.on("/api/config/output/21", HTTP_GET, []() { handleGetDigitalOutputConfig(21); });
  server.on("/api/config/output/22", HTTP_GET, []() { handleGetDigitalOutputConfig(22); });
  server.on("/api/config/output/23", HTTP_GET, []() { handleGetDigitalOutputConfig(23); });
  server.on("/api/config/output/24", HTTP_GET, []() { handleGetDigitalOutputConfig(24); });
  server.on("/api/config/output/25", HTTP_GET, []() { handleGetDigitalOutputConfig(25); });
  
  server.on("/api/config/output/21", HTTP_POST, []() { handleSaveDigitalOutputConfig(21); });
  server.on("/api/config/output/22", HTTP_POST, []() { handleSaveDigitalOutputConfig(22); });
  server.on("/api/config/output/23", HTTP_POST, []() { handleSaveDigitalOutputConfig(23); });
  server.on("/api/config/output/24", HTTP_POST, []() { handleSaveDigitalOutputConfig(24); });
  server.on("/api/config/output/25", HTTP_POST, []() { handleSaveDigitalOutputConfig(25); });
  
  // Digital Output Runtime Control
  server.on("/api/output/21/state", HTTP_POST, []() { handleSetOutputState(21); });
  server.on("/api/output/22/state", HTTP_POST, []() { handleSetOutputState(22); });
  server.on("/api/output/23/state", HTTP_POST, []() { handleSetOutputState(23); });
  server.on("/api/output/24/state", HTTP_POST, []() { handleSetOutputState(24); });
  server.on("/api/output/25/state", HTTP_POST, []() { handleSetOutputState(25); });
  
  server.on("/api/output/21/value", HTTP_POST, []() { handleSetOutputValue(21); });
  server.on("/api/output/22/value", HTTP_POST, []() { handleSetOutputValue(22); });
  server.on("/api/output/23/value", HTTP_POST, []() { handleSetOutputValue(23); });
  server.on("/api/output/24/value", HTTP_POST, []() { handleSetOutputValue(24); });
  server.on("/api/output/25/value", HTTP_POST, []() { handleSetOutputValue(25); });
  
  // Analog Output (DAC) Runtime Control
  server.on("/api/dac/8/value", HTTP_POST, []() { handleSetAnalogOutputValue(8); });
  server.on("/api/dac/9/value", HTTP_POST, []() { handleSetAnalogOutputValue(9); });
  
  // Stepper Motor Configuration & Control
  server.on("/api/config/stepper", HTTP_GET, handleGetStepperConfig);
  server.on("/api/config/stepper", HTTP_POST, handleSaveStepperConfig);
  server.on("/api/stepper/rpm", HTTP_POST, handleSetStepperRPM);
  server.on("/api/stepper/direction", HTTP_POST, handleSetStepperDirection);
  server.on("/api/stepper/start", HTTP_POST, handleStartStepper);
  server.on("/api/stepper/stop", HTTP_POST, handleStopStepper);
  
  // DC Motor Configuration & Control (indices 27-30)
  server.on("/api/config/dcmotor/27", HTTP_GET, []() { handleGetDCMotorConfig(27); });
  server.on("/api/config/dcmotor/28", HTTP_GET, []() { handleGetDCMotorConfig(28); });
  server.on("/api/config/dcmotor/29", HTTP_GET, []() { handleGetDCMotorConfig(29); });
  server.on("/api/config/dcmotor/30", HTTP_GET, []() { handleGetDCMotorConfig(30); });
  
  server.on("/api/config/dcmotor/27", HTTP_POST, []() { handleSaveDCMotorConfig(27); });
  server.on("/api/config/dcmotor/28", HTTP_POST, []() { handleSaveDCMotorConfig(28); });
  server.on("/api/config/dcmotor/29", HTTP_POST, []() { handleSaveDCMotorConfig(29); });
  server.on("/api/config/dcmotor/30", HTTP_POST, []() { handleSaveDCMotorConfig(30); });
  
  server.on("/api/dcmotor/27/power", HTTP_POST, []() { handleSetDCMotorPower(27); });
  server.on("/api/dcmotor/28/power", HTTP_POST, []() { handleSetDCMotorPower(28); });
  server.on("/api/dcmotor/29/power", HTTP_POST, []() { handleSetDCMotorPower(29); });
  server.on("/api/dcmotor/30/power", HTTP_POST, []() { handleSetDCMotorPower(30); });
  
  server.on("/api/dcmotor/27/direction", HTTP_POST, []() { handleSetDCMotorDirection(27); });
  server.on("/api/dcmotor/28/direction", HTTP_POST, []() { handleSetDCMotorDirection(28); });
  server.on("/api/dcmotor/29/direction", HTTP_POST, []() { handleSetDCMotorDirection(29); });
  server.on("/api/dcmotor/30/direction", HTTP_POST, []() { handleSetDCMotorDirection(30); });
  
  server.on("/api/dcmotor/27/start", HTTP_POST, []() { handleStartDCMotor(27); });
  server.on("/api/dcmotor/28/start", HTTP_POST, []() { handleStartDCMotor(28); });
  server.on("/api/dcmotor/29/start", HTTP_POST, []() { handleStartDCMotor(29); });
  server.on("/api/dcmotor/30/start", HTTP_POST, []() { handleStartDCMotor(30); });
  
  server.on("/api/dcmotor/27/stop", HTTP_POST, []() { handleStopDCMotor(27); });
  server.on("/api/dcmotor/28/stop", HTTP_POST, []() { handleStopDCMotor(28); });
  server.on("/api/dcmotor/29/stop", HTTP_POST, []() { handleStopDCMotor(29); });
  server.on("/api/dcmotor/30/stop", HTTP_POST, []() { handleStopDCMotor(30); });
  
  // ============================================================================
  // Temperature Controllers & pH Controller API Endpoints (indices 40-43)
  // ============================================================================
  
  // Get all controllers status (temp + pH)
  server.on("/api/controllers", HTTP_GET, handleGetControllers);
  
  // Configuration endpoints (used by web UI)
  server.on("/api/config/tempcontroller/40", HTTP_GET, []() { handleGetTempControllerConfig(40); });
  server.on("/api/config/tempcontroller/41", HTTP_GET, []() { handleGetTempControllerConfig(41); });
  server.on("/api/config/tempcontroller/42", HTTP_GET, []() { handleGetTempControllerConfig(42); });
  
  server.on("/api/config/tempcontroller/40", HTTP_POST, []() { handleSaveTempControllerConfig(40); });
  server.on("/api/config/tempcontroller/41", HTTP_POST, []() { handleSaveTempControllerConfig(41); });
  server.on("/api/config/tempcontroller/42", HTTP_POST, []() { handleSaveTempControllerConfig(42); });
  
  server.on("/api/config/phcontroller/43", HTTP_GET, handleGetpHControllerConfig);
  server.on("/api/config/phcontroller/43", HTTP_POST, handleSavepHControllerConfig);
  
  // Temperature controller control endpoints (runtime actions)
  server.on("/api/controller/40/setpoint", HTTP_POST, []() { handleUpdateControllerSetpoint(40); });
  server.on("/api/controller/41/setpoint", HTTP_POST, []() { handleUpdateControllerSetpoint(41); });
  server.on("/api/controller/42/setpoint", HTTP_POST, []() { handleUpdateControllerSetpoint(42); });
  
  server.on("/api/controller/40/enable", HTTP_POST, []() { handleEnableController(40); });
  server.on("/api/controller/41/enable", HTTP_POST, []() { handleEnableController(41); });
  server.on("/api/controller/42/enable", HTTP_POST, []() { handleEnableController(42); });
  
  server.on("/api/controller/40/disable", HTTP_POST, []() { handleDisableController(40); });
  server.on("/api/controller/41/disable", HTTP_POST, []() { handleDisableController(41); });
  server.on("/api/controller/42/disable", HTTP_POST, []() { handleDisableController(42); });
  
  server.on("/api/controller/40/start", HTTP_POST, []() { handleStartController(40); });
  server.on("/api/controller/41/start", HTTP_POST, []() { handleStartController(41); });
  server.on("/api/controller/42/start", HTTP_POST, []() { handleStartController(42); });
  
  server.on("/api/controller/40/stop", HTTP_POST, []() { handleStopController(40); });
  server.on("/api/controller/41/stop", HTTP_POST, []() { handleStopController(41); });
  server.on("/api/controller/42/stop", HTTP_POST, []() { handleStopController(42); });
  
  server.on("/api/controller/40/autotune", HTTP_POST, []() { handleStartAutotune(40); });
  server.on("/api/controller/41/autotune", HTTP_POST, []() { handleStartAutotune(41); });
  server.on("/api/controller/42/autotune", HTTP_POST, []() { handleStartAutotune(42); });
  
  // pH controller control endpoints (runtime actions)
  server.on("/api/phcontroller/43/setpoint", HTTP_POST, handleUpdatepHSetpoint);
  server.on("/api/phcontroller/43/enable", HTTP_POST, handleEnablepHController);
  server.on("/api/phcontroller/43/disable", HTTP_POST, handleDisablepHController);
  server.on("/api/phcontroller/43/dose-acid", HTTP_POST, handleDosepHAcid);
  server.on("/api/phcontroller/43/dose-alkaline", HTTP_POST, handleDosepHAlkaline);
  server.on("/api/phcontroller/43/reset-acid-volume", HTTP_POST, handleResetpHAcidVolume);
  server.on("/api/phcontroller/43/reset-alkaline-volume", HTTP_POST, handleResetpHAlkalineVolume);
  
  // Flow controller config endpoints (indices 44-47)
  server.on("/api/config/flowcontroller/44", HTTP_GET, []() { handleGetFlowControllerConfig(44); });
  server.on("/api/config/flowcontroller/44", HTTP_POST, []() { handleSaveFlowControllerConfig(44); });
  server.on("/api/config/flowcontroller/45", HTTP_GET, []() { handleGetFlowControllerConfig(45); });
  server.on("/api/config/flowcontroller/45", HTTP_POST, []() { handleSaveFlowControllerConfig(45); });
  server.on("/api/config/flowcontroller/46", HTTP_GET, []() { handleGetFlowControllerConfig(46); });
  server.on("/api/config/flowcontroller/46", HTTP_POST, []() { handleSaveFlowControllerConfig(46); });
  server.on("/api/config/flowcontroller/47", HTTP_GET, []() { handleGetFlowControllerConfig(47); });
  server.on("/api/config/flowcontroller/47", HTTP_POST, []() { handleSaveFlowControllerConfig(47); });
  
  // Flow controller control endpoints (runtime actions)
  server.on("/api/flowcontroller/44/flowrate", HTTP_POST, []() { handleSetFlowRate(44); });
  server.on("/api/flowcontroller/44/enable", HTTP_POST, []() { handleEnableFlowController(44); });
  server.on("/api/flowcontroller/44/disable", HTTP_POST, []() { handleDisableFlowController(44); });
  server.on("/api/flowcontroller/44/dose", HTTP_POST, []() { handleManualFlowDose(44); });
  server.on("/api/flowcontroller/44/reset-volume", HTTP_POST, []() { handleResetFlowVolume(44); });
  
  server.on("/api/flowcontroller/45/flowrate", HTTP_POST, []() { handleSetFlowRate(45); });
  server.on("/api/flowcontroller/45/enable", HTTP_POST, []() { handleEnableFlowController(45); });
  server.on("/api/flowcontroller/45/disable", HTTP_POST, []() { handleDisableFlowController(45); });
  server.on("/api/flowcontroller/45/dose", HTTP_POST, []() { handleManualFlowDose(45); });
  server.on("/api/flowcontroller/45/reset-volume", HTTP_POST, []() { handleResetFlowVolume(45); });
  
  server.on("/api/flowcontroller/46/flowrate", HTTP_POST, []() { handleSetFlowRate(46); });
  server.on("/api/flowcontroller/46/enable", HTTP_POST, []() { handleEnableFlowController(46); });
  server.on("/api/flowcontroller/46/disable", HTTP_POST, []() { handleDisableFlowController(46); });
  server.on("/api/flowcontroller/46/dose", HTTP_POST, []() { handleManualFlowDose(46); });
  server.on("/api/flowcontroller/46/reset-volume", HTTP_POST, []() { handleResetFlowVolume(46); });
  
  server.on("/api/flowcontroller/47/flowrate", HTTP_POST, []() { handleSetFlowRate(47); });
  server.on("/api/flowcontroller/47/enable", HTTP_POST, []() { handleEnableFlowController(47); });
  server.on("/api/flowcontroller/47/disable", HTTP_POST, []() { handleDisableFlowController(47); });
  server.on("/api/flowcontroller/47/dose", HTTP_POST, []() { handleManualFlowDose(47); });
  server.on("/api/flowcontroller/47/reset-volume", HTTP_POST, []() { handleResetFlowVolume(47); });
  
  // DO controller config endpoints (index 48)
  server.on("/api/config/docontroller/48", HTTP_GET, handleGetDOControllerConfig);
  server.on("/api/config/docontroller/48", HTTP_POST, handleSaveDOControllerConfig);
  server.on("/api/config/docontroller/48", HTTP_DELETE, handleDeleteDOController);
  
  // DO controller control endpoints (runtime actions)
  server.on("/api/docontroller/48/setpoint", HTTP_POST, handleSetDOSetpoint);
  server.on("/api/docontroller/48/enable", HTTP_POST, handleEnableDOController);
  server.on("/api/docontroller/48/disable", HTTP_POST, handleDisableDOController);
  
  // DO profile endpoints (indices 0-2)
  server.on("/api/doprofiles", HTTP_GET, handleGetAllDOProfiles);
  server.on("/api/doprofile/0", HTTP_GET, []() { handleGetDOProfile(0); });
  server.on("/api/doprofile/0", HTTP_POST, []() { handleSaveDOProfile(0); });
  server.on("/api/doprofile/0", HTTP_DELETE, []() { handleDeleteDOProfile(0); });
  server.on("/api/doprofile/1", HTTP_GET, []() { handleGetDOProfile(1); });
  server.on("/api/doprofile/1", HTTP_POST, []() { handleSaveDOProfile(1); });
  server.on("/api/doprofile/1", HTTP_DELETE, []() { handleDeleteDOProfile(1); });
  server.on("/api/doprofile/2", HTTP_GET, []() { handleGetDOProfile(2); });
  server.on("/api/doprofile/2", HTTP_POST, []() { handleSaveDOProfile(2); });
  server.on("/api/doprofile/2", HTTP_DELETE, []() { handleDeleteDOProfile(2); });
  
  // Note: RESTful controller endpoints are handled dynamically in onNotFound():
  //   - GET    /api/controller/{40-43}     - Get controller config (REST)
  //   - PUT    /api/controller/{40-43}     - Save controller config (REST)
  //   - DELETE /api/controller/{40-43}     - Delete controller (REST)
  //
  // Both endpoint styles are supported:
  //   1. Config endpoints: /api/config/tempcontroller/{40-42} (static, for backward compatibility)
  //   2. REST endpoints: /api/controller/{40-43} (dynamic, cleaner URLs)
  //   3. Control endpoints: /api/controller/{40-42}/{action} and /api/phcontroller/43/{action} (static)
  
  // ============================================================================
  // Device Control API Endpoints (Peripheral Devices - Control indices 50-69)
  // ============================================================================
  
  // Note: Device control endpoints are handled dynamically in onNotFound():
  //   - POST /api/device/{50-69}/setpoint    - Set device setpoint

  // Handle dynamic routes and static files
  server.onNotFound([]() {
    String uri = server.uri();
    Serial.printf("[WEB] onNotFound: %s (method: %d)\\n", uri.c_str(), server.method());
    
    // Check device control routes first
    if (uri.startsWith("/api/device/")) {
      String remaining = uri.substring(12);
      int slashPos = remaining.indexOf('/');
      if (slashPos > 0) {
        String indexStr = remaining.substring(0, slashPos);
        String action = remaining.substring(slashPos + 1);
        uint16_t controlIndex = indexStr.toInt();
        if (controlIndex >= 50 && controlIndex < 70 && indexStr.length() > 0) {
          if (server.method() == HTTP_POST) {
            if (action == "setpoint") {
              handleSetDeviceSetpoint(controlIndex);
              return;
            } 
          }
        }
      }
    }
    
    // Check if this is a device API route: /api/devices/{number}
    if (uri.startsWith("/api/devices/")) {
      // Extract the index part after "/api/devices/"
      String indexStr = uri.substring(13);
      
      // Remove any query parameters or trailing slashes
      int queryPos = indexStr.indexOf('?');
      if (queryPos > 0) {
        indexStr = indexStr.substring(0, queryPos);
      }
      int slashPos = indexStr.indexOf('/');
      if (slashPos > 0) {
        indexStr = indexStr.substring(0, slashPos);
      }
      
      // Parse as integer
      uint8_t index = indexStr.toInt();
      
      Serial.printf("[WEB] Device API route detected: index=%d, indexStr='%s'\n", index, indexStr.c_str());
      
      // Validate it's in the dynamic index range (70-99)
      // All devices now use sensor indices 70-99, control indices are auto-mapped to 50-69
      if ((index >= DYNAMIC_INDEX_START && index <= DYNAMIC_INDEX_END) && indexStr.length() > 0) {
        // Valid device route - dispatch to appropriate handler
        HTTPMethod method = server.method();
        Serial.printf("[WEB] Dispatching to device handler (method: %d)\n", method);
        if (method == HTTP_GET) {
          handleGetDevice();
          return;
        } else if (method == HTTP_PUT) {
          handleUpdateDevice();
          return;
        } else if (method == HTTP_DELETE) {
          handleDeleteDevice();
          return;
        } else {
          server.send(405, "application/json", "{\"error\":\"Method not allowed\"}");
          return;
        }
      }
    }
    
    // Check if this is a controller API route: /api/controller/{number}
    if (uri.startsWith("/api/controller/")) {
      // Extract the index part after "/api/controller/"
      String indexStr = uri.substring(16);
      
      // Remove any query parameters or trailing slashes
      int queryPos = indexStr.indexOf('?');
      if (queryPos > 0) {
        indexStr = indexStr.substring(0, queryPos);
      }
      int slashPos = indexStr.indexOf('/');
      if (slashPos > 0) {
        indexStr = indexStr.substring(0, slashPos);
      }
      
      // Parse as integer
      uint8_t index = indexStr.toInt();
      
      Serial.printf("[WEB] Controller API route detected: index=%d, indexStr='%s'\n", index, indexStr.c_str());
      
      // Validate controller range: temp controllers (40-42), pH controller (43), flow controllers (44-47), or DO controller (48)
      bool validRange = (index >= 40 && index < 40 + MAX_TEMP_CONTROLLERS) ||  // 40-42
                        index == 43 ||                                           // pH controller
                        (index >= 44 && index < 44 + MAX_FLOW_CONTROLLERS) ||   // 44-47
                        index == 48;                                             // DO controller
      
      if (validRange && indexStr.length() > 0) {
        // Valid controller route - dispatch to appropriate handler
        HTTPMethod method = server.method();
        Serial.printf("[WEB] Dispatching to controller handler (method: %d)\n", method);
        
        if (index == 43) {
          // pH Controller (index 43)
          if (method == HTTP_GET) {
            handleGetpHControllerConfig();
            return;
          } else if (method == HTTP_PUT) {
            handleSavepHControllerConfig();
            return;
          } else if (method == HTTP_DELETE) {
            handleDeletepHController();
            return;
          } else {
            server.send(405, "application/json", "{\"error\":\"Method not allowed\"}");
            return;
          }
        } else if (index >= 44 && index < 44 + MAX_FLOW_CONTROLLERS) {
          // Flow Controllers (44-47)
          if (method == HTTP_GET) {
            handleGetFlowControllerConfig(index);
            return;
          } else if (method == HTTP_PUT) {
            handleSaveFlowControllerConfig(index);
            return;
          } else if (method == HTTP_DELETE) {
            handleDeleteFlowController(index);
            return;
          } else {
            server.send(405, "application/json", "{\"error\":\"Method not allowed\"}");
            return;
          }
        } else if (index == 48) {
          // DO Controller (index 48)
          if (method == HTTP_GET) {
            handleGetDOControllerConfig();
            return;
          } else if (method == HTTP_PUT) {
            handleSaveDOControllerConfig();
            return;
          } else if (method == HTTP_DELETE) {
            handleDeleteDOController();
            return;
          } else {
            server.send(405, "application/json", "{\"error\":\"Method not allowed\"}");
            return;
          }
        } else {
          // Temperature Controllers (40-42)
          if (method == HTTP_GET) {
            handleGetTempControllerConfig(index);
            return;
          } else if (method == HTTP_PUT) {
            handleSaveTempControllerConfig(index);
            return;
          } else if (method == HTTP_DELETE) {
            handleDeleteController(index);
            return;
          } else {
            server.send(405, "application/json", "{\"error\":\"Method not allowed\"}");
            return;
          }
        }
      }
    }
    
    // Not a device API route - try serving static file
    Serial.printf("[WEB] Serving static file: %s\n", uri.c_str());
    handleFile(uri.c_str());
  });

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
  doc["mqttPassword"] = ""; // never return stored password
  doc["mqttPublishIntervalMs"] = networkConfig.mqttPublishIntervalMs;
  doc["mqttDevicePrefix"] = networkConfig.mqttDevicePrefix;
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
        strlcpy(networkConfig.mqttBroker, doc["mqttBroker"] | "", sizeof(networkConfig.mqttBroker));
        networkConfig.mqttPort = doc["mqttPort"] | 1883;
        strlcpy(networkConfig.mqttUsername, doc["mqttUsername"] | "", sizeof(networkConfig.mqttUsername));
        const char* newPassword = doc["mqttPassword"] | "";
        if (strlen(newPassword) > 0) {
            strlcpy(networkConfig.mqttPassword, newPassword, sizeof(networkConfig.mqttPassword));
        }
    // Optional fields
    if (doc.containsKey("mqttPublishIntervalMs")) {
      networkConfig.mqttPublishIntervalMs = doc["mqttPublishIntervalMs"];
    }
    if (doc.containsKey("mqttDevicePrefix")) {
      strlcpy(networkConfig.mqttDevicePrefix, doc["mqttDevicePrefix"], sizeof(networkConfig.mqttDevicePrefix));
    }
  saveNetworkConfig();
  // Apply MQTT config immediately and attempt reconnect
  mqttApplyConfigAndReconnect();
  server.send(200, "application/json", "{\"status\":\"success\",\"message\":\"MQTT configuration applied\"}");
    });
    
  // MQTT topics API endpoints not needed at this stage
  // Diagnostics endpoint
  server.on("/api/mqtt/diag", HTTP_GET, []() {
    StaticJsonDocument<256> doc;
    doc["broker"] = networkConfig.mqttBroker;
    doc["port"] = networkConfig.mqttPort;
    doc["connected"] = mqttIsConnected();
    doc["state"] = mqttGetState();
    doc["prefix"] = mqttGetDeviceTopicPrefix();
    String response;
    serializeJson(doc, response);
    server.send(200, "application/json", response);
  });
}

void setupTimeAPI()
{
  server.on("/api/time", HTTP_GET, []()
            {
        Serial.println("[WEB] /api/time GET request received");
        StaticJsonDocument<256> doc;
        DateTime dt;
        if (getGlobalDateTime(dt)) {
            Serial.println("[WEB] Successfully got datetime");
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
            Serial.printf("[WEB] Sending /api/time response (%d bytes)\n", response.length());
            server.send(200, "application/json", response);
            Serial.println("[WEB] /api/time response sent successfully");
        } else {
            Serial.println("[WEB] ERROR: Failed to get current time");
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

void handleFileManager(void) {
  // Check if SD card is ready
  if (!sdInfo.ready) {
    server.send(503, "application/json", "{\"error\":\"SD card not available\"}");
    return;
  }
  
  // Serve the main index page since file manager is now integrated
  handleRoot();
}

void handleFileManagerPage(void) {
  // Redirects to handleRoot (index.html) as file manager is now integrated
  handleRoot();
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
  if (strcmp(path, "/") == 0 || strcmp(path, "/index.html") == 0)
    contentType = "text/html";
  else if (strstr(path, ".html"))
    contentType = "text/html";
  else if (strstr(path, ".css"))
    contentType = "text/css";
  else if (strstr(path, ".js"))
    contentType = "application/javascript";
  else if (strstr(path, ".json"))
    contentType = "application/json";
  else if (strstr(path, ".ico"))
    contentType = "image/x-icon";
  else if (strstr(path, ".png"))
    contentType = "image/png";
  else if (strstr(path, ".jpg") || strstr(path, ".jpeg"))
    contentType = "image/jpeg";
  else if (strstr(path, ".gif"))
    contentType = "image/gif";
  else if (strstr(path, ".svg"))
    contentType = "image/svg+xml";
  else
    contentType = "text/plain";

  String filePath = path;
  if (filePath.endsWith("/"))
    filePath += "index.html";
  if (!filePath.startsWith("/"))
    filePath = "/" + filePath;

  Serial.printf("[WEB] Request: %s (type: %s)\n", filePath.c_str(), contentType.c_str());

  if (LittleFS.exists(filePath))
  {
    File file = LittleFS.open(filePath, "r");
    if (!file) {
      Serial.printf("[WEB] ERROR: Failed to open file: %s\n", filePath.c_str());
      server.send(500, "text/plain", "Failed to open file");
      return;
    }
    
    size_t fileSize = file.size();
    unsigned long startTime = millis();
    Serial.printf("[WEB] Serving file: %s (%d bytes)\n", filePath.c_str(), fileSize);
    
    size_t sent = server.streamFile(file, contentType);
    file.close();
    
    unsigned long elapsed = millis() - startTime;
    Serial.printf("[WEB] Sent %d/%d bytes in %lu ms\n", sent, fileSize, elapsed);
  }
  else
  {
    Serial.printf("[WEB] File not found: %s\n", filePath.c_str());
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

void handleSDDownloadFile(void) {
  if (sdLocked) {
    server.send(423, "application/json", "{\"error\":\"SD card is locked\"}");
    return;
  }
  
  if (!sdInfo.ready) {
    server.send(503, "application/json", "{\"error\":\"SD card not available\"}");
    return;
  }
  
  // Get the requested file path from the query parameter
  String path = server.hasArg("path") ? server.arg("path") : "";
  
  if (path.length() == 0) {
    server.send(400, "application/json", "{\"error\":\"File path not specified\"}");
    return;
  }
  
  // Make sure path starts with a forward slash
  if (!path.startsWith("/")) {
    path = "/" + path;
  }
  
  sdLocked = true;
  
  // Check if the file exists
  if (!sd.exists(path.c_str())) {
    sdLocked = false;
    server.send(404, "application/json", "{\"error\":\"File not found\"}");
    return;
  }
  
  // Open the file
  FsFile file = sd.open(path.c_str(), O_RDONLY);
  
  if (!file) {
    sdLocked = false;
    server.send(500, "application/json", "{\"error\":\"Failed to open file\"}");
    return;
  }
  
  if (file.isDirectory()) {
    file.close();
    sdLocked = false;
    server.send(400, "application/json", "{\"error\":\"Path is a directory, not a file\"}");
    return;
  }
  
  // Get file size
  size_t fileSize = file.size();
  
  // Check file size limit
  if (fileSize > MAX_DOWNLOAD_SIZE) {
    file.close();
    sdLocked = false;
    char errorMsg[128];
    snprintf(errorMsg, sizeof(errorMsg), 
             "{\"error\":\"File is too large for download (%u bytes). Maximum size is %u bytes.\"}",
             fileSize, MAX_DOWNLOAD_SIZE);
    server.send(413, "application/json", errorMsg);
    return;
  }
  
  // Get the filename from the path
  String fileName = path;
  int lastSlash = fileName.lastIndexOf('/');
  if (lastSlash >= 0) {
    fileName = fileName.substring(lastSlash + 1);
  }
  
  // Enhanced headers to force download with the correct filename
  String contentDisposition = "attachment; filename=\"" + fileName + "\"; filename*=UTF-8''" + fileName;
  
  // Use simpler header approach to avoid memory issues
  server.sendHeader("Content-Type", "application/octet-stream");
  server.sendHeader("Content-Disposition", contentDisposition);
  server.sendHeader("Cache-Control", "no-cache");
  
  // Set a watchdog timer and timeout to prevent system hangs
  uint32_t startTime = millis();
  uint32_t lastProgressTime = startTime;
  const uint32_t timeout = 30000; // 30 second timeout
  
  WiFiClient client = server.client();
  
  // Stream the file in chunks with timeout checks
  const size_t bufferSize = 1024; // Smaller buffer size for better reliability
  uint8_t buffer[bufferSize];
  size_t bytesRead;
  size_t totalBytesRead = 0;
  bool timeoutOccurred = false;
  
  server.setContentLength(fileSize);
  server.send(200, "application/octet-stream", ""); // Send headers only
  
  // Stream file with careful progress monitoring
  while (totalBytesRead < fileSize) {
    // Check for timeout
    if (millis() - lastProgressTime > timeout) {
      log(LOG_WARNING, true, "Timeout occurred during file download\n");
      timeoutOccurred = true;
      break;
    }
    
    // Read a chunk from the file
    bytesRead = file.read(buffer, min(bufferSize, fileSize - totalBytesRead));
    
    if (bytesRead == 0) {
      // End of file or error condition
      break;
    }
    
    // Write chunk to client
    if (client.write(buffer, bytesRead) != bytesRead) {
      // Client disconnected or write error
      log(LOG_WARNING, true, "Client write error during file download\n");
      break;
    }
    
    totalBytesRead += bytesRead;
    lastProgressTime = millis(); // Update progress timer
    
    // Allow other processes to run
    yield();
  }
  
  // Clean up
  file.close();
  sdLocked = false;
  
  if (timeoutOccurred) {
    log(LOG_ERROR, true, "File download timed out after %u bytes\n", totalBytesRead);
  } else if (totalBytesRead == fileSize) {
    log(LOG_INFO, true, "File download completed successfully: %s (%u bytes)\n", 
        fileName.c_str(), totalBytesRead);
  } else {
    log(LOG_WARNING, true, "File download incomplete: %u of %u bytes transferred\n", 
        totalBytesRead, fileSize);
  }
}

void handleSDViewFile(void) {
  if (sdLocked) {
    server.send(423, "application/json", "{\"error\":\"SD card is locked\"}");
    return;
  }
  
  if (!sdInfo.ready) {
    server.send(503, "application/json", "{\"error\":\"SD card not available\"}");
    return;
  }
  
  // Get the requested file path from the query parameter
  String path = server.hasArg("path") ? server.arg("path") : "";
  
  if (path.length() == 0) {
    server.send(400, "application/json", "{\"error\":\"File path not specified\"}");
    return;
  }
  
  // Make sure path starts with a forward slash
  if (!path.startsWith("/")) {
    path = "/" + path;
  }
  
  sdLocked = true;
  
  // Check if the file exists
  if (!sd.exists(path.c_str())) {
    sdLocked = false;
    server.send(404, "application/json", "{\"error\":\"File not found\"}");
    return;
  }
  
  // Open the file
  FsFile file = sd.open(path.c_str(), O_RDONLY);
  
  if (!file) {
    sdLocked = false;
    server.send(500, "application/json", "{\"error\":\"Failed to open file\"}");
    return;
  }
  
  if (file.isDirectory()) {
    file.close();
    sdLocked = false;
    server.send(400, "application/json", "{\"error\":\"Path is a directory, not a file\"}");
    return;
  }
  
  // Get file size
  size_t fileSize = file.size();
  
  // Get the filename from the path
  String fileName = path;
  int lastSlash = fileName.lastIndexOf('/');
  if (lastSlash >= 0) {
    fileName = fileName.substring(lastSlash + 1);
  }
  
  // Determine content type based on file extension
  String contentType = "text/plain";
  if (fileName.endsWith(".html") || fileName.endsWith(".htm")) contentType = "text/html";
  else if (fileName.endsWith(".css")) contentType = "text/css";
  else if (fileName.endsWith(".js")) contentType = "application/javascript";
  else if (fileName.endsWith(".json")) contentType = "application/json";
  else if (fileName.endsWith(".png")) contentType = "image/png";
  else if (fileName.endsWith(".jpg") || fileName.endsWith(".jpeg")) contentType = "image/jpeg";
  else if (fileName.endsWith(".gif")) contentType = "image/gif";
  else if (fileName.endsWith(".ico")) contentType = "image/x-icon";
  else if (fileName.endsWith(".pdf")) contentType = "application/pdf";
  
  // Set headers for displaying in browser
  server.sendHeader("Content-Type", contentType);
  server.sendHeader("Content-Length", String(fileSize));
  server.sendHeader("Cache-Control", "max-age=86400");
  
  // Stream the file to the client
  WiFiClient client = server.client();
  
  // Use a buffer for more efficient file transfer
  const size_t bufferSize = 2048;
  uint8_t buffer[bufferSize];
  size_t bytesRead;
  
  do {
    bytesRead = file.read(buffer, bufferSize);
    if (bytesRead > 0) {
      client.write(buffer, bytesRead);
    }
  } while (bytesRead == bufferSize);
  
  file.close();
  sdLocked = false;
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

// SD Card File Manager API functions ---------------------------------------->
void handleSDListDirectory(void) {
  if (sdLocked) {
    server.send(423, "application/json", "{\"error\":\"SD card is locked\"}");
    return;
  }
  
  if (!sdInfo.ready) {
    server.send(503, "application/json", "{\"error\":\"SD card not available\"}");
    return;
  }
  
  // Get the requested directory path from the query parameter
  String path = server.hasArg("path") ? server.arg("path") : "/";
  
  // Make sure path starts with a forward slash
  if (!path.startsWith("/")) {
    path = "/" + path;
  }
  
  sdLocked = true;
  
  // Check if the path exists and is a directory
  if (!sd.exists(path.c_str())) {
    sdLocked = false;
    server.send(404, "application/json", "{\"error\":\"Directory not found\"}");
    return;
  }
  
  FsFile dir = sd.open(path.c_str());
  
  if (!dir.isDirectory()) {
    dir.close();
    sdLocked = false;
    server.send(400, "application/json", "{\"error\":\"Not a directory\"}");
    return;
  }
  
  // Create a JSON document for the response
  DynamicJsonDocument doc(16384); // Adjust size based on expected directory size
  
  doc["path"] = path;
  JsonArray files = doc.createNestedArray("files");
  JsonArray directories = doc.createNestedArray("directories");
  
  // Read all files and directories in the requested path
  dir.rewindDirectory();
  
  FsFile file;
  char filename[256];
  while (file.openNext(&dir)) {
    file.getName(filename, sizeof(filename));
    
    // Skip hidden files and . and ..
    if (filename[0] == '.') {
      file.close();
      continue;
    }
    
    // Create a JSON object for the file or directory
    if (file.isDirectory()) {
      JsonObject dirObj = directories.createNestedObject();
      dirObj["name"] = filename;
      
      // Calculate full path for this directory
      String fullPath = path;
      if (!path.endsWith("/")) fullPath += "/";
      fullPath += filename;
      dirObj["path"] = fullPath;
    } else {
      JsonObject fileObj = files.createNestedObject();
      fileObj["name"] = filename;
      fileObj["size"] = file.size();
      
      // Calculate full path for this file
      String fullPath = path;
      if (!path.endsWith("/")) fullPath += "/";
      fullPath += filename;
      fileObj["path"] = fullPath;
      
      // Add last modified date
      uint16_t fileDate, fileTime;
      file.getModifyDateTime(&fileDate, &fileTime);
      
      int year = FS_YEAR(fileDate);
      int month = FS_MONTH(fileDate);
      int day = FS_DAY(fileDate);
      int hour = FS_HOUR(fileTime);
      int minute = FS_MINUTE(fileTime);
      int second = FS_SECOND(fileTime);
      
      char dateTimeStr[32];
      snprintf(dateTimeStr, sizeof(dateTimeStr), "%04d-%02d-%02d %02d:%02d:%02d", 
               year, month, day, hour, minute, second);
      fileObj["modified"] = dateTimeStr;
    }
    
    file.close();
  }
  
  dir.close();
  sdLocked = false;
  
  // Send the JSON response
  String response;
  serializeJson(doc, response);
  server.send(200, "application/json", response);
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