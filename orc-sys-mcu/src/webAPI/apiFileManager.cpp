/**
 * @file apiFileManager.cpp
 * @brief SD card file manager API implementation
 */

#include "apiFileManager.h"
#include "../network/networkManager.h"
#include "../storage/sdManager.h"
#include "../utils/logger.h"
#include <ArduinoJson.h>

void setupFileManagerAPI(void)
{
    server.on("/api/sd/list", HTTP_GET, handleSDListDirectory);
    server.on("/api/sd/download", HTTP_GET, handleSDDownloadFile);
    server.on("/api/sd/view", HTTP_GET, handleSDViewFile);
    server.on("/api/sd/delete", HTTP_DELETE, handleSDDeleteFile);
}

void handleSDListDirectory(void) {
    if (sdLocked) {
        server.send(423, "application/json", "{\"error\":\"SD card is locked\"}");
        return;
    }
    
    if (!sdInfo.ready) {
        server.send(503, "application/json", "{\"error\":\"SD card not available\"}");
        return;
    }
    
    // Get the requested directory path from the query parameter
    String path = server.hasArg("path") ? server.arg("path") : "/";
    
    // Make sure path starts with a forward slash
    if (!path.startsWith("/")) {
        path = "/" + path;
    }
    
    sdLocked = true;
    
    // Check if the path exists and is a directory
    if (!sd.exists(path.c_str())) {
        sdLocked = false;
        server.send(404, "application/json", "{\"error\":\"Directory not found\"}");
        return;
    }
    
    FsFile dir = sd.open(path.c_str());
    
    if (!dir.isDirectory()) {
        dir.close();
        sdLocked = false;
        server.send(400, "application/json", "{\"error\":\"Not a directory\"}");
        return;
    }
    
    // Create a JSON document for the response
    DynamicJsonDocument doc(16384);
    
    doc["path"] = path;
    JsonArray files = doc.createNestedArray("files");
    JsonArray directories = doc.createNestedArray("directories");
    
    // Read all files and directories in the requested path
    dir.rewindDirectory();
    
    FsFile file;
    char filename[256];
    while (file.openNext(&dir)) {
        file.getName(filename, sizeof(filename));
        
        // Skip hidden files/directories
        if (filename[0] == '.' || file.isHidden()) {
            file.close();
            continue;
        }
        
        // Create a JSON object for the file or directory
        if (file.isDirectory()) {
            JsonObject dirObj = directories.createNestedObject();
            dirObj["name"] = filename;
            
            String fullPath = path;
            if (!path.endsWith("/")) fullPath += "/";
            fullPath += filename;
            dirObj["path"] = fullPath;
        } else {
            JsonObject fileObj = files.createNestedObject();
            fileObj["name"] = filename;
            fileObj["size"] = file.size();
            
            String fullPath = path;
            if (!path.endsWith("/")) fullPath += "/";
            fullPath += filename;
            fileObj["path"] = fullPath;
            
            // Add last modified date
            uint16_t fileDate, fileTime;
            file.getModifyDateTime(&fileDate, &fileTime);
            
            int year = FS_YEAR(fileDate);
            int month = FS_MONTH(fileDate);
            int day = FS_DAY(fileDate);
            int hour = FS_HOUR(fileTime);
            int minute = FS_MINUTE(fileTime);
            int second = FS_SECOND(fileTime);
            
            char dateTimeStr[32];
            snprintf(dateTimeStr, sizeof(dateTimeStr), "%04d-%02d-%02d %02d:%02d:%02d", 
                     year, month, day, hour, minute, second);
            fileObj["modified"] = dateTimeStr;
        }
        
        file.close();
    }
    
    dir.close();
    sdLocked = false;
    
    String response;
    serializeJson(doc, response);
    server.send(200, "application/json", response);
}

