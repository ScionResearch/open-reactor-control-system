#include "sdManager.h"

SdFs sd;
FsFile file;

// State variables for SD manager
bool sdReady = false;
bool sdInserted = false;
SDManagerState sdState = SD_STATE_INIT;
unsigned long lastSDOperation = 0;

void init_sdManager(void) {
    log(LOG_INFO, false, "Initializing SD manager\n");
    SPI1.setMISO(PIN_SD_MISO);
    SPI1.setMOSI(PIN_SD_MOSI);
    SPI1.setSCK(PIN_SD_SCK);
    
    FsDateTime::setCallback(dateTimeCallback);
    
    // Set initial state
    sdState = SD_STATE_MOUNT;
}

void handleSDManager(void) {
    // State machine for SD card management
    switch (sdState) {
        case SD_STATE_INIT:
            // Nothing to do, wait for transition to mount
            sdState = SD_STATE_MOUNT;
            break;
            
        case SD_STATE_MOUNT:
            mountSD();
            break;
            
        case SD_STATE_READY:
            maintainSD();
            break;
            
        case SD_STATE_ERROR:
            // Wait some time before trying to remount
            if (millis() - lastSDOperation > 5000) {
                log(LOG_INFO, false, "Retrying SD card mount after error\n");
                sdState = SD_STATE_MOUNT;
            }
            break;
            
        case SD_STATE_REMOVED:
            // Check if card has been inserted
            if (!digitalRead(PIN_SD_CD)) {
                log(LOG_INFO, false, "SD card inserted, attempting to mount\n");
                sdState = SD_STATE_MOUNT;
            }
            break;
    }
}

void mountSD(void) {
    lastSDOperation = millis();
    
    if (digitalRead(PIN_SD_CD)) {
        log(LOG_WARNING, false,"SD card not inserted\n");
        sdInserted = false;
        sdReady = false;
        sdState = SD_STATE_REMOVED;
        
        // Update system status
        status.sdCardOK = false;
        return;
    }
    else {
        bool sdSPIinitialised = false;
        bool sdSDIOinitialised = false;
        sdInserted = true;
        
        // Try SDIO first, then fall back to SPI
        if (!sd.begin(SDIO_CONFIG)) {
            log(LOG_ERROR, false, "SD card initialisation with SDIO config failed, attempting SPI config\n");
            if (!sd.begin(SdSpiConfig(PIN_SD_CS, DEDICATED_SPI, SD_SCK_MHZ(4), &SPI1))) {
                if (sd.card()->errorCode()) {
                    log(LOG_ERROR, false, "SD card initialisation failed with error code %d\n", sd.card()->errorCode());
                }
                sdState = SD_STATE_ERROR;
                return;
            } else sdSPIinitialised = true;
        } else sdSDIOinitialised = true;
        
        if (sdSPIinitialised || sdSDIOinitialised) {
            log(LOG_INFO, false, "SD card initialisation successful, using %s\n", sdSPIinitialised ? "SPI" : "SDIO");
            
            // Check for correct folder structure and create if missing
            log(LOG_INFO, false, "Checking for correct folder structure\n");
            sd_mkdir("/sensors");
            sd_mkdir("/logs");
            
            // Check for log files and create if missing
            if (!sd_exists("/logs/system.txt")) {
                file = sd_open("/logs/system.txt", O_CREAT | O_RDWR | O_APPEND);
                file.close();
            }
            if (!sd_exists("/sensors/sensors.csv")) {
                file = sd_open("/sensors/sensors.csv", O_CREAT | O_RDWR | O_APPEND);
                file.close();
            }
            sdReady = true;
            sdState = SD_STATE_READY;
            
            // Update system status
            status.sdCardOK = true;
            
            log(LOG_INFO, false, "SD card mounted OK\n");
            printSDInfo();
        }
    }
}

void maintainSD(void) {
    // Check if card is still inserted
    if (digitalRead(PIN_SD_CD) && sdInserted) {
        log(LOG_WARNING, false, "SD card removed\n");
        sdInserted = false;
        sdReady = false;
        sdState = SD_STATE_REMOVED;
        
        // Update system status
        status.sdCardOK = false;
    }
}

