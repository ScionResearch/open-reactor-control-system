#include "utils/statusManager.h"
#include "utils/objectCache.h"
#include "config/ioConfig.h"
#include "sdManager.h"
#include <ArduinoJson.h>
#include <LittleFS.h>

SdFs sd;
FsFile file;

sdInfo_t sdInfo;
uint32_t sdTS;
volatile bool sdLocked = false;

// Recording configuration and state
RecordingConfig recordingConfig = {
    .enabled = false,
    .inputs = { .enabled = false, .interval = 60 },
    .outputs = { .enabled = false, .interval = 60 },
    .motors = { .enabled = false, .interval = 60 },
    .sensors = { .enabled = false, .interval = 60 },
    .energy = { .enabled = false, .interval = 60 },
    .controllers = { .enabled = false, .interval = 60 },
    .devices = { .enabled = false, .interval = 60 }
};

RecordingScheduler recordingScheduler = {0};
bool ioConfigChanged = false;

// Forward declaration for internal function
void initRecordingDirectoriesInternal(void);

void init_sdManager(void) {
    SPI1.setMISO(PIN_SD_MISO);
    SPI1.setMOSI(PIN_SD_MOSI);
    SPI1.setSCK(PIN_SD_SCK);
    
    FsDateTime::setCallback(dateTimeCallback);
    
    sdTS = millis();
    log(LOG_INFO, false, "SD card manager initialised\n");
}

void manageSD(void) {
    if (millis() - sdTS < SD_MANAGE_INTERVAL) return;
    sdTS = millis();
    
    if (!sdInfo.ready && !digitalRead(PIN_SD_CD)) {
        mountSD();
    } else {
        maintainSD();
    }
    
    // Every 10 minutes, update SD info for the status display
    static uint32_t sdInfoTS = 0;
    if (sdInfo.ready && (millis() - sdInfoTS > 600000)) {
        sdInfoTS = millis();
        printSDInfo();
    }
}

void mountSD(void) {
    // Check if SD card is inserted
    if (digitalRead(PIN_SD_CD)) {
        log(LOG_WARNING, false,"SD card not inserted\n");
        if (sdLocked) return;
        sdLocked = true;
        sdInfo.inserted = false;
        sdInfo.ready = false;
        if (!statusLocked) {
            statusLocked = true;
            status.sdCardOK = false;
            status.updated = true;
            statusLocked = false;
        }
        sdLocked = false;
        return;
    }
    // Mount SD card
    bool sdSPIinitialised = false;
    bool sdSDIOinitialised = false;
    if (sdLocked) {
        log(LOG_WARNING, false, "SD card mount locked, aborting\n");
        return;
    }
    sdLocked = true;
    sdInfo.inserted = true;
    log(LOG_INFO, false, "SD card inserted, mounting FS\n");
    if (!sd.begin(SDIO_CONFIG)) {
        log(LOG_ERROR, false, "Attempt 1 failed, retrying\n");
        delay(100);
        if (!sd.begin(SDIO_CONFIG)) {
            log(LOG_ERROR, false, "SD card initialisation with SDIO config failed, attempting SPI config\n");
            if (!sd.begin(SdSpiConfig(PIN_SD_CS, DEDICATED_SPI, SD_SCK_MHZ(40), &SPI1))) {
                if (sd.card()->errorCode()) {
                    log(LOG_ERROR, false, "SD card initialisation failed with error code %d\n", sd.card()->errorCode());
                }
                log(LOG_ERROR, false, "SD card initialisation failed with all init attempts failed\n");
            } else {
                log(LOG_INFO, false, "SD card initialisation with SPI config succeeded\n");
                sdSPIinitialised = true;
            }
        }
    } else {
        log(LOG_INFO, false, "SD card initialisation with SDIO config succeeded\n");
        sdSDIOinitialised = true;
    }
    if (sdSPIinitialised || sdSDIOinitialised) {
        log(LOG_INFO, false, "SD card initialisation successful, using %s\n", sdSPIinitialised ? "SPI" : "SDIO");
        // Check for correct folder structure and create if missing
        log(LOG_INFO, false, "Checking for correct folder structure\n");
        if (!sd.exists("/logs")) sd.mkdir("/logs");
        // Check for log files and create if missing
        if (!sd.exists("/logs/system.txt")) {
            file = sd.open("/logs/system.txt", O_CREAT | O_RDWR | O_APPEND);
            file.close();
        }
        // Initialize recording directories
        initRecordingDirectoriesInternal();
        sdInfo.ready = true;
    }
    if (sdInfo.ready) {
        log(LOG_INFO, false, "SD card mounted OK\n");
    }
    if (!statusLocked) {
        statusLocked = true;
        status.sdCardOK = true;
        status.updated = true;
        statusLocked = false;
    }
    sdLocked = false;
    printSDInfo();
}

void maintainSD(void) {
    // Just check if the SD card is still inserted
    if (sdLocked) {
        return;
    }
    sdLocked = true;
    if (digitalRead(PIN_SD_CD) && sdInfo.inserted) {
        log(LOG_WARNING, false, "SD card removed\n");
        sdInfo.inserted = false;
        sdInfo.ready = false;
        if (!statusLocked) {
            statusLocked = true;
            status.sdCardOK = false;
            status.updated = true;
            statusLocked = false;
        }
    }
    sdLocked = false;
}

