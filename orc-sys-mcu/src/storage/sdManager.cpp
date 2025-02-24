#include "sdManager.h"

SdFs sd;
FsFile file;

SemaphoreHandle_t sdMutex = NULL;
bool sdReady = false;
bool sdInserted = false;

void init_sdManager(void) {
    sdMutex = xSemaphoreCreateMutex();
  if (sdMutex == NULL) {
    debug_printf(LOG_ERROR, "Failed to create sdMutex!\n");
    while (1);
  }
  xTaskCreate(manageSD, "SD mngr", 512, NULL, 1, NULL);
}

void manageSD(void *param) {
    (void)param;
    debug_printf(LOG_INFO, "Initialising SD card\n");
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
        debug_printf(LOG_WARNING, "SD card not inserted\n");
        sdInserted = false;
        sdReady = false;
        return;
    }
    else {
        sdInserted = true;
        if (xSemaphoreTake(sdMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            if (!sd.begin(SdSpiConfig(PIN_SD_CS, DEDICATED_SPI, SD_SCK_MHZ(4), &SPI1))) {
                if (sd.card()->errorCode()) {
                    debug_printf(LOG_ERROR, "SD card initialisation failed with error code %d\n", sd.card()->errorCode());
                }
                sdReady = false;
            }
            else {
                debug_printf(LOG_INFO, "SD card initialisation successful\n");
                // Check for correct folder structure and create if missing
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
                printSDInfo();
            }
            xSemaphoreGive(sdMutex);
        }
    }
}

void maintainSD(void) {
    if (digitalRead(PIN_SD_CD) && sdInserted) {
        debug_printf(LOG_WARNING, "SD card removed\n");
        sdInserted = false;
        sdReady = false;
    }
}

void printSDInfo(void) {
    if (!sdReady) {
        if (digitalRead(PIN_SD_CD)) debug_printf(LOG_INFO, "SD card not inserted\n");
        else debug_printf(LOG_INFO, "SD card not ready\n");
        return;
    }
    if (xSemaphoreTake(sdMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        uint32_t size = sd.card()->sectorCount();
        debug_printf(LOG_INFO, "SD card size: %d sectors, %0.1f GB\n", size, 0.000000512 * (float)size + 0.5);
        debug_printf(LOG_INFO, "Volume is FAT%d\n", sd.vol()->fatType());
        debug_printf(LOG_INFO, "Cluster size (bytes): %d\n", sd.vol()->bytesPerCluster());
        debug_printf(LOG_INFO, "Files found (date time size name):\n[INFO] ");
        sd.ls(LS_R | LS_DATE | LS_SIZE);
        xSemaphoreGive(sdMutex);
    }
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