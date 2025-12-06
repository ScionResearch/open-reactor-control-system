/**
 * @file apiSystem.cpp
 * @brief System status and control API implementation
 */

#include "apiSystem.h"
#include "../network/networkManager.h"
#include "../utils/logger.h"
#include "../utils/statusManager.h"
#include "../utils/objectCache.h"
#include "../utils/timeManager.h"
#include "../config/ioConfig.h"
#include "../storage/sdManager.h"
#include <ArduinoJson.h>
#include <LittleFS.h>

// =============================================================================
// Setup Function
// =============================================================================

void setupSystemAPI(void) {
    // System status endpoint for the UI
    server.on("/api/system/status", HTTP_GET, handleSystemStatus);

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

    // Recording Configuration API endpoints
    server.on("/api/config/recording", HTTP_GET, handleGetRecordingConfig);
    server.on("/api/config/recording", HTTP_POST, handleSaveRecordingConfig);
    
    // Backup/Restore API endpoints
    server.on("/api/config/backup", HTTP_GET, handleGetConfigBackup);
    server.on("/api/config/restore", HTTP_POST, handleRestoreConfig);
    server.on("/api/config/backup/sd", HTTP_POST, handleSaveBackupToSD);
    server.on("/api/config/backup/sd/list", HTTP_GET, handleListSDBackups);
}

// =============================================================================
// Status Handlers
// =============================================================================

void handleSystemStatus() {
    // NOTE: This handler only READS from status/sdInfo structs, it doesn't write.
    // Therefore we don't need to acquire statusLocked - reads are safe without it.

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
    ipc["ok"] = status.ipcOK;
    ipc["connected"] = status.ipcConnected;
    ipc["timeout"] = status.ipcTimeout;
    
    // Modbus status with detailed state
    JsonObject modbus = doc.createNestedObject("modbus");
    modbus["configured"] = status.modbusConfigured;
    modbus["connected"] = status.modbusConnected;
    modbus["fault"] = status.modbusFault;

    // SD card info
    JsonObject sd = doc.createNestedObject("sd");
    sd["inserted"] = sdInfo.inserted;
    sd["ready"] = sdInfo.ready;
    sd["capacityGB"] = sdInfo.cardSizeBytes * 0.000000001;
    sd["freeSpaceGB"] = sdInfo.cardFreeBytes * 0.000000001;
    sd["logFileSizeKB"] = sdInfo.logSizeBytes * 0.001;

    String response;
    serializeJson(doc, response);
    server.send(200, "application/json", response);
}

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

// =============================================================================
// Recording Configuration Handlers
// =============================================================================

void handleGetRecordingConfig() {
    StaticJsonDocument<768> doc;
    
    doc["enabled"] = recordingConfig.enabled;
    
    JsonObject inputs = doc.createNestedObject("inputs");
    inputs["enabled"] = recordingConfig.inputs.enabled;
    inputs["interval"] = recordingConfig.inputs.interval;
    
    JsonObject outputs = doc.createNestedObject("outputs");
    outputs["enabled"] = recordingConfig.outputs.enabled;
    outputs["interval"] = recordingConfig.outputs.interval;
    
    JsonObject motors = doc.createNestedObject("motors");
    motors["enabled"] = recordingConfig.motors.enabled;
    motors["interval"] = recordingConfig.motors.interval;
    
    JsonObject sensors = doc.createNestedObject("sensors");
    sensors["enabled"] = recordingConfig.sensors.enabled;
    sensors["interval"] = recordingConfig.sensors.interval;
    
    JsonObject energy = doc.createNestedObject("energy");
    energy["enabled"] = recordingConfig.energy.enabled;
    energy["interval"] = recordingConfig.energy.interval;
    
    JsonObject controllers = doc.createNestedObject("controllers");
    controllers["enabled"] = recordingConfig.controllers.enabled;
    controllers["interval"] = recordingConfig.controllers.interval;
    
    JsonObject devices = doc.createNestedObject("devices");
    devices["enabled"] = recordingConfig.devices.enabled;
    devices["interval"] = recordingConfig.devices.interval;
    
    String response;
    serializeJson(doc, response);
    server.send(200, "application/json", response);
}

