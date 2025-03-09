#include "sdManager.h"

SdFs sd;
FsFile file;

SemaphoreHandle_t sdMutex = NULL;
bool sdReady = false;
bool sdInserted = false;

void init_sdManager(void) {
    sdMutex = xSemaphoreCreateMutex();
  if (sdMutex == NULL) {
    log(LOG_ERROR, false, "Failed to create sdMutex!\n");
    while (1);
  }
  xTaskCreate(manageSD, "SD mngr", 1024, NULL, 1, NULL);
}

void manageSD(void *param) {
    (void)param;
    log(LOG_INFO, false, "Initialising SD card\n");
    SPI1.setMISO(PIN_SD_MISO);
    SPI1.setMOSI(PIN_SD_MOSI);
    SPI1.setSCK(PIN_SD_SCK);

    FsDateTime::setCallback(dateTimeCallback);
    
    mountSD();

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(500));
        if (!sdReady && !digitalRead(PIN_SD_CD)) mountSD();
        else maintainSD();
    }
}

void mountSD(void) {
    if (digitalRead(PIN_SD_CD)) {
        log(LOG_WARNING, false,"SD card not inserted\n");
        sdInserted = false;
        sdReady = false;
        
        return;
    }
    else {
        bool sdSPIinitialised = false;
        bool sdSDIOinitialised = false;
        sdInserted = true;
        if (xSemaphoreTake(sdMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            if (!sd.begin(SDIO_CONFIG)) {
                log(LOG_ERROR, false, "SD card initialisation with SDIO config failed, attempting SPI config\n");
                if (!sd.begin(SdSpiConfig(PIN_SD_CS, DEDICATED_SPI, SD_SCK_MHZ(4), &SPI1))) {
                    if (sd.card()->errorCode()) {
                        log(LOG_ERROR, false, "SD card initialisation failed with error code %d\n", sd.card()->errorCode());
                    }
                } else sdSPIinitialised = true;

            } else sdSDIOinitialised = true;
            if (sdSPIinitialised || sdSDIOinitialised) {
                log(LOG_INFO, false, "SD card initialisation successful, using %s\n", sdSPIinitialised ? "SPI" : "SDIO");
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
                sdReady = true;
            }
            xSemaphoreGive(sdMutex);
            if (sdReady) log(LOG_INFO, false, "SD card mounted OK\n");
            printSDInfo();
        }
    }
}

void maintainSD(void) {
    if (digitalRead(PIN_SD_CD) && sdInserted) {
        log(LOG_WARNING, false, "SD card removed\n");
        sdInserted = false;
        sdReady = false;
    }
}

uint64_t getFileSize(const char* path) {
    FsFile file;
    uint64_t size = 0;
    if (xSemaphoreTake(sdMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        if (sd.exists(path)) {
            if (file.open(path, O_RDONLY)) {
                size = file.fileSize();
                file.close();
            }
        }
        xSemaphoreGive(sdMutex);
    } //else log(LOG_INFO, false, "SD mutex not available for get file size operation\n");
    return size;
}

void printSDInfo(void) {
    if (!sdReady) {
        if (digitalRead(PIN_SD_CD)) log(LOG_INFO, false, "SD card not inserted\n");
        else log(LOG_INFO, false, "SD card not ready\n");
        return;
    }
    if (xSemaphoreTake(sdMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        uint32_t size = sd.card()->sectorCount();
        log(LOG_INFO, false, "SD card size: %d sectors, %0.1f GB\n", size, 0.000000512 * (float)size);
        log(LOG_INFO, false, "Free space: %0.1f GB\n", 0.000000001 * (float)sd.vol()->bytesPerCluster() * (float)sd.freeClusterCount());
        log(LOG_INFO, false, "Volume is FAT%d\n", sd.vol()->fatType());
        xSemaphoreGive(sdMutex);

        log(LOG_INFO, false, "File info:\n");
        
        // Get file sizes using the helper function
        uint64_t logFileSize = getFileSize("/logs/system.txt");
        uint64_t sensorFileSize = getFileSize("/sensors/sensors.csv");
        
        log(LOG_INFO, false, "Log file size: %0.1f kbytes\n", 0.001 * (float)logFileSize);
        log(LOG_INFO, false, "Sensor file size: %0.1f kbytes\n", 0.001 * (float)sensorFileSize);
    }
    else log(LOG_INFO, false, "SD mutex not available\n");
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
        
        if (xSemaphoreTake(sdMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
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
            xSemaphoreGive(sdMutex);
        }
        return;
    }

    if (xSemaphoreTake(sdMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        file = sd.open("/logs/system.txt", O_CREAT | O_RDWR | O_APPEND);
        if (file) {
            file.print(buf);
            file.close();
        }
        xSemaphoreGive(sdMutex);
    }
}

void writeSensorData(/* Sensor data struct to be defined */) {
    
}