uint64_t getFileSize(const char* path) {
    FsFile file;
    uint64_t size = 0;
    if (sdLocked) return 0;
    sdLocked = true;
    if (sd.exists(path)) {
        if (file.open(path, O_RDONLY)) {
            size = file.fileSize();
            file.close();
        }
    }
    sdLocked = false;
    return size;
}

void printSDInfo(void) {
    if (sdLocked) return;
    sdLocked = true;
    if (!sdInfo.ready) {
        if (digitalRead(PIN_SD_CD)) log(LOG_INFO, false, "SD card not inserted\n");
        else log(LOG_INFO, false, "SD card not ready\n");
        sdLocked = false;
        return;
    }

    sdInfo.cardSizeBytes = (uint64_t)sd.card()->sectorCount() * 512;
    sdInfo.cardFreeBytes = (uint64_t)sd.vol()->bytesPerCluster() * (uint64_t)sd.freeClusterCount();
    sdLocked = false;
    uint64_t logFileSize = getFileSize("/logs/system.txt");
    sdLocked = true;
    sdInfo.logSizeBytes = logFileSize;
    
    log(LOG_INFO, false, "SD card size: %0.1f GB\n", sdInfo.cardSizeBytes * 0.000000001);
    log(LOG_INFO, false, "Free space: %0.1f GB\n", sdInfo.cardFreeBytes * 0.000000001);
    log(LOG_INFO, false, "Volume is FAT%d\n", sd.vol()->fatType());
    log(LOG_INFO, false, "Log file size: %0.1f kbytes\n", 0.001 * (float)logFileSize);
    
    sdLocked = false;
}

void dateTimeCallback(uint16_t* date, uint16_t* time) {
    DateTime now;
    if (!getGlobalDateTime(now, 10)) return;

    // Return date using FS_DATE macro to format fields.
    *date = FS_DATE(now.year, now.month, now.day);

    // Return time using FS_TIME macro to format fields.
    *time = FS_TIME(now.hour, now.minute, now.second);
}

bool writeLog(const char *message) {
    if (sdLocked) return false;
    sdLocked = true;
    if (!sdInfo.ready) {
        sdLocked = false;
        return false;
    }
    sdLocked = false;
    DateTime now;
    if (!getGlobalDateTime(now, 10)) return false;
    char dateTimeStr[20];
    snprintf(dateTimeStr, sizeof(dateTimeStr), "%04d-%02d-%02d %02d:%02d:%02d", now.year, now.month, now.day, now.hour, now.minute, now.second);

    char buf[strlen(dateTimeStr) + strlen(message) + 10];
    snprintf(buf, sizeof(buf), "[%s]\t\t%s", dateTimeStr, message);

    // Log file size check
    uint64_t logFileSize = getFileSize("/logs/system.txt");
    sdLocked = true;
    sdInfo.logSizeBytes = logFileSize;
    if (logFileSize > SD_LOG_MAX_SIZE) {
        // Rename the existing log file and create a new one
        char fNameBuf[50];
        snprintf(fNameBuf, sizeof(fNameBuf), "/logs/system-log-archive-%04d-%02d-%02d.txt", now.year, now.month, now.day);
        
        if (!sd.exists(fNameBuf)) {
            for (int i = 0; i < 100; i++) {
                char tempBuf[50];
                snprintf(tempBuf, sizeof(tempBuf), "%s-%d", fNameBuf, i);
                if (!sd.exists(tempBuf)) {
                        strcpy(fNameBuf, tempBuf);
                        break;
                    }
                }
            }
            if (sd.exists("/logs/system.txt")) {
                sd.rename("/logs/system.txt", fNameBuf);
            }
            file = sd.open("/logs/system.txt", O_CREAT | O_RDWR | O_APPEND);
            if (file) {
                file.print(buf);
                file.close();
            }
        sdLocked = false;
        return true;
    }
    // Otherwise just write to the existing log file
    file = sd.open("/logs/system.txt", O_CREAT | O_RDWR | O_APPEND);
    if (file) {
        file.print(buf);
        file.close();
    }
    sdLocked = false;
    return true;
}
// DATA RECORDING FUNCTIONS
// ============================================================================

void initRecordingDirectoriesInternal(void) {
    // Create main recording directory
    if (!sd.exists(RECORDED_DATA_DIR)) {
        if (sd.mkdir(RECORDED_DATA_DIR)) {
            log(LOG_INFO, false, "Created %s directory\n", RECORDED_DATA_DIR);
        } else {
            log(LOG_ERROR, false, "Failed to create %s directory\n", RECORDED_DATA_DIR);
        }
    }
    // Create controllers subdirectory
    if (!sd.exists(CONTROLLERS_DIR)) {
        if (sd.mkdir(CONTROLLERS_DIR)) {
            log(LOG_INFO, false, "Created %s directory\n", CONTROLLERS_DIR);
        } else {
            log(LOG_ERROR, false, "Failed to create %s directory\n", CONTROLLERS_DIR);
        }
    }
    // Create devices subdirectory
    if (!sd.exists(DEVICES_DIR)) {
        if (sd.mkdir(DEVICES_DIR)) {
            log(LOG_INFO, false, "Created %s directory\n", DEVICES_DIR);
        } else {
            log(LOG_ERROR, false, "Failed to create %s directory\n", DEVICES_DIR);
        }
    }
}