uint64_t getFileSize(const char* path) {
    FsFile file;
    uint64_t size = 0;
    
    if (sdReady) {
        if (sd_exists(path)) {
            file = sd_open(path, O_RDONLY);
            if (file) {
                size = file.fileSize();
                file.close();
            }
        }
    }
    return size;
}

void printSDInfo(void) {
    if (!sdReady) {
        if (digitalRead(PIN_SD_CD)) log(LOG_INFO, false, "SD card not inserted\n");
        else log(LOG_INFO, false, "SD card not ready\n");
        return;
    }
    
    uint32_t size = sd.card()->sectorCount();
    log(LOG_INFO, false, "SD card size: %d sectors, %0.1f GB\n", size, 0.000000512 * (float)size);
    log(LOG_INFO, false, "Free space: %0.1f GB\n", 0.000000001 * (float)sd.vol()->bytesPerCluster() * (float)sd.freeClusterCount());
    log(LOG_INFO, false, "Volume is FAT%d\n", sd.vol()->fatType());

    log(LOG_INFO, false, "File info:\n");
    
    // Get file sizes using the helper function
    uint64_t logFileSize = getFileSize("/logs/system.txt");
    uint64_t sensorFileSize = getFileSize("/sensors/sensors.csv");
    
    log(LOG_INFO, false, "Log file size: %0.1f kbytes\n", 0.001 * (float)logFileSize);
    log(LOG_INFO, false, "Sensor file size: %0.1f kbytes\n", 0.001 * (float)sensorFileSize);
}

void dateTimeCallback(uint16_t* date, uint16_t* time) {
    DateTime now;
    getGlobalDateTime(now);

    // Return date using FS_DATE macro to format fields.
    *date = FS_DATE(now.year, now.month, now.day);

    // Return time using FS_TIME macro to format fields.
    *time = FS_TIME(now.hour, now.minute, now.second);
}

void writeLog(const char *message) {
    if (!sdReady) return;
    DateTime now;
    getGlobalDateTime(now);
    char dateTimeStr[20];
    snprintf(dateTimeStr, sizeof(dateTimeStr), "%04d-%02d-%02d %02d:%02d:%02d", now.year, now.month, now.day, now.hour, now.minute, now.second);

    char buf[strlen(dateTimeStr) + strlen(message) + 10];
    snprintf(buf, sizeof(buf), "[%s]\t\t%s", dateTimeStr, message);

    // Log file size check
    uint64_t logFileSize = getFileSize("/logs/system.txt");
    if (logFileSize > SD_LOG_MAX_SIZE) {
        // Rename the existing log file and create a new one
        char fNameBuf[50];
        snprintf(fNameBuf, sizeof(fNameBuf), "/logs/system-log-archive-%04d-%02d-%02d.txt", now.year, now.month, now.day);
        
        if (sd_exists(fNameBuf)) {
            for (int i = 0; i < 100; i++) {
                char tempBuf[50];
                snprintf(tempBuf, sizeof(tempBuf), "%s-%d", fNameBuf, i);
                if (!sd_exists(tempBuf)) {
                    strcpy(fNameBuf, tempBuf);
                    break;
                }
            }
        }
        sd_rename("/logs/system.txt", fNameBuf);
        file = sd_open("/logs/system.txt", O_CREAT | O_RDWR | O_APPEND);
        if (file) {
            file.print(buf);
            file.close();
        }
        return;
    }

    file = sd_open("/logs/system.txt", O_CREAT | O_RDWR | O_APPEND);
    if (file) {
        file.print(buf);
        file.close();
    }
}

void writeSensorData(/* Sensor data struct to be defined */) {
    // To be implemented
}

// Thread-safe file operations implementations
bool sd_mkdir(const char* path) {
    if (!sdReady) return false;
    return sd.mkdir(path);
}

bool sd_exists(const char* path) {
    if (!sdReady) return false;
    return sd.exists(path);
}

bool sd_rename(const char* oldPath, const char* newPath) {
    if (!sdReady) return false;
    return sd.rename(oldPath, newPath);
}

FsFile sd_open(const char* path, oflag_t oflag) {
    FsFile file;
    if (sdReady) {
        file = sd.open(path, oflag);
    }
    return file;
}