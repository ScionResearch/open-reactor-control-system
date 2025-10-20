#include "network.h"
#include "mqttManager.h"
#include "../sys_init.h"
#include "../utils/statusManager.h"
#include "../utils/timeManager.h"
#include "../utils/ipcManager.h"
#include "../storage/sdManager.h"
#include "../controls/controlManager.h"
#include "../utils/objectCache.h"
#include "../config/ioConfig.h"

#include <ArduinoJson.h>

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
  if (!loadIOConfig())
  {
    // Set default configuration if load fails or file doesn't exist
    log(LOG_INFO, false, "IO config not found, creating defaults\n");
    setDefaultIOConfig();
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
  if (statusLocked || sdLocked) {
    server.send(503, "application/json", "{\"error\":\"Status temporarily unavailable\"}");
    return;
  }
  statusLocked = true;
  sdLocked = true;

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
  doc["ipc"] = status.ipcOK;
  doc["mqtt"] = status.mqttConnected;
  doc["modbus"] = status.modbusConnected;

  // SD card info
  JsonObject sd = doc.createNestedObject("sd");
  sd["inserted"] = sdInfo.inserted;
  sd["ready"] = sdInfo.ready;
  sd["capacityGB"] = sdInfo.cardSizeBytes * 0.000000001;
  sd["freeSpaceGB"] = sdInfo.cardFreeBytes * 0.000000001;
  sd["logFileSizeKB"] = sdInfo.logSizeBytes * 0.001;
  sd["sensorFileSizeKB"] = sdInfo.sensorSizeBytes * 0.001;

  statusLocked = false;
  sdLocked = false;

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
  StaticJsonDocument<2048> doc;  // Increased from 1536 to fit all inputs
  
  // Analog Inputs (ADC) - Indices 0-7
  JsonArray adc = doc.createNestedArray("adc");
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
  JsonArray rtd = doc.createNestedArray("rtd");
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
  JsonArray gpio = doc.createNestedArray("gpio");
  for (uint8_t i = 13; i < 21; i++) {
    ObjectCache::CachedObject* obj = objectCache.getObject(i);
    if (obj && obj->valid) {
      JsonObject o = gpio.createNestedObject();
      o["i"] = i;
      o["n"] = ioConfig.gpio[i - 13].name;  // Custom name from config
      o["s"] = (obj->value > 0.5) ? 1 : 0;  // State as boolean
      o["d"] = ioConfig.gpio[i - 13].showOnDashboard;  // Dashboard flag
      if (obj->flags & IPC_SENSOR_FLAG_FAULT) o["f"] = 1;
    } else {
      log(LOG_DEBUG, false, "GPIO %d skipped: obj=%p, valid=%d\n", 
          i, obj, obj ? obj->valid : 0);
    }
  }
  
  String response;
  serializeJson(doc, response);
  
  // Check for overflow
  if (doc.overflowed()) {
    log(LOG_ERROR, true, "JSON document overflow in /api/inputs! Increase buffer size.\n");
  }
  
  // Debug: Log the JSON response size
  log(LOG_DEBUG, false, "API /api/inputs response (%d bytes): %s\n", response.length(), response.c_str());
  
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
  cfg.index = index;
  strncpy(cfg.unit, ioConfig.adcInputs[index].unit, sizeof(cfg.unit) - 1);
  cfg.unit[sizeof(cfg.unit) - 1] = '\0';
  cfg.calScale = ioConfig.adcInputs[index].cal.scale;
  cfg.calOffset = ioConfig.adcInputs[index].cal.offset;
  
  log(LOG_DEBUG, false, "handleSaveADCConfig: Sending IPC packet\n");
  
  if (ipc.sendPacket(IPC_MSG_CONFIG_ANALOG_INPUT, (uint8_t*)&cfg, sizeof(cfg))) {
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
  cfg.index = index;
  strncpy(cfg.unit, ioConfig.rtdSensors[rtdIndex].unit, sizeof(cfg.unit) - 1);
  cfg.unit[sizeof(cfg.unit) - 1] = '\0';
  cfg.calScale = ioConfig.rtdSensors[rtdIndex].cal.scale;
  cfg.calOffset = ioConfig.rtdSensors[rtdIndex].cal.offset;
  cfg.wireConfig = ioConfig.rtdSensors[rtdIndex].wireConfig;
  cfg.nominalOhms = ioConfig.rtdSensors[rtdIndex].nominalOhms;
  
  log(LOG_DEBUG, false, "handleSaveRTDConfig: Sending IPC packet\n");
  
  if (ipc.sendPacket(IPC_MSG_CONFIG_RTD, (uint8_t*)&cfg, sizeof(cfg))) {
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
  cfg.index = index;
  strlcpy(cfg.name, ioConfig.gpio[gpioIndex].name, sizeof(cfg.name));
  cfg.pullMode = (uint8_t)ioConfig.gpio[gpioIndex].pullMode;
  cfg.enabled = ioConfig.gpio[gpioIndex].enabled;
  
  log(LOG_DEBUG, false, "handleSaveGPIOConfig: Sending IPC packet\n");
  
  if (ipc.sendPacket(IPC_MSG_CONFIG_GPIO, (uint8_t*)&cfg, sizeof(cfg))) {
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

// ============================================================================
// OUTPUTS API HANDLERS
// ============================================================================

/**
 * @brief Get all outputs status for monitoring
 */
void handleGetOutputs() {
  StaticJsonDocument<2048> doc;
  
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
    } else {
      motor["running"] = false;
      motor["power"] = 0;
      motor["direction"] = true;
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
  
  // Save configuration to file
  saveIOConfig();
  
  // Send updated config to IO MCU via IPC
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

void setupWebServer()
{
  // Initialize LittleFS for serving web files
  if (!LittleFS.begin())
  {
    log(LOG_ERROR, true, "LittleFS Mount Failed\n");
    return;
  }


  // ...existing code...

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

  // ...existing code...

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

  // Setup API endpoints
  setupNetworkAPI();
  setupMqttAPI();
  setupTimeAPI();

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