void handleSaveRecordingConfig() {
    if (!server.hasArg("plain")) {
        server.send(400, "application/json", "{\"error\":\"No data received\"}");
        return;
    }
    
    StaticJsonDocument<768> doc;
    DeserializationError error = deserializeJson(doc, server.arg("plain"));
    
    if (error) {
        server.send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
        return;
    }
    
    // Update recording configuration
    recordingConfig.enabled = doc["enabled"] | false;
    
    JsonObject inputs = doc["inputs"];
    recordingConfig.inputs.enabled = inputs["enabled"] | false;
    recordingConfig.inputs.interval = inputs["interval"] | 60;
    if (recordingConfig.inputs.interval < RECORDING_MIN_INTERVAL)
        recordingConfig.inputs.interval = RECORDING_MIN_INTERVAL;
    
    JsonObject outputs = doc["outputs"];
    recordingConfig.outputs.enabled = outputs["enabled"] | false;
    recordingConfig.outputs.interval = outputs["interval"] | 60;
    if (recordingConfig.outputs.interval < RECORDING_MIN_INTERVAL)
        recordingConfig.outputs.interval = RECORDING_MIN_INTERVAL;
    
    JsonObject motors = doc["motors"];
    recordingConfig.motors.enabled = motors["enabled"] | false;
    recordingConfig.motors.interval = motors["interval"] | 60;
    if (recordingConfig.motors.interval < RECORDING_MIN_INTERVAL)
        recordingConfig.motors.interval = RECORDING_MIN_INTERVAL;
    
    JsonObject sensors = doc["sensors"];
    recordingConfig.sensors.enabled = sensors["enabled"] | false;
    recordingConfig.sensors.interval = sensors["interval"] | 60;
    if (recordingConfig.sensors.interval < RECORDING_MIN_INTERVAL)
        recordingConfig.sensors.interval = RECORDING_MIN_INTERVAL;
    
    JsonObject energy = doc["energy"];
    recordingConfig.energy.enabled = energy["enabled"] | false;
    recordingConfig.energy.interval = energy["interval"] | 60;
    if (recordingConfig.energy.interval < RECORDING_MIN_INTERVAL)
        recordingConfig.energy.interval = RECORDING_MIN_INTERVAL;
    
    JsonObject controllers = doc["controllers"];
    recordingConfig.controllers.enabled = controllers["enabled"] | false;
    recordingConfig.controllers.interval = controllers["interval"] | 60;
    if (recordingConfig.controllers.interval < RECORDING_MIN_INTERVAL)
        recordingConfig.controllers.interval = RECORDING_MIN_INTERVAL;
    
    JsonObject devices = doc["devices"];
    recordingConfig.devices.enabled = devices["enabled"] | false;
    recordingConfig.devices.interval = devices["interval"] | 60;
    if (recordingConfig.devices.interval < RECORDING_MIN_INTERVAL)
        recordingConfig.devices.interval = RECORDING_MIN_INTERVAL;
    
    // Save to network config (which includes recording config)
    saveNetworkConfig();
    
    log(LOG_INFO, true, "Recording configuration saved: master=%s\n", 
        recordingConfig.enabled ? "enabled" : "disabled");
    
    server.send(200, "application/json", "{\"success\":true,\"message\":\"Recording configuration saved\"}");
}

// =============================================================================
// Backup/Restore Handlers
// =============================================================================

void handleGetConfigBackup() {
    log(LOG_INFO, true, "Generating configuration backup\n");
    
    // Create JSON document to combine both config files
    DynamicJsonDocument doc(32768);
    
    // Add backup metadata
    doc["backup_version"] = 1;
    doc["backup_timestamp"] = getISO8601Timestamp(100);
    
    // Read system_config.json directly from flash
    File sysFile = LittleFS.open(CONFIG_FILENAME, "r");
    if (sysFile) {
        DynamicJsonDocument sysDoc(2048);
        DeserializationError error = deserializeJson(sysDoc, sysFile);
        sysFile.close();
        
        if (!error) {
            doc["system_config"] = sysDoc.as<JsonObject>();
        } else {
            log(LOG_WARNING, true, "Failed to parse system config: %s\n", error.c_str());
            server.send(500, "application/json", "{\"error\":\"Failed to read system configuration\"}");
            return;
        }
    } else {
        log(LOG_WARNING, true, "System config file not found\n");
        server.send(500, "application/json", "{\"error\":\"System configuration file not found\"}");
        return;
    }
    
    // Read io_config.json directly from flash
    File ioFile = LittleFS.open(IO_CONFIG_FILENAME, "r");
    if (ioFile) {
        DynamicJsonDocument ioDoc(16384);
        DeserializationError error = deserializeJson(ioDoc, ioFile);
        ioFile.close();
        
        if (!error) {
            doc["io_config"] = ioDoc.as<JsonObject>();
        } else {
            log(LOG_WARNING, true, "Failed to parse IO config: %s\n", error.c_str());
            server.send(500, "application/json", "{\"error\":\"Failed to read IO configuration\"}");
            return;
        }
    } else {
        log(LOG_WARNING, true, "IO config file not found\n");
        server.send(500, "application/json", "{\"error\":\"IO configuration file not found\"}");
        return;
    }
    
    // Serialize and send
    String response;
    serializeJson(doc, response);
    server.send(200, "application/json", response);
    
    log(LOG_INFO, true, "Configuration backup generated (%d bytes)\n", response.length());
}