void initRecordingDirectories(void) {
    if (sdLocked) return;
    sdLocked = true;
    if (!sdInfo.ready) {
        sdLocked = false;
        return;
    }
    initRecordingDirectoriesInternal();
    sdLocked = false;
}

void invalidateRecordingHeaders(void) {
    // Mark all headers as needing rewrite
    recordingScheduler.inputsHeadersWritten = false;
    recordingScheduler.outputsHeadersWritten = false;
    recordingScheduler.motorsHeadersWritten = false;
    recordingScheduler.sensorsHeadersWritten = false;
    recordingScheduler.energyHeadersWritten = false;
    recordingScheduler.controllersHeadersWritten = false;
    recordingScheduler.devicesHeadersWritten = false;
    log(LOG_INFO, false, "Recording headers invalidated - will rewrite on next record\n");
}

// Local timestamp function for recording (renamed to avoid conflict with timeManager.h)
static String getRecordingTimestamp(void) {
    DateTime now;
    if (!getGlobalDateTime(now, 10)) {
        return "1970-01-01T00:00:00";
    }
    char buf[32];
    snprintf(buf, sizeof(buf), "%04d-%02d-%02dT%02d:%02d:%02d",
             now.year, now.month, now.day, now.hour, now.minute, now.second);
    return String(buf);
}

