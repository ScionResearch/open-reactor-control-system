/**
 * @file apiTime.cpp
 * @brief Time and NTP configuration API implementation
 */

#include "apiTime.h"
#include "../network/networkManager.h"
#include "../utils/logger.h"
#include "../utils/timeManager.h"
#include <ArduinoJson.h>

void setupTimeAPI()
{
    server.on("/api/time", HTTP_GET, []()
    {
        Serial.println("[WEB] /api/time GET request received");
        StaticJsonDocument<256> doc;
        DateTime dt;
        if (getGlobalDateTime(dt)) {
            Serial.println("[WEB] Successfully got datetime");
            char dateStr[11];
            snprintf(dateStr, sizeof(dateStr), "%04d-%02d-%02d", 
                    dt.year, dt.month, dt.day);
            doc["date"] = dateStr;
            
            char timeStr[9];
            snprintf(timeStr, sizeof(timeStr), "%02d:%02d:%02d", 
                    dt.hour, dt.minute, dt.second);
            doc["time"] = timeStr;
            
            doc["timezone"] = networkConfig.timezone;
            doc["ntpEnabled"] = networkConfig.ntpEnabled;
            doc["dst"] = networkConfig.dstEnabled;
            
            // Add NTP status information
            if (networkConfig.ntpEnabled) {
                // Calculate NTP status
                uint8_t ntpStatus = NTP_STATUS_FAILED;
                uint32_t timeSinceLastUpdate = 0;
                
                if (lastNTPUpdateTime > 0) {
                    timeSinceLastUpdate = millis() - lastNTPUpdateTime;
                    if (timeSinceLastUpdate < (NTP_UPDATE_INTERVAL * 3)) {
                        ntpStatus = NTP_STATUS_CURRENT;
                    } else {
                        ntpStatus = NTP_STATUS_STALE;
                    }
                }
                
                doc["ntpStatus"] = ntpStatus;
                
                // Format last update time
                if (lastNTPUpdateTime > 0) {
                    char lastUpdateStr[20];
                    uint32_t seconds = timeSinceLastUpdate / 1000;
                    uint32_t minutes = seconds / 60;
                    uint32_t hours = minutes / 60;
                    uint32_t days = hours / 24;
                    
                    if (days > 0) {
                        snprintf(lastUpdateStr, sizeof(lastUpdateStr), "%d days ago", days);
                    } else if (hours > 0) {
                        snprintf(lastUpdateStr, sizeof(lastUpdateStr), "%d hours ago", hours);
                    } else if (minutes > 0) {
                        snprintf(lastUpdateStr, sizeof(lastUpdateStr), "%d minutes ago", minutes);
                    } else {
                        snprintf(lastUpdateStr, sizeof(lastUpdateStr), "%d seconds ago", seconds);
                    }
                    doc["lastNtpUpdate"] = lastUpdateStr;
                } else {
                    doc["lastNtpUpdate"] = "Never";
                }
            }
            
            String response;
            serializeJson(doc, response);
            Serial.printf("[WEB] Sending /api/time response (%d bytes)\n", response.length());
            server.send(200, "application/json", response);
            Serial.println("[WEB] /api/time response sent successfully");
        } else {
            Serial.println("[WEB] ERROR: Failed to get current time");
            server.send(500, "application/json", "{\"error\": \"Failed to get current time\"}");
        }
    });

    server.on("/api/time", HTTP_POST, []() {
        StaticJsonDocument<200> doc;
        String json = server.arg("plain");
        DeserializationError error = deserializeJson(doc, json);

        log(LOG_INFO, true, "Received JSON: %s\n", json.c_str());
        
        if (error) {
            server.send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
            log(LOG_ERROR, true, "JSON parsing error: %s\n", error.c_str());
            return;
        }

        // Validate required fields
        if (!doc.containsKey("date") || !doc.containsKey("time")) {
            server.send(400, "application/json", "{\"error\":\"Missing required fields\"}");
            log(LOG_ERROR, true, "Missing required fields in JSON\n");
            return;
        }

        // Update timezone if provided
        if (doc.containsKey("timezone")) {
            const char* tz = doc["timezone"];
            log(LOG_INFO, true, "Received timezone: %s\n", tz);
            // Basic timezone format validation (+/-HH:MM)
            int tzHour, tzMin;
            if (sscanf(tz, "%d:%d", &tzHour, &tzMin) != 2 ||
                tzHour < -12 || tzHour > 14 || tzMin < 0 || tzMin > 59) {
                server.send(400, "application/json", "{\"error\":\"Invalid timezone format\"}");
                return;
            }
            strncpy(networkConfig.timezone, tz, sizeof(networkConfig.timezone) - 1);
            networkConfig.timezone[sizeof(networkConfig.timezone) - 1] = '\0';
            log(LOG_INFO, true, "Updated timezone: %s\n", networkConfig.timezone);
        }

        // Update NTP enabled status if provided
        if (doc.containsKey("ntpEnabled")) {
            bool ntpWasEnabled = networkConfig.ntpEnabled;
            networkConfig.ntpEnabled = doc["ntpEnabled"];
            if (networkConfig.ntpEnabled) {
                // Update DST setting if provided
                if (doc.containsKey("dstEnabled")) {
                    networkConfig.dstEnabled = doc["dstEnabled"];
                }
                handleNTPUpdates(true);
                server.send(200, "application/json", "{\"status\": \"success\", \"message\": \"NTP enabled, manual time update ignored\"}");
                saveNetworkConfig();
                return;
            }
            if (ntpWasEnabled) {
                server.send(200, "application/json", "{\"status\": \"success\", \"message\": \"NTP disabled, manual time update required\"}");
                saveNetworkConfig();
            }
        }

        // Validate and parse date and time
        const char* dateStr = doc["date"];
        uint16_t year;
        uint8_t month, day;
        const char* timeStr = doc["time"];
        uint8_t hour, minute;

        // Parse date string (format: YYYY-MM-DD)
        if (sscanf(dateStr, "%hu-%hhu-%hhu", &year, &month, &day) != 3 ||
            year < 2000 || year > 2099 || month < 1 || month > 12 || day < 1 || day > 31) {
            server.send(400, "application/json", "{\"error\": \"Invalid date format or values\"}");
            log(LOG_ERROR, true, "Invalid date format or values in JSON\n");
            return;
        }

        // Parse time string (format: HH:MM)          
        if (sscanf(timeStr, "%hhu:%hhu", &hour, &minute) != 2 ||
            hour > 23 || minute > 59) {
            server.send(400, "application/json", "{\"error\": \"Invalid time format or values\"}");
            return;
        }

        DateTime newDateTime = {year, month, day, hour, minute, 0};
        if (updateGlobalDateTime(newDateTime)) {
            server.send(200, "application/json", "{\"status\": \"success\"}");
        } else {
            server.send(500, "application/json", "{\"error\": \"Failed to update time\"}");
        }
    });
}
