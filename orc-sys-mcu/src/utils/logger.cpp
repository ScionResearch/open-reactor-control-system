#include "logger.h"

// Serial port mutex
SemaphoreHandle_t serialMutex = NULL;

bool serialReady = false;

// Log entry types
const char *logType[] = {"INFO", "WARNING", "ERROR", "DEBUG"};

void init_logger(void) {
    Serial.begin(115200);
    uint32_t terminalTimout = millis() + 5000;
    while (!Serial) {
        if (millis() > terminalTimout) {
            break;
        }
    }
    // Create serial debug mutex
    serialMutex = xSemaphoreCreateMutex();
    if (serialMutex == NULL) {
        Serial.println("[ERROR] Failed to create serial mutex");
    }
    else serialReady = true;
}

void log(uint8_t logLevel, bool logToSD, const char* format, ...) {
    // Create a buffer to store the output
    static char buffer[DEBUG_PRINTF_BUFFER_SIZE];
      
    // Prepare the log level string.
    const char* logLevelStr = (logLevel < sizeof(logType) / sizeof(logType[0])) ? logType[logLevel] : "UNKNOWN";
    
    // Format the string into the buffer.
    va_list args;
    va_start(args, format);
    int len = snprintf(buffer, DEBUG_PRINTF_BUFFER_SIZE, "[%s] ", logLevelStr); // Add log level pretext to buffer.

    if (len < DEBUG_PRINTF_BUFFER_SIZE) {
        len += vsnprintf(buffer + len, DEBUG_PRINTF_BUFFER_SIZE - len, format, args);
    } else {
        len = DEBUG_PRINTF_BUFFER_SIZE; //Ensure we don't write past buffer
    }

    va_end(args);
    
    // Check for errors or truncation.
    if(len > 0) {
        if (logToSD) writeLog(buffer);
        if (debug && xSemaphoreTake(serialMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            Serial.print(buffer);
            xSemaphoreGive(serialMutex);
        }
    } else if (debug && xSemaphoreTake(serialMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        Serial.println("Error during logging: formatting error or buffer overflow");
        xSemaphoreGive(serialMutex);
    }
}