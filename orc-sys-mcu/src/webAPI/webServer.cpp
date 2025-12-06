/**
 * @file webServer.cpp
 * @brief Web server setup and static file serving implementation
 */

#include "webServer.h"
#include "../network/networkManager.h"
#include "../storage/sdManager.h"
#include "../utils/logger.h"
#include "../utils/statusManager.h"

// Include all API module headers
#include "apiSystem.h"
#include "apiNetwork.h"
#include "apiTime.h"
#include "apiMqtt.h"
#include "apiInputs.h"
#include "apiOutputs.h"
#include "apiControllers.h"
#include "apiDevices.h"
#include "apiFileManager.h"
#include "apiDashboard.h"

// =============================================================================
// Web Server Setup
// =============================================================================

void setupWebServer()
{
    // Initialize LittleFS for serving web files
    if (!LittleFS.begin())
    {
        log(LOG_ERROR, true, "LittleFS Mount Failed\n");
        return;
    }

    // Setup API endpoints from each module
    setupSystemAPI();
    setupNetworkAPI();
    setupTimeAPI();
    setupMqttAPI();
    setupInputsAPI();
    setupOutputsAPI();
    setupControllersAPI();
    setupDevicesAPI();
    setupFileManagerAPI();
    setupDashboardAPI();

    // Root and static file handlers
    server.on("/", HTTP_GET, handleRoot);
    server.on("/index.html", HTTP_GET, handleRoot);
    server.on("/files", HTTP_GET, handleFileManager);
    server.on("/filemanager", HTTP_GET, handleFileManagerPage);

    // Handle all other requests - static files or 404
    server.onNotFound([]() {
        String uri = server.uri();
        
        // Check if it's a dynamic API route that needs special handling
        // Device endpoints: /api/devices/{index}
        if (uri.startsWith("/api/devices/") && uri.length() > 13) {
            handleDynamicDeviceRoute();
            return;
        }
        
        // Controller endpoints: /api/controller/{index} (REST style)
        if (uri.startsWith("/api/controller/") && uri.length() > 16) {
            handleDynamicControllerRoute();
            return;
        }
        
        // Device control endpoints: /api/device/{index}/setpoint
        if (uri.startsWith("/api/device/") && uri.indexOf("/setpoint") > 0) {
            handleDynamicDeviceControlRoute();
            return;
        }
        
        // Otherwise try to serve as static file
        handleFile(uri.c_str());
    });

    server.begin();
    log(LOG_INFO, true, "Web server started on port 80\n");
}

// =============================================================================
// Request Handling
// =============================================================================

void handleWebServer() {
    if(!ethernetConnected) {
        return;
    }
    server.handleClient();
    if (!statusLocked) {
        statusLocked = true;
        status.webserverBusy = false;
        status.webserverUp = true;
        status.updated = true;
        statusLocked = false;
    }
}

// =============================================================================
// Static File Serving
// =============================================================================

void handleRoot()
{
    handleFile("/index.html");
}

void handleFileManager(void) {
    // Check if SD card is ready
    extern sdInfo_t sdInfo;
    if (!sdInfo.ready) {
        server.send(503, "application/json", "{\"error\":\"SD card not available\"}");
        return;
    }
    
    // Serve the main index page since file manager is now integrated
    handleRoot();
}

void handleFileManagerPage(void) {
    // Redirects to handleRoot (index.html) as file manager is now integrated
    handleRoot();
}

void handleDynamicControllerRoute(void)
{
    String uri = server.uri();
    HTTPMethod method = server.method();
    
    // Extract index from URI: /api/controller/{index}
    String indexStr = uri.substring(16);  // Skip "/api/controller/"
    
    int queryPos = indexStr.indexOf('?');
    if (queryPos > 0) {
        indexStr = indexStr.substring(0, queryPos);
    }
    
    uint8_t index = indexStr.toInt();
    
    if (method == HTTP_DELETE) {
        // Temperature controllers (40-42)
        if (index >= 40 && index < 43) {
            handleDeleteController(index);
        }
        // pH controller (43)
        else if (index == 43) {
            handleDeletepHController();
        }
        // Flow controllers (44-47)
        else if (index >= 44 && index < 48) {
            handleDeleteFlowController(index);
        }
        // DO controller (48)
        else if (index == 48) {
            handleDeleteDOController();
        }
        else {
            server.send(400, "application/json", "{\"error\":\"Invalid controller index\"}");
        }
    } else {
        server.send(405, "application/json", "{\"error\":\"Method not allowed\"}");
    }
}

void handleFile(const char *path)
{
    if(eth.status() != WL_CONNECTED) {
        if (!statusLocked) {
            statusLocked = true;
            status.webserverBusy = false;
            status.webserverUp = false;
            status.updated = true;
            statusLocked = false;
        }
        return;
    }
    if (!statusLocked) {
        statusLocked = true;
        status.webserverBusy = true;
        statusLocked = false;
    }
    
    String contentType;
    if (strcmp(path, "/") == 0 || strcmp(path, "/index.html") == 0)
        contentType = "text/html";
    else if (strstr(path, ".html"))
        contentType = "text/html";
    else if (strstr(path, ".css"))
        contentType = "text/css";
    else if (strstr(path, ".js"))
        contentType = "application/javascript";
    else if (strstr(path, ".json"))
        contentType = "application/json";
    else if (strstr(path, ".ico"))
        contentType = "image/x-icon";
    else if (strstr(path, ".png"))
        contentType = "image/png";
    else if (strstr(path, ".jpg") || strstr(path, ".jpeg"))
        contentType = "image/jpeg";
    else if (strstr(path, ".gif"))
        contentType = "image/gif";
    else if (strstr(path, ".svg"))
        contentType = "image/svg+xml";
    else
        contentType = "text/plain";

    String filePath = path;
    if (filePath.endsWith("/"))
        filePath += "index.html";
    if (!filePath.startsWith("/"))
        filePath = "/" + filePath;

    Serial.printf("[WEB] Request: %s (type: %s)\n", filePath.c_str(), contentType.c_str());

    if (LittleFS.exists(filePath))
    {
        File file = LittleFS.open(filePath, "r");
        if (!file) {
            Serial.printf("[WEB] ERROR: Failed to open file: %s\n", filePath.c_str());
            server.send(500, "text/plain", "Failed to open file");
            return;
        }
        
        size_t fileSize = file.size();
        unsigned long startTime = millis();
        Serial.printf("[WEB] Serving file: %s (%d bytes)\n", filePath.c_str(), fileSize);
        
        size_t sent = server.streamFile(file, contentType);
        file.close();
        
        unsigned long elapsed = millis() - startTime;
        Serial.printf("[WEB] Sent %d/%d bytes in %lu ms\n", sent, fileSize, elapsed);
    }
    else
    {
        Serial.printf("[WEB] File not found: %s\n", filePath.c_str());
        server.send(404, "text/plain", "File not found");
    }
    if (!statusLocked) {
        statusLocked = true;
        status.webserverBusy = false;
        status.webserverUp = true;
        status.updated = true;
        statusLocked = false;
    }
}