void handleRestoreConfig() {
    if (!server.hasArg("plain")) {
        server.send(400, "application/json", "{\"error\":\"No data received\"}");
        return;
    }
    
    // Get payload length first without copying the full string
    size_t payloadLen = server.arg("plain").length();
    log(LOG_INFO, true, "Restoring configuration from backup (%d bytes, free heap: %d)\n", 
        payloadLen, rp2040.getFreeHeap());
    
    // Calculate buffer size: payload * 2 for JSON overhead, minimum 16KB, maximum 40KB
    size_t bufferSize = payloadLen * 2;
    if (bufferSize < 16384) bufferSize = 16384;
    if (bufferSize > 40960) bufferSize = 40960;  // Max 40KB to leave heap space
    
    DynamicJsonDocument doc(bufferSize);
    DeserializationError error = deserializeJson(doc, server.arg("plain"));
    
    if (error) {
        log(LOG_WARNING, true, "Failed to parse backup JSON: %s (payload: %d, buffer: %d, heap: %d)\n", 
            error.c_str(), payloadLen, bufferSize, rp2040.getFreeHeap());
        server.send(400, "application/json", "{\"error\":\"JSON parse failed - not enough memory\"}");
        return;
    }
    log(LOG_DEBUG, true, "JSON parsed, using %d bytes\n", doc.memoryUsage());
    
    // Get the backup data and import options
    JsonObject backupData = doc["data"].as<JsonObject>();
    bool importSystem = doc["import_system"] | false;
    
    if (backupData.isNull()) {
        server.send(400, "application/json", "{\"error\":\"Missing backup data\"}");
        return;
    }
    
    // Validate io_config exists
    if (!backupData.containsKey("io_config")) {
        server.send(400, "application/json", "{\"error\":\"Invalid backup: missing io_config\"}");
        return;
    }
    
    bool needsReboot = false;
    
    // =========================================================================
    // Restore System Configuration (if requested)
    // =========================================================================
    if (importSystem && backupData.containsKey("system_config")) {
        JsonObject sysConfig = backupData["system_config"];
        
        // Network settings
        networkConfig.useDHCP = sysConfig["use_dhcp"] | true;
        
        String ip = sysConfig["ip"] | "192.168.1.100";
        String subnet = sysConfig["subnet"] | "255.255.255.0";
        String gateway = sysConfig["gateway"] | "192.168.1.1";
        String dns = sysConfig["dns"] | "8.8.8.8";
        
        networkConfig.ip.fromString(ip);
        networkConfig.subnet.fromString(subnet);
        networkConfig.gateway.fromString(gateway);
        networkConfig.dns.fromString(dns);
        
        strlcpy(networkConfig.hostname, sysConfig["hostname"] | "orcs", sizeof(networkConfig.hostname));
        strlcpy(networkConfig.ntpServer, sysConfig["ntp_server"] | "pool.ntp.org", sizeof(networkConfig.ntpServer));
        strlcpy(networkConfig.timezone, sysConfig["timezone"] | "+12:00", sizeof(networkConfig.timezone));
        networkConfig.ntpEnabled = sysConfig["ntp_enabled"] | true;
        networkConfig.dstEnabled = sysConfig["dst_enabled"] | false;
        
        // MQTT
        networkConfig.mqttEnabled = sysConfig["mqtt_enabled"] | false;
        strlcpy(networkConfig.mqttBroker, sysConfig["mqtt_broker"] | "", sizeof(networkConfig.mqttBroker));
        networkConfig.mqttPort = sysConfig["mqtt_port"] | 1883;
        strlcpy(networkConfig.mqttUsername, sysConfig["mqtt_username"] | "", sizeof(networkConfig.mqttUsername));
        strlcpy(networkConfig.mqttPassword, sysConfig["mqtt_password"] | "", sizeof(networkConfig.mqttPassword));
        strlcpy(networkConfig.mqttDevicePrefix, sysConfig["mqtt_device_prefix"] | "", sizeof(networkConfig.mqttDevicePrefix));
        networkConfig.mqttPublishIntervalMs = sysConfig["mqtt_publish_interval_ms"] | 5000;
        
        // Recording config
        if (sysConfig.containsKey("recording")) {
            JsonObject rec = sysConfig["recording"];
            recordingConfig.enabled = rec["enabled"] | false;
            recordingConfig.inputs.enabled = rec["inputs"]["enabled"] | false;
            recordingConfig.inputs.interval = rec["inputs"]["interval"] | 60;
            recordingConfig.outputs.enabled = rec["outputs"]["enabled"] | false;
            recordingConfig.outputs.interval = rec["outputs"]["interval"] | 60;
            recordingConfig.motors.enabled = rec["motors"]["enabled"] | false;
            recordingConfig.motors.interval = rec["motors"]["interval"] | 60;
            recordingConfig.sensors.enabled = rec["sensors"]["enabled"] | false;
            recordingConfig.sensors.interval = rec["sensors"]["interval"] | 60;
            recordingConfig.energy.enabled = rec["energy"]["enabled"] | false;
            recordingConfig.energy.interval = rec["energy"]["interval"] | 60;
            recordingConfig.controllers.enabled = rec["controllers"]["enabled"] | false;
            recordingConfig.controllers.interval = rec["controllers"]["interval"] | 60;
            recordingConfig.devices.enabled = rec["devices"]["enabled"] | false;
            recordingConfig.devices.interval = rec["devices"]["interval"] | 60;
        }
        
        saveNetworkConfig();
        needsReboot = true;
        log(LOG_INFO, true, "System configuration restored from backup\n");
    }
    
    // =========================================================================
    // Restore IO Configuration (always)
    // =========================================================================
    log(LOG_DEBUG, true, "Starting IO config restore (heap: %d)\n", rp2040.getFreeHeap());
    JsonObject ioConfig_json = backupData["io_config"];
    
    // Debug: check if io_config was retrieved successfully
    if (ioConfig_json.isNull()) {
        log(LOG_WARNING, true, "io_config is null in backup data!\n");
        server.send(400, "application/json", "{\"error\":\"io_config missing or null in backup\"}");
        return;
    }
    
    // Serialize to String first, then write (more reliable than direct file write)
    String ioConfigStr;
    size_t serializedSize = serializeJson(ioConfig_json, ioConfigStr);
    log(LOG_DEBUG, true, "Serialized IO config to string: %d bytes\n", serializedSize);
    
    if (serializedSize == 0 || ioConfigStr.length() == 0) {
        log(LOG_WARNING, true, "Failed to serialize IO config to string!\n");
        server.send(500, "application/json", "{\"error\":\"Failed to serialize IO config\"}");
        return;
    }
    
    // Check LittleFS space
    FSInfo fsInfo;
    LittleFS.info(fsInfo);
    size_t freeSpace = fsInfo.totalBytes - fsInfo.usedBytes;
    log(LOG_INFO, true, "LittleFS: %d/%d bytes used, %d free\n", 
        fsInfo.usedBytes, fsInfo.totalBytes, freeSpace);
    
    if (freeSpace < serializedSize + 1024) {
        log(LOG_WARNING, true, "Not enough space on LittleFS! Need %d, have %d\n", 
            serializedSize, freeSpace);
        server.send(507, "application/json", "{\"error\":\"Not enough storage space\"}");
        return;
    }
    
    // Delete existing file first to avoid corruption
    if (LittleFS.exists(IO_CONFIG_FILENAME)) {
        LittleFS.remove(IO_CONFIG_FILENAME);
        log(LOG_DEBUG, true, "Removed existing IO config file\n");
    }
    
    // Write string to file
    File ioFile = LittleFS.open(IO_CONFIG_FILENAME, "w");
    if (ioFile) {
        size_t bytesWritten = ioFile.print(ioConfigStr);
        ioFile.close();
        log(LOG_INFO, true, "IO configuration file written from backup (%d bytes)\n", bytesWritten);
        
        // Verify file was written correctly
        File verifyFile = LittleFS.open(IO_CONFIG_FILENAME, "r");
        if (verifyFile) {
            size_t fileSize = verifyFile.size();
            verifyFile.close();
            log(LOG_INFO, true, "Verified IO config file size: %d bytes\n", fileSize);
            if (fileSize != bytesWritten) {
                log(LOG_WARNING, true, "File size mismatch! Written: %d, On disk: %d\n", bytesWritten, fileSize);
            }
        }
        
        // Reload IO config into memory
        loadIOConfig();
        
        // Push to IO MCU
        pushIOConfigToIOmcu();
    } else {
        log(LOG_WARNING, true, "Failed to write IO config file\n");
        server.send(500, "application/json", "{\"error\":\"Failed to write IO configuration\"}");
        return;
    }
    
    // Send success response
    String response = "{\"success\":true,\"reboot\":" + String(needsReboot ? "true" : "false") + "}";
    server.send(200, "application/json", response);
    
    log(LOG_INFO, true, "Configuration restore complete, reboot=%s\n", needsReboot ? "yes" : "no");
    
    // Trigger reboot if system config was changed
    if (needsReboot) {
        delay(500);  // Allow response to be sent
        log(LOG_INFO, true, "Rebooting after system config restore...\n");
        delay(500);
        rp2040.reboot();
    }
}