void handleSDDownloadFile(void) {
    if (sdLocked) {
        server.send(423, "application/json", "{\"error\":\"SD card is locked\"}");
        return;
    }
    
    if (!sdInfo.ready) {
        server.send(503, "application/json", "{\"error\":\"SD card not available\"}");
        return;
    }
    
    String path = server.hasArg("path") ? server.arg("path") : "";
    
    if (path.length() == 0) {
        server.send(400, "application/json", "{\"error\":\"File path not specified\"}");
        return;
    }
    
    if (!path.startsWith("/")) {
        path = "/" + path;
    }
    
    sdLocked = true;
    
    if (!sd.exists(path.c_str())) {
        sdLocked = false;
        server.send(404, "application/json", "{\"error\":\"File not found\"}");
        return;
    }
    
    FsFile file = sd.open(path.c_str(), O_RDONLY);
    
    if (!file) {
        sdLocked = false;
        server.send(500, "application/json", "{\"error\":\"Failed to open file\"}");
        return;
    }
    
    if (file.isDirectory()) {
        file.close();
        sdLocked = false;
        server.send(400, "application/json", "{\"error\":\"Path is a directory, not a file\"}");
        return;
    }
    
    size_t fileSize = file.size();
    
    if (fileSize > MAX_DOWNLOAD_SIZE) {
        file.close();
        sdLocked = false;
        char errorMsg[128];
        snprintf(errorMsg, sizeof(errorMsg), 
                 "{\"error\":\"File is too large for download (%u bytes). Maximum size is %u bytes.\"}",
                 fileSize, MAX_DOWNLOAD_SIZE);
        server.send(413, "application/json", errorMsg);
        return;
    }
    
    String fileName = path;
    int lastSlash = fileName.lastIndexOf('/');
    if (lastSlash >= 0) {
        fileName = fileName.substring(lastSlash + 1);
    }
    
    String contentDisposition = "attachment; filename=\"" + fileName + "\"; filename*=UTF-8''" + fileName;
    
    server.sendHeader("Content-Type", "application/octet-stream");
    server.sendHeader("Content-Disposition", contentDisposition);
    server.sendHeader("Cache-Control", "no-cache");
    
    uint32_t startTime = millis();
    uint32_t lastProgressTime = startTime;
    const uint32_t timeout = 30000;
    
    WiFiClient client = server.client();
    
    const size_t bufferSize = 1024;
    uint8_t buffer[bufferSize];
    size_t bytesRead;
    size_t totalBytesRead = 0;
    bool timeoutOccurred = false;
    
    server.setContentLength(fileSize);
    server.send(200, "application/octet-stream", "");
    
    while (totalBytesRead < fileSize) {
        if (millis() - lastProgressTime > timeout) {
            log(LOG_WARNING, true, "Timeout occurred during file download\n");
            timeoutOccurred = true;
            break;
        }
        
        bytesRead = file.read(buffer, min(bufferSize, fileSize - totalBytesRead));
        
        if (bytesRead == 0) {
            break;
        }
        
        if (client.write(buffer, bytesRead) != bytesRead) {
            log(LOG_WARNING, true, "Client write error during file download\n");
            break;
        }
        
        totalBytesRead += bytesRead;
        lastProgressTime = millis();
        
        yield();
    }
    
    file.close();
    sdLocked = false;
    
    if (timeoutOccurred) {
        log(LOG_ERROR, true, "File download timed out after %u bytes\n", totalBytesRead);
    } else if (totalBytesRead == fileSize) {
        log(LOG_INFO, true, "File download completed successfully: %s (%u bytes)\n", 
            fileName.c_str(), totalBytesRead);
    } else {
        log(LOG_WARNING, true, "File download incomplete: %u of %u bytes transferred\n", 
            totalBytesRead, fileSize);
    }
}

