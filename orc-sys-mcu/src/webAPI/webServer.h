#pragma once

/**
 * @file webServer.h
 * @brief Web server setup and static file serving
 * 
 * This module handles:
 * - Web server initialization and route registration
 * - Static file serving from LittleFS
 * - Route dispatching to API modules
 */

#include <Arduino.h>
#include <WebServer.h>
#include <LittleFS.h>

// =============================================================================
// Function Declarations
// =============================================================================

/**
 * @brief Initialize and configure the web server
 * Sets up all API endpoints and static file serving
 */
void setupWebServer(void);

/**
 * @brief Handle web server requests (called from main loop)
 */
void handleWebServer(void);

/**
 * @brief Serve the root page (index.html)
 */
void handleRoot(void);

/**
 * @brief Serve static files from LittleFS
 * @param path Path to the file to serve
 */
void handleFile(const char* path);

/**
 * @brief File manager page handler (redirects to index)
 */
void handleFileManager(void);
void handleFileManagerPage(void);

/**
 * @brief Handle dynamic controller routes (DELETE /api/controller/{index})
 */
void handleDynamicControllerRoute(void);