void handleSaveBackupToSD() {
    if (!server.hasArg("plain")) {
        server.send(400, "application/json", "{\"error\":\"No data received\"}");
        return;
    }
    
    if (!sdInfo.ready) {
        server.send(503, "application/json", "{\"error\":\"SD card not ready\"}");
        return;
    }
    
    // Calculate buffer based on payload size
    size_t payloadLen = server.arg("plain").length();
    size_t bufferSize = payloadLen * 2;
    if (bufferSize < 16384) bufferSize = 16384;
    if (bufferSize > 40960) bufferSize = 40960;
    
    DynamicJsonDocument doc(bufferSize);
    DeserializationError error = deserializeJson(doc, server.arg("plain"));
    
    if (error) {
        log(LOG_WARNING, true, "Failed to parse SD backup JSON: %s (heap: %d)\n", error.c_str(), rp2040.getFreeHeap());
        server.send(400, "application/json", "{\"error\":\"Invalid JSON - not enough memory\"}");
        return;
    }
    
    const char* filename = doc["filename"] | "backup.json";
    JsonObject backupData = doc["data"];
    
    if (backupData.isNull()) {
        server.send(400, "application/json", "{\"error\":\"Missing backup data\"}");
        return;
    }
    
    // Ensure /backups directory exists
    if (!sd.exists("/backups")) {
        sd.mkdir("/backups");
    }
    
    // Build full path
    String path = "/backups/";
    path += filename;
    
    // Write to SD card
    FsFile backupFile = sd.open(path.c_str(), O_WRITE | O_CREAT | O_TRUNC);
    if (!backupFile) {
        log(LOG_WARNING, true, "Failed to create backup file: %s\n", path.c_str());
        server.send(500, "application/json", "{\"error\":\"Failed to create backup file\"}");
        return;
    }
    
    size_t written = serializeJsonPretty(backupData, backupFile);
    backupFile.close();
    
    if (written == 0) {
        server.send(500, "application/json", "{\"error\":\"Failed to write backup data\"}");
        return;
    }
    
    log(LOG_INFO, true, "Backup saved to SD: %s (%d bytes)\n", path.c_str(), written);
    server.send(200, "application/json", "{\"success\":true}");
}

