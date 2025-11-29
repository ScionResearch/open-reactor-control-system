#pragma once

#include "../sys_init.h"

#ifndef DISABLE_FS_H_WARNING
#define DISABLE_FS_H_WARNING
#endif
#include <SdFat.h>

#define SD_FAT_TYPE 3
#define SPI_SPEED SD_SCK_MHZ(4)
#define SDIO_CONFIG SdioConfig(PIN_SDIO_CLK, PIN_SDIO_CMD, PIN_SDIO_D0)

#define SD_LOG_MAX_SIZE 5000000           // 5MB max size for log files
#define SD_RECORDING_MAX_SIZE 5000000     // 5MB max size for recording files

#define SD_MANAGE_INTERVAL 1000
#define RECORDING_MIN_INTERVAL 15         // Minimum recording interval in seconds

// Recording directories
#define RECORDED_DATA_DIR "/recorded_data"
#define CONTROLLERS_DIR "/recorded_data/controllers"
#define DEVICES_DIR "/recorded_data/devices"

// ============================================================================
// RECORDING CONFIGURATION
// ============================================================================

// Recording type enable/interval structure
struct RecordingTypeConfig {
    bool enabled;
    uint16_t interval;    // Recording interval in seconds (min 15)
};

// Main recording configuration structure
struct RecordingConfig {
    bool enabled;                        // Master enable for all recording
    RecordingTypeConfig inputs;          // ADC 0-7, Digital Inputs 13-20
    RecordingTypeConfig outputs;         // DAC 8-9, Digital Outputs 21-25
    RecordingTypeConfig motors;          // Stepper 26, DC Motors 27-30
    RecordingTypeConfig sensors;         // RTD 10-12, Device sensors 70-89
    RecordingTypeConfig energy;          // INA260 31-32 (V, A, W)
    RecordingTypeConfig controllers;     // Indices 40-49
    RecordingTypeConfig devices;         // Indices 50-69
};

// Recording scheduler state
struct RecordingScheduler {
    uint32_t lastInputsRecord;           // Last record time (epoch seconds)
    uint32_t lastOutputsRecord;
    uint32_t lastMotorsRecord;
    uint32_t lastSensorsRecord;
    uint32_t lastEnergyRecord;
    uint32_t lastControllersRecord;
    uint32_t lastDevicesRecord;
    bool inputsHeadersWritten;
    bool outputsHeadersWritten;
    bool motorsHeadersWritten;
    bool sensorsHeadersWritten;
    bool energyHeadersWritten;
    bool controllersHeadersWritten;
    bool devicesHeadersWritten;
};

// ============================================================================
// SD CARD INFO STRUCTURE
// ============================================================================

struct sdInfo_t {
    bool inserted;
    bool ready;
    uint64_t cardSizeBytes;
    uint64_t cardFreeBytes;
    uint64_t logSizeBytes;
};

// ============================================================================
// FUNCTION DECLARATIONS
// ============================================================================

// SD Card management
void init_sdManager(void);
void manageSD(void);
void mountSD(void);
void maintainSD(void);
void printSDInfo(void);
uint64_t getFileSize(const char* path);
void dateTimeCallback(uint16_t* date, uint16_t* time);

// Logging
bool writeLog(const char *message);

// Data Recording
void initRecordingDirectories(void);
void manageDataRecording(void);
void invalidateRecordingHeaders(void);    // Call when ioConfig changes

// Recording file operations
bool writeInputsRecord(void);
bool writeOutputsRecord(void);
bool writeMotorsRecord(void);
bool writeSensorsRecord(void);
bool writeEnergyRecord(void);
bool writeControllersRecord(void);
bool writeDevicesRecord(void);

// Header writing
bool writeInputsHeader(void);
bool writeOutputsHeader(void);
bool writeMotorsHeader(void);
bool writeSensorsHeader(void);
bool writeEnergyHeader(void);
bool writeControllerHeader(uint8_t index, const char* name);
bool writeDeviceHeader(uint8_t index, const char* name);

// Helper functions
bool appendToCSV(const char* path, const String& line);
void archiveRecordingFile(const char* path, const char* archivePrefix);

// Terminal backup
bool createTerminalBackup(void);

// ============================================================================
// EXTERNAL VARIABLES
// ============================================================================

extern SdFs sd;
extern volatile bool sdLocked;
extern sdInfo_t sdInfo;
extern uint32_t sdTS;
extern RecordingConfig recordingConfig;
extern RecordingScheduler recordingScheduler;
extern bool ioConfigChanged;              // Flag to trigger header rewrite