void handleSDViewFile(void) {
    if (sdLocked) {
        server.send(423, "application/json", "{\"error\":\"SD card is locked\"}");
        return;
    }
    
    if (!sdInfo.ready) {
        server.send(503, "application/json", "{\"error\":\"SD card not available\"}");
        return;
    }
    
    String path = server.hasArg("path") ? server.arg("path") : "";
    
    if (path.length() == 0) {
        server.send(400, "application/json", "{\"error\":\"File path not specified\"}");
        return;
    }
    
    if (!path.startsWith("/")) {
        path = "/" + path;
    }
    
    sdLocked = true;
    
    if (!sd.exists(path.c_str())) {
        sdLocked = false;
        server.send(404, "application/json", "{\"error\":\"File not found\"}");
        return;
    }
    
    FsFile file = sd.open(path.c_str(), O_RDONLY);
    
    if (!file) {
        sdLocked = false;
        server.send(500, "application/json", "{\"error\":\"Failed to open file\"}");
        return;
    }
    
    if (file.isDirectory()) {
        file.close();
        sdLocked = false;
        server.send(400, "application/json", "{\"error\":\"Path is a directory, not a file\"}");
        return;
    }
    
    size_t fileSize = file.size();
    
    String fileName = path;
    int lastSlash = fileName.lastIndexOf('/');
    if (lastSlash >= 0) {
        fileName = fileName.substring(lastSlash + 1);
    }
    
    String contentType = "text/plain";
    if (fileName.endsWith(".html") || fileName.endsWith(".htm")) contentType = "text/html";
    else if (fileName.endsWith(".css")) contentType = "text/css";
    else if (fileName.endsWith(".js")) contentType = "application/javascript";
    else if (fileName.endsWith(".json")) contentType = "application/json";
    else if (fileName.endsWith(".png")) contentType = "image/png";
    else if (fileName.endsWith(".jpg") || fileName.endsWith(".jpeg")) contentType = "image/jpeg";
    else if (fileName.endsWith(".gif")) contentType = "image/gif";
    else if (fileName.endsWith(".ico")) contentType = "image/x-icon";
    else if (fileName.endsWith(".pdf")) contentType = "application/pdf";
    
    server.sendHeader("Content-Type", contentType);
    server.sendHeader("Content-Length", String(fileSize));
    server.sendHeader("Cache-Control", "max-age=86400");
    
    WiFiClient client = server.client();
    
    const size_t bufferSize = 2048;
    uint8_t buffer[bufferSize];
    size_t bytesRead;
    
    do {
        bytesRead = file.read(buffer, bufferSize);
        if (bytesRead > 0) {
            client.write(buffer, bytesRead);
        }
    } while (bytesRead == bufferSize);
    
    file.close();
    sdLocked = false;
}

void handleSDDeleteFile(void) {
    if (sdLocked) {
        server.send(423, "application/json", "{\"error\":\"SD card is locked\"}");
        return;
    }
    
    if (!sdInfo.ready) {
        server.send(503, "application/json", "{\"error\":\"SD card not available\"}");
        return;
    }
    
    String path = server.hasArg("path") ? server.arg("path") : "";
    
    if (path.length() == 0) {
        server.send(400, "application/json", "{\"error\":\"File path not specified\"}");
        return;
    }
    
    if (!path.startsWith("/")) {
        path = "/" + path;
    }
    
    // Safety check: prevent deletion of root or system directories
    if (path == "/" || path == "/logs" || path == "/sensor_data") {
        server.send(403, "application/json", "{\"error\":\"Cannot delete protected path\"}");
        return;
    }
    
    sdLocked = true;
    
    if (!sd.exists(path.c_str())) {
        sdLocked = false;
        server.send(404, "application/json", "{\"error\":\"File not found\"}");
        return;
    }
    
    FsFile file = sd.open(path.c_str(), O_RDONLY);
    if (!file) {
        sdLocked = false;
        server.send(500, "application/json", "{\"error\":\"Failed to access file\"}");
        return;
    }
    
    bool isDir = file.isDirectory();
    file.close();
    
    if (isDir) {
        sdLocked = false;
        server.send(400, "application/json", "{\"error\":\"Cannot delete directories, only files\"}");
        return;
    }
    
    String fileName = path;
    int lastSlash = fileName.lastIndexOf('/');
    if (lastSlash >= 0) {
        fileName = fileName.substring(lastSlash + 1);
    }
    
    if (sd.remove(path.c_str())) {
        sdLocked = false;
        log(LOG_INFO, true, "File deleted: %s\n", path.c_str());
        server.send(200, "application/json", "{\"success\":true,\"message\":\"File deleted successfully\"}");
    } else {
        sdLocked = false;
        log(LOG_ERROR, true, "Failed to delete file: %s\n", path.c_str());
        server.send(500, "application/json", "{\"error\":\"Failed to delete file\"}");
    }
}
