#pragma once

/**
 * @file apiFileManager.h
 * @brief SD card file manager API endpoints
 * 
 * Handles:
 * - /api/sd/list - List directory contents
 * - /api/sd/download - Download file
 * - /api/sd/view - View file in browser
 * - /api/sd/delete - Delete file
 */

#include <Arduino.h>

void setupFileManagerAPI(void);

void handleSDListDirectory(void);
void handleSDDownloadFile(void);
void handleSDViewFile(void);
void handleSDDeleteFile(void);
