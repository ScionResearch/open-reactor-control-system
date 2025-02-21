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

void debug_printf(uint8_t logLevel, const char* format, ...) {
    // Create a buffer to store the output
    static char buffer[DEBUG_PRINTF_BUFFER_SIZE];
    
    // Acquire the Mutex: Blocks until the Mutex becomes available.
    if (xSemaphoreTake(serialMutex, portMAX_DELAY) == pdTRUE) {
      
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
            Serial.print(buffer);
        } else {
            Serial.println("Error during debug_printf: formatting error or buffer overflow");
        }

        // Release the mutex
        xSemaphoreGive(serialMutex);
    } else {
        // Error: Failed to acquire the Mutex. Print error message on the unprotected port
        Serial.println("Error: Failed to acquire Serial Mutex for debug_printf!");
    }
}

void osDebugPrint(void)
{
  const char *taskStateName[5] = {
      "Ready",
      "Blocked",
      "Suspended",
      "Deleted",
      "Invalid"};
  int numberOfTasks = uxTaskGetNumberOfTasks();

  /*DateTime current;
  if (getGlobalDateTime(current))
  {
    // Use the current time safely
    debug_printf(LOG_INFO, "Time: %02d:%02d:%02d\n",
                  current.hour, current.minute, current.second);
  }*/

  TaskStatus_t *pxTaskStatusArray = new TaskStatus_t[numberOfTasks];
  uint32_t runtime;
  numberOfTasks = uxTaskGetSystemState(pxTaskStatusArray, numberOfTasks, &runtime);
  debug_printf(LOG_INFO, "Tasks: %d\n", numberOfTasks);
  for (int i = 0; i < numberOfTasks; i++)
  {
    debug_printf(LOG_INFO, "ID: %d %s", i, pxTaskStatusArray[i].pcTaskName);
    int currentState = pxTaskStatusArray[i].eCurrentState;
    debug_printf(LOG_INFO, " Current state: %s", taskStateName[currentState]);
    debug_printf(LOG_INFO, " Priority: %u\n", pxTaskStatusArray[i].uxBasePriority);
    debug_printf(LOG_INFO, " Free stack: %u\n", pxTaskStatusArray[i].usStackHighWaterMark);
    debug_printf(LOG_INFO, " Runtime: %u\n", pxTaskStatusArray[i].ulRunTimeCounter);
  }
  delete[] pxTaskStatusArray;
}