void handleListSDBackups() {
    if (!sdInfo.ready) {
        server.send(503, "application/json", "{\"error\":\"SD card not ready\"}");
        return;
    }
    
    DynamicJsonDocument doc(4096);
    JsonArray backups = doc.createNestedArray("backups");
    
    // Check if /backups directory exists
    if (!sd.exists("/backups")) {
        // Return empty list
        String response;
        serializeJson(doc, response);
        server.send(200, "application/json", response);
        return;
    }
    
    FsFile backupsDir = sd.open("/backups");
    if (!backupsDir || !backupsDir.isDirectory()) {
        server.send(200, "application/json", "{\"backups\":[]}");
        return;
    }
    
    FsFile file;
    char nameBuffer[64];
    while (file.openNext(&backupsDir, O_RDONLY)) {
        if (!file.isDirectory()) {
            file.getName(nameBuffer, sizeof(nameBuffer));
            String name = String(nameBuffer);
            // Only include .json files
            if (name.endsWith(".json")) {
                JsonObject backup = backups.createNestedObject();
                backup["name"] = name;
                backup["path"] = String("/backups/") + name;
                backup["size"] = file.size();
                backup["modified"] = "";
            }
        }
        file.close();
    }
    backupsDir.close();
    
    String response;
    serializeJson(doc, response);
    server.send(200, "application/json", response);
}
