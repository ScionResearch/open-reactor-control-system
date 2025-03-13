#pragma once

#include "../sys_init.h"

#define DISABLE_FS_H_WARNING
#include <SdFat.h>

#define SD_FAT_TYPE 3
#define SPI_SPEED SD_SCK_MHZ(4)
#define SDIO_CONFIG SdioConfig(PIN_SDIO_CLK, PIN_SDIO_CMD, PIN_SDIO_D0)

#define SD_LOG_MAX_SIZE 10000000        //10MB max size
#define SD_SENSOR_MAX_SIZE 10000000     //10MB max size

// SD Manager state machine states
enum SDManagerState {
  SD_STATE_INIT,
  SD_STATE_MOUNT,
  SD_STATE_READY,
  SD_STATE_ERROR,
  SD_STATE_REMOVED
};

void init_sdManager(void);
void handleSDManager(void);
void mountSD(void);
void maintainSD(void);
void printSDInfo(void);
uint64_t getFileSize(const char* path);
void dateTimeCallback(uint16_t* date, uint16_t* time);
void writeLog(const char *message);
void writeSensorData(/* Sensor data struct to be defined */);

extern SdFs sd;
extern bool sdReady;
extern bool sdInserted;
extern SDManagerState sdState;

// Thread-safe file operations - use these instead of direct file access
bool sd_mkdir(const char* path);
bool sd_exists(const char* path);
bool sd_rename(const char* oldPath, const char* newPath);
FsFile sd_open(const char* path, oflag_t oflag);