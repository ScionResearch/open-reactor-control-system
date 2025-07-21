#include "utils/statusManager.h"
#include "sdManager.h"

SdFs sd;
FsFile file;

sdInfo_t sdInfo;
uint32_t sdTS;
volatile bool sdLocked = false;

void init_sdManager(void) {
    SPI1.setMISO(PIN_SD_MISO);
    SPI1.setMOSI(PIN_SD_MOSI);
    SPI1.setSCK(PIN_SD_SCK);
    
    FsDateTime::setCallback(dateTimeCallback);
    
    sdTS = millis();
    log(LOG_INFO, false, "SD card manager initialised\n");
}

void manageSD(void) {
    log(LOG_DEBUG, false, "[SD] manageSD start\n");
    if (millis() - sdTS < SD_MANAGE_INTERVAL) return;
    sdTS = millis();
    
    if (!sdInfo.ready && !digitalRead(PIN_SD_CD)) {
        log(LOG_DEBUG, false, "[SD] mountSD called\n");
        mountSD();
    } else {
        log(LOG_DEBUG, false, "[SD] maintainSD called\n");
        maintainSD();
    }
    
    // Every 10 minutes, update SD info for the status display
    static uint32_t sdInfoTS = 0;
    if (sdInfo.ready && (millis() - sdInfoTS > 600000)) {
        sdInfoTS = millis();
        printSDInfo();
    }
    log(LOG_DEBUG, false, "[SD] manageSD end\n");
}

void mountSD(void) {
    // Check if SD card is inserted
    if (digitalRead(PIN_SD_CD)) {
        log(LOG_WARNING, false,"SD card not inserted\n");
        log(LOG_DEBUG, false, "[SD] mountSD: card not inserted, aborting\n");
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
        log(LOG_DEBUG, false, "[SD] mountSD: sdLocked, aborting\n");
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
                log(LOG_DEBUG, false, "[SD] mountSD: all init attempts failed\n");
            } else {
                log(LOG_DEBUG, false, "[SD] mountSD: SPI init succeeded\n");
                sdSPIinitialised = true;
            }
        }
    } else {
        log(LOG_DEBUG, false, "[SD] mountSD: SDIO init succeeded\n");
        sdSDIOinitialised = true;
    }
    if (sdSPIinitialised || sdSDIOinitialised) {
        log(LOG_INFO, false, "SD card initialisation successful, using %s\n", sdSPIinitialised ? "SPI" : "SDIO");
        log(LOG_DEBUG, false, "[SD] mountSD: init success, checking folders\n");
        // Check for correct folder structure and create if missing
        log(LOG_INFO, false, "Checking for correct folder structure\n");
        if (!sd.exists("/sensors")) sd.mkdir("/sensors");
        if (!sd.exists("/logs")) sd.mkdir("/logs");
        // Check for log files and create if missing
        if (!sd.exists("/logs/system.txt")) {
            file = sd.open("/logs/system.txt", O_CREAT | O_RDWR | O_APPEND);
            file.close();
        }
        if (!sd.exists("/sensors/sensors.csv")) {
            file = sd.open("/sensors/sensors.csv", O_CREAT | O_RDWR | O_APPEND);
            file.close();
        }
        sdInfo.ready = true;
    }
    if (sdInfo.ready) {
        log(LOG_INFO, false, "SD card mounted OK\n");
        log(LOG_DEBUG, false, "[SD] mountSD: mount complete\n");
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
        log(LOG_DEBUG, false, "[SD] maintainSD: sdLocked, aborting\n");
        return;
    }
    sdLocked = true;
    if (digitalRead(PIN_SD_CD) && sdInfo.inserted) {
        log(LOG_WARNING, false, "SD card removed\n");
        log(LOG_DEBUG, false, "[SD] maintainSD: card removed\n");
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
    uint64_t sensorFileSize = getFileSize("/sensors/sensors.csv");
    sdLocked = true;
    sdInfo.logSizeBytes = logFileSize;
    sdInfo.sensorSizeBytes = sensorFileSize;
    
    log(LOG_INFO, false, "SD card size: %0.1f GB\n", sdInfo.cardSizeBytes * 0.000000001);
    log(LOG_INFO, false, "Free space: %0.1f GB\n", sdInfo.cardFreeBytes * 0.000000001);
    log(LOG_INFO, false, "Volume is FAT%d\n", sd.vol()->fatType());
    log(LOG_INFO, false, "Log file size: %0.1f kbytes\n", 0.001 * (float)logFileSize);
    log(LOG_INFO, false, "Sensor file size: %0.1f kbytes\n", 0.001 * (float)sensorFileSize);
    
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

void writeSensorData(/* Sensor data struct to be defined */) {
    // To be implemented
}