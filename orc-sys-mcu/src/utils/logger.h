#pragma once

#include <Arduino.h>
#include <FreeRTOS.h>
#include <semphr.h>

// Buffer sizes
#define DEBUG_PRINTF_BUFFER_SIZE 256

// Log entry types
#define LOG_INFO 0
#define LOG_WARNING 1
#define LOG_ERROR 2
#define LOG_DEBUG 3

void init_logger(void);

// Debug functions
void debug_printf(uint8_t logLevel, const char* format, ...);
void osDebugPrint(void);

// Serial port mutex
extern SemaphoreHandle_t serialMutex;

extern bool serialReady;