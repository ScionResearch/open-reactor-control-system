#pragma once

#include "../sys_init.h"

#define SD_FAT_TYPE 3
#define SPI_SPEED SD_SCK_MHZ(4)

void init_sdManager(void);
void manageSD(void *param);
void mountSD(void);
void maintainSD(void);
void printSDInfo(void);
void dateTimeCallback(uint16_t* date, uint16_t* time);
void writeLog(const char *message);
void writeSensorData(/* Sensor data struct to be defined */);

extern SemaphoreHandle_t sdMutex;
extern bool sdReady;
extern bool sdInserted;