uint32_t getEpochSeconds(void) {
    DateTime now;
    if (!getGlobalDateTime(now, 10)) {
        return 0;
    }
    // Simple epoch calculation (not accounting for leap seconds, good enough for scheduling)
    // Days since 1970-01-01
    uint32_t days = 0;
    for (int y = 1970; y < now.year; y++) {
        days += (y % 4 == 0 && (y % 100 != 0 || y % 400 == 0)) ? 366 : 365;
    }
    static const int daysInMonth[] = {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    for (int m = 1; m < now.month; m++) {
        days += daysInMonth[m];
        if (m == 2 && (now.year % 4 == 0 && (now.year % 100 != 0 || now.year % 400 == 0))) {
            days += 1; // Leap year February
        }
    }
    days += now.day - 1;
    return days * 86400UL + now.hour * 3600UL + now.minute * 60UL + now.second;
}

bool appendToCSV(const char* path, const String& line) {
    if (sdLocked) return false;
    sdLocked = true;
    if (!sdInfo.ready) {
        sdLocked = false;
        return false;
    }
    
    FsFile csvFile = sd.open(path, O_CREAT | O_RDWR | O_APPEND);
    if (!csvFile) {
        sdLocked = false;
        log(LOG_ERROR, false, "Failed to open %s for writing\n", path);
        return false;
    }
    
    csvFile.println(line);
    csvFile.close();
    sdLocked = false;
    return true;
}

void archiveRecordingFile(const char* path, const char* archivePrefix) {
    if (sdLocked) return;
    sdLocked = true;
    if (!sdInfo.ready || !sd.exists(path)) {
        sdLocked = false;
        return;
    }
    
    DateTime now;
    if (!getGlobalDateTime(now, 10)) {
        sdLocked = false;
        return;
    }
    
    // Build archive filename
    char archivePath[128];
    snprintf(archivePath, sizeof(archivePath), "%s-%04d-%02d-%02d.csv",
             archivePrefix, now.year, now.month, now.day);
    
    // If archive exists, add a number suffix
    if (sd.exists(archivePath)) {
        for (int i = 1; i < 100; i++) {
            snprintf(archivePath, sizeof(archivePath), "%s-%04d-%02d-%02d-%d.csv",
                     archivePrefix, now.year, now.month, now.day, i);
            if (!sd.exists(archivePath)) break;
        }
    }
    
    sd.rename(path, archivePath);
    log(LOG_INFO, true, "Archived %s to %s\n", path, archivePath);
    sdLocked = false;
}

// Check if it's time to record based on RTC alignment
bool shouldRecord(uint32_t lastRecord, uint16_t interval) {
    uint32_t now = getEpochSeconds();
    if (now == 0) return false;  // RTC not ready
    
    // Check alignment: record at multiples of interval from :00
    bool aligned = (now % interval) == 0;
    bool notDuplicate = (now != lastRecord);
    
    return aligned && notDuplicate;
}

// Flag to track if directories have been initialized this session
static bool recordingDirsInitialized = false;

void manageDataRecording(void) {
    if (!recordingConfig.enabled || !sdInfo.ready) return;
    
    // Ensure directories exist (only check once per session)
    if (!recordingDirsInitialized) {
        initRecordingDirectories();
        recordingDirsInitialized = true;
    }
    
    // Check if ioConfig changed - invalidate headers
    if (ioConfigChanged) {
        invalidateRecordingHeaders();
        ioConfigChanged = false;
    }
    
    uint32_t now = getEpochSeconds();
    if (now == 0) return;  // RTC not ready
    
    // Inputs recording
    if (recordingConfig.inputs.enabled && shouldRecord(recordingScheduler.lastInputsRecord, recordingConfig.inputs.interval)) {
        recordingScheduler.lastInputsRecord = now;  // Update timestamp first to prevent spam
        // Check if file was deleted - reset header flag if so
        if (recordingScheduler.inputsHeadersWritten && getFileSize(RECORDED_DATA_DIR "/inputs.csv") == 0) {
            recordingScheduler.inputsHeadersWritten = false;
        }
        if (!recordingScheduler.inputsHeadersWritten) {
            if (writeInputsHeader()) {
                recordingScheduler.inputsHeadersWritten = true;
            }
        }
        writeInputsRecord();
    }
    
    // Outputs recording
    if (recordingConfig.outputs.enabled && shouldRecord(recordingScheduler.lastOutputsRecord, recordingConfig.outputs.interval)) {
        recordingScheduler.lastOutputsRecord = now;
        if (recordingScheduler.outputsHeadersWritten && getFileSize(RECORDED_DATA_DIR "/outputs.csv") == 0) {
            recordingScheduler.outputsHeadersWritten = false;
        }
        if (!recordingScheduler.outputsHeadersWritten) {
            if (writeOutputsHeader()) {
                recordingScheduler.outputsHeadersWritten = true;
            }
        }
        writeOutputsRecord();
    }
    
    // Motors recording
    if (recordingConfig.motors.enabled && shouldRecord(recordingScheduler.lastMotorsRecord, recordingConfig.motors.interval)) {
        recordingScheduler.lastMotorsRecord = now;
        if (recordingScheduler.motorsHeadersWritten && getFileSize(RECORDED_DATA_DIR "/motors.csv") == 0) {
            recordingScheduler.motorsHeadersWritten = false;
        }
        if (!recordingScheduler.motorsHeadersWritten) {
            if (writeMotorsHeader()) {
                recordingScheduler.motorsHeadersWritten = true;
            }
        }
        writeMotorsRecord();
    }
    
    // Sensors recording
    if (recordingConfig.sensors.enabled && shouldRecord(recordingScheduler.lastSensorsRecord, recordingConfig.sensors.interval)) {
        recordingScheduler.lastSensorsRecord = now;
        if (recordingScheduler.sensorsHeadersWritten && getFileSize(RECORDED_DATA_DIR "/sensors.csv") == 0) {
            recordingScheduler.sensorsHeadersWritten = false;
        }
        if (!recordingScheduler.sensorsHeadersWritten) {
            if (writeSensorsHeader()) {
                recordingScheduler.sensorsHeadersWritten = true;
            }
        }
        writeSensorsRecord();
    }
    
    // Energy recording
    if (recordingConfig.energy.enabled && shouldRecord(recordingScheduler.lastEnergyRecord, recordingConfig.energy.interval)) {
        recordingScheduler.lastEnergyRecord = now;
        if (recordingScheduler.energyHeadersWritten && getFileSize(RECORDED_DATA_DIR "/energy.csv") == 0) {
            recordingScheduler.energyHeadersWritten = false;
        }
        if (!recordingScheduler.energyHeadersWritten) {
            if (writeEnergyHeader()) {
                recordingScheduler.energyHeadersWritten = true;
            }
        }
        writeEnergyRecord();
    }
    
    // Controllers recording (headers checked per-file in writeControllersRecord)
    if (recordingConfig.controllers.enabled && shouldRecord(recordingScheduler.lastControllersRecord, recordingConfig.controllers.interval)) {
        recordingScheduler.lastControllersRecord = now;
        writeControllersRecord();
    }
    
    // Devices recording (headers checked per-file in writeDevicesRecord)
    if (recordingConfig.devices.enabled && shouldRecord(recordingScheduler.lastDevicesRecord, recordingConfig.devices.interval)) {
        recordingScheduler.lastDevicesRecord = now;
        writeDevicesRecord();
    }
}

// ============================================================================
// HEADER WRITING FUNCTIONS
// ============================================================================

bool writeInputsHeader(void) {
    const char* path = RECORDED_DATA_DIR "/inputs.csv";
    
    // Check file size and archive if needed
    uint64_t fileSize = getFileSize(path);
    if (fileSize > SD_RECORDING_MAX_SIZE) {
        archiveRecordingFile(path, RECORDED_DATA_DIR "/inputs-archive");
    }
    
    String header = "Timestamp";
    
    // ADC inputs 0-7
    for (int i = 0; i <= 7; i++) {
        header += ",";
        header += ioConfig.adcInputs[i].name;
        header += " (";
        header += ioConfig.adcInputs[i].unit;
        header += ")";
    }
    
    // Digital inputs 13-20
    for (int i = 0; i <= 7; i++) {
        header += ",";
        header += ioConfig.gpio[i].name;
        header += " (state)";
    }
    
    return appendToCSV(path, header);
}

bool writeOutputsHeader(void) {
    const char* path = RECORDED_DATA_DIR "/outputs.csv";
    
    uint64_t fileSize = getFileSize(path);
    if (fileSize > SD_RECORDING_MAX_SIZE) {
        archiveRecordingFile(path, RECORDED_DATA_DIR "/outputs-archive");
    }
    
    String header = "Timestamp";
    
    // DAC outputs 8-9
    for (int i = 0; i <= 1; i++) {
        header += ",";
        header += ioConfig.dacOutputs[i].name;
        header += " (";
        header += ioConfig.dacOutputs[i].unit;
        header += ")";
    }
    
    // Digital outputs 21-25
    for (int i = 0; i <= 4; i++) {
        header += ",";
        header += ioConfig.digitalOutputs[i].name;
        header += " (%)";
    }
    
    return appendToCSV(path, header);
}

bool writeMotorsHeader(void) {
    const char* path = RECORDED_DATA_DIR "/motors.csv";
    
    uint64_t fileSize = getFileSize(path);
    if (fileSize > SD_RECORDING_MAX_SIZE) {
        archiveRecordingFile(path, RECORDED_DATA_DIR "/motors-archive");
    }
    
    String header = "Timestamp";
    
    // Stepper motor (index 26)
    header += ",";
    header += ioConfig.stepperMotor.name;
    header += " RPM,";
    header += ioConfig.stepperMotor.name;
    header += " Running,";
    header += ioConfig.stepperMotor.name;
    header += " Direction";
    
    // DC motors (indices 27-30)
    for (int i = 0; i < 4; i++) {
        header += ",";
        header += ioConfig.dcMotors[i].name;
        header += " (%),";
        header += ioConfig.dcMotors[i].name;
        header += " Running,";
        header += ioConfig.dcMotors[i].name;
        header += " Direction,";
        header += ioConfig.dcMotors[i].name;
        header += " (A)";
    }
    
    return appendToCSV(path, header);
}

bool writeSensorsHeader(void) {
    const char* path = RECORDED_DATA_DIR "/sensors.csv";
    
    uint64_t fileSize = getFileSize(path);
    if (fileSize > SD_RECORDING_MAX_SIZE) {
        archiveRecordingFile(path, RECORDED_DATA_DIR "/sensors-archive");
    }
    
    String header = "Timestamp";
    
    // RTD sensors 10-12
    for (int i = 0; i <= 2; i++) {
        header += ",";
        header += ioConfig.rtdSensors[i].name;
        header += " (";
        header += ioConfig.rtdSensors[i].unit;
        header += ")";
    }
    
    // Device sensors 70-89 (dynamically check which exist)
    for (int i = 70; i <= 89; i++) {
        ObjectCache::CachedObject* obj = objectCache.getObject(i);
        if (obj && obj->valid) {
            header += ",";
            header += obj->name;
            header += " (";
            header += obj->unit;
            header += ")";
        }
    }
    
    return appendToCSV(path, header);
}

bool writeEnergyHeader(void) {
    const char* path = RECORDED_DATA_DIR "/energy.csv";
    
    uint64_t fileSize = getFileSize(path);
    if (fileSize > SD_RECORDING_MAX_SIZE) {
        archiveRecordingFile(path, RECORDED_DATA_DIR "/energy-archive");
    }
    
    String header = "Timestamp";
    
    // INA260 sensors at indices 31-32 (multi-value: V, A, W)
    for (int i = 31; i <= 32; i++) {
        ObjectCache::CachedObject* obj = objectCache.getObject(i);
        String name = obj && obj->valid ? String(obj->name) : String("Energy") + String(i - 30);
        header += ",";
        header += name;
        header += " (V),";
        header += name;
        header += " (A),";
        header += name;
        header += " (W)";
    }
    
    return appendToCSV(path, header);
}

bool writeControllerHeader(uint8_t index, const char* name) {
    char path[64];
    snprintf(path, sizeof(path), "%s/%s.csv", CONTROLLERS_DIR, name);
    
    uint64_t fileSize = getFileSize(path);
    if (fileSize > SD_RECORDING_MAX_SIZE) {
        char archivePrefix[64];
        snprintf(archivePrefix, sizeof(archivePrefix), "%s/%s-archive", CONTROLLERS_DIR, name);
        archiveRecordingFile(path, archivePrefix);
    }
    
    // Generic controller header - fields depend on controller type
    String header = "Timestamp,Enabled,Setpoint,ProcessValue,Output,Error";
    
    return appendToCSV(path, header);
}

bool writeDeviceHeader(uint8_t index, const char* name) {
    char path[64];
    snprintf(path, sizeof(path), "%s/%s.csv", DEVICES_DIR, name);
    
    uint64_t fileSize = getFileSize(path);
    if (fileSize > SD_RECORDING_MAX_SIZE) {
        char archivePrefix[64];
        snprintf(archivePrefix, sizeof(archivePrefix), "%s/%s-archive", DEVICES_DIR, name);
        archiveRecordingFile(path, archivePrefix);
    }
    
    // Generic device header - value + additional values from cache
    ObjectCache::CachedObject* obj = objectCache.getObject(index);
    String header = "Timestamp,Value";
    
    if (obj && obj->valid && obj->valueCount > 0) {
        for (int i = 0; i < obj->valueCount; i++) {
            header += ",";
            header += obj->additionalUnits[i];
        }
    }
    
    return appendToCSV(path, header);
}

// ============================================================================
// RECORD WRITING FUNCTIONS
// ============================================================================

bool writeInputsRecord(void) {
    const char* path = RECORDED_DATA_DIR "/inputs.csv";
    
    String line = getRecordingTimestamp();
    
    // ADC inputs 0-7
    for (int i = 0; i <= 7; i++) {
        ObjectCache::CachedObject* obj = objectCache.getObject(i);
        line += ",";
        if (obj && obj->valid) {
            line += String(obj->value, 3);
        } else {
            line += "NaN";
        }
    }
    
    // Digital inputs 13-20
    for (int i = 13; i <= 20; i++) {
        ObjectCache::CachedObject* obj = objectCache.getObject(i);
        line += ",";
        if (obj && obj->valid) {
            line += String((int)obj->value);
        } else {
            line += "-1";
        }
    }
    
    return appendToCSV(path, line);
}

bool writeOutputsRecord(void) {
    const char* path = RECORDED_DATA_DIR "/outputs.csv";
    
    String line = getRecordingTimestamp();
    
    // DAC outputs 8-9
    for (int i = 8; i <= 9; i++) {
        ObjectCache::CachedObject* obj = objectCache.getObject(i);
        line += ",";
        if (obj && obj->valid) {
            line += String(obj->value, 2);
        } else {
            line += "NaN";
        }
    }
    
    // Digital outputs 21-25
    for (int i = 21; i <= 25; i++) {
        ObjectCache::CachedObject* obj = objectCache.getObject(i);
        line += ",";
        if (obj && obj->valid) {
            line += String(obj->value, 1);
        } else {
            line += "NaN";
        }
    }
    
    return appendToCSV(path, line);
}

bool writeMotorsRecord(void) {
    const char* path = RECORDED_DATA_DIR "/motors.csv";
    
    String line = getRecordingTimestamp();
    
    // Stepper motor (index 26)
    ObjectCache::CachedObject* stepper = objectCache.getObject(26);
    if (stepper && stepper->valid) {
        line += ",";
        line += String(stepper->value, 1);  // RPM
        line += ",";
        line += (stepper->flags & IPC_SENSOR_FLAG_RUNNING) ? "1" : "0";
        line += ",";
        line += (stepper->flags & IPC_SENSOR_FLAG_DIRECTION) ? "FWD" : "REV";
    } else {
        line += ",NaN,0,N/A";
    }
    
    // DC motors (indices 27-30)
    for (int i = 27; i <= 30; i++) {
        ObjectCache::CachedObject* motor = objectCache.getObject(i);
        if (motor && motor->valid) {
            line += ",";
            line += String(motor->value, 1);  // Power %
            line += ",";
            line += (motor->flags & IPC_SENSOR_FLAG_RUNNING) ? "1" : "0";
            line += ",";
            line += (motor->flags & IPC_SENSOR_FLAG_DIRECTION) ? "FWD" : "REV";
            line += ",";
            // Current from additionalValues[0] if available
            if (motor->valueCount > 0) {
                line += String(motor->additionalValues[0], 3);
            } else {
                line += "NaN";
            }
        } else {
            line += ",NaN,0,N/A,NaN";
        }
    }
    
    return appendToCSV(path, line);
}

bool writeSensorsRecord(void) {
    const char* path = RECORDED_DATA_DIR "/sensors.csv";
    
    String line = getRecordingTimestamp();
    
    // RTD sensors 10-12
    for (int i = 10; i <= 12; i++) {
        ObjectCache::CachedObject* obj = objectCache.getObject(i);
        line += ",";
        if (obj && obj->valid) {
            line += String(obj->value, 2);
        } else {
            line += "NaN";
        }
    }
    
    // Device sensors 70-89
    for (int i = 70; i <= 89; i++) {
        ObjectCache::CachedObject* obj = objectCache.getObject(i);
        if (obj && obj->valid) {
            line += ",";
            line += String(obj->value, 3);
        }
    }
    
    return appendToCSV(path, line);
}

bool writeEnergyRecord(void) {
    const char* path = RECORDED_DATA_DIR "/energy.csv";
    
    String line = getRecordingTimestamp();
    
    // INA260 sensors at indices 31-32 (multi-value: V, A, W)
    for (int i = 31; i <= 32; i++) {
        ObjectCache::CachedObject* obj = objectCache.getObject(i);
        if (obj && obj->valid) {
            // Primary value is voltage
            line += ",";
            line += String(obj->value, 3);
            // Additional values: current, power
            if (obj->valueCount >= 2) {
                line += ",";
                line += String(obj->additionalValues[0], 3);  // Current
                line += ",";
                line += String(obj->additionalValues[1], 3);  // Power
            } else {
                line += ",NaN,NaN";
            }
        } else {
            line += ",NaN,NaN,NaN";
        }
    }
    
    return appendToCSV(path, line);
}

// Helper function to get a good name for recording, with sensible defaults
// Always includes index number for unnamed objects to ensure unique filenames
static String getRecordingName(uint8_t index, uint8_t objectType) {
    // First try the configured name from ioConfig
    const char* configName = getObjectNameByIndex(index);
    if (configName && configName[0] != '\0') {
        // User-provided name - append index to ensure uniqueness if multiple have same name
        char nameWithIndex[64];
        snprintf(nameWithIndex, sizeof(nameWithIndex), "%s_%d", configName, index);
        return String(nameWithIndex);
    }
    
    // Generate sensible default names based on object type
    // Always include the actual index to guarantee uniqueness
    char defaultName[48];
    switch (objectType) {
        // Controllers (40-49)
        case OBJ_T_TEMPERATURE_CONTROL:
            snprintf(defaultName, sizeof(defaultName), "Temperature_Controller_%d", index);
            break;
        case OBJ_T_PH_CONTROL:
            snprintf(defaultName, sizeof(defaultName), "pH_Controller_%d", index);
            break;
        case OBJ_T_FLOW_CONTROL:
            snprintf(defaultName, sizeof(defaultName), "Flow_Controller_%d", index);
            break;
        case OBJ_T_DISSOLVED_OXYGEN_CONTROL:
            snprintf(defaultName, sizeof(defaultName), "DO_Controller_%d", index);
            break;
        case OBJ_T_OPTICAL_DENSITY_CONTROL:
            snprintf(defaultName, sizeof(defaultName), "OD_Controller_%d", index);
            break;
        case OBJ_T_GAS_FLOW_CONTROL:
            snprintf(defaultName, sizeof(defaultName), "Gas_Flow_Controller_%d", index);
            break;
        case OBJ_T_STIRRER_CONTROL:
            snprintf(defaultName, sizeof(defaultName), "Stirrer_Controller_%d", index);
            break;
        case OBJ_T_PUMP_CONTROL:
            snprintf(defaultName, sizeof(defaultName), "Pump_Controller_%d", index);
            break;
        
        // Devices (50-69)
        case OBJ_T_ALICAT_MFC:
            snprintf(defaultName, sizeof(defaultName), "Alicat_MFC_%d", index);
            break;
        case OBJ_T_HAMILTON_PH_PROBE:
            snprintf(defaultName, sizeof(defaultName), "Hamilton_pH_Probe_%d", index);
            break;
        case OBJ_T_HAMILTON_DO_PROBE:
            snprintf(defaultName, sizeof(defaultName), "Hamilton_DO_Probe_%d", index);
            break;
        case OBJ_T_HAMILTON_OD_PROBE:
            snprintf(defaultName, sizeof(defaultName), "Hamilton_OD_Probe_%d", index);
            break;
        
        // Device sensors (70-89)
        case OBJ_T_FLOW_SENSOR:
            snprintf(defaultName, sizeof(defaultName), "Flow_Sensor_%d", index);
            break;
        case OBJ_T_PRESSURE_SENSOR:
            snprintf(defaultName, sizeof(defaultName), "Pressure_Sensor_%d", index);
            break;
        case OBJ_T_DISSOLVED_OXYGEN_SENSOR:
            snprintf(defaultName, sizeof(defaultName), "DO_Sensor_%d", index);
            break;
        case OBJ_T_PH_SENSOR:
            snprintf(defaultName, sizeof(defaultName), "pH_Sensor_%d", index);
            break;
        case OBJ_T_OPTICAL_DENSITY_SENSOR:
            snprintf(defaultName, sizeof(defaultName), "OD_Sensor_%d", index);
            break;
        case OBJ_T_TEMPERATURE_SENSOR:
            snprintf(defaultName, sizeof(defaultName), "Temperature_Sensor_%d", index);
            break;
        
        default:
            snprintf(defaultName, sizeof(defaultName), "Object_%d", index);
            break;
    }
    return String(defaultName);
}

// Helper to make a filename-safe version of a name
static String sanitizeFilename(const String& name) {
    String safe = name;
    // Replace spaces and special characters with underscores
    for (size_t i = 0; i < safe.length(); i++) {
        char c = safe.charAt(i);
        if (c == ' ' || c == '/' || c == '\\' || c == ':' || c == '*' || 
            c == '?' || c == '"' || c == '<' || c == '>' || c == '|') {
            safe.setCharAt(i, '_');
        }
    }
    // Truncate to reasonable length for FAT32
    if (safe.length() > 32) {
        safe = safe.substring(0, 32);
    }
    return safe;
}

bool writeControllersRecord(void) {
    bool anyWritten = false;
    int controllersFound = 0;
        
    // Controllers at indices 40-49
    for (int i = 40; i <= 49; i++) {
        ObjectCache::CachedObject* obj = objectCache.getObject(i);
        if (obj && obj->valid) {
            controllersFound++;
            
            // Get a good name using the helper (checks ioConfig first, then generates default)
            String name = getRecordingName(i, obj->objectType);
            String safeFilename = sanitizeFilename(name);
            
            char path[64];
            snprintf(path, sizeof(path), "%s/%s.csv", CONTROLLERS_DIR, safeFilename.c_str());
            
            // Check if header needs to be written (file doesn't exist)
            uint64_t fileSize = getFileSize(path);
            if (fileSize == 0) {
                writeControllerHeader(i, safeFilename.c_str());
            }
            
            String line = getRecordingTimestamp();
            line += ",";
            line += (obj->flags & IPC_SENSOR_FLAG_RUNNING) ? "1" : "0";  // Enabled
            line += ",";
            line += String(obj->value, 3);  // Setpoint or primary value
            
            // Additional values if available
            for (int v = 0; v < obj->valueCount && v < 4; v++) {
                line += ",";
                line += String(obj->additionalValues[v], 3);
            }
            
            if (appendToCSV(path, line)) {
                anyWritten = true;
            }
        }
    }
    
    if (controllersFound == 0) {
        static bool warnedNoControllers = false;
        if (!warnedNoControllers) {
            log(LOG_INFO, false, "Controllers recording enabled but no controllers found in cache (indices 40-49)\n");
            warnedNoControllers = true;
        }
    }
    
    return anyWritten;
}

bool writeDevicesRecord(void) {
    bool anyWritten = false;
    int devicesFound = 0;
    
    // Devices at indices 50-69
    for (int i = 50; i <= 69; i++) {
        ObjectCache::CachedObject* obj = objectCache.getObject(i);
        if (obj && obj->valid) {
            devicesFound++;
            
            // Get a good name using the helper (checks ioConfig first, then generates default)
            String name = getRecordingName(i, obj->objectType);
            String safeFilename = sanitizeFilename(name);
            
            char path[64];
            snprintf(path, sizeof(path), "%s/%s.csv", DEVICES_DIR, safeFilename.c_str());
            
            // Check if header needs to be written (file doesn't exist or is empty)
            uint64_t fileSize = getFileSize(path);
            if (fileSize == 0) {
                writeDeviceHeader(i, safeFilename.c_str());
            }
            
            String line = getRecordingTimestamp();
            line += ",";
            line += String(obj->value, 3);
            
            // Additional values
            for (int v = 0; v < obj->valueCount && v < 4; v++) {
                line += ",";
                line += String(obj->additionalValues[v], 3);
            }
            
            if (appendToCSV(path, line)) {
                anyWritten = true;
            }
        }
    }
    
    if (devicesFound == 0) {
        static bool warnedNoDevices = false;
        if (!warnedNoDevices) {
            log(LOG_INFO, false, "Devices recording enabled but no devices found in cache (indices 50-69)\n");
            warnedNoDevices = true;
        }
    }
    
    return anyWritten;
}

// =============================================================================
// TERMINAL BACKUP
// =============================================================================

bool createTerminalBackup() {
    if (!sdInfo.ready) {
        return false;
    }
    
    // Generate timestamp for filename
    char timestamp[20];
    snprintf(timestamp, sizeof(timestamp), "%04d-%02d-%02d_%02d-%02d-%02d",
             globalDateTime.year, globalDateTime.month, globalDateTime.day,
             globalDateTime.hour, globalDateTime.minute, globalDateTime.second);
    
    // Build filename
    char filename[64];
    snprintf(filename, sizeof(filename), "/backups/backup_%s.json", timestamp);
    
    log(LOG_INFO, false, "Creating backup: %s\n", filename);
    
    // Ensure /backups directory exists
    if (!sd.exists("/backups")) {
        if (!sd.mkdir("/backups")) {
            log(LOG_ERROR, false, "Failed to create /backups directory\n");
            return false;
        }
    }
    
    // Create JSON document - read config files directly from flash
    DynamicJsonDocument doc(32768);
    
    // Add backup metadata
    doc["backup_version"] = 1;
    doc["backup_timestamp"] = timestamp;
    doc["backup_source"] = "terminal";
    
    // Read system_config.json directly from flash
    File sysFile = LittleFS.open(CONFIG_FILENAME, "r");
    if (sysFile) {
        DynamicJsonDocument sysDoc(2048);
        DeserializationError error = deserializeJson(sysDoc, sysFile);
        sysFile.close();
        
        if (!error) {
            doc["system_config"] = sysDoc.as<JsonObject>();
        } else {
            log(LOG_WARNING, false, "Failed to parse system config: %s\n", error.c_str());
            return false;
        }
    } else {
        log(LOG_WARNING, false, "System config file not found\n");
        return false;
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
            log(LOG_WARNING, false, "Failed to parse IO config: %s\n", error.c_str());
            return false;
        }
    } else {
        log(LOG_WARNING, false, "IO config file not found\n");
        return false;
    }
    
    // Write to SD card
    FsFile backupFile = sd.open(filename, O_WRITE | O_CREAT | O_TRUNC);
    if (!backupFile) {
        log(LOG_ERROR, false, "Failed to create backup file\n");
        return false;
    }
    
    size_t written = serializeJsonPretty(doc, backupFile);
    backupFile.close();
    
    if (written == 0) {
        log(LOG_ERROR, false, "Failed to write backup data\n");
        return false;
    }
    
    log(LOG_INFO, false, "Backup saved: %s (%d bytes)\n", filename, written);
    return